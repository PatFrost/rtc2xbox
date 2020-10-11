
// Uncomment below to make the in-game temp readouts display in Fahrenheit.
// #define USE_FAHRENHEIT


#include <LiquidCrystal.h>
#include "src/Log.h"       // Used in debug mode
#include "src/xboxsmbus.h" // Lib and Xbox Functions by Ryzee119

// Max 5v Vout and (REQUIRED) Protect by Resistor (R2 / (R1 + R2))
#define SENSOR_3V()   (analogRead(A2)) // Resistor (10k / 10k + 10k)) = 1.65 vout of  3.3v
#define SENSOR_5V()   (analogRead(A1)) // Resistor (10k / 10k + 10k)) = 2.50 vout of  5.0v
#define SENSOR_12V()  (analogRead(A3)) // Resistor (10k / 25k + 10k)) = 3.43 vout of 12.0v
#define SENSOR_POT()  (analogRead(A0)) // Potentiometer 10kΩ to adjust contrast and backlight in same time
#define SENSOR_TEMP() (analogRead(9))  // Temperature with thermistor 10kΩ and resistor 10kΩ

// set up a new (soft) serial port (master to slave, slave to master)
// https://www.arduino.cc/reference/en/language/functions/communication/serial/
#define Master Serial1


// Declare our LCD object: LiquidCrystal(RS, E, D4, D5, D6, D7)
LiquidCrystal LCD(5, 6, 7, 14, 15, 16);

String LCD_LINES[4];

int BACKLIGHT_PIN   = 10;   // Backlight PIN 16bits
int BACKLIGHT_VALUE = 255;  // 0-255 Higher is brighter.

int CONTRAST_PIN    = 4;    // Contrast PIN 8bits (without flickering)
int CONTRAST_VALUE  = 127;  // 0-127 Higher is brighter on PIN 8bits.
                            // 0-255 Lower is higher contrast on PIN 16bits.

char DEGREE         = 223;  // ° symbol
// char OHMS           = 244;  // Ω symbol
// char OHMS           = 0xF4; // Ω symbol

uint32_t WELCOME_TIME;      // Timer for show welcome message (1 hour)

// Custom chars
byte PUNISHER[] = { B00000, B01110, B10101, B11011, B01110, B01010, B01010, B00000 };


void getDateTime() {
  String str = "";
  while (Master.available()) {
    delay(5);
    char c = Master.read();
    str += c;
  }
  if (str.length() >= 19) {
    LCD.setCursor(0, 0);
    LCD.print(str.substring(0, 20));
    LOG(F("DateTime: ")); LOG_END(str.substring(0, 20));
  }
}

void setup () {
  // init Serial port
  Master.begin(9600);
  delay(100);
  // Master.println("Master: Hello Slave");

  // Start log output if enabled
  LOG_BEGIN();

  // Speed up PWM frequency. Gets rid of flickering
  TCCR1B &= 0b11111000;
  TCCR1B |= (1 << CS00);  // Change Timer Prescaler for PWM

  setContrastBacklight(true); // Contrast and background control of the LCD

  // Start LCD
  LCD.begin(20, 4);
  LCD.createChar(0, PUNISHER);
  delay(2);
  LCD.noCursor();
  welcomeMessage();
  WELCOME_TIME = millis();

  SMBusBegin();
}

void loop () {
  // Debug infos
  LOG((SENSOR_POT() / 100.0)); LOG_END(F(" k"));
  LOG(((SENSOR_POT() + 0.5) * 10.0 / 1024.0)); LOG_END(F(" k"));
  LOG(((SENSOR_POT() + 0.5) *  5.0 / 1024.0)); LOG_END(F("v"));
  LOG_END();

  setContrastBacklight(false);

  if (millis() - WELCOME_TIME >= 3600000) { // 1 hour
    welcomeMessage();
    WELCOME_TIME = millis();
  }

  // Check that i2cBus is free
  if (!i2cBusy()) {
    LCD_LINES[1]  = getXboxFanSpeed();         // Xbox SMBus
    LCD_LINES[1] += getXboxResolution();       // Xbox SMBus
#ifdef USE_FAHRENHEIT
    LCD_LINES[2] = getXboxTemperatures(true);  // Xbox SMBus
#else
    LCD_LINES[2] = getXboxTemperatures(false); // Xbox SMBus
#endif
  }

  // Send the information to the LCD
  /* LINE 1 */
  getDateTime();

  /* LINE 2 */
  LCD.setCursor(0, 1);
  LCD.print(LCD_LINES[1]);
  LCD.setCursor(17, 1);
  LCD.print(round(getTemperature()));
  LCD.print(DEGREE); // ° symbol

  /* LINE 3 */
  LCD.setCursor(0, 2);
  LCD.print(LCD_LINES[2]);
  // LCD.print(I2C_CHECK_COUNT);

  /* LINE 4 */
  LCD.setCursor(0, 3);
  // LCD.print(F("3.30v 5.00v 12.00v  ")); // voltages
  double v3, v5, v12;
  getVoltages(v3, v5, v12);
  LCD.print(v3);  LCD.print(F("v  "));
  LCD.print(v5);  LCD.print(F("v  "));
  LCD.print(v12); LCD.print(F("v"));

  delay(1000);
}

