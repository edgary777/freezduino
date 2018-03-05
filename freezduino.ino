/**
 * Name     : Freezduino
 * Author   : Edgar Solis Vizcarra  
 * Date     : 2018/03/04
 * Modified : 2018/03/05
 * Version  : V0.1.1
 * Notes    : Sketch designed for the control of a walk-in freezer evaporator.
 *    It takes input from a temperature sensor and uses it to control
 *    3 relays.
 *    Relay 1 controls the solenoid valve on the evaporator that makes the
 *    freezer compressor turn on and off.
 *    Relay 2 controls the fans that push air throught the freezer diffusor.
 *    Relay 3 controls the heating elements that defrost the diffusor at the
 *    time intervals set by the user.
 *    
 *    A 16x2 LCD display and 3 buttons are used for user interaction.
 *    
 *    There's also an on/off button
**/

#include <LiquidCrystal.h>
#include <Wire.h>
#include "Adafruit_MCP9808.h"

// initialize the LCD library by associating any needed LCD interface pin
// with the arduino pin number it is connected to
const int rs = 12, en = 11, d4 = 5, d5 = 4, d6 = 3, d7 = 2;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);

// initialize the temp sensor library, as it connects via I2C the pins are defined in the library.
// SCL = A5 on UNO || D21 on MEGA
// SDA = A4 on UNO || D20 on MEGA
// vdd = 5v
// gnd = gnd
// the other pins are not needed.
Adafruit_MCP9808 tempsensor = Adafruit_MCP9808();


// Custom characters for the lcd.
byte thermometer[8] = {
  0b00100,
  0b01010,
  0b01010,
  0b01110,
  0b01110,
  0b10111,
  0b11111,
  0b01110
};

byte degree[8] = {
  0b00111,
  0b00101,
  0b00111,
  0b00000,
  0b00000,
  0b00000,
  0b00000,
  0b00000,
};

void setup() {
  // initialize LCD and set up the number of columns and rows:
  lcd.begin(16, 2);

  // Check if the temp sensor is connected.
  if (!tempsensor.begin()) {
    // The temperature sensor was not found, send error message.
    lcd.setCursor(0, 0);
    lcd.print("Sensor no");
    lcd.setCursor(0, 1);
    lcd.print("conectado!");
    while (1); // loop until reset.
  }
  
  // create new characters
  lcd.createChar(0, thermometer);
  lcd.createChar(1, degree);  
}

void loop() {
  lcd.setCursor(0, 0); // set the cursor to the top row, 1st position
  lcd.write(byte(0)); // thermometer character
  float c = tempsensor.readTempC(); // read temp
  lcd.print(c); // write temp
  lcd.write(byte(1)); // degree character
  lcd.print("C");
  delay(1200);
  lcd.clear();
}
