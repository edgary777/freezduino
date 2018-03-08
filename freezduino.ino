/**
   Name     : Freezduino
   Author   : Edgar Solis Vizcarra
   Date     : 2018/03/04
   Modified : 2018/03/05
   Version  : V0.1.2
   Notes    : Sketch designed for the control of a walk-in freezer evaporator.
      It takes input from a temperature sensor and uses it to control
      3 relays.
      relay1 controls the solenoid valve on the evaporator that makes the
      freezer compressor turn on and off.
      relay2 controls the fans that push air throught the freezer diffusor.
      relay3 controls the heating elements that defrost the diffusor at the
      time intervals set by the user.

      A 16x2 LCD display and 3 buttons are used for user interaction.

      There's also an on/off button
*/

#include <LiquidCrystal.h>
#include <Wire.h>
#include "Adafruit_MCP9808.h"
#include <TimedAction.h>

//Refresh rate for the display data.
unsigned long refreshRate = 800;
//anything that uses millis needs to bean unsigned long, otherwise it will cause bugs.
unsigned long b1Millis = 0;
unsigned long b2Millis = 0;
unsigned long b3Millis = 0;

// current state of the main menu
int mainMenuTop = 0;
int mainMenuCurrent = 0;

const int b1Pin = 24, b2Pin = 28, b3Pin = 32, b4Pin = 36;
const int Rcompressor = 6, Rfans = 7, Rdefroster = 8;

int setpoint = 25, tolerance = 2, defrostDuration = 30;
bool evaporator = false, defroster = false;

//global variables to write the temp on the lcdscreen
int lcdTempRow, lcdTempColumn;
bool lcdTempPrecise;

//global variables to write the evaporator status on the lcdscreen.
int lcdEvaporatorRow, lcdEvaporatorColumn;

//global variables to write the defroster status on the lcdscreen.
int lcdDefrosterRow, lcdDefrosterColumn;

// initialize the LCD library by associating any needed LCD interface pin
// with the arduino pin number it is connected to
const int rs = 12, en = 11, d4 = 5, d5 = 4, d6 = 3, d7 = 2;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);

//Dirty menu. When there's a change in the menu this sets it to be updated.
bool dirtyMenu = true;
// menuOn signals when the menu is being shown.
bool menuOn = false;
// Signals when the home screen needs to be cleaned.
bool homeClean = true;

//indicates the currently active submenu, 0 = no active submenu.
int activeSubMenu = 0;

// This variable is shared by all the submenus, it signals the current selection of the submenu.
int subMenuCurrent = 0;

// Temporary setpoint copy storage, this copy is the one you interact with the screen and the real setpoint is set after you leave.
int newSetpoint = setpoint;

// When there's a change in a submenu this sets it to be updated.
bool dirtySubMenu = true;

// Sets the data of this submenu to be updated
int dirtyConfig;

// initialize the temp sensor library, as it connects via I2C the pins are
// defined in the library. SCL = A5 on UNO || D21 on MEGA SDA = A4 on UNO || D20
// on MEGA vdd = 5v gnd = gnd the other pins are not needed.
Adafruit_MCP9808 tempsensor = Adafruit_MCP9808();

