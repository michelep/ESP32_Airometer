// Handy Air Quality monitor
// by Michele <o-zone@zerozone.it> Pinassi
//
// https://github.com/michelep
//
// Released under GPLv3 - No any warranty
//
// based on BME680 and DOIT ESP32 DEVKIT v1
// LCD display SPI 65k 128x160 driver ST7735S
//
// 23.10.2024 - v0.0.1 - First public release
//

#include <esp_bt.h>

#include <Wire.h>

#define __DEBUG__
const char BUILD[] = __DATE__ " " __TIME__;

#define FW_NAME "esp32-airometer"
#define FW_VERSION "0.0.1"

/* i2c bus pins */
#define I2C_SDA_PIN 23
#define I2C_SCL_PIN 22

#define SEALEVELPRESSURE_HPA (1013.25)

// BSEC Arduino Library v1.8.1492
//https://github.com/BoschSensortec/BSEC-Arduino-library/
#include "bsec.h"

#define BME680_ADDRESS 0x77
Bsec bme680;

// SPIFFS
#include "FS.h"
#include "SPIFFS.h"

#define FORMAT_SPIFFS_IF_FAILED 1

#include "soc/soc.h"           //disable brownour problems
#include "soc/rtc_cntl_reg.h"  //disable brownour problems

// ST7735S Driver
// https://github.com/adafruit/Adafruit-ST7735-Library
#include <Adafruit_GFX.h>     // Core graphics library
#include <Adafruit_ST7735.h>  // Hardware-specific library
#include <SPI.h>

#define TFT_MOSI 13
#define TFT_SCLK 14
#define TFT_CS 15  // Chip select control pin
#define TFT_DC 2   // Data Command control pin
#define TFT_RST 12
#define TFT_BL 27

Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_MOSI, TFT_SCLK, TFT_RST);

#include <Fonts/Org_01.h>

// Task Scheduler
// https://github.com/arkhipenko/TaskScheduler
#include <TaskScheduler.h>

Scheduler runner;

#define MINUTE 60000
#define SECOND 1000

#define DISPLAY_INTERVAL SECOND * 5
void displayCallback();
Task displayTask(DISPLAY_INTERVAL, TASK_FOREVER, &displayCallback);

#define TICK_INTERVAL SECOND * 1
void tickCallback();
Task tickTask(TICK_INTERVAL, TASK_FOREVER, &tickCallback);

#define TOUCH_INTERVAL 250
void touchCallback();
Task touchTask(TOUCH_INTERVAL, TASK_FOREVER, &touchCallback);

void DEBUG_PRINTLN(String);
void DEBUG_PRINT(String);

unsigned long last;

bool isSleeping = false;

// BTLE Service description
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>

#define bme680Service BLEUUID((uint16_t)0x181A)  // Environmental Sensing

BLECharacteristic bmeTempCharacteristics(BLEUUID((uint16_t)0x2A6E), BLECharacteristic::PROPERTY_NOTIFY);
BLEDescriptor bmeTempDescriptor(BLEUUID((uint16_t)0x2901));

BLECharacteristic bmeHumCharacteristics(BLEUUID((uint16_t)0x2A6F), BLECharacteristic::PROPERTY_NOTIFY);
BLEDescriptor bmeHumDescriptor(BLEUUID((uint16_t)0x2901));

BLECharacteristic bmePresCharacteristics(BLEUUID((uint16_t)0x2A6D), BLECharacteristic::PROPERTY_NOTIFY);
BLEDescriptor bmePresDescriptor(BLEUUID((uint16_t)0x2901));

BLECharacteristic batteryLevelCharacteristic(BLEUUID((uint16_t)0x2A19), BLECharacteristic::PROPERTY_NOTIFY);
BLEDescriptor batteryLevelDescriptor(BLEUUID((uint16_t)0x2901));

BLECharacteristic timeCharacteristics(BLEUUID((uint16_t)0x2A2B), BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_READ);
BLEDescriptor timeDescriptor(BLEUUID((uint16_t)0x2901));

uint8_t btDeviceConnected = 0;

// Config
struct Config {
  // Host config
  char hostname[16];
};

