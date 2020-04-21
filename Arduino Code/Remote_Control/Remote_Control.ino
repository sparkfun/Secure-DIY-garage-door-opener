/*
  Remote Control
  This sketch is used to create a cryptographically secure wireless controller to open you garage.
  The complete system uses SparkFun Pro RF modules, Cryptographic Co-processors, and the qwiic relay.
  Note, it also requires a base transceiver setup with separate sketch.

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
*/

#include <SPI.h>

//Radio Head Library:
#include <RH_RF95.h>

// We need to provide the RFM95 module's chip select and interrupt pins to the
// rf95 instance below.On the SparkFun ProRF those pins are 12 and 6 respectively.
RH_RF95 rf95(12, 6);

int LED = 13; //Status LED is on pin 13

int packetCounter = 0; //Counts the number of packets sent
long timeSinceLastPacket = 0; //Tracks the time stamp of last packet received

// The broadcast frequency is set to 921.2, but the SADM21 ProRf operates
// anywhere in the range of 902-928MHz in the Americas.
// Europe operates in the frequencies 863-870, center frequency at 868MHz.
// This works but it is unknown how well the radio configures to this frequency:
//float frequency = 864.1;
float frequency = 921.2; //Broadcast frequency


//////////////crypto stuff

#include <SparkFun_ATECCX08a_Arduino_Library.h> //Click here to get the library: http://librarymanager/All#SparkFun_ATECCX08a
#include <Wire.h>

ATECCX08A atecc;

uint8_t token[32]; // time to live token, created randomly each authentication event

uint8_t BasePublicKey[64] = {
  0x56, 0x4C, 0x07, 0x3C, 0x44, 0x3D, 0xF3, 0xC0, 0x2F, 0x3C, 0x7F, 0xCF, 0x39, 0xBF, 0x87, 0x31,
  0x99, 0xCC, 0x10, 0x1A, 0x1F, 0xE5, 0x3F, 0x7B, 0xE7, 0x62, 0x9D, 0x82, 0xA6, 0xDB, 0xCC, 0xC6,
  0x17, 0xCF, 0x9E, 0x1E, 0xEF, 0x19, 0x56, 0x37, 0x3E, 0xAB, 0x55, 0x3E, 0x87, 0x5F, 0x56, 0x61,
  0xF9, 0xCB, 0x13, 0x58, 0x88, 0x58, 0xE9, 0xF4, 0x47, 0xDA, 0xD0, 0xCC, 0x3E, 0xDB, 0x59, 0xC3
};

uint8_t baseSignature[64]; // used to store incoming signature from Base

/////////////

byte buf[RH_RF95_MAX_MESSAGE_LEN];

void setup()
{
  pinMode(LED, OUTPUT);

  SerialUSB.begin(115200);
  // It may be difficult to read serial messages on startup. The following line
  // will wait for serial to be ready before continuing. Comment out if not needed.
  //while (!SerialUSB);
  SerialUSB.println("RFM Client!");

  //Initialize the Radio.
  if (rf95.init() == false) {
    SerialUSB.println("Radio Init Failed - Freezing");
    while (1);
  }
  else {
    //An LED inidicator to let us know radio initialization has completed.
    //SerialUSB.println("Transmitter up!");
    //digitalWrite(LED, HIGH);
    //delay(500);
    //digitalWrite(LED, LOW);
    //delay(500);
  }

  // Set frequency
  rf95.setFrequency(frequency);

  // The default transmitter power is 13dBm, using PA_BOOST.
  // If you are using RFM95/96/97/98 modules which uses the PA_BOOST transmitter pin, then
  // you can set transmitter powers from 5 to 23 dBm:
  // Transmitter power can range from 14-20dbm.
  rf95.setTxPower(14, false);

  pinMode(5, INPUT_PULLUP);

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

  attempt_cycle();

}


void loop()
{

  if (digitalRead(5) == LOW)
  {
    //    attempt_cycle();
  }
  delay(10); // button debounce
}


void printBuf96()
{
  SerialUSB.println();
  SerialUSB.println("uint8_t buf[96] = {");
  for (int i = 0; i < 96 ; i++)
  {
    SerialUSB.print("0x");
    if ((buf[i] >> 4) == 0) SerialUSB.print("0"); // print preceeding high nibble if it's zero
    SerialUSB.print(buf[i], HEX);
    if (i != 31) SerialUSB.print(", ");
    if ((31 - i) % 16 == 0) SerialUSB.println();
  }
  SerialUSB.println("};");
  SerialUSB.println();
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


// Note, in Example4_Alice we are printing the signature we JUST created,
// and it lives inside the library as a public array called "atecc.signature"
void printSignature()
{
  SerialUSB.println("uint8_t signature[64] = {");
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

void attempt_cycle()
{
  SerialUSB.println("Sending message");

  //Send a message to the other radio
  uint8_t toSend[] = "$$$";
  //sprintf(toSend, "Hi, my counter is: %d", packetCounter++);
  rf95.send(toSend, sizeof(toSend));
  rf95.waitPacketSent();

  // Now wait for a reply

  byte len = sizeof(buf);

  if (rf95.waitAvailableTimeout(2000)) {
    // Should be a reply message for us now
    if (rf95.recv(buf, &len)) {
      SerialUSB.print("Got reply: ");
      printBuf96();
      for (int i = 0 ; i < 32 ; i++) token[i] = buf[i]; // read in token from buffer.
      printToken(); // nice debug to see what token we just sent. see function below

      for (int i = 0 ; i < 64 ; i++) baseSignature[i] = buf[i + 32]; // read in Base signature from buffer.
      printBaseSignature(); // nice debug to see what signautre we just received. see function below

      // Let's verirfy the base is legit!
      if (atecc.verifySignature(token, baseSignature, BasePublicKey))
      {
        SerialUSB.println("Success! Base Signature Verified.");

        SerialUSB.println("\n\rCreating signature with TTL-token and my private key (remote)...");
        
        boolean sigStat = false;
        sigStat = atecc.createSignature(token); // by default, this uses the private key securely stored and locked in slot 0.

        SerialUSB.print("sigStat: ");
        SerialUSB.println(sigStat);

        //printSignature();
        
        //Send signature to the Base
        rf95.send(atecc.signature, sizeof(atecc.signature));
        rf95.waitPacketSent();
        SerialUSB.println("\n\rSent signature!");
        //SerialUSB.println((char*)buf);
        //SerialUSB.print(" RSSI: ");
        //SerialUSB.print(rf95.lastRssi(), DEC);
      }
      else
      {
        SerialUSB.println("Base Verification failure. Ignoring...");
      }
    }
    else {
      SerialUSB.println("Receive failed");
    }
  }
  else {
    SerialUSB.println("No reply, is the receiver running?");
  }
  delay(500);
}

void printBaseSignature()
{
  SerialUSB.println("uint8_t baseSignature[64] = {");
  for (int i = 0; i < sizeof(baseSignature) ; i++)
  {
    SerialUSB.print("0x");
    if ((baseSignature[i] >> 4) == 0) SerialUSB.print("0"); // print preceeding high nibble if it's zero
    SerialUSB.print(baseSignature[i], HEX);
    if (i != 63) SerialUSB.print(", ");
    if ((63 - i) % 16 == 0) SerialUSB.println();
  }
  SerialUSB.println("};");
  SerialUSB.println();
}
