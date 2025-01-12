#include "EEPROM.h"
#include <Adafruit_GFX.h>    // Core graphics library
#include <Adafruit_ST7735.h> // Hardware-specific library
#include <SPI.h>


int upper_threshold, lower_threshold;





int in_ADC0, in_ADC1;  //variables for 2 ADCs values (ADC0, ADC1)
int pot0, pot1, pot2, out_DAC0, out_DAC1; //variables for 3 pots (ADC8, ADC9, ADC10)
int pot0_mod_old = 0, pot1_mod_old = 0, pot2_mod_old = 0;

const int LED         = 3;
const int FOOTSWITCH  = 7;
const int TOGGLE      = 2;
const int SAVE_BUTTON = 1;
const int TFT_CS      = 10;
const int TFT_RST     = 8;
const int TFT_DC      = 9;

const int STANDBY_MODE = 0;
const int BUTTON_MODE  = 1;
const int SENSOR_MODE  = 2;

const int DEBOUNCE_DELAY = 50;

const int MIN =     0;
const int MAX =  4096;

const int MIN_SCREEN =   0;
const int MAX_SCREEN = 115;

const int MIN_POT   =     0;
const int MAX_POT   =  4096;
const int LIMIT_POT = 10000;

const int MIN_SENSOR =   0;
const int MAX_SENSOR = 180;

Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS,  TFT_DC, TFT_RST);

int footswitch_detect;
int footswitch_detect_old;
int footswitch_detect_older;

int save_button_detect;
int save_button_press_time;
boolean save_button_pressed;

int footswitch_mode = STANDBY_MODE;

int last_debounce_time = 0;
int last_update_time   = 0;

int p0 = 0, p1 = 0, p2 = 0;
int p0_old, p1_old, p2_old;


byte incomingByte;
byte byteTemp;
int16_t counter;
int16_t currentInt;
int16_t int1;
byte byteArray[3];
byte byte1;
byte byte2;
byte byte3;
byte lastByte;
int16_t angles[3];
int16_t mask;



void setup()
{
  //ADC Configuration
  ADC->ADC_MR |= 0x80;   // DAC in free running mode.
  ADC->ADC_CR = 2;       // Starts ADC conversion.
  ADC->ADC_CHER = 0xFFFF; // Enable all ADC channels

  //DAC Configuration
  analogWrite(DAC0, 0); // Enables DAC0
  analogWrite(DAC1, 0); // Enables DAC0




  pinMode(FOOTSWITCH, INPUT);
  // pinMode(SAVE_BUTTON, INPUT);
  pinMode(LED, OUTPUT);

  tft.initR(INITR_BLACKTAB);

  tft.setRotation(-1);
  tft.setTextSize(2);
  tft.fillScreen(ST7735_WHITE);
  tft.setTextColor(ST7735_RED);
  tft.println("EFFECT NAME");
  tft.println("");
  tft.println("          P1");
  tft.println("");
  tft.println("          P2");
  tft.println("");
  tft.println("          P3");

  // record the state of the footswitch when turned on
  footswitch_detect_old = digitalRead(FOOTSWITCH);
  footswitch_detect_older = footswitch_detect_old;

  p0_old = p0;
  p1_old = p1;
  p2_old = p2;

  // initialize serial communication (for edge (on/off) detection)
  Serial.begin(9600);

  // initialize the XBee's serial port transmission
  Serial1.begin(57600);

  // From XBee
  lastByte = (byte)0;
}



