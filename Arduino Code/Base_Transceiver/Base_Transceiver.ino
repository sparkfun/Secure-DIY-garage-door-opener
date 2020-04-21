/*
  Base Transceiver
  This sketch is used to create a cryptographically secure wireless controller to open you garage.
  The complete system uses SparkFun Pro RF modules, Cryptographic Co-processors, and the qwiic relay.
  Note, it also requires a remote control setup with separate sketch.

  See the complete tutorial here:
  https://learn.sparkfun.com/tutorials/secure-diy-garage-door-opener

  By: Pete Lewis
  SparkFun Electronics
  Date: January 13th, 2020
  License: This code is public domain but you can buy me a beer if you use this and we meet someday (Beerware license).

  Feel like supporting our work? Please buy a board from SparkFun!
  https://www.sparkfun.com/products/15573

  Some of this code is a modified version of the example provided by the Radio Head
  Library which can be found here:
  www.github.com/PaulStoffregen/RadioHeadd

  Some of this code is a modified version of the example provided by the SparkFun ATECCX08a
  Arduino Library which can be found here:
  https://github.com/sparkfun/SparkFun_ATECCX08a_Arduino_Library

  Some of the code is a modified version of the example provided by the SparkFun Qwiic
  Relay Arduino Library which can be found here:
  https://github.com/sparkfun/SparkFun_Qwiic_Relay_Arduino_Library

  Changes 2/27/2020
  As brought to my attention by SparkFun customer #437 in a comment on the tutorial
  https://learn.sparkfun.com/tutorials/secure-diy-garage-door-opener/discuss#comment-5e54522397c4d30f2a423da3
  previous code was susceptible to a reverse roll jam attack, where the attacker impersonates the base,
  and prevents the base from invalidating the token by sending repeated invalid signatures.
  Also, the fact that it was exposed in the source code that an invalid token was all 0x00s,
  meant that the attacker to ask the remote to sign a null token (all 0x00s), and then use this
  any time they wish. Thanks #437!!!
*/



#include <SPI.h>

//Radio Head Library:
#include <RH_RF95.h>

// We need to provide the RFM95 module's chip select and interrupt pins to the
// rf95 instance below.On the SparkFun ProRF those pins are 12 and 6 respectively.
RH_RF95 rf95(12, 6);

int LED = 13; //Status LED on pin 13

int successLED = 3; // green
int failLED = 4; // red
int statLED = 2; // blue

int packetCounter = 0; //Counts the number of packets sent
long timeSinceLastPacket = 0; //Tracks the time stamp of last packet received
// The broadcast frequency is set to 921.2, but the SADM21 ProRf operates
// anywhere in the range of 902-928MHz in the Americas.
// Europe operates in the frequencies 863-870, center frequency at
// 868MHz.This works but it is unknown how well the radio configures to this frequency:
//float frequency = 864.1;
float frequency = 921.2;

uint8_t buf[RH_RF95_MAX_MESSAGE_LEN];


////////////////relay

#include "SparkFun_Qwiic_Relay.h"

#define RELAY_ADDR 0x18 // Alternate address 0x19


Qwiic_Relay relay(RELAY_ADDR);

////////////////

/////////////////crypto

#include <SparkFun_ATECCX08a_Arduino_Library.h> //Click here to get the library: http://librarymanager/All#SparkFun_ATECCX08a
#include <Wire.h>

ATECCX08A atecc;

uint8_t token[32]; // time to live token, created randomly each authentication event

uint8_t signature[64]; // incoming signature from Alice

int headerCount = 0; // used to count incoming "$", when we reach 3 we know it's a good fresh new message.

#define VALID 1
#define INVALID 0
boolean tokenStatus = INVALID;  // used to keep track of a complete cycle (and prevent repeat false signature attempts)
// true = currently ready to attempt a verification
// false = timed out or a single verification success or failure has happened

// Alice's public key.
// Note, this will be unique to each co-processor, so your will be different.
// copy/paste Alice's true unique public key from her terminal printout in Example6_Challenge_Alice.

