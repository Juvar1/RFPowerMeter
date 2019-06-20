/* RFPowerMeter.ino
 * 
 * Displays 12V lead acid battery voltage level on display
 * and RF transmitting power on same display in watts or dBm
 * 
 * Author: jpvarjonen@gmail.com
 * Copyright (C) 2019 Juha-Pekka Varjonen
 * 
 * Goes to sleep and wakes every 2s to check button state
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
  wdt_enable(WDTO_2S);

  // display frequency always on
  TCCR2A = B00000000;
  TCCR2B = B00000110; //61Hz
  analogWrite(3,128); // 50% duty cycle

  // define outputs, default output state is low
  pinMode(0,OUTPUT);  // BCD2
  pinMode(1,OUTPUT);  // BCD1
  pinMode(2,OUTPUT);  // BCD3
  pinMode(4,OUTPUT);  // BCD0
  pinMode(6,OUTPUT);  // P1
  pinMode(7,OUTPUT);  // ST1
  pinMode(8,OUTPUT);  // P2
  pinMode(9,OUTPUT);  // ENB
  pinMode(A4,OUTPUT); // ST2
  pinMode(A5,OUTPUT); // ST3

  pinMode(13,INPUT_PULLUP); //button
  if (!digitalRead(13)) { // if pressed then wake up
    // set analog reference voltage to external
    analogReference(EXTERNAL); // 2.5V set by hardware
    // activate measurement
    digitalWrite(9, HIGH);
  } else {
    // if not pressed then go back to sleep
    goToSleep();
  }
}

void loop() {

  wdt_reset(); // prevent sleep

  changeReading(); // read button state, takes >500ms

  wdt_reset(); // prevent sleep

  long result = 0; // decimal point removed with '* 100'
  if (measureState == 0) { // RF power in watts
    float val = analogRead(A1);
    float PdBm = 40*((2.5/1023*val)-1);
    float PW = pow(10,(PdBm/10))/1000;
    result = (long)(PW * 100);
  }
  else if (measureState == 1) { // RF power in dBm
    float val = analogRead(A1);
    float PdBm = 40*((2.5/1023*val)-1);
    result = (long)(PdBm * 100);
  }
  else if (measureState == 2) { // supply voltage measurement
    float val = analogRead(A0);
    float voltage = (12.205/1023*val);
    if(val == 1023) voltage = 999;
    result = (long)(voltage * 100);
  }
  
  short shift = 0;

  if (result < 0) {
    shift = 1;
    result = -result;
    writeToDisplay(1, 0xe, false); // minus sign
  }
  
  boolean dp1 = false;
  boolean dp2 = false;

  if (result < 999) {
    dp1 = true;
  }
  if (result > 999) {
    result = result / 10;
    dp2 = true;
  }
  if (result > 999) {
    result = result / 10;
    dp2 = false;
  }

  writeToDisplay(1 + shift, (int)(result / 100), dp1);
  writeToDisplay(2 + shift, (int)((result % 100) / 10), dp2);
  if (shift == 0) writeToDisplay(3, (int)(result % 10), false);

  //if 1 minute is up from last button press
  if (millis() - shutdownDelay >= 60000) {
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
        if(measureState == 0) {
          writeToDisplay(1,0xc,false); //P
          writeToDisplay(2,0,false);   //0
          writeToDisplay(3,1,false);   //1
        } else if(measureState == 1) {
          writeToDisplay(1,0xc,false); //P
          writeToDisplay(2,0,false);   //0
          writeToDisplay(3,2,false);   //2
        } else if(measureState == 2) {
          writeToDisplay(1,0xb,false); //H
          writeToDisplay(2,1,false);   //1
          writeToDisplay(3,0xf,false); //(blank)
        }
        delay(500);
        shutdownDelay = millis(); // button resets the timeout
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
  delay(15);
  if(disp == 1) {digitalWrite(7, LOW);}
  else if(disp == 2) {digitalWrite(A4, LOW);}
  else if(disp == 3) {digitalWrite(A5, LOW);}
  delay(15);
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
  //power_timer2_disable();
  power_adc_disable();
  // go to sleep (watchdog will restart)
  sleep_mode();
}

