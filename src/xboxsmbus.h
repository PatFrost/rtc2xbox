/* XBMC LCD Settings not supported */

// Original Lib and Functions by Ryzee119

#include "SMWire/SMWire.h"

// I2C Bus
uint32_t SMBUS_TIMER;         // Timer used to trigger SMBus reads
uint8_t I2C_MAX_COUNT   = 30; // First time 30, next time 10 (1 count = 1 second)
uint8_t I2C_CHECK_COUNT = 0;  // Tracks what check we're up to of the i2c bus busy state
uint8_t I2C_BUSY_CHECKS = 5;  // To ensure we don't interfere with the actual Xbox's SMBus activity, we check the bus for activity for sending.


/*
 * XBOX System Management Controller
 * See https://web.archive.org/web/20100708015155/http://www.xbox-linux.org/wiki/PIC
 * Note 1:  The PIC version can be read from register 0x01 as a 3-character ASCII string. Each time you read that register, the next of the 3 characters is returned.
 *          "DBG" = DEBUGKIT Green
 *          "B11" = "DEBUGKIT Green"
 *          "P01" = "v1.0"
 *          "P05" = "v1.1"
 *          "P11" or "1P1" or "11P" = v1.2/v1.3 or v1.4. Need to read Video encoder. If FOCUS_ADDRESS returns valid = V1.4, else v1.2/1,3
 *          "P2L" or "1P1" or "11P" = v1.6
 *
 * Note 2:  0x00  SCART
            0x01  HDTV
            0x02  VGA
            0x03  RFU
            0x04  S-Video
            0x05  undefined
            0x06  standard
            0x07  missing/disconnected
 */
#define SMC_ADDRESS 0x10
#define SMC_VER 0x01             //PIC version string. Note 1.
#define SMC_TRAY 0x03            //tray state (Tray open=16, Tray closed no disk=64, Tray closed with disk=96, 49=Opening, 97=Eject pressed?, 81=Closing)
#define SMC_AVSTATE 0x04         //A/V Pack state Note 2.
#define SMC_CPUTEMP 0x09         //CPU temperature (°C) //Read from ADM1032 for better update rate
#define SMC_BOARDTEMP 0x0A       //board temperature (°C) //Read from ADM1032 for better update rate
#define SMC_FANSPEED 0x10        //current fan speed (0~50) multiply by 2 to get 0-100%


/* XBOX ADM1032 System Temperature Monitor (Ref http://xboxdevwiki.net/SMBus)
 * https://www.onsemi.com/pub/Collateral/ADM1032-D.PDF
 */
#define ADM1032_ADDRESS 0x4C
#define ADM1032_MB 0x00             //°C
#define ADM1032_CPU 0x01            //°C


/* XBOX Conexant CX25871 Video Encoder. This chip was used in Xbox versions 1.0 through 1.3
 * https://pdf1.alldatasheet.com/datasheet-pdf/view/153349/CONEXANT/CX25871.html See Section 2.4 Reading Registers
 * Anything interesting to read from Conexant?
 */
#define CONEX_ADDRESS 0x45
#define CONEX_PID     0x00 //Returns 1 byte product ID. Bit 7-5=ID, Bit 4-0 = Version of chip read with readSMBus(CONEX_ADDRESS, CONEX_PID, &rxBuffer, 1)
#define CONEX_A2      0xA2
#define CONEX_A2_NI_OUT     (1<<0) //Bit zero of address 0xA2. 0=interlaced, 1=progressive
#define CONEX_2E      0x2E
#define CONEX_2E_HDTV_EN     (1<<7) //Bit 7 of address 0x2E. 1 = HDTV output mode enabled.


/* XBOX Focus FS453 Video Encoder, This chip was used in Xbox Version 1.4
 * https://www.manualslib.com/manual/52885/Focus-Fs453.html
 * https://assemblergames.com/attachments/focus-datasheet-pdf-zip.17803/
 * Anything interesting to read from Focus chip? See second attachment "2.1 Register Reference Table
 * for address offsets
 */
