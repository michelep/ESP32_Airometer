// Handy Air Quality monitor
// by Michele <o-zone@zerozone.it> Pinassi
//
// https://github.com/michelep
//
// Released under GPLv3 - No any warranty
//

#include <ArduinoJson.h>
// File System
#include <FS.h>   
#include "SPIFFS.h"

// ************************************
// Config, save and load functions
//
// save and load configuration from config file in SPIFFS. JSON format (need ArduinoJson library)
// ************************************
bool loadConfigFile() {
  JsonDocument root;
  
  DEBUG_PRINT("[#] Loading config...");
  
  configFile = SPIFFS.open(CONFIG_FILE, "r");
  if (!configFile) {
    DEBUG_PRINTLN("ERROR: Config file not available");
    return false;
  } else {
    // Get the root object in the document
    DeserializationError err = deserializeJson(root, configFile);
    if (err) {
      DEBUG_PRINTLN("ERROR: "+String(err.c_str()));
      return false;
    } else {
      strlcpy(config.hostname, root["hostname"] | "aiq-sensor", sizeof(config.hostname));

      DEBUG_PRINTLN("OK");
    }
  }
  configFile.close();
  return true;
}

bool saveConfigFile() {
  JsonDocument root;
  DEBUG_PRINT("[#] Saving config...");

  root["hostname"] = config.hostname;
  
  configFile = SPIFFS.open(CONFIG_FILE, "w");
  if(!configFile) {
    DEBUG_PRINTLN("ERROR: Failed to create config file !");
    return false;
  }
  serializeJson(root,configFile);
  configFile.close();
  DEBUG_PRINTLN("OK");
  return true;
}