#define CONFIG_FILE "/config.json"

File configFile;
Config config;  // Global config object

#include <ArduinoJson.h>
JsonDocument env;

/*
 * Initialize BME680 sensor
 * 
 * true=OK, false=FAILED
 */
void initBME680() {
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);

  bme680.begin(BME680_ADDRESS, Wire);

  DEBUG_PRINTLN("\nBSEC library version " + String(bme680.version.major) + "." + String(bme680.version.minor) + "." + String(bme680.version.major_bugfix) + "." + String(bme680.version.minor_bugfix));

  bsec_virtual_sensor_t sensorList[13] = {
    BSEC_OUTPUT_IAQ,
    BSEC_OUTPUT_STATIC_IAQ,
    BSEC_OUTPUT_CO2_EQUIVALENT,
    BSEC_OUTPUT_BREATH_VOC_EQUIVALENT,
    BSEC_OUTPUT_RAW_TEMPERATURE,
    BSEC_OUTPUT_RAW_PRESSURE,
    BSEC_OUTPUT_RAW_HUMIDITY,
    BSEC_OUTPUT_RAW_GAS,
    BSEC_OUTPUT_STABILIZATION_STATUS,
    BSEC_OUTPUT_RUN_IN_STATUS,
    BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_TEMPERATURE,
    BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_HUMIDITY,
    BSEC_OUTPUT_GAS_PERCENTAGE
  };

  bme680.updateSubscription(sensorList, 13, BSEC_SAMPLE_RATE_LP);
}

// ******************************
#define RGB(r, g, b) (((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3))

#define GREEN ST7735_GREEN
#define RED ST7735_RED
#define BLACK ST77XX_BLACK
#define WHITE ST77XX_WHITE
#define YELLOW ST7735_YELLOW
#define GRAY RGB(183, 183, 183)

#define TEMP_BG RGB(122, 178, 211)
#define TEMP_FG WHITE
#define HUM_BG RGB(74, 98, 138)
#define HUM_FG WHITE
#define VOC_BG RGB(23, 155, 174)
#define VOC_FG WHITE
#define CO2_BG RGB(255, 131, 67)
#define CO2_FG WHITE
#define PRESS_BG RGB(56, 75, 112)
#define PRESS_FG WHITE
#define UPTIME_FG RGB(0, 175, 145)
#define UPTIME_BG BLACK

// Background color for status box
#define STATUS_DEFAULT RGB(128, 185, 173)
#define STATUS_GOOD RGB(31, 138, 112)
#define STATUS_AVERAGE RGB(191, 219, 56)
#define STATUS_NOTSOGOOD RGB(252, 115, 0)
#define STATUS_BAD RGB(255, 145, 0)
#define STATUS_VERYBAD RGB(255, 101, 0)
#define STATUS_DANGEROUS RGB(255, 32, 78)

// ******************************
// Initialize 1,8" TFT display
// 128 x 160
//
void initMainDisplay() {
  tft.setRotation(0);
  tft.fillScreen(BLACK);
  tft.setTextColor(WHITE);
  tft.setFont(&Org_01);
  tft.setCursor(5, 10);
  tft.printf("Booting...");
}

// ******************************
//
// sleepMode ON and OFF
//
#define SLEEP_COUNTDOWN 30
int sleepCountdown = SLEEP_COUNTDOWN;

void sleepModeOn() {
  DEBUG_PRINTLN("sleepModeON()");
  // Turn screen OFF via backlight
  digitalWrite(TFT_BL, LOW);
}

void sleepModeOff() {
  DEBUG_PRINTLN("sleepModeOFF()");
  // Turn sceen ON via backlight
  digitalWrite(TFT_BL, HIGH);
  // reset sleep countdown
  sleepCountdown = SLEEP_COUNTDOWN;
}

// ******************************
//
// tickCallback
//
uint8_t tickStatus = true;

void tickCallback() {
  sleepCountdown--;
  if (sleepCountdown == 0) {
    sleepModeOn();
    isSleeping = true;
  }

  if (tickStatus) {
    tickStatus = false;
  } else {
    tickStatus = true;
  }
}