uint8_t RemotePublicKey[64] = {
  0x38, 0xD6, 0xE5, 0x49, 0xAC, 0x57, 0x2D, 0x1F, 0xD0, 0x58, 0x0A, 0xE8, 0x59, 0xB8, 0xF8, 0x20,
  0x1E, 0x0A, 0x7E, 0x8D, 0x5B, 0x7D, 0xD9, 0x8A, 0x26, 0xAF, 0x88, 0x73, 0x6D, 0x8C, 0xB7, 0x2D,
  0x8D, 0x3A, 0xB9, 0x5F, 0x60, 0x9D, 0x3F, 0x49, 0x72, 0xF1, 0x44, 0x74, 0x82, 0x3F, 0x7B, 0xCF,
  0x1F, 0x18, 0xD3, 0xA4, 0xBF, 0x62, 0x15, 0xCC, 0xAF, 0xAD, 0x7E, 0x03, 0xD8, 0xE9, 0x93, 0x7E
};

void setup()
{
  pinMode(LED, OUTPUT);
  pinMode(successLED, OUTPUT);
  pinMode(failLED, OUTPUT);
  pinMode(statLED, OUTPUT);

  SerialUSB.begin(115200);
  // It may be difficult to read serial messages on startup. The following
  // line will wait for serial to be ready before continuing. Comment out if not needed.
  //while (!SerialUSB);
  SerialUSB.println("RFM Server!");

  //Initialize the Radio.
  if (rf95.init() == false) {
    SerialUSB.println("Radio Init Failed - Freezing");
    while (1);
  }
  else {
    // An LED indicator to let us know radio initialization has completed.
    SerialUSB.println("Receiver up!");
    digitalWrite(LED, HIGH);
    delay(500);
    digitalWrite(LED, LOW);
    delay(500);
  }

  rf95.setFrequency(frequency);

  // The default transmitter power is 13dBm, using PA_BOOST.
  // If you are using RFM95/96/97/98 modules which uses the PA_BOOST transmitter pin, then
  // you can set transmitter powers from 5 to 23 dBm:
  // rf95.setTxPower(14, false);


  Wire.begin();

  if (atecc.begin() == true)
  {
    SerialUSB.println("Successful wakeUp(). I2C connections are good.");
  }
  else
  {
    SerialUSB.println("Device not found. Check wiring.");
    while (1); // stall out forever
  }

  if (!relay.begin())
  {
    Serial.println("Check connections to Qwiic Relay.");
    while (1); // stall out forever
  }
  else
  {
    Serial.println("Qwiic Relay is ready to go.");
  }

}

void loop()
{
  if (rf95.available()) {
    // Should be a message for us now

    uint8_t len = sizeof(buf);

    if (rf95.recv(buf, &len)) {
      digitalWrite(LED, HIGH); //Turn on status LED
      timeSinceLastPacket = millis(); //Timestamp this packet

      SerialUSB.print("Got message: ");
      printBuf64();
      //SerialUSB.print(" RSSI: ");
      //SerialUSB.print(rf95.lastRssi(), DEC);
      SerialUSB.println();

      // if message from Alice is "$$$", Send a random token for her to sign.
      if (buf[0] == '$' && buf[1] == '$' && buf[2] == '$')
      {
        SerialUSB.println("Received $$$. Creating a new random TTL-token now...");

        refreshToken();
        tokenStatus = VALID; // Keep track of status, this will be used later to prevent repeated invalid signature attempts
        printToken();

        // Create a signature on the newly refreshed token
        SerialUSB.println("Creating signature with TTL-token and my private key (base)...");
        boolean sigStat = false;
        sigStat = atecc.createSignature(token); // by default, this uses the private key securely stored and locked in slot 0.
        SerialUSB.print("sigStat: ");
        SerialUSB.println(sigStat);
        printBaseSignature();

        uint8_t toSend[96]; // buffer to send to remote (32 for token + 64 for base's signature = 96 total)
        for (int i = 0 ; i < 32 ; i++) toSend[i] = token[i]; // place token in first 32 bytes of toSend
        for (int i = 0 ; i < 64 ; i++) toSend[i + 32] = atecc.signature[i]; // place base's signature of token in reamining 64 bytes of toSend

        rf95.send(toSend, sizeof(toSend));
        rf95.waitPacketSent();
        SerialUSB.println("Sent token + Base signature");
        digitalWrite(LED, LOW); //Turn off status LED
      }
      else // this means Alice just sent us a signature
      {
        if (tokenStatus == VALID)
        {
          SerialUSB.println("Received signature. Verifying now...");
          for (int i = 0 ; i < 64 ; i++) signature[i] = buf[i]; // read in signature from buffer.
          printSignature();
          // Let's verirfy!
          if (atecc.verifySignature(token, signature, RemotePublicKey))
          {
            SerialUSB.println("Success! Signature Verified.");
            // Let's turn on the relay...
            relay.turnRelayOn();
            delay(1000);
            // Let's turn that relay off...
            relay.turnRelayOff();
            blinkStatus(successLED);
            tokenStatus = INVALID;
          }
          else
          {
            SerialUSB.println("Verification failure.");
            blinkStatus(failLED);
            tokenStatus = INVALID; // a single faliure now requires a complete new cycle (most importantly a new token)
          }
        }
        else
        {
          SerialUSB.println("Received message. However, token status is currently INVALID...");
        }
      }

    }
    else
      SerialUSB.println("Recieve failed");
  }
  //Turn off status LED if we haven't received a packet after 1s
  if ((millis() - timeSinceLastPacket > 1000) && (tokenStatus == VALID)) {
    digitalWrite(LED, LOW); //Turn off status LED
    timeSinceLastPacket = millis(); //Don't write LED but every 1s
    clearBuf();
    // "destroy" token, to require a new cycle if this doesn't complete within 1 second
    // Actually, we are not going to destroy the token, we are going to create a new random token
    // using the crypto co-processor. This way the currently saved "bad" token is not exposed
    // here in the source code.
    SerialUSB.println("Timed Out...");
    SerialUSB.println("Destroying Token (aka refreshing it so the contents of token is not exposed in source code)");
    refreshToken();
    tokenStatus = INVALID;
  }
}