void loop() {
  readFootSwitch();
  updateScreen();
  // readSaveButton();

  while ((ADC->ADC_ISR & 0x1CC0) != 0x1CC0); // wait for ADC 0,1,8,9,10 conversion complete.
  in_ADC0 = ADC->ADC_CDR[7];                 // read data from ADC0
  in_ADC1 = ADC->ADC_CDR[6];                 // read data from ADC1

  // POT0=ADC->ADC_CDR[10];                 // read data from ADC8
  // POT1=ADC->ADC_CDR[11];                 // read data from ADC9
  // POT2=ADC->ADC_CDR[12];                 // read data from ADC10

  switch (footswitch_mode) {
    case STANDBY_MODE:
      Serial.println("Standby Mode ");
      break;

    case BUTTON_MODE:
      Serial.println("Button Mode ");
      readPotentiometer();
      break;

    case SENSOR_MODE:
      Serial.println("Sensor Mode ");
      readSensor();
      break;

    default:
      Serial.print("Invalid state ");
  }
  
  upper_threshold=map(p0,0,4095,4095,2047);
  lower_threshold=map(p0,0,4095,0000,2047);
  
  if(in_ADC0>=upper_threshold) in_ADC0=upper_threshold;
  else if(in_ADC0<lower_threshold)  in_ADC0=lower_threshold;
 
  if(in_ADC1>=upper_threshold) in_ADC1=upper_threshold;
  else if(in_ADC1<lower_threshold)  in_ADC1=lower_threshold;
 
  //adjust the volume with POT2
  out_DAC0=map(in_ADC0,0,4095,1,p2);
  out_DAC1=map(in_ADC1,0,4095,1,p2);
 
  //Write the DACs
  dacc_set_channel_selection(DACC_INTERFACE, 0);          //select DAC channel 0
  dacc_write_conversion_data(DACC_INTERFACE, out_DAC0);   //write on DAC
  dacc_set_channel_selection(DACC_INTERFACE, 1);          //select DAC channel 1
  dacc_write_conversion_data(DACC_INTERFACE, out_DAC1);   //write on DAC

}



void testdrawtext(char *text, uint16_t color) {
  tft.setCursor(0, 0);
  tft.setTextColor(color);
  tft.setTextWrap(true);
  tft.print(text);
}

void readFootSwitch() {
  footswitch_detect = digitalRead(FOOTSWITCH);

  if (footswitch_detect != footswitch_detect_old) {
    last_debounce_time = millis();
  }

  if (footswitch_detect != footswitch_detect_older && millis() - last_debounce_time > DEBOUNCE_DELAY) {
    footswitch_mode = (footswitch_mode + 1) % 3;
    footswitch_detect_older = footswitch_detect;
  }

  footswitch_detect_old = footswitch_detect;
}


void readSaveButton() {
  save_button_detect = digitalRead(SAVE_BUTTON);

  if (save_button_detect == HIGH) {
    last_debounce_time = millis();

    if (save_button_pressed = true) {
      if (millis() - save_button_press_time > 3000) {
        EEPROM.write(0, p0);
        EEPROM.write(1, p1);
        EEPROM.write(2, p2);
      } else {
        p0 = EEPROM.read(0);
        p1 = EEPROM.read(1);
        p2 = EEPROM.read(2);
      }

      save_button_pressed = false;
    }
  }

  if (save_button_detect == LOW && millis() - last_debounce_time > DEBOUNCE_DELAY && save_button_pressed == false) {
    save_button_press_time = millis();
    save_button_pressed = true;
  }
}

void readPotentiometer() {
  int pot0_mod = ADC->ADC_CDR[2]; //read from pot0
  pot0 = updatePot(pot0, pot0_mod_old, pot0_mod);
  pot0_mod_old = pot0_mod;

  int pot1_mod = ADC->ADC_CDR[1]; //read from pot1
  pot1 = updatePot(pot1, pot1_mod_old, pot1_mod);
  pot1_mod_old = pot1_mod;

  int pot2_mod = ADC->ADC_CDR[0]; //read from pot2
  pot2 = updatePot(pot2, pot2_mod_old, pot2_mod);
  pot2_mod_old = pot2_mod;

  p0 = map(pot0, MIN_POT, LIMIT_POT, MIN, MAX);
  p1 = map(pot1, MIN_POT, LIMIT_POT, MIN, MAX);
  p2 = map(pot2, MIN_POT, LIMIT_POT, MIN, MAX);
}


int updatePot(int pot, int pot_mod_old, int pot_mod) {
  int value = 0;
  if ((pot_mod - pot_mod_old) < -0.9 * MAX_POT) {
    value = MAX_POT + pot_mod - pot_mod_old;//+POTSENSOR

    if ((pot + value) > LIMIT_POT) {
      pot = LIMIT_POT;
    }
    else {
      pot = pot + value ;
    }
  }
  else if ((pot_mod - pot_mod_old) > 0.9 * MAX_POT) {
    value = - MAX_POT + pot_mod - pot_mod_old;//+POTSENSOR
    if ((pot + value) < MIN_POT) {
      pot = MIN_POT;
    }
    else {
      pot = pot + value ;
    }
  }
  else if ((pot_mod - pot_mod_old) > 0) {
    value = pot_mod - pot_mod_old;//+POTSENSOR
    if ((pot + value) > LIMIT_POT) {
      pot = LIMIT_POT;
    }
    else {
      pot = pot + value ;
    }
  }
  else if ((pot_mod - pot_mod_old) < 0) {
    value = pot_mod - pot_mod_old;//+POTSENSOR
    if ((pot + value) < MIN_POT) {
      pot = MIN_POT;
    }
    else {
      pot = pot + value ;
    }
  }
  return pot;
}


