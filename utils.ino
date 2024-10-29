// Handy Air Quality monitor
// by Michele <o-zone@zerozone.it> Pinassi
//
// https://github.com/michelep
//
// Released under GPLv3 - No any warranty
//

void i2cScanner() {
  uint8_t address;
  Serial.println("Start i2c bus scan...");
  for(address = 1; address < 127; address++) {
    Wire.beginTransmission(address);
    uint8_t error = Wire.endTransmission();
    if (error == 0) {
      Serial.print("I2C device found at address 0x");
      if (address<16)
        Serial.print("0");
      Serial.print(address,HEX);
      Serial.println("  !");
    } else if (error==4) {
      Serial.print("Unknown error at address 0x");
      if (address<16)
        Serial.print("0");
      Serial.println(address,HEX);
    }    
  }
  Serial.println("DONE!");
}

// ************************************
// DEBUG_PRINT() and DEBUG_PRINTLN()
//
// send message to Serial 
// ************************************
void DEBUG_PRINT(String message) {
#ifdef __DEBUG__
  Serial.print(message);
#endif
}

void DEBUG_PRINTLN(String message) {
#ifdef __DEBUG__
  Serial.println(message);
#endif
}

int64_t getTimestamp() {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return (tv.tv_sec * 1000LL + (tv.tv_usec / 1000LL));
}
