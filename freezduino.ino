/**
   Name     : Freezduino
   Author   : Edgar Solis Vizcarra
   Date     : 2018/03/04
   Modified : 2018/03/10
   Version  : V0.3.3
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
#include <EEPROM.h>

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

int setpoint = (int8_t)EEPROM.read(0), tolerance = (int8_t)EEPROM.read(1);
// defrostDuration indicates minutes, defrostFrequency indicates hours.
unsigned int defrostDuration = EEPROM.read(2), defrostFrequency = EEPROM.read(3);

//Everytime we restart, everything must be set off.
bool evaporator = false, defroster = false, fans = false;

//global variables to write the temp on the lcdscreen
int lcdTempRow, lcdTempColumn;
bool lcdTempPrecise;

//global variables to write the evaporator status on the lcdscreen.
int lcdEvaporatorRow, lcdEvaporatorColumn;

//global variables to write the defroster status on the lcdscreen.
int lcdDefrosterRow, lcdDefrosterColumn;

//global variables to write the timer status on the lcdscreen.
int lcdTimerRow, lcdTimerColumn;

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

// Temporary tolerance copy storage, this copy is the one you interact with the screen and the real tolerance is set after you leave.
int newTolerance = tolerance;

// Temporary defrostDuration copy storage, this copy is the one you interact with the screen and the real defrostDuration is set after you leave.
int newDefrostDuration = defrostDuration;

// Temporary defrostFrequency copy storage, this copy is the one you interact with the screen and the real defrostFrequency is set after you leave.
int newDefrostFrequency = defrostFrequency;

// Temporary state change for the fans, this state will not be saved, only temporarily used for testing
bool tempFansState;
// Temporarily force the the fans to the temp state.
// it is an int because I needed a 3rd state, that state is used whenever outside the submenu
// 0 = false, 1 = true, 2 = none
int forceFans = 2;

// Temporary state change for the fans, this state will not be saved, only temporarily used for testing
bool tempEvaporatorState;
// Temporarily force the the evaporator to the temp state.
// it is an int because I needed a 3rd state, that state is used whenever outside the submenu
// 0 = false, 1 = true, 2 = none
int forceEvaporator = 2;

// Temporary state change for the fans, this state will not be saved, only temporarily used for testing
bool tempDefrosterState;
// Temporarily force the the defroster to the temp state.
// it is an int because I needed a 3rd state, that state is used whenever outside the submenu
// 0 = false, 1 = true, 2 = none
int forceDefroster = 2;

unsigned long evaporatorStopAt, defrosterStopAt;
bool newCycleState, newCurrentAction;
bool cycleState = false;
// This signals the current action being executed by the cycler, false = evaporator, true = defroster
bool currentAction = true;


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
  0b01110,
  0b10101,
  0b10111,
  0b10001,
  0b01110,
  0b00000,
  0b00000,
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

byte toleranceC[8]{  
  0b00100,
  0b01110,
  0b00100,
  0b00000,
  0b11111,
  0b00000,
  0b01110,
  0b00000
};

void setup() {
  Serial.begin(9600);

  // Initial EEPROM setup.
  if (EEPROM.read(0) == 255){
    // Setpoint
    EEPROM.write(0, -25);
    // Tolerance
    EEPROM.write(1, 2);
    // Defroster Duration
    EEPROM.write(2, 30);
    // Defroster Frequency
    EEPROM.write(3, 8);
  }
    // Negative values can be stored rather easily, but reading
    // them requires we read them slightly differently.
    int value = (int8_t)EEPROM.read(0);
    Serial.println(value);
    Serial.println(EEPROM.read(1));
    Serial.println(EEPROM.read(2));
    Serial.println(EEPROM.read(3));

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
  lcd.createChar(6, toleranceC);
}


//This controls how often we check for input from the buttons first value is in ms second is callback function
TimedAction lcdTempThread = TimedAction(refreshRate, lcdTemp);
TimedAction lcdDefrosterThread = TimedAction(refreshRate / 2, lcdDefrost);
TimedAction lcdEvaporatorThread = TimedAction(refreshRate / 2, lcdEvaporator);
TimedAction lcdTimerThread = TimedAction(1000, lcdTimer);

void tempControl(bool state = true) {
  /**
     Check temperature, if it is at the setpoint or lower, or higher but within
     the tolerance then turn relay off. If it is higher than the setpoint + the
     tolerance then turn the relay on.
  */
  if (state == true){
    int temp = round(tempsensor.readTempC()); // round temperature reading.
    if (temp <= setpoint + tolerance) {
      evaporator = true;
      fans = true;
    } else {
      evaporator = false;
      fans = false;
    }
  }
  else{
      evaporator = false;
      fans = false;    
  }
}