void updateScreen() {
  if (millis() - last_update_time > 100) {
    last_update_time = millis();

    if (p0_old < p0) {
      tft.fillRoundRect(MIN_SCREEN, 32, map(p0, MIN, MAX, MIN_SCREEN, MAX_SCREEN), 16, 0, ST7735_BLUE);
    }
    if (p0_old > p0) {
      tft.fillRoundRect(map(p0, MIN, MAX, MIN_SCREEN, MAX_SCREEN), 32, map(p0, MIN, MAX, MAX_SCREEN, MIN_SCREEN), 16, 0, ST7735_WHITE);
    }

    p0_old = p0;


    if (p1_old < p1) {
      tft.fillRoundRect(MIN_SCREEN, 64, map(p1, MIN, MAX, MIN_SCREEN, MAX_SCREEN), 16, 0, ST7735_BLUE);
    }
    if (p1_old > p1) {
      tft.fillRoundRect(map(p1, MIN, MAX, MIN_SCREEN, MAX_SCREEN), 64, map(p1, MIN, MAX, MAX_SCREEN, MIN_SCREEN), 16, 0, ST7735_WHITE);
    }

    p1_old = p1;

    if (p2_old < p2) {
      tft.fillRoundRect(MIN_SCREEN, 96, map(p2, MIN, MAX, MIN_SCREEN, MAX_SCREEN), 16, 0, ST7735_BLUE);
    }
    if (p2_old > p2) {
      tft.fillRoundRect(map(p2, MIN, MAX, MIN_SCREEN, MAX_SCREEN), 96, map(p2, MIN, MAX, MAX_SCREEN, MIN_SCREEN), 16, 0, ST7735_WHITE);
    }

    p2_old = p2;
  }
}


void readSensor() {
  if (Serial1.available()) {

    // The general idea is that when the pedal ends its effect loop, there can be a very big buffer of bytes waiting, that are not good, so we clear it
    boolean header = false;
    lastByte = Serial1.read();

    while (Serial1.available() && !header) {
      incomingByte = Serial1.read();

      int1 = assembleInt(lastByte, incomingByte);

      //Serial.println("waiting"); // Uncomment this line to see how much it takes to clear the buffer

      if (int1 == -21846) {
        header = true;
      } else {
        lastByte = incomingByte;
      }
    }


    readAngles();
    lastByte = (byte)0;
  }
}

// assemble an int from the two bytes just read into byteArray
int16_t assembleInt(byte byte1, byte byte2) {

  currentInt = (int16_t)byte1 << 8;
  mask = 0xFF;
  mask = mask & (int16_t)byte2; // mask the first 8 bit to zero, in case the byte is sign-extended

  currentInt = currentInt | mask;

  // the byte1*256 + byte2 before is wrong because byte2 had been sign-extended - have to mask the first 8 bits
  //mask = 0xFF;
  //currentInt = byte1*256 + (byte2 & mask);
  return currentInt;
}


void readAngles() {
  for (int i = 0; i < 3; i++) {
    while (!Serial1.available()) {

      Serial.println("----Im waiting111111-");
    }

    byteArray[0] = Serial1.read();


    while (!Serial1.available()) {

      Serial.println("----Im waiting222222-");
    }

    byteArray[1] = Serial1.read();
    angles[i] = assembleInt(byteArray[0], byteArray[1]);
  }

  if (angles[0] >= 0 && angles[0] <= 360 && angles[1] >= 0 && angles[1] <= 360 && angles[2] >= 0 && angles[2] <= 360) {
    Serial.print(angles[0]);
    Serial.print("|");
    Serial.print(angles[1], DEC);
    Serial.print("|");
    Serial.print(angles[2], DEC);
    Serial.print("|");

    Serial.println("");

    // Writing the angles into the global variables
    p0 = map(angles[0], MIN_SENSOR, MAX_SENSOR, MIN, MAX);
    p1 = map(angles[1], MIN_SENSOR, MAX_SENSOR, MIN, MAX);
    p2 = map(angles[2], MIN_SENSOR, MAX_SENSOR, MIN, MAX);
  }
}