#define FOCUS_ADDRESS 0x6A
#define FOCUS_PID 0x32 //Returns a 2 byte product ID. Low byte is first. Read with readSMBus(FOCUS_ADDRESS, FOCUS_PID, &rxBuffer, 2);
    /* This is the outputs from VID_CNTL0 register for each mode using component cable
       480P  48 3e  = 0100100000111110
       480I  48 c5  = 0100100011000101
       720P  58 2e  = 0101100000101110
       1080I 50 ae  = 0101000010101110
    */
#define FOCUS_VIDCNTL 0x92 //Video Control 0 register
#define FOCUS_VIDCNTL_VSYNC5_6 (1<<12) //Selects HDTV composite vertical sync width parameter. 1=HDTV
#define FOCUS_VIDCNTL_INT_PROG (1<<7)  //Interlaced/Progressive.  When set =1, input image is interlaced

/*
 * XBOX XCALIBUR For Xbox 1.6. I dont know anything about it?
 */
#define XCALIBUR_ADDRESS 0x70


/* XBOX EEPROM
 * Could read anything stored in EEPROM, but doesnt seem like anything useful for a LCD screen
 * See https://web.archive.org/web/20081216023342/http://www.xbox-linux.org/wiki/Xbox_Serial_EEPROM for addresses
 * for different info
 */
#define EEPROM_ADDRESS 0x54
#define EEPROM_SERIAL 0x34 //Read 12 bytes starting at 0x34.


/* Functions */

void SMBusBegin() {
  Wire.begin(0xDD);                  // Random address that is different from existing bus devices.
  TWBR = ((F_CPU / 72000) - 16) / 2; // Change I2C frequency closer to OG Xbox SMBus speed. ~72kHz Not compulsory really, but a safe bet
}

/*
   Function: Check if the SMBus/I2C bus is busy
   ----------------------------
     returns: false if busy is free, true if busy or still checking

  Pins wire SDA and SCL
  https://github.com/arduino/ArduinoCore-avr/blob/master/variants/leonardo/pins_arduino.h#L100
*/
bool i2cBusy() {
  bool busy = true;
  if (digitalRead(SDA) == 0 || digitalRead(SCL) == 0) { //If either the data or clock line is low, the line must be busy
    I2C_CHECK_COUNT = I2C_BUSY_CHECKS;
  } else {
    I2C_CHECK_COUNT--; //Bus isn't busy, decrease check counter so we check multiple times to be sure.
  }
  // Check that i2cBus is free, it has been atleast 2 seconds since last call
  I2C_CHECK_COUNT = min(I2C_CHECK_COUNT, I2C_MAX_COUNT);
  if (I2C_CHECK_COUNT == 0 && (millis() - SMBUS_TIMER) > 2000) {
    SMBUS_TIMER = millis();
    I2C_MAX_COUNT = 10;
    busy = false;
  }
  return busy;
}

/*
   Function: Read the Xbox SMBus
   ----------------------------
     address: The address of the device on the SMBus
     command: The command to send to the device. Normally the register to read from
     rx: A pointer to a receive data buffer
     len: How many bytes to read
     returns: 0 on success, -1 on error.
*/
int8_t readSMBus(uint8_t address, uint8_t command, char* rx, uint8_t len) {
  Wire.beginTransmission(address);
  Wire.write(command);
  if (Wire.endTransmission(false) == 0) { //Needs to be false. Send I2c repeated start, dont release bus yet
    Wire.requestFrom(address, len);
    for (uint8_t i=0; i<len; i++) {
      rx[i] = Wire.read();
    }
    return 0;
  } else {
    return -1;
  }
}

String getXboxFanSpeed() {
  char rxBuffer[20];   // Raw data received from SMBus
  char lineBuffer[20]; // Fomatted data for printing to LCD
  //Read the current fan speed directly from the SMC and print to LCD
  if (readSMBus(SMC_ADDRESS, SMC_FANSPEED, &rxBuffer[0], 1) == 0) {
    if (rxBuffer[0] >= 0 && rxBuffer[0] <= 50) { //Sanity check. Number should be 0-50
      snprintf(lineBuffer, sizeof lineBuffer, "FAN: %1u%% ", rxBuffer[0] * 2);
      return lineBuffer;
    }
  }
  return "FAN:   % ";
}

