/*
  Both the TX and RX ProRF boards will need a wire antenna. We recommend a 3" piece of wire.
  This example is a modified version of the example provided by the Radio Head
  Library which can be found here:
  www.github.com/PaulStoffregen/RadioHeadd
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
    SerialUSB.println("Transmitter up!");
    digitalWrite(LED, HIGH);
    delay(500);
    digitalWrite(LED, LOW);
    delay(500);
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

}


void loop()
{

  if (digitalRead(5) == LOW)
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
        printBuf64();
        for (int i = 0 ; i < 32 ; i++) token[i] = buf[i]; // read in token from buffer.
        printToken(); // nice debug to see what token we just sent. see function below

        boolean sigStat = false;
        sigStat = atecc.createSignature(token); // by default, this uses the private key securely stored and locked in slot 0.

        SerialUSB.print("sigStat: ");
        SerialUSB.println(sigStat);
        
        //printSignature();


        // Copy our signature from library array to local toSend array
        //for (int i = 0 ; i < 64 ; i++) toSend[i] = atecc.signature[i]; // store locally

        //Send signature to the other radio
        rf95.send(atecc.signature, sizeof(atecc.signature));
        rf95.waitPacketSent();
        SerialUSB.println("Sent signature");
        //SerialUSB.println((char*)buf);
        //SerialUSB.print(" RSSI: ");
        //SerialUSB.print(rf95.lastRssi(), DEC);
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
  delay(10); // button debounce
}


void printBuf64()
{
  SerialUSB.println();
  SerialUSB.println("uint8_t buf[64] = {");
  for (int i = 0; i < 32 ; i++)
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