void defrost(bool state = false) {
  /**
     Activate the defrosting function.

     When defrosting is activated the compressor and the fans are turned off
     and the heating element is turned on.
     it must stay like this for the set duration or until the user decides to
     cancel the defrosting.
  */
  if (state == true) {
    defroster = true;
  }
  else {
    defroster = false;
  }
}

void fansController(){
  /*
   * Controls the relay of the evaporator.
   */
  if (forceFans != 2){
    if (forceFans == 1) {
      digitalWrite(Rfans, LOW);
    }
    else{
      digitalWrite(Rfans, HIGH);
    }
  }
  else{
    if (fans == true) {
      digitalWrite(Rfans, LOW);
    }
    else{
      digitalWrite(Rfans, HIGH);
    }    
  }
}

void evaporatorController(){
  /*
   * Controls the relay of the evaporator.
   */
  if (forceEvaporator != 2){
    if (forceEvaporator == 1) {
      digitalWrite(Rcompressor, LOW);
    }
    else{
      digitalWrite(Rcompressor, HIGH);
    }
  }
  else{
    if (evaporator == true) {
      digitalWrite(Rcompressor, LOW);
    }
    else{
      digitalWrite(Rcompressor, HIGH);
    }    
  }
}

void defrosterController(){
  /*
   * Controls the relay of the evaporator.
   */
  if (forceDefroster != 2){
    if (forceDefroster == 1) {
      digitalWrite(Rdefroster, LOW);
    }
    else{
      digitalWrite(Rdefroster, HIGH);
    }
  }
  else{
    if (defroster == true) {
      digitalWrite(Rdefroster, LOW);
    }
    else{
      digitalWrite(Rdefroster, HIGH);
    }    
  }
}

void relaysController(){
  /*
   * Handles the updating of the relays states.
   */
   fansController();
   evaporatorController();
   defrosterController();
}

void lcdTimer() {
  /**
     Write on the LCD the time left for the currentAction to finish.

     It is written LTR starting on the selected row and column.
     it displays hours, minutes and the currentAction.

     it needs 7 spaces.
  */
  int hoursLeft, minutesLeft;
  lcd.setCursor(lcdTimerColumn, lcdTimerRow);
  if (cycleState == true){
    lcd.write(byte(4));
    if (currentAction == false){
      // Display freeze cycle timer
      hoursLeft = (((evaporatorStopAt - millis()) / 1000) / 60) / 60;
      minutesLeft = (((evaporatorStopAt - millis()) / 1000) / 60) % 60;
      if (hoursLeft < 10) {
        lcd.print(0);
        lcd.print(hoursLeft);
      }
      else{
        lcd.print(hoursLeft);
      }
      lcd.print(":");
      if (minutesLeft < 10){
        lcd.print(0);
        lcd.print(minutesLeft);
      }
      else{
        lcd.print(minutesLeft);
      }
      lcd.write(byte(2));
    }
    else {
      hoursLeft = (((defrosterStopAt - millis()) / 1000) / 60) / 60;
      minutesLeft = (((defrosterStopAt - millis()) / 1000) / 60) % 60;
      if (hoursLeft < 10) {
        lcd.print(0);
        lcd.print(hoursLeft);
      }
      else{
        lcd.print(hoursLeft);
      }
      lcd.print(":");
      if (minutesLeft < 10){
        lcd.print(0);
        lcd.print(minutesLeft);
      }
      else{
        lcd.print(minutesLeft);
      }      
      lcd.write(byte(3));
    }
  }
  else{
    lcd.print("       ");
  }
}