String getXboxResolution() {
  char rxBuffer[20]; // Raw data received from SMBus
  // Read Focus Chip to determine video resolution (for Version 1.4 console only)
  if (readSMBus(FOCUS_ADDRESS, FOCUS_PID, &rxBuffer[0], 2) == 0) {
    uint16_t PID = ((uint16_t)rxBuffer[1]) << 8 | rxBuffer[0];
    if (PID == 0xFE05) {
      readSMBus(FOCUS_ADDRESS, FOCUS_VIDCNTL, &rxBuffer[0], 2);
      uint16_t VID_CNTL0 = ((uint16_t)rxBuffer[1]) << 8 | rxBuffer[0];

      if (VID_CNTL0 & FOCUS_VIDCNTL_VSYNC5_6 && VID_CNTL0 & FOCUS_VIDCNTL_INT_PROG) {
        //Must be HDTV, interlaced (1080i)
        return " 1080i  ";
      }
      else if (VID_CNTL0 & FOCUS_VIDCNTL_VSYNC5_6 && !(VID_CNTL0 & FOCUS_VIDCNTL_INT_PROG)) {
        //Must be HDTV, Progressive 720p
        return "  720p  ";
      }
      else if (!(VID_CNTL0 & FOCUS_VIDCNTL_VSYNC5_6) && VID_CNTL0 & FOCUS_VIDCNTL_INT_PROG) {
        //Must be SDTV, interlaced 480i
        return "  480i  ";
      }
      else if (!(VID_CNTL0 & FOCUS_VIDCNTL_VSYNC5_6) && !(VID_CNTL0 & FOCUS_VIDCNTL_INT_PROG)) {
        //Must be SDTV, Progressive 480p
        return "  480p  ";
      }
      else {
        return String(VID_CNTL0, HEX); //Not sure what it is. Print the code.
      }
    }
  }
  //Read Conexant Chip to determine video resolution (for Version 1.0 to 1.3 console only)
  else if (readSMBus(CONEX_ADDRESS, CONEX_2E, &rxBuffer[0], 1) == 0) {
    if ((uint8_t)(rxBuffer[0] & 3) == 3) {
      //Must be 1080i
      return "  1080i ";
    }
    else if ((uint8_t)(rxBuffer[0] & 3) == 2) {
      //Must be 720p
      return "  720p  ";
    }
    else if ((uint8_t)(rxBuffer[0] & 3) == 1 && rxBuffer[0]&CONEX_2E_HDTV_EN) {
      //Must be 480p
      return "  480p  ";
    }
    else {
      return "  480i  ";
    }
  }
  return "        ";
}

String getXboxTemperatures(bool fahrenheit) {
  // Read the CPU and M/B temps
  char rxBuffer[20];   // Raw data received from SMBus
  char lineBuffer[21]; // Fomatted data for printing to LCD
  // Try ADM1032
  if ((readSMBus(ADM1032_ADDRESS, ADM1032_CPU, &rxBuffer[0], 1) == 0 &&
       readSMBus(ADM1032_ADDRESS, ADM1032_MB, &rxBuffer[1], 1) == 0) ||
      //If fails, its probably a 1.6. Read SMC instead.
      (readSMBus(SMC_ADDRESS, SMC_CPUTEMP, &rxBuffer[0], 1) == 0 &&
       readSMBus(SMC_ADDRESS, SMC_BOARDTEMP, &rxBuffer[1], 1) == 0))
  {
    if (rxBuffer[0] < 200 && rxBuffer[1] < 200 && rxBuffer[0] > 0 && rxBuffer[1] > 0) {
      if (fahrenheit) {
        snprintf(lineBuffer, sizeof lineBuffer, "CPU: %1u%c  M/B: %1u%c  ",
                 (uint8_t)((float)rxBuffer[0] * 1.8 + 32.0), (char)223,
                 (uint8_t)((float)rxBuffer[1] * 1.8 + 32.0), (char)223);
      } else {
        snprintf(lineBuffer, sizeof lineBuffer, "CPU: %1u%c  M/B: %1u%c  ",
                 rxBuffer[0], (char)223, rxBuffer[1], (char)223);
      }
      return lineBuffer;
    }
  }
  // return "                    ";
  snprintf(lineBuffer, sizeof lineBuffer, "CPU:   %c  M/B:   %c  ", (char)223, (char)223);
  return lineBuffer;
}