// Custom characters for the lcd.
byte thermometer[8] = {
  0b00100,
  0b01010,
  0b01010,
  0b01010,
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

byte fan[8] = {
  0b00000,
  0b00000,
  0b11001,
  0b01011,
  0b00100,
  0b11010,
  0b10011,
  0b00000
};

byte defrosting[8] = {
  0b11000,
  0b10100,
  0b10100,
  0b11000,
  0b00011,
  0b00100,
  0b00100,
  0b00011
};

byte timer[8] = {
  0b00000,
  0b00000,
  0b01110,
  0b10101,
  0b10111,
  0b10001,
  0b01110,
  0b00000
};

byte arrow[8] = {
  0b11000,
  0b11100,
  0b11110,
  0b11111,
  0b11111,
  0b11110,
  0b11100,
  0b11000
};

void setup() {
  Serial.begin(9600);
  pinMode(Rcompressor, OUTPUT);
  digitalWrite(Rcompressor, HIGH);
  pinMode(Rfans, OUTPUT);
  digitalWrite(Rfans, HIGH);
  pinMode(Rdefroster, OUTPUT);
  digitalWrite(Rdefroster, HIGH);

  pinMode(b1Pin, INPUT);
  pinMode(b2Pin, INPUT);
  pinMode(b3Pin, INPUT);
  pinMode(b4Pin, INPUT);

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
  lcd.createChar(2, fan);
  lcd.createChar(3, defrosting);
  lcd.createChar(4, timer);
  lcd.createChar(5, arrow);
}
//This controls how often we check for input from the buttons first value is in ms second is callback function
TimedAction lcdTempThread = TimedAction(refreshRate, lcdTemp);
TimedAction lcdDefrosterThread = TimedAction(refreshRate / 2, lcdDefrost);
TimedAction lcdEvaporatorThread = TimedAction(refreshRate / 2, lcdEvaporator);

void tempControl(int setpoint, int tolerance) {
  /**
     Check temperature, if it is at the setpoint or lower, or higher but within
     the tolerance then turn relay off. If it is higher than the setpoint + the
     tolerance then turn the relay on.
  */
  int temp = round(tempsensor.readTempC()); // round temperature reading.
  if (temp <= setpoint + tolerance) {
    digitalWrite(Rcompressor, LOW);
    digitalWrite(Rfans, LOW);
    evaporator = true;
  } else {
    digitalWrite(Rcompressor, HIGH);
    digitalWrite(Rfans, HIGH);
    evaporator = false;
  }
}

void defrost(bool order = false) {
  /**
     Activate the defrosting function.

     When defrosting is activated the compressor and the fans are turned off
     and the heating element is turned on.
     it must stay like this for the set duration or until the user decides to
     cancel the defrosting.
  */
  if (order == true) {
    digitalWrite(Rdefroster, HIGH);
    defroster = true;
  }
  else {
    defroster = false;
    digitalWrite(Rdefroster, LOW);
  }
}

void lcdEvaporator(int row, int column) {
  /**
     Write the current state of the evaporator on the lcd display.

     It is written LTR starting on the selected row and column.
     it displays either ON or OFF

     it needs 4 spaces.
  */
  lcd.setCursor(lcdEvaporatorColumn, lcdEvaporatorRow);
  lcd.write(byte(2));
  if (evaporator == false) {
    lcd.write("OFF");
  }
  else {
    lcd.write("ON ");
  }
}

void lcdDefrost() {
  /**
     Write the current state of the defroster on the lcd display.

     It is written LTR starting on the selected row and column.
     it displays either ON or OFF

     it needs 4 spaces.
  */
  lcd.setCursor(lcdDefrosterColumn, lcdDefrosterRow);
  lcd.write(byte(3));
  if (defroster == false) {
    lcd.write("OFF");
  }
  else {
    lcd.write("ON ");
  }
}

void lcdTemp() {
  /**
     Write the temperature reading from the sensor to the lcd display.

     It is written LTR starting on the selected row and column.
     By default the temperature is rounded, if precise is set to true it is not
     rounded and it shows 2 decimals.

     It needs 3 spaces when not precise,  when precise it needs 6
  */
  checkTempSensor();
  lcd.setCursor(lcdTempColumn, lcdTempRow); // set the cursor to the top row, 1st position
  lcd.write(byte(0));         // thermometer character
  if (lcdTempPrecise == true) {
    lcd.print("      ");
    lcd.setCursor(lcdTempColumn, lcdTempRow);
    float ftemp = tempsensor.readTempC(); // read temp without rounding
    lcd.print(ftemp);                     // write temp
  } else {
    lcd.print("   ");
    lcd.setCursor(lcdTempColumn, lcdTempRow);
    int itemp = round(tempsensor.readTempC()); // round temperature reading.
    lcd.print(itemp);                          // write temp
  }
  lcd.write(byte(1)); // degree character
  lcd.print("C");
}

void checkTempSensor() {
  /**
     Check if the temp sensor is connected.

     If it is connected do nothing, if it is not connected stop everything.
  */

  if (!tempsensor.begin()) {
    lcd.clear();
    // The temperature sensor was not found, send error message.
    lcd.setCursor(0, 0);
    lcd.print("Sensor no");
    lcd.setCursor(0, 1);
    lcd.print("conectado!");
    while (!tempsensor.begin()) {
    }
  }
}

void killAll() {
  /**
     Kill all processes and show an alert message.
  */
}

void updateTempPos(int column, int row, bool precise) {
  lcdTempColumn = column;
  lcdTempRow = row;
  lcdTempPrecise = precise;
}

void updateEvaporatorPos(int column, int row) {
  lcdEvaporatorColumn = column;
  lcdEvaporatorRow = row;
}

void updateDefrosterPos(int column, int row) {
  lcdDefrosterColumn = column;
  lcdDefrosterRow = row;
}

void homeScreen() {
  updateTempPos(0, 0, true);
  updateEvaporatorPos(12, 0);
  updateDefrosterPos(0, 1);
  lcdTempThread.check();
  lcdEvaporatorThread.check();
  lcdDefrosterThread.check();
}

void setpointConfig() {
  /*
     Display and interact with the temperature setpoint on the LCD display.

     newSetpoint is a temporary copy of the real setpoint, the real setpoint is only updated
     after you leave the menu so you don't change the temp in real time.
  */
  if (dirtySubMenu == true) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.write(byte(0));
    lcd.print(newSetpoint);
    lcd.write(byte(1));
    lcd.print("C");
    dirtySubMenu = false;
  }
}

