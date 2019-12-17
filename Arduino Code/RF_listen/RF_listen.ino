#include <SPI.h>

//Radio Head Library:
#include <RH_RF95.h>

// We need to provide the RFM95 module's chip select and interrupt pins to the
// rf95 instance below.On the SparkFun ProRF those pins are 12 and 6 respectively.
RH_RF95 rf95(12, 6);

int LED = 13; //Status LED on pin 13

int packetCounter = 0; //Counts the number of packets sent
long timeSinceLastPacket = 0; //Tracks the time stamp of last packet received
// The broadcast frequency is set to 921.2, but the SADM21 ProRf operates
// anywhere in the range of 902-928MHz in the Americas.
// Europe operates in the frequencies 863-870, center frequency at
// 868MHz.This works but it is unknown how well the radio configures to this frequency:
//float frequency = 864.1;
float frequency = 921.2;

/////////////////crypto

#include <SparkFun_ATECCX08a_Arduino_Library.h> //Click here to get the library: http://librarymanager/All#SparkFun_ATECCX08a
#include <Wire.h>

ATECCX08A atecc;

uint8_t token[32]; // time to live token, created randomly each authentication event

uint8_t signature[64]; // incoming signature from Alice

int headerCount = 0; // used to count incoming "$", when we reach 3 we know it's a good fresh new message.

// Delete this "blank" public key,
// copy/paste Alice's true unique public key from her terminal printout in Example6_Challenge_Alice.

uint8_t AlicesPublicKey[64] = {
  0x38, 0xD6, 0xE5, 0x49, 0xAC, 0x57, 0x2D, 0x1F, 0xD0, 0x58, 0x0A, 0xE8, 0x59, 0xB8, 0xF8, 0x20,
  0x1E, 0x0A, 0x7E, 0x8D, 0x5B, 0x7D, 0xD9, 0x8A, 0x26, 0xAF, 0x88, 0x73, 0x6D, 0x8C, 0xB7, 0x2D,
  0x8D, 0x3A, 0xB9, 0x5F, 0x60, 0x9D, 0x3F, 0x49, 0x72, 0xF1, 0x44, 0x74, 0x82, 0x3F, 0x7B, 0xCF,
  0x1F, 0x18, 0xD3, 0xA4, 0xBF, 0x62, 0x15, 0xCC, 0xAF, 0xAD, 0x7E, 0x03, 0xD8, 0xE9, 0x93, 0x7E
};

void setup()
{
  pinMode(LED, OUTPUT);

  SerialUSB.begin(115200);
  // It may be difficult to read serial messages on startup. The following
  // line will wait for serial to be ready before continuing. Comment out if not needed.
  while (!SerialUSB);
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


  if (atecc.begin() == true)
  {
    SerialUSB.println("Successful wakeUp(). I2C connections are good.");
  }
  else
  {
    SerialUSB.println("Device not found. Check wiring.");
    while (1); // stall out forever
  }

}

void loop()
{
  if (rf95.available()) {
    // Should be a message for us now
    uint8_t buf[RH_RF95_MAX_MESSAGE_LEN];
    uint8_t len = sizeof(buf);

    if (rf95.recv(buf, &len)) {
      digitalWrite(LED, HIGH); //Turn on status LED
      timeSinceLastPacket = millis(); //Timestamp this packet

      SerialUSB.print("Got message: ");
      SerialUSB.print((char*)buf);
      //SerialUSB.print(" RSSI: ");
      //SerialUSB.print(rf95.lastRssi(), DEC);
      SerialUSB.println();

      // Send a reply

            SerialUSB.print("Creating a new random TTL-token now...");

      // update library instance public variable.
      atecc.updateRandom32Bytes();
 
      uint8_t toSend[32];
      // copy from library public variable into our local variable
      // also send each byte  to Alice.
      for (int i = 0 ; i < 32 ; i++)
      {
        toSend[i] = atecc.random32Bytes[i]; // store locally
      }

      rf95.send(toSend, sizeof(toSend));
      rf95.waitPacketSent();
      SerialUSB.println("Sent a reply");
      digitalWrite(LED, LOW); //Turn off status LED

    }
    else
      SerialUSB.println("Recieve failed");
  }
  //Turn off status LED if we haven't received a packet after 1s
  if (millis() - timeSinceLastPacket > 1000) {
    digitalWrite(LED, LOW); //Turn off status LED
    timeSinceLastPacket = millis(); //Don't write LED but every 1s
  }
}
