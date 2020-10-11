
#include <Wire.h>
#include "DS1307.h"


DS1307 Clock;

const char strMonth[12][4] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};


void setup() {
  //init Serial port
  Serial.begin(9600);
  while(!Serial); //wait for serial port to connect - needed for Leonardo only

  //init RTC
  Serial.println("Init RTC...");

  //only set the date+time one time
  // Clock.set(0, 0, 8, 4, 10, 2020); //08:00:00 04.10.2020 //sec, min, hour, day, month, year

  //stop/pause RTC
  // Clock.stop();

  //start RTC
  Clock.start();
}

void loop() {
  char lineBuffer[21]; // Fomatted data for printing to LCD
  uint8_t sec, min, hour, day, month;
  uint16_t year;

  //get time from RTC
  Clock.get(&sec, &min, &hour, &day, &month, &year);
  snprintf(lineBuffer, sizeof lineBuffer, "%02d %s %4d %02d:%02d:%02d",
           day, strMonth[month-1], year, hour, min, sec);

  Serial.println(lineBuffer);

  //wait a second
  delay(1000);
}
