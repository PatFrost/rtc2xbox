
// In debug mode the hex codes of buttons pressed on your remote
// will be printed to the serial console
// Uncomment below to enable debug output.
// #define DEBUG_MODE

// These defines setup log output if enabled above (otherwise it
// turns into no-ops that compile out). Status message will be sent to PC at 57600 baud.
// And Wait for serial to be connected. Required on Leonardo with integrated USB.
#ifdef DEBUG_MODE
  #define LOG_BEGIN(...) { Serial.begin(57600); while (!Serial); \
                           Serial.println(F("Start " __DATE__ ", " __TIME__)); \
                           Serial.println(F("Beginning Debug Mode...")); }
  #define LOG_END(...)     Serial.println(__VA_ARGS__)
  #define LOG(...)         Serial.print(__VA_ARGS__)
  #define LOG_FLUSH(...)   Serial.flush()

#else
  // turn off builtin led
  #define LOG_BEGIN(...) { pinMode(LED_BUILTIN_RX, OUTPUT); RXLED0; \
                           pinMode(LED_BUILTIN_TX, OUTPUT); TXLED0; }
  #define LOG_END(...)
  #define LOG(...)
  #define LOG_FLUSH(...)
#endif