void menuScreen(int top, int current) {
  /**
     Menu with options to change settings.

     parameters:
     - top: signals the message that is on the top of the screen.
     - current: signals the message that is currently selected.

     We always start with 0, 0

     0- return
     1- setpoint
     2- tolerance
     3- defrost time
     4- toggle fans
     5- toggle evaporator
     6- toggle defroster
     7- shutdown
  */
  if (dirtyMenu == true) {
    char message [8][15] = {"REGRESAR", "TEMPERATURA", "TOLERANCIA", "TIEMPO DESCONG", "VENTILADORES", "DIFUSOR", "DESCONGELAR", "APAGAR"};

    lcd.clear();
    lcd.setCursor(1, 0);
    lcd.print(message[top]);
    lcd.setCursor(1, 1);
    lcd.print(message[top + 1]);
    lcd.setCursor(0, current - top);
    lcd.write(byte(5));

    dirtyMenu = false;
  }
}


void button1() {
  /**
     This button does many things.
     1- Get in the mainMenu from the home screen
     2- Act as a click to interact with the menus and submenus options inside.
     If button is pressed deactivate for 200 ms.
  */
  if (b1Millis <= millis()) {
    if (digitalRead(b1Pin) == HIGH) {
      b1Millis = millis() + 500;
      if (activeSubMenu == 0){
        switch (mainMenuCurrent) {
          case 0:
            // RETURN TO HOME
            menuOn = !menuOn;
            break;
          case 1:
            // SETPOINT CONFIG
            activeSubMenu = 1;
            newSetpoint = setpoint;
            break;
          case 2:
            // TOLERANCE CONFIG
            break;
          case 3:
            // DEFROST CONFIG
            break;
          case 4:
            // FORCE FANS ON/OFF
            break;
          case 5:
            // FORCE EVAPORATOR ON/OFF
            break;
          case 6:
            // FORCE DEFROST ON/OFF
            break;
          case 7:
            // SHUTDOWN
            break;
        }
      }
      else{
        switch (activeSubMenu) {
          case 1:
            // INTERACT WITH SETPOINT CONFIG
            setpoint = newSetpoint;
            activeSubMenu = 0;
            dirtySubMenu = true;
            break;
          case 2:
            // INTERACT WITH TOLERANCE CONFIG
            break;
          case 3:
            // INTERACT WITH DEFROST CONFIG
            break;
          case 4:
            // FORCE FANS ON/OFF
            break;
          case 5:
            // FORCE EVAPORATOR ON/OFF
            break;
          case 6:
            // FORCE DEFROST ON/OFF
            break;
          case 7:
            // SHUTDOWN
            break;
        }
      }
      dirtyMenu = true;
    }
  }
}