const char *months[12] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
const char *days[7] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };

uint8_t curDay, curMin;

void drawMyBox(int x, int y, int w, int h, uint16_t fc, uint16_t bc, const char *text, uint8_t tsize=1) {
    // Check for valid width and height
    if (w <= 0 || h <= 0) return;

    int16_t x1, y1;
    uint16_t width, height;

    GFXcanvas1 canvas(w, h);

    // Get text bounds
    canvas.setFont(&Org_01);
    canvas.setTextSize(tsize);
    canvas.getTextBounds(text, x, y, &x1, &y1, &width, &height);
    canvas.setCursor((w - width) / 2, (h + height) / 2);
    canvas.setTextColor(fc);
    canvas.print(text);

    // Draw the bitmap on the display
    tft.drawBitmap(x, y, canvas.getBuffer(), w, h, fc, bc);
}

void displayCallback() {
  uint16_t color;

  // Top box - Overall air quality
  String t;
  char c[32];
  float v;

  if (env["iaq_accuracy"] == 1) {
    v = env["iaq_static"];

    /*IAQ Index Classification

    0 .. 50 Good
    51 .. 100 Average
    101 .. 150 Little bad
    151 .. 200 Bad
    201 .. 300 Worse
    301 .. 500 Very bad
    */

    if ((v > 0) && (v < 50)) {
      t = "GOOD";
      color = STATUS_GOOD;
    } else if ((v > 50) && (v < 100)) {
      t = "AVERAGE";
      color = STATUS_AVERAGE;
    } else if ((v > 100) && (v < 150)) {
      t = "NOT SO GOOD";
      color = STATUS_NOTSOGOOD;
    } else if ((v > 150) && (v < 200)) {
      t = "BAD";
      color = STATUS_BAD;
    } else if ((v > 200) && (v < 300)) {
      t = "VERY BAD";
      color = STATUS_VERYBAD;
    } else if (v > 300) {
      t = "DANGEROUS";
      color = STATUS_DANGEROUS;
    }
  } else {
    color = STATUS_DEFAULT;
    t = "Please wait...";
  }
  // First row - FULL - Status
  drawMyBox(0,0,128,30,WHITE,color,t.c_str());
  
  // Second row - LEFT - TEMP box
  v = env["temp"];
  sprintf(c,"%.1fC", v);
  drawMyBox(0,30,64,50,TEMP_FG,TEMP_BG,c,2);

  // Second row - RIGHT - HUM box
  v = env["humidity"];
  sprintf(c,"%.1f%%", v);
  drawMyBox(64,30,64,50,HUM_FG,HUM_BG,c,2);
  
  // Third row - LEFT - VOC box - VOC ppm
  if (env["iaq_accuracy"] == 1) {
    v = env["bvoc"];
    if(v < 0.06) {
      t = "Excel";
    } else if((v > 0.05) && (v < 0.22)) {
      t = "Good";
    } else if((v > 0.21) && (v < 0.66)) {
      t = "Fair";
    } else if((v > 0.65) && (v < 2.2)) {
      t = "Poor";
    } else if(v > 2.1) {
      t = "Bad";
    }

    sprintf(c,"%.2f", v);
  } else {
    sprintf(c,"...");
    t = "";
  }
  drawMyBox(0,80,64,15,VOC_FG,VOC_BG,c,1);
  drawMyBox(0,95,64,15,VOC_FG,VOC_BG,t.c_str(),1);
  
  // Third row - RIGHT - CO2 box
  if (env["iaq_accuracy"] == 1) {
    v = env["co2"];

    /* CO2 
     *  300-600 Excellent
     *  601-800 Good
     *  801-1001 Fair
     *  1001-1500 Mediocre
     *  1501-2000 Bad
     *  >2000     Insane
     */

    if (v < 600) {
      t = "Excel";
    } else if ((v > 600) && (v < 800)) {
      t = "Good";
    } else if ((v > 800) && (v < 1000)) {
      t = "Fair";
    } else if ((v > 1001) && (v < 1500)) {
      t = "Bad";
    } else if ((v > 1500) && (v < 2000)) {
      t = "Worse";
    } else if (v > 2000) {
      t = "Insane";
    }
    sprintf(c,"%.2f", v);
  } else {
    sprintf(c,"...");
    t = "";
  }
  
  drawMyBox(64,80,64,15,CO2_FG,CO2_BG,c,1);
  drawMyBox(64,95,64,15,CO2_FG,CO2_BG,t.c_str(),1);

  // Fourth row - FULL - Pressure  
  v = float(env["pressure"]);  
  sprintf(c,"%.2f hPa", v);

  drawMyBox(0,110,127,25,PRESS_FG,PRESS_BG,c,2);
  
  // Fifth row - LEFT - Bluetooth
  sprintf(c,"B");
  if (btDeviceConnected) {
    drawMyBox(0,135,30,30,RGB(57, 181, 224),BLACK,c,2);
  } else {
    drawMyBox(0,135,30,30,GRAY,BLACK,c,2);    
  }

  // Fifth row - CENTER - Time
  itoa(env["uptime"],c,32);
  drawMyBox(30,135,68,30,UPTIME_FG,UPTIME_BG,c,1);    

  // Fifth row - RIGHT - Battery
  tft.fillRect(118, 140, 3, 2, GRAY);
  tft.drawRect(116, 142, 7, 13, GRAY);

  batteryLevel();

  if (env["battery"] > 1.7) {
    tft.fillRect(117, 144, 5, 3, GRAY);
  } else {
    // tft.fillRect(117, 144, 5, 3, BLACK);
  }
  if (env["battery"] > 1.6) {
    tft.fillRect(117, 148, 5, 3, GRAY);
  } else {
    // tft.fillRect(117, 148, 5, 3, BLACK);
  }
  if (env["battery"] > 1.5) {
    tft.fillRect(117, 152, 5, 3, GRAY);
  } else {
    // tft.fillRect(117, 152, 5, 3, RED);
  }
  if (env["battery"] > 1.0) {
    tft.fillRect(117, 152, 5, 3, RED);
  }
}