void lcdEvaporator() {
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
  /*
   * Because the temperature is shown and updated with a timer,
   * and because it can be reused, calling this function will
   * set it to the set position.
   */
  lcdTempColumn = column;
  lcdTempRow = row;
  lcdTempPrecise = precise;
}

void updateEvaporatorPos(int column, int row) {
  /*
   * Because the evaporator state is shown and updated with a timer,
   * and because it can be reused, calling this function will
   * set it to the set position.
   */
  lcdEvaporatorColumn = column;
  lcdEvaporatorRow = row;
}

void updateDefrosterPos(int column, int row) {
  /*
   * Because the defroster state is shown and updated with a timer,
   * and because it can be reused, calling this function will
   * set it to the set position.
   */
  lcdDefrosterColumn = column;
  lcdDefrosterRow = row;
}

void updateTimerPos(int column, int row){
  /*
   * Because the timer is shown and updated with a timer,
   * and because it can be reused, calling this function will
   * set it to the set position.
   */
  lcdTimerColumn = column;
  lcdTimerRow = row;
  
}

void homeScreen() {
  /*
   * Update the homescreen data.
   */
  updateTempPos(0, 0, true);
  updateTimerPos(9, 0);
  updateEvaporatorPos(0, 1);
  updateDefrosterPos(12, 1);
  lcdTempThread.check();
  lcdEvaporatorThread.check();
  lcdDefrosterThread.check();
  lcdTimerThread.check();
}

void startConfig() {
  /*
   * Display and interact with the state of the cycle.
   *
   * newCycleState is a temporary copy of the real cycleState, the real cycleState is only updated
   * after you leave the menu so you don't change the cycleState in real time.
  */
  if (dirtySubMenu == true) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("ESTADO");
    if (newCycleState == true){
      lcd.setCursor(3, 1);
      lcd.print("TRABAJANDO");
    }
    else{
      lcd.setCursor(5, 1);
      lcd.print("APAGADO");
    }
    dirtySubMenu = false;
  }
}

void currentActionConfig() {
  /*
   * Display and interact with the state of the currentAction.
   *
   * newCurrentAction is a temporary copy of the real currentAction, the real currentAction is only updated
   * after you leave the menu so you don't change the currentAction in real time.
  */
  if (dirtySubMenu == true) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("ACCION ACTUAL");
    if (newCurrentAction == false) {
      lcd.setCursor(4, 1);
      lcd.print("CONGELANDO");
    }
    else{
      lcd.setCursor(3, 1);
      lcd.print("DESCONGELANDO");
    }
  }
  dirtySubMenu = false;
}

void setpointConfig() {
  /*
   * Display and interact with the temperature setpoint on the LCD display.
   *
   * newSetpoint is a temporary copy of the real setpoint, the real setpoint is only updated
   * after you leave the menu so you don't change the temp in real time.
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

void toleranceConfig() {
  /**
   * Display and interact with the tolerance config on the LCD display.
   * 
   * newTolerance is a temporary copy of the real tolerance, the real tolerance is only updated
   * after you leave the submenu so you don't change the tolerance in real time.
   */
  if (dirtySubMenu == true) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.write(byte(6));
    lcd.print(newTolerance);
    lcd.write(byte(1));
    lcd.print("C");
    dirtySubMenu = false;
  }  
}

void defrostDurationConfig(){
  /**
   * Display and interact with the defrostDuration config on the LCD display.
   * 
   * newDefrostDuration is a temporary copy of the real defrostDuration, the real defrostDuration is only updated
   * after you leave the submenu so you don't change the defrostDuration in real time.
   */
  if (dirtySubMenu == true) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.write(byte(4));
    lcd.print(newDefrostDuration);
    lcd.print(" MINUTOS");
    dirtySubMenu = false;
  }  
}