void button2() {
  /**
     This button moves down the current menu.

     If button is pressed deactivate for 200 ms.
  */
  if (b2Millis <= millis()) {
    if (digitalRead(b2Pin) == HIGH) {
      b2Millis = millis() + 500;
      if (activeSubMenu == 0) {
        if (mainMenuCurrent - mainMenuTop != 0) {
          if (mainMenuTop < 7) {
            mainMenuTop = mainMenuTop + 1;
          }
          else {
            mainMenuCurrent = mainMenuTop;
          }
        }
        if (mainMenuTop - mainMenuCurrent == 0) {
          mainMenuCurrent = mainMenuCurrent + 1;
        }
      }
      else{
        switch (activeSubMenu) {
          case 1:
            // INTERACT WITH SETPOINT CONFIG
            newSetpoint = newSetpoint - 1;
            dirtySubMenu = true;
            break;
          case 2:
            // INTERACT WITH TOLERANCE CONFIG
            break;
          case 3:
            // INTERACT WITH DEFROST CONFIG
            break;
          case 4:
            // FORCE FANS ON/OFF
            break;
          case 5:
            // FORCE EVAPORATOR ON/OFF
            break;
          case 6:
            // FORCE DEFROST ON/OFF
            break;
          case 7:
            // SHUTDOWN
            break;
        }
      }
      dirtyMenu = true;
    }
  }
}

void button3() {
  /**
     This button moves up the current menu.

     If button is pressed deactivate for 200 ms.
  */
  if (b3Millis <= millis()) {
    if (digitalRead(b3Pin) == HIGH) {
      b3Millis = millis() + 500;
      if (activeSubMenu == 0){
        if (mainMenuCurrent - mainMenuTop == 0) {
          if (mainMenuTop > 0) {
            mainMenuTop = mainMenuTop - 1;
          }
          else {
            mainMenuCurrent = mainMenuTop;
          }
        }
        if (mainMenuTop - mainMenuCurrent != 0) {
          mainMenuCurrent = mainMenuCurrent - 1;
        }
      }
      else {
        switch (activeSubMenu) {
          case 1:
            // SETPOINT CONFIG
            newSetpoint = newSetpoint + 1;
            dirtySubMenu = true;
            break;
          case 2:
            // TOLERANCE CONFIG
            break;
          case 3:
            // DEFROST CONFIG
            break;
          case 4:
            // FORCE FANS ON/OFF
            break;
          case 5:
            // FORCE EVAPORATOR ON/OFF
            break;
          case 6:
            // FORCE DEFROST ON/OFF
            break;
          case 7:
            // SHUTDOWN
            break;
        }
      }
      dirtyMenu = true;
    }
  }
}

void updateMenu() {
  /*
     Update the menu by pressing of the buttons.
  */
  button3();
  button2();
  button1();
}

void showSubMenu(){
  /*
   * display the actively selected submenu.
   * 
   * The global variable activeSubMenu controls which is shown.
   */
  switch(activeSubMenu){
    case 1:
    // Setpoint config.
    setpointConfig();
  }
}

void loop() {
  tempControl(setpoint, tolerance);
  button1();
  if (menuOn == true) {
    if (activeSubMenu == 0) {
      homeClean = false;
      updateMenu();
      menuScreen(mainMenuTop, mainMenuCurrent);
    }
    else {
      updateMenu();
      showSubMenu();
    }
  }
  else {
    if (homeClean == false) {
      lcd.clear();
      homeClean = true;
    }
    homeScreen();
  }
}