// 
// Touch de-bouncer
//
bool onTouch=false;
uint8_t onTouchTimer=0;

void touchCallback() {
  if(!onTouch) {
    DEBUG_PRINTLN("Touch detected!");

    onTouch = true;
    onTouchTimer = 20;
      
    // Se il sistema era in fase di sleep, sveglia!
    if (isSleeping) {
      sleepModeOff();
      isSleeping = false;
    }
  }
}

/*
 * BATTERY LEVEL - Power connected to ADC PIN via voltage-divider
 *
 */
#define BATTERY_ADC 36  // PIN14 GPIO36

void batteryLevel() {
  int ADC_VALUE = analogRead(BATTERY_ADC);
  float voltage_value = (ADC_VALUE * 3.3) / (4095);
  Serial.print("[#] Battery is ");
  Serial.print(voltage_value);
  Serial.println("v");
  env["battery"] = voltage_value;
}

void checkBME680() {
  if (bme680.bsecStatus != BSEC_OK) {
    if (bme680.bsecStatus < BSEC_OK) {
      DEBUG_PRINTLN("[!] BSEC error code : " + String(bme680.bsecStatus));
      env["message"] = "BSEC E: "+bme680.bsecStatus;
    } else {
      DEBUG_PRINTLN("[!] BSEC warning code : " + String(bme680.bsecStatus));
      env["message"] = "BSEC W: "+bme680.bsecStatus;
    }
  }

  if (bme680.bme68xStatus != BME68X_OK) {
    if (bme680.bme68xStatus < BME68X_OK) {
      DEBUG_PRINTLN("[!] BME680 error code : " + String(bme680.bme68xStatus));
      env["message"] = "BME680 E: "+bme680.bme68xStatus;
    } else {
      DEBUG_PRINTLN("[!] BME680 warning code : " + String(bme680.bme68xStatus));
      env["message"] = "BME680 W: "+bme680.bme68xStatus;
    }
  }
}

/*
 * BME680 Task on CORE0, fully dedicated to fetch data from BME680 sensor
 */