void defrostFrequencyConfig(){
  /**
   * Display and interact with the defrostFrequency config on the LCD display.
   * 
   * newDefrostFrequency is a temporary copy of the real defrostFrequencyuration, the real defrostFrequency is only updated
   * after you leave the submenu so you don't change the defrostFrequency in real time.
   */
  if (dirtySubMenu == true) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.write(byte(4));
    lcd.print(newDefrostFrequency);
    lcd.print(" HORAS");
    dirtySubMenu = false;
  }  
}

void fansState(){
  /**
   * Display and interact with the fans state on the LCD display.
   * 
   * For safety reasons no part of the system can be left to be forced neither ON nor OFF
   * by the user for indefinite spans of time.
   * This is only meant to be used as a short test and when the user exits
   * the submenu the control of this will be taken back by the system.
   * 
   * Submenus can only last open for a maximum of 3 minutes.
   * 
   * THIS FEATURE WILL MOST LIKELY BE REMOVED ON THE RELEASE VERSION.
   */  
  if (dirtySubMenu == true) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.write(byte(2));
    if (forceFans == 0) {
      lcd.write("OFF");
    }
    else {
      lcd.write("ON ");
    }
    dirtySubMenu = false;
  }  
}

void evaporatorState(){
  /**
   * Display and interact with the evaporator state on the LCD display.
   * 
   * For safety reasons no part of the system can be left to be forced neither ON nor OFF
   * by the user for indefinite spans of time.
   * This is only meant to be used as a short test and when the user exits
   * the submenu the control of this will be taken back by the system.
   * 
   * Submenus can only last open for a maximum of 3 minutes.
   * 
   * THIS FEATURE WILL MOST LIKELY BE REMOVED ON THE RELEASE VERSION.
   */  
  if (dirtySubMenu == true) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.write(byte(2));
    if (forceEvaporator == 0) {
      lcd.write("OFF");
    }
    else {
      lcd.write("ON ");
    }
    dirtySubMenu = false;
  }  
}

void defrosterState(){
  /**
   * Display and interact with the defroster state on the LCD display.
   * 
   * For safety reasons no part of the system can be left to be forced neither ON nor OFF
   * by the user for indefinite spans of time.
   * This is only meant to be used as a short test and when the user exits
   * the submenu the control of this will be taken back by the system.
   * 
   * Submenus can only last open for a maximum of 3 minutes.
   * 
   * THIS FEATURE WILL MOST LIKELY BE REMOVED ON THE RELEASE VERSION.
   */  
  if (dirtySubMenu == true) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.write(byte(0));
    if (forceDefroster == 0) {
      lcd.write("OFF");
    }
    else {
      lcd.write("ON ");
    }
    dirtySubMenu = false;
  }  
}

void menuScreen(int top, int current) {
  /**
   * Menu with options to change settings.
   *
   * parameters:
   * - top: signals the message that is on the top of the screen.
   * - current: signals the message that is currently selected.
   *
   * We always start with 0, 0
   *
   * 0- return
   * 1- setpoint
   * 2- tolerance
   * 3- defrost time
   * 4- defrost frequency
   * 5- toggle fans
   * 6- toggle evaporator
   * 7- toggle defroster
   * 8- shutdown
   */
  if (dirtyMenu == true) {
    char message [10][15] = {"REGRESAR", "INICIAR", "CAMBIAR ACCION", "TEMPERATURA", "TOLERANCIA", "TIEMPO DESCONG", "DESCONGELA CADA", "VENTILADORES", "DIFUSOR", "DESCONGELAR"};

    lcd.clear();
    lcd.setCursor(1, 0);

    // CHANGE MESSAGE IF CYCLE IS ON
    if (top == 1){
      if (cycleState == true){
        lcd.print("APAGAR");
      }
      else{
        lcd.print(message[top]);
      }
    }
    else{
      lcd.print(message[top]);
    }
    // FINISH CHANGING MESSAGE

    lcd.setCursor(1, 1);

    // CHANGE MESSAGE IF CYCLE IS ON
    if (top + 1 == 1){
      if (cycleState == true){
      lcd.print("APAGAR");
      }
      else{
        lcd.print(message[top + 1]);
      }
    }
    else{
      lcd.print(message[top + 1]);
    }
    // FINISH CHANGING MESSAGE
    
    lcd.setCursor(0, current - top);
    lcd.write(byte(5));

    dirtyMenu = false;
  }
}