void printSignature()
{
  SerialUSB.println("uint8_t signature[64] = {");
  for (int i = 0; i < sizeof(signature) ; i++)
  {
    SerialUSB.print("0x");
    if ((signature[i] >> 4) == 0) SerialUSB.print("0"); // print preceeding high nibble if it's zero
    SerialUSB.print(signature[i], HEX);
    if (i != 63) SerialUSB.print(", ");
    if ((63 - i) % 16 == 0) SerialUSB.println();
  }
  SerialUSB.println("};");
  SerialUSB.println();
}

void printBaseSignature()
{
  SerialUSB.println("uint8_t BASEsignature[64] = {");
  for (int i = 0; i < sizeof(atecc.signature) ; i++)
  {
    SerialUSB.print("0x");
    if ((atecc.signature[i] >> 4) == 0) SerialUSB.print("0"); // print preceeding high nibble if it's zero
    SerialUSB.print(atecc.signature[i], HEX);
    if (i != 63) SerialUSB.print(", ");
    if ((63 - i) % 16 == 0) SerialUSB.println();
  }
  SerialUSB.println("};");
  SerialUSB.println();
}

void printBuf64()
{
  SerialUSB.println();
  SerialUSB.println("uint8_t buf[64] = {");
  for (int i = 0; i < 64 ; i++)
  {
    SerialUSB.print("0x");
    if ((buf[i] >> 4) == 0) SerialUSB.print("0"); // print preceeding high nibble if it's zero
    SerialUSB.print(buf[i], HEX);
    if (i != 63) SerialUSB.print(", ");
    if ((63 - i) % 16 == 0) SerialUSB.println();
  }
  SerialUSB.println("};");
  SerialUSB.println();
}

void clearBuf()
{
  for (int i = 0; i < 64 ; i++) buf[i] = 0x00;
}

void blinkStatus(int LED)
{
  digitalWrite(LED, HIGH);
  delay(2000);
  digitalWrite(LED, LOW);
  delay(500);
}

void refreshToken()
{
  // update library instance public variable.
  atecc.updateRandom32Bytes();

  // copy from library public variable into our local variable
  for (int i = 0 ; i < 32 ; i++)
  {
    token[i] = atecc.random32Bytes[i]; // store locally
  }
}

void printToken()
{
  SerialUSB.println();
  SerialUSB.println("uint8_t token[32] = {");
  for (int i = 0; i < sizeof(token) ; i++)
  {
    SerialUSB.print("0x");
    if ((token[i] >> 4) == 0) SerialUSB.print("0"); // print preceeding high nibble if it's zero
    SerialUSB.print(token[i], HEX);
    if (i != 31) SerialUSB.print(", ");
    if ((31 - i) % 16 == 0) SerialUSB.println();
  }
  SerialUSB.println("};");
  SerialUSB.println();
}