void bme680Task(void *pvParameters) {
  while (1) {
    if (bme680.run()) {  // If new data is available
      String output = String(bme680.temperature);
      output += ", " + String(bme680.rawTemperature);
      output += ", " + String(bme680.pressure);
      output += ", " + String(bme680.rawHumidity);
      output += ", " + String(bme680.gasResistance);
      output += ", " + String(bme680.gasPercentage);
      output += ", " + String(bme680.iaq);
      output += ", " + String(bme680.iaqAccuracy);
      output += ", " + String(bme680.humidity);
      output += ", " + String(bme680.staticIaq);
      output += ", " + String(bme680.co2Equivalent);
      output += ", " + String(bme680.breathVocEquivalent);
      Serial.println(output);

      env["temp"] = bme680.temperature;
      env["pressure"] = bme680.pressure / 100;  // Convert to hPa
      env["humidity"] = bme680.humidity;

      env["co2"] = bme680.co2Equivalent;

      env["bvoc"] = bme680.breathVocEquivalent;

      env["iaq"] = bme680.iaq;
      env["iaq_accuracy"] = bme680.iaqAccuracy;

      env["iaq_static"] = bme680.staticIaq;
    } else {
      checkBME680();
    }
    delay(10);  
  }
}

/*
 * setup()
 */
void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println();
  Serial.println("Handy air quality monitor " + String(FW_NAME) + "/" + String(FW_VERSION));
  delay(1000);

  DEBUG_PRINT("[#] Initializing OLED display...");
  tft.setSPISpeed(40000000);
  tft.initR(INITR_BLACKTAB);
  // Backlight
  DEBUG_PRINT("BL ON...");
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);
  // Initialize
  initMainDisplay();
  DEBUG_PRINTLN("DONE!");

  DEBUG_PRINT("[#] Mounting SPIFFS...");

  if (!SPIFFS.begin(true)) {
    DEBUG_PRINT("FAILED. Try formatting...");
    if (SPIFFS.format()) {
      DEBUG_PRINTLN("DONE!");
    } else {
      DEBUG_PRINTLN("ERROR!");
      ESP.restart();
    }
    DEBUG_PRINTLN("FAILED! An error has occurred while mounting SPIFFS");
    ESP.restart();
  } else {
    DEBUG_PRINTLN("DONE!");
  }

  if (!loadConfigFile()) {
    tft.setTextColor(RED);
    tft.setCursor(5, 15);
    tft.printf("FATAL: NO CONFIG");
    delay(5000);
    ESP.restart();
  }

  //  DEBUG_PRINTLN("[#] Initializing i2c bus...");
  //  i2cScanner();

  initBME680();

  DEBUG_PRINTLN("[#] Initializing tasks...");
  // Add tick task
  runner.addTask(tickTask);
  tickTask.enable();

  // Add display task
  runner.addTask(displayTask);
  displayTask.enable();

  // touch interrupt
  touchAttachInterrupt(T0, touchCallback, 20);

  // Bluetooth initialize
  btInit();

  // Launch BME680 on CORE0 with normal priority
  // disableCore0WDT();  // Disable watchdog on CORE0
  
  xTaskCreatePinnedToCore(
    bme680Task,   /* Function to implement the task */
    "bme680Task", /* Name of the task */
    10000,        /* Stack size in words */
    NULL,         /* Task input parameter */
    10,           /* Priority of the task */
    NULL,         /* Task handle. */
    0);           /* Core where the task should run */
    
  env["status"] = "Running";
}

/*
 * main loop()
 */
 
void loop() {
  if ((millis() - last) > 100) {
    
    env["uptime"] = millis() / 1000;
    
    last = millis();

    if(onTouch) {
      onTouchTimer--;
      if(onTouchTimer == 0) {
        onTouch = false;
      }
    }
    
    // If there's bt device connected, update services value
    if (btDeviceConnected) {
      bmeTempCharacteristics.setValue(env["temp"]);
      bmeTempCharacteristics.notify();

      bmeHumCharacteristics.setValue(env["humidity"]);
      bmeHumCharacteristics.notify();

      bmePresCharacteristics.setValue(env["pressure"]);
      bmePresCharacteristics.notify();
    }
  }
  
  runner.execute();
}