void button1() {
  /**
   * This button does many things.
   * 1- Get in the mainMenu from the home screen
   * 2- Act as a click to interact with the menus and submenus options inside.
   * If button is pressed deactivate for 200 ms.
  */
  if (b1Millis <= millis()) {
    if (digitalRead(b1Pin) == HIGH) {
      b1Millis = millis() + 500;
      if (activeSubMenu == 0){
        // If activeSubMenu == 0, this means that we are not in any submenu,
        // therefore, we are in the main menu and the button will act as a menu selector.
        switch (mainMenuCurrent) {
          case 0:
            // RETURN TO HOME
            menuOn = !menuOn;
            break;
          case 1:
            // START
            activeSubMenu = 1;
            newCycleState = cycleState;
            break;
          case 2:
            // FORCE CYCLE ACTION CHANGE
            if (cycleState == true){
              activeSubMenu = 2;
              newCurrentAction = currentAction;
            }
            else{
              activeSubMenu = 0;
            }
            break;
          case 3:
            // SETPOINT CONFIG
            activeSubMenu = 3;
            newSetpoint = setpoint;
            break;
          case 4:
            // TOLERANCE CONFIG
            activeSubMenu = 4;
            newTolerance = tolerance;
            break;
          case 5:
            // DEFROST TIME CONFIG
            activeSubMenu = 5;
            newDefrostDuration = defrostDuration;
            break;
          case 6:
            // DEFROST FREQUENCY CONFIG
            activeSubMenu = 6;
            newDefrostFrequency = defrostFrequency;
            break;
          case 7:
            // FORCE FANS ON/OFF
            if (fans == true){
              forceFans = 1;
            }
            else {
              forceFans = 0;
            }
            activeSubMenu = 7;
            break;
          case 8:
            // FORCE EVAPORATOR ON/OFF
            if (evaporator == true){
              forceEvaporator = 1;
            }
            else {
              forceEvaporator = 0;
            }
            tempEvaporatorState = evaporator;
            activeSubMenu = 8;
            break;
          case 9:
            // FORCE DEFROST ON/OFF
            if (fans == true){
              forceDefroster = 1;
            }
            else {
              forceDefroster = 0;
            }
            tempDefrosterState = defroster;
            activeSubMenu = 9;
            break;
        }
      }
      else{
        // If we are here that means that we are in a subMenu, and the button
        // will therefore behave as a config saver.
        int diff;
        unsigned long timeDiff;
        switch (activeSubMenu) {
          case 1:
            // START
            cycleState = newCycleState;
            if (cycleState == true){
              evaporatorStopAt = millis() + (((defrostFrequency * 60.0) * 60.0) * 1000.0);
              currentAction = true;
            }
            activeSubMenu = 0;
            dirtySubMenu = true;
            break;
          case 2:
            // FORCE CYCLE ACTION CHANGE
            if (currentAction != newCurrentAction){
              if (currentAction == false) {
                forceDefrostCycle();
              }
              else{
                forceFreezingCycle();
              }
            }
              
            activeSubMenu = 0;
            dirtySubMenu = true;
            break;
          case 3:
            // INTERACT WITH SETPOINT CONFIG
            setpoint = newSetpoint;
            EEPROM.update(0, setpoint);
            activeSubMenu = 0;
            dirtySubMenu = true;
            break;
          case 4:
            // INTERACT WITH TOLERANCE CONFIG
            tolerance = newTolerance;
            EEPROM.update(1, tolerance);
            activeSubMenu = 0;
            dirtySubMenu = true;
            break;
          case 5:
            // INTERACT WITH DEFROST TIME CONFIG
            diff = newDefrostDuration - defrostDuration;
            defrostDuration = newDefrostDuration;
            EEPROM.update(2, defrostDuration);
            timeDiff = (abs(diff) * 60.0) * 1000.0;
            if (currentAction == true) {
              if (diff >= 0){
                // if diff >= 0 then do a sum.
                defrosterStopAt = defrosterStopAt + timeDiff;
              }
              else{
                // else do a substraction.
                defrosterStopAt = defrosterStopAt - timeDiff;
              }
            }
            activeSubMenu = 0;
            dirtySubMenu = true;
            break;
          case 6:
            // DEFROST FREQUENCY CONFI
            diff = newDefrostFrequency - defrostFrequency;
            defrostFrequency = newDefrostFrequency;
            EEPROM.update(3, defrostFrequency);
            timeDiff = ((abs(diff) * 60.0) * 60.0) * 1000.0;
            if (currentAction == false) {
              if (diff >= 0){
                evaporatorStopAt = evaporatorStopAt + timeDiff;
              }
              else{
                evaporatorStopAt = evaporatorStopAt - timeDiff;
              }
            }
            activeSubMenu = 0;
            dirtySubMenu = true;
            break;
          case 7:
            // FORCE FANS ON/OFF
            forceFans = 2;
            activeSubMenu = 0;
            dirtySubMenu = true;
            break;
          case 8:
            // FORCE EVAPORATOR ON/OFF
            forceEvaporator = 2;
            activeSubMenu = 0;
            dirtySubMenu = true;
            break;
          case 9:
            // FORCE DEFROST ON/OFF
            forceDefroster = 2;
            activeSubMenu = 0;
            dirtySubMenu = true;
            break;
        }
      }
      dirtyMenu = true;
    }
  }
}

