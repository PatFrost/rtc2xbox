// Slave for RTC, because not work with SMWire for xbox and cause conflict...

// Uncomment below, if you want force adjust date time. Only set the datetime one time.
// #define FORCE_ADJUST_DATETIME

#include <Wire.h>
#include "src/DS1307/DS1307.h"

// set up a new (soft) serial port (master to slave, slave to master)
// https://www.arduino.cc/reference/en/language/functions/communication/serial/
#define Slave Serial1

DS1307 clock;

const char strMonth[12][4] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun",
                               "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };

void setup() {
  // init Serial port
  Slave.begin(9600);
  delay(100);
  // Slave.println("Slave: Hello Master");

  // Serial.begin(57600);
  // delay(1000);
  // while(!Serial); // wait for serial port to connect - needed for Leonardo only

  // only set the date+time one time. You can add 30 sec for compiling and uploading.
#ifdef FORCE_ADJUST_DATETIME
  uint8_t hour  =   13; // 0-23
  uint8_t min   =    0; // 0-59
  uint8_t sec   =    0; // 0-59

  uint8_t day   =   11; // 1-31
  uint8_t month =   10; // 1-12
  uint16_t year = 2020; // 2000-2099

  clock.set(sec, min, hour, day, month, year);
#endif

  // stop/pause RTC
  // clock.stop();

  // start RTC
  clock.start();
}

void loop() {
  char datetime[21]; // Fomatted data for printing to LCD
  uint8_t sec, min, hour, day, month;
  uint16_t year;

  // get time from RTC
  clock.get(&sec, &min, &hour, &day, &month, &year);
  snprintf(datetime, sizeof datetime, "%02d %s %4d %02d:%02d:%02d",
                                      day, strMonth[month-1], year,
                                      hour, min, sec);
  // Serial.print(F("Slave: "));
  // Serial.println(datetime);
  Slave.print(datetime); // send datetime to Master

  // wait a second
  delay(1000);
}
