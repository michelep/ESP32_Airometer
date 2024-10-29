// Handy Air Quality monitor
// by Michele <o-zone@zerozone.it> Pinassi
//
// https://github.com/michelep
//
// Released under GPLv3 - No any warranty
//

class BTServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) 
    {
        btDeviceConnected++;
        BLEDevice::startAdvertising();
        DEBUG_PRINTLN("[#] BTLE client connected!");
    };

    void onDisconnect(BLEServer* pServer) 
    {
        btDeviceConnected--;
        pServer->startAdvertising(); // restart advertising
        DEBUG_PRINTLN("[#] BTLE client disconnected!");
    };
};

class timeCallback: public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pCharacteristic) {
    String rxValue = pCharacteristic->getValue().c_str();
    DEBUG_PRINTLN("[#] BTLE onWrite(): "+rxValue);
  };
  void onRead(BLECharacteristic *pCharacteristic) {
    DEBUG_PRINTLN("[#] BTLE onRead()");
//    pCharacteristic->setValue();
  };
};

BLEServer* pServer = NULL;

void btInit() {
  DEBUG_PRINT("[#] Initializing BTLE...");
  
  BLEDevice::init(config.hostname);
  
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new BTServerCallbacks());

  BLEService *pService = pServer->createService(bme680Service);

  pService->addCharacteristic(&bmeTempCharacteristics);
  bmeTempDescriptor.setValue("BME temperature");
  bmeTempCharacteristics.addDescriptor(new BLE2902());
  
  pService->addCharacteristic(&bmeHumCharacteristics);
  bmeHumDescriptor.setValue("BME humidity");
  bmeHumCharacteristics.addDescriptor(new BLE2902());

  pService->addCharacteristic(&bmePresCharacteristics);
  bmePresDescriptor.setValue("BME pressure");
  bmePresCharacteristics.addDescriptor(new BLE2902());

  pService->addCharacteristic(&timeCharacteristics);
  timeCharacteristics.addDescriptor(new BLE2902());
  timeCharacteristics.setCallbacks(new timeCallback());
  
  pService->addCharacteristic(&batteryLevelCharacteristic);
  batteryLevelDescriptor.setValue("Percentage 0 - 100");
  batteryLevelCharacteristic.addDescriptor(new BLE2902());

  // Start the service
  pService->start();

  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  
  pAdvertising->addServiceUUID(bme680Service);
  
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);
  BLEDevice::startAdvertising();

  DEBUG_PRINTLN("DONE!");
}