void button2() {
  /**
   *This button moves down the current menu.
   *
   *If button is pressed deactivate for 200 ms.
   */
  if (b2Millis <= millis()) {
    if (digitalRead(b2Pin) == HIGH) {
      // This is the time the button will be inactive.
      b2Millis = millis() + 500;

      // If activeSubMenu == 0, this means that we are not in any submenu,
      // therefore, we are in the main menu and the button will act as a selector mover.
      if (activeSubMenu == 0) {
        if (mainMenuCurrent - mainMenuTop != 0) {
          if (mainMenuTop < 8) {
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
        // If we are here that means we are in a submenu,
        // The button will therefore behave as a config editor.
        switch (activeSubMenu) {
          case 1:
            // START
            newCycleState = !newCycleState;
            dirtySubMenu = true;
            break;
          case 2:
            // FORCE CYCLE ACTION CHANGE
            newCurrentAction = !newCurrentAction;
            dirtySubMenu = true;
            break;
          case 3:
            // INTERACT WITH SETPOINT CONFIG
            newSetpoint = newSetpoint - 1;
            dirtySubMenu = true;
            break;
          case 4:
            // INTERACT WITH TOLERANCE CONFIG
            newTolerance = newTolerance - 1;
            dirtySubMenu = true;
            break;
          case 5:
            // INTERACT WITH DEFROST TIME CONFIG
            newDefrostDuration = newDefrostDuration - 1;
            dirtySubMenu = true;
            break;
          case 6:
            // DEFROST FREQUENCY CONFIG
            newDefrostFrequency = newDefrostFrequency - 1;
            dirtySubMenu = true;
            break;
          case 7:
            // FORCE FANS ON/OFF
            if (forceFans == 1) {
              forceFans = 0;
            }
            else {
              forceFans = 1;
            }
            dirtySubMenu = true;
            break;
          case 8:
            // FORCE EVAPORATOR ON/OFF
            if (forceEvaporator == 1) {
              forceEvaporator = 0;
            }
            else {
              forceEvaporator = 1;
            }
            dirtySubMenu = true;
            break;
          case 9:
            // FORCE DEFROST ON/OFF
            if (forceDefroster == 1) {
              forceDefroster = 0;
            }
            else {
              forceDefroster = 1;
            }
            dirtySubMenu = true;
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
      // This is the time the button will be inactive.
      b3Millis = millis() + 500;

      // If activeSubMenu == 0, this means that we are not in any submenu,
      // therefore, we are in the main menu and the button will act as a selector mover.
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
        // If we are here that means we are in a submenu,
        // The button will therefore behave as a config editor.
        switch (activeSubMenu) {
          case 1:
            // START
            newCycleState = !newCycleState;
            dirtySubMenu = true;
            break;
          case 2:
            // FORCE CYCLE ACTION CHANGE
            newCurrentAction = !newCurrentAction;
            dirtySubMenu = true;
            break;
          case 3:
            // SETPOINT CONFIG
            newSetpoint = newSetpoint + 1;
            dirtySubMenu = true;
            break;
          case 4:
            // TOLERANCE CONFIG
            newTolerance = newTolerance + 1;
            dirtySubMenu = true;
            break;
          case 5:
            // INTERACT WITH DEFROST TIME CONFIG
            newDefrostDuration = newDefrostDuration + 1;
            dirtySubMenu = true;
            break;
          case 6:
            // DEFROST FREQUENCY CONFIG
            newDefrostFrequency = newDefrostFrequency + 1;
            dirtySubMenu = true;
            break;
          case 7:
            // FORCE FANS ON/OFF
            if (forceFans == 1) {
              forceFans = 0;
            }
            else {
              forceFans = 1;
            }
            dirtySubMenu = true;
            break;
          case 8:
            // FORCE EVAPORATOR ON/OFF
            if (forceEvaporator == 1) {
              forceEvaporator = 0;
            }
            else {
              forceEvaporator = 1;
            }
            dirtySubMenu = true;
            break;
          case 9:
            // FORCE DEFROST ON/OFF
            if (forceDefroster == 1) {
              forceDefroster = 0;
            }
            else {
              forceDefroster = 1;
            }
            dirtySubMenu = true;
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
      // Start config.
      startConfig();
      break;
    case 2:
      // Current action config.
      currentActionConfig();
      break;
    case 3:
      // Setpoint config.
      setpointConfig();
      break;
    case 4:
      // Tolerance config.
      toleranceConfig();
      break;
    case 5:
      // Tolerance config.
      defrostDurationConfig();
      break;
    case 6:
      // Tolerance config.
      defrostFrequencyConfig();
      break;
    case 7:
      // Tolerance config.
      fansState();
      break;
    case 8:
      // Tolerance config.
      evaporatorState();
      break;
    case 9:
      // Tolerance config.
      defrosterState();
      break;
  }
}

void cycler(){
  /**
   * Activate and deactivate the relays according to the
   * timers set by the user.
   * 
   * currentAction == false when the freezing Cycle is on.
   * 
   * currentAction == true when the defroster Cycle is on.
   */
  if (currentAction == false){
    if (millis() <= evaporatorStopAt){
      defrost(false);
      tempControl();
    }
    else{
      currentAction = true;
      // The defroster is in MINUTES so we multiply the minutes by 60 to get the seconds
      //   and then by 1000 to get the ms.
      defrosterStopAt = millis() + ((defrostDuration * 60.0) * 1000.0);
    }
  }
  else{
    if (millis() <= defrosterStopAt){
      tempControl(false);
      defrost(true);
    }
    else{
      currentAction = false;
      // The evaporator is in HOURS so we multiply the hours by 60 to get the minutes
      //   and then by 60 again to get the seconds, and lastly by 1000 to get the ms.
      evaporatorStopAt = millis() + (((defrostFrequency * 60.0) * 60.0) * 1000.0);
    }
  }
}

void forceDefrostCycle(){
  /*
   * If the user so desires, they can force start the defrost cycle during a running cycle.
   */
  if (currentAction == false){
    currentAction = true;
    evaporatorStopAt = millis();
    defrosterStopAt = millis() + ((defrostDuration * 60.0) * 1000.0);
  }
}

void forceFreezingCycle(){
  /*
   * If the user so desires, they can force start the freezing cycle during a running cycle.
   */
  if (currentAction == true){
    currentAction = false;
    defrosterStopAt = millis();
    evaporatorStopAt = millis() + (((defrostFrequency * 60.0) * 60.0) * 1000.0);
  }
}

void loop() {
  relaysController();
  Serial.println(cycleState);
  if (cycleState == true){
    cycler();
  }
  else{
    tempControl(false);
    defrost(false);
  }
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
    button1();
    if (homeClean == false) {
      lcd.clear();
      homeClean = true;
    }
    homeScreen();
  }
}
