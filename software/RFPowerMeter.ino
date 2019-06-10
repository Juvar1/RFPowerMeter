/* RFPowerMeter.ino
 * 
 * Displays 12V lead acid battery voltage level on display
 * and RF transmitting power on same display in watts or dBm
 * 
 * Author: jpvarjonen@gmail.com
 * Copyright (C) 2019 Juha-Pekka Varjonen
 * 
 * Goes to sleep and wakes every 1s to check button state
 */

#include <avr/sleep.h>
#include <avr/wdt.h>
#include <avr/power.h>

int measureState = 0;
int buttonState;             // the current reading from the input pin
int lastButtonState = HIGH;   // the previous reading from the input pin

unsigned long lastDebounceTime = 0;  // the last time the output pin was toggled
unsigned long debounceDelay = 100;   // the debounce time; increase if the output flickers
unsigned long shutdownDelay = 0;

void setup() {
  // power up settings
  wdt_enable(WDTO_1S);
  
  pinMode(13,INPUT_PULLUP); //button
  if (!digitalRead(13)) {
    // if pressed then wake up
    // enable display frequency
    TCCR2B = (TCCR2B & B11111000) | B00000111; // for PWM frequency of 30.64 Hz
    pinMode(3,OUTPUT); // display frequency
    analogWrite(3,128); // 50% duty cycle
    // define outputs
    pinMode(0,OUTPUT); // BCD2
    pinMode(1,OUTPUT); // BCD1
    pinMode(2,OUTPUT); // BCD3
    pinMode(4,OUTPUT); // BCD0
    pinMode(6,OUTPUT); // P1
    pinMode(7,OUTPUT); // ST1
    pinMode(8,OUTPUT); // P2
    pinMode(9,OUTPUT); // ENB
    pinMode(A4,OUTPUT); // ST2
    pinMode(A5,OUTPUT); // ST3
    // set analog reference voltage to external
    analogReference(EXTERNAL); // 2.5V set by hardware
    // activate measurement
    digitalWrite(9, HIGH);
  } else {
    // if not pressed then go to sleep
    goToSleep();
  }
}

void loop() {
  // wait 5 minutes and after that goes to sleep
  wdt_reset(); // prevent sleep
  // how many times button has pressed
  // do action regarding of that
  // button also resets the 5 min timeout

  changeReading(); // read button state

  int result = 0; // decimal point removed with '* 100'
  if (measureState == 0) { // RF power in watts
    float val = analogRead(A1);
    float PdBm = 40*((2.5/1023*val)-1);
    float PW = pow(10,(PdBm/10))/1000;
    result = (int)(PW * 100);
  }
  else if (measureState == 1) { // RF power in dBm
    float val = analogRead(A1);
    float PdBm = 40*((2.5/1023*val)-1);
    result = (int)(PdBm * 100);
  }
  else if (measureState == 2) { // supply voltage measurement
    float val = analogRead(A0);
    float voltage = (13.864/1023*val);
    result = (int)(voltage * 100);
  }
  
  int shift = 0;

  if (result < 0) {
    shift = 1;
    result = -result;
    writeToDisplay(1, 0xe, false); // minus sign
  }
  
  boolean dp2 = true;

  if (result > 999) { // no decimal point
    result = result / 10;
    dp2 = false;
  } 
  if (result > 999) { // dp2 active
    result = result / 10;
    writeToDisplay(1+shift, result / 100, false);
    writeToDisplay(2+shift, (result % 100) / 10, dp2);
  } 
  else if (result >= 100) { // dp1 active
    writeToDisplay(1+shift, result / 100, true);
    writeToDisplay(2+shift, (result % 100) / 10, false);
  } 
  else if (result >= 10) {
    writeToDisplay(1+shift, 0, true);
    writeToDisplay(2+shift, result / 10, false);
  }
  else if (result >= 0) {
    writeToDisplay(1+shift, 0, true);
    writeToDisplay(2+shift, 0, false);
  }
  if (shift == 0) { writeToDisplay(3, result % 10, false); }
  
  //if 5 minutes is up from last button press
  if (millis() - shutdownDelay >= 300000) {
    writeToDisplay(1,0xf,false);
    writeToDisplay(2,0xf,false);
    writeToDisplay(3,0xf,false);
    digitalWrite(9, LOW); // deactivate measurement
    goToSleep();
  }
  
}

void changeReading() {
  int reading = digitalRead(13);
  // If the switch changed, due to noise or pressing:
  if (reading != lastButtonState) {
    // reset the debouncing timer
    lastDebounceTime = millis();
  }
    if ((millis() - lastDebounceTime) > debounceDelay) {
    // whatever the reading is at, it's been there for longer than the debounce
    // delay, so take it as the actual current state:

    // if the button state has changed:
    if (reading != buttonState) {
      buttonState = reading;

      // only use state if the new button state is LOW
      if (buttonState == LOW) {
        measureState++;
        if(measureState > 2) { measureState = 0; }
        // display measurement state 'icon'
        writeToDisplay(1,0xf,false);
        writeToDisplay(2,measureState+11,false);
        writeToDisplay(3,0xf,false);
        delay(500);
        shutdownDelay = millis();
      }
    }
  }
  // save the reading. Next time through the loop, it'll be the lastButtonState:
  lastButtonState = reading;
}

void writeToDisplay(int disp, int number, boolean dp) {

  // error correction
  if(number > 0xf || number < 0) { number = 0xf; }
  if(disp > 3) { return; }

  //encode BCD code
  digitalWrite(2,(number & 0b1000) >> 3);
  digitalWrite(0,(number & 0b100) >> 2);
  digitalWrite(1,(number & 0b10) >> 1);
  digitalWrite(4,number & 0b1);
  digitalWrite(6, LOW); // decimal point initialization
  digitalWrite(8, LOW); // other decimal point

  // strobe
  if(disp == 1) {
    if(dp) { digitalWrite(6, HIGH); }
    digitalWrite(7, HIGH);
  }
  else if(disp == 2) {
    if(dp) { digitalWrite(8, HIGH); }
    digitalWrite(A4, HIGH);
  }
  else if(disp == 3) {digitalWrite(A5, HIGH);}
  delay(5);
  if(disp == 1) {digitalWrite(7, LOW);}
  else if(disp == 2) {digitalWrite(A4, LOW);}
  else if(disp == 3) {digitalWrite(A5, LOW);}
  delay(5);
}

void goToSleep() {
  // set sleep mode (not activated yet)
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  // disable things
  power_spi_disable();
  power_usart0_disable();
  power_twi_disable();
  power_timer0_disable();
  power_timer1_disable();
  power_timer2_disable();
  power_adc_disable();
  // go to sleep (watchdog will restart)
  sleep_mode();
}

