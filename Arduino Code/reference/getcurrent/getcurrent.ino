#include <Wire.h>
#include <Adafruit_INA219.h>

Adafruit_INA219 ina219;

long current_time;

#include <Servo.h>

Servo myservo;  // create servo object to control a servo
// twelve servo objects can be created on most boards

int pos = 0;    // variable to store the servo position

float current_mA = 0;

void setup(void)
{
  Serial.begin(230400);
  while (!Serial) {
    // will pause Zero, Leonardo, etc until serial console opens
    delay(1);
  }

  uint32_t currentFrequency;

  Serial.println("Hello!");

  // Initialize the INA219.
  // By default the initialization will use the largest range (32V, 2A).  However
  // you can call a setCalibration function to change this range (see comments).
  ina219.begin();
  // To use a slightly lower 32V, 1A range (higher precision on amps):
  //ina219.setCalibration_32V_1A();
  // Or to use a lower 16V, 400mA range (higher precision on volts and amps):
  //ina219.setCalibration_16V_400mA();

  Serial.println("Current(mA)");
  current_time = millis();

  myservo.attach(9);  // attaches the servo on pin 9 to the servo object
  myservo.write(0);
  delay(1000);
}

void loop(void)
{

  myservo.write(180);

while(1)
{
  current_mA = ina219.getCurrent_mA();

  //  Serial.print(millis() - current_time );
  //  Serial.print(" ");
  Serial.println(current_mA); //Serial.println(" mA");

  current_time = millis();


  //  Serial.print("Power:         "); Serial.print(power_mW); Serial.println(" mW");
  //  Serial.println("");

  //delay(1);
}
}