float getTemperature() {
  // See http://en.wikipedia.org/wiki/Thermistor for explanation of formula
  float temp = log(((10240000/SENSOR_TEMP()) - 10000));
  temp = 1 / (0.001129148 + (0.000234125 * temp) + (0.0000000876741 * temp * temp * temp));
  temp = temp - 273.15; // Convert Kelvin to Celcius
#ifdef USE_FAHRENHEIT
  temp = (temp * 1.8 + 32.0);
#endif
  return temp;
}

void getVoltages(double & v3, double & v5, double & v12) {
  // read the raw data coming in on analog pin A1, A2, A3
  v3 = ((SENSOR_3V() + 0.5) * 5.0 / 1024);
  if ((int)v3 > 0) v3 += (3.3 - (3.3 * (10.0 / (10.0 + 10.0))));

  v5 = ((SENSOR_5V() + 0.5) * 5.0 / 1024);
  if ((int)v5 > 0) v5 += (5.0 - (5.0 * (10.0 / (10.0 + 10.0))));

  v12 = ((SENSOR_12V() + 0.5) * 5.0 / 1024);
  if ((int)v12 > 0) v12 += (12.0 - (12.0 * (10.0 / (25.0 + 10.0))));
}

void setContrastBacklight(bool force) {
  int value = map(SENSOR_POT(), 0, 1023, 0, 255);
  if (force || value != BACKLIGHT_VALUE) {
    BACKLIGHT_VALUE = value;
    analogWrite(BACKLIGHT_PIN, BACKLIGHT_VALUE);
    LOG(F("BACKLIGHT_VALUE: ")); LOG_END(BACKLIGHT_VALUE);
  }
  // value = map(SENSOR_POT(), 0, 1023, 255, 0); // if pin is 16bits use this and remove ( / 2)
  if (force || value / 2 != CONTRAST_VALUE) {
    CONTRAST_VALUE = value / 2;
    analogWrite(CONTRAST_PIN, CONTRAST_VALUE);
    LOG(F("CONTRAST_VALUE: ")); LOG_END(CONTRAST_VALUE);
  }
}

void welcomeMessage() {
  /* totals 12 secs */
  int speed = 50;
  // Draw Punisher 2200ms
  LCD.setCursor(0, 0);
  for (int i=0; i<20; i++) {
    LCD.write(byte(0));
    delay(speed);
  }
  LCD.setCursor(19, 1);
  LCD.write(byte(0));
  delay(speed);
  LCD.setCursor(19, 2);
  LCD.write(byte(0));
  delay(speed);
  for (int i=19; i>=0; i--) {
    LCD.setCursor(i, 3);
    LCD.write(byte(0));
    delay(speed);
  }
  LCD.setCursor(0, 2);
  LCD.write(byte(0));
  delay(speed);
  LCD.setCursor(0, 1);
  LCD.write(byte(0));
  delay(speed);

  // Draw Title and Author 1800ms + 3000ms
  typewriter("     RTC2Xbox     ", speed, 1, 1);
  typewriter("     by Frost     ", speed, 1, 2);
  delay(3000);

  // Draw Special thanks 2700ms + 2300ms
  typewriter("  Special Thanks  ", speed, 1, 1);
  typewriter("   to  Ryzee119   ", speed, 1, 2);
  delay(500);
  LCD.setCursor(1, 2);
  LCD.print(F("                  "));
  LCD.setCursor(1, 1);
  LCD.print(F("   to  Ryzee119   "));
  typewriter(" for  spi2par2019 ", speed, 1, 2);
  delay(1800);

  LCD.clear();
}

void typewriter(char * line, int speed, int x, int y) {
  LCD.setCursor(x, y);
  for (int l=0; l <=strlen(line); l++) {
    LCD.print(line[l]);
    delay(speed);
  }
}

void clearLine(int line) {
  LCD.setCursor(0, line);
  LCD.print(F("                    "));
  LCD.setCursor(0, line);
}
