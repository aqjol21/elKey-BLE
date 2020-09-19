#include <Arduino.h>
#include "BLEDevice.h"
#include "SPIFFS.h"
#include <Preferences.h>
Preferences preferences;
//#include "BLEScan.h"
#include <Wire.h>        // Library to use I2C to display
#include "SSD1306Wire.h" // Display library

static BLEUUID serviceUUID("ba10");
static BLEUUID passUUID("ba30");
static BLEUUID adminUUID("ba40");
static BLEUUID modeUUID("ba50");
static boolean doConnect = false;
static boolean connected = false;
static boolean doScan = false;
//static unsigned long current;
static BLERemoteCharacteristic *mode;
static BLERemoteCharacteristic *password;
static BLERemoteCharacteristic *admin;

static BLEAdvertisedDevice *myDevice;
BLEClient *pClient;

static int rssi;
static String distanceType = "";
const String serialKey = "060593";
static String key = "";
static bool pass = false;
static bool adminMode = false;
static bool addMode;
// Display and Scan activities
SSD1306Wire display(0x3c, 5, 4);

//mac
const int size = 10;
String mac[size];
String macString;

void write(char *key, String value)
{
  preferences.begin("BlueGate", false);
  preferences.putString(key, value);
  Serial.print(key);
  Serial.print(" value has changed to");
  Serial.println(value);
  preferences.end();
}

void writeBool(char *key, bool value)
{
  preferences.begin("BlueGate", false);
  preferences.putBool(key, value);
  Serial.printf("New addmode: %s", value ? "true" : "false");
  preferences.end();
}

void readMAC()
{
  if (!SPIFFS.begin(true))
  {
    Serial.println("An Error has occurred while mounting SPIFFS");
    return;
  }
  File fileToRead = SPIFFS.open("/mac.txt");
  if (!fileToRead)
  {
    Serial.println("Failed to open file for reading");
    return;
  }

  // Serial.println("MAC addresses which can open: ");
  while (fileToRead.available())
  {
    //int rd = fileToRead.read();
    String content = fileToRead.readString();
    //Serial.println(content);
    content.trim();
    macString = content;
  }
  fileToRead.close();
}

void split(String array[], String string, char delimiter)
{
  int r = 0;
  int t = 0;
  for (int i = 0; i < string.length(); i++)
  {
    if (string.charAt(i) == delimiter)
    {
      array[t] = string.substring(r, i);
      r = (i + 1);
      t++;
    }
  }
}

void printArray(String array[])
{
  for (int i = 0; i < size; i++)
  {
    // Serial.print(i);
    // Serial.print(": ");
    // Serial.println(array[i]);
  }
}

void show(String text)
{
  display.init();
  display.clear();
  delay(100);
  // display.flipScreenVertically();
  display.setFont(ArialMT_Plain_16);
  // clear the display
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.drawStringMaxWidth(0, 0, 120, text);
  display.display();
  delay(200);
}

void appendFile(fs::FS &fs, const char *path, String message)
{
  Serial.printf("Appending to file: %s\r\n", path);

  File file = fs.open(path, FILE_APPEND);
  if (!file)
  {
    Serial.println("- failed to open file for appending");
    return;
  }
  if (file.print(message))
  {
    Serial.println("- message appended");
  }
  else
  {
    Serial.println("- append failed");
  }
}

void open()
{
  display.init();
  Serial.println("Door opened");
  //Trigger
  digitalWrite(14, LOW);
  digitalWrite(16, LOW);
  show("Door opened");
  delay(500);
  digitalWrite(14, HIGH);
  digitalWrite(16, HIGH);
  show("Connected Home");
}

void read()
{
  preferences.begin("BlueGate", false);
  distanceType = preferences.getString("mode");
  // Serial.print("Mode: ");
  // Serial.println(distanceType);
  key = preferences.getString("password");
  Serial.print("Password: ");
  Serial.println(key);
  addMode = preferences.getBool("addMode");
  // Serial.printf("Addmode: %s \n", addMode ? "true" : "false");
  preferences.end();
}

class MyClientCallback : public BLEClientCallbacks
{
  void onConnect(BLEClient *pclient)
  {
    Serial.print("RSSI is: ");
    Serial.println(rssi);
    // Serial.println(esp_get_free_heap_size());
  };

  void onDisconnect(BLEClient *pclient)
  {
    connected = false;
    doScan = true;
    doConnect = false;
    //myDevice = NULL;
    //pclient->setClientCallbacks(NULL);
    Serial.println("onDisconnect");
    //display.init();
    delay(200);
    show("Connected Home");
    //delay(24000);
    esp_deep_sleep_start();
    // Serial.println(esp_get_free_heap_size());
    ESP.restart();
  }
};

bool connectToServer()
{
  Serial.print("Forming a connection to ");
  Serial.println(myDevice->getAddress().toString().c_str());
  Serial.println(" - Created client");
  pClient->setClientCallbacks(new MyClientCallback());
  // Change?
  if (!pClient->isConnected())
    pClient->connect(myDevice); // if you pass BLEAdvertisedDevice instead of address, it will be recognized type of peer device address (public or private)
  delay(1000);
  if (!pClient->isConnected())
  {
    Serial.println(" - Not connected");
    return false;
  } // if you pass BLEAdvertisedDevice instead of address, it will be recognized type of peer device address (public or private)
  Serial.println(" - Connected to server");

  // Obtain a reference to the service we are after in the remote BLE server.
  BLERemoteService *pRemoteService = pClient->getService(serviceUUID);
  if (pRemoteService == nullptr)
  {
    Serial.print("Failed to find our service UUID: ");
    Serial.println(serviceUUID.toString().c_str());
    pClient->disconnect();
    return false;
  }
  Serial.println(" - Found our service");
  // Obtain a reference to the characteristic in the service of the remote BLE server.
  Serial.println(" pass get");

  password = pRemoteService->getCharacteristic(passUUID);

  if (password == nullptr)
  {
    Serial.print("Failed to find our characteristic UUID: ");
    Serial.println(passUUID.toString().c_str());
    pClient->disconnect();
    return false;
  }
  Serial.println(" - Found our characteristic");
  //display.init();
  delay(100);

  if (password->canRead())
  {
    std::string value = password->readValue();
    Serial.print("Typed password was: ");
    Serial.println(value.c_str());
    String s = value.c_str();
    if (key == s)
    {
      pass = true;
      show("Password is correct");
    }
    else if (s == "mac")
    {
      Serial.println("equals to mac");
      writeBool("addMode", true);
      addMode = true;
    }
    else if (serialKey == s)
    {
      adminMode = true;
      show("Admin mode");
    }
    else
    {
      pass = false;
      show("Password is incorrect");
    }
  }

  if (adminMode)
  {
    Serial.println("Admin mode");
    password = NULL;
    delay(200);

    mode = pRemoteService->getCharacteristic(modeUUID);
    Serial.println(" mode is ");

    if (mode == nullptr)
    {
      Serial.print("Failed to find our characteristic UUID: ");
      Serial.println(modeUUID.toString().c_str());
      pClient->disconnect();
      return false;
    }

    if (mode->canRead())
    {
      std::string typeValue = mode->readValue();
      Serial.print("The characteristic value was: ");
      if (distanceType != typeValue.c_str())
      {
        distanceType = typeValue.c_str();
        write("mode", distanceType);
        String s = "Changed to " + distanceType;
        show(s);
      }
    }
    mode = NULL;

    admin = pRemoteService->getCharacteristic(adminUUID);

    if (admin == nullptr)
    {
      Serial.print("Failed to find our characteristic UUID: ");
      Serial.println(adminUUID.toString().c_str());
      pClient->disconnect();
      return false;
    }
    Serial.println(" - Found our characteristic");
    Serial.println("Read  admin");
    delay(500);
    if (admin->canRead())
    {
      std::string adminValue = admin->readValue();
      Serial.print("Typed password was: ");
      Serial.println(adminValue.c_str());

      if (key != adminValue.c_str())
      {
        key = adminValue.c_str();
        write("password", key);
        String s = "Password is set to " + key;
        show(s);
        Serial.println("Password update");
      }
      else
      {
        Serial.println("Password is not set");
        show("Password is not changed");
      }
      delay(400);
    }
    adminMode = false;
  }
  connected = true;
}

class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks
{
  void onResult(BLEAdvertisedDevice advertisedDevice)
  {
    Serial.print("BLE Advertised Device found: ");
    rssi = advertisedDevice.getRSSI();
    Serial.printf(" %s ", advertisedDevice.getAddress().toString().c_str());
    Serial.printf("RSSI: %d \n", rssi);
    String s = advertisedDevice.getAddress().toString().c_str();
    String k = "," + s;

    for (int i = 0; i < size; i++)
    {
      if (s == mac[i] && rssi > -70)
      {
        open();
      }
    }

    if (addMode && rssi > -55)
    {
      appendFile(SPIFFS, "/mac.txt", k);
      Serial.print(k);
      Serial.println(" is added");
      addMode = false;
      show("adding device");
      writeBool("addMode", false);
      readMAC();
    }

    //Serial.println(advertisedDevice.toString().c_str());
    // We have found a device, let us now see if it contains the service we are looking for.
    if (advertisedDevice.haveServiceUUID() && advertisedDevice.isAdvertisingService(serviceUUID))
    {
      if (distanceType == "tap" && rssi >= -65)
      {
        BLEDevice::getScan()->stop();
        myDevice = new BLEAdvertisedDevice(advertisedDevice);
        doConnect = true;
        doScan = false;
      }
      else if (distanceType == "touch" && rssi < -65 && rssi > -76)
      {
        BLEDevice::getScan()->stop();
        myDevice = new BLEAdvertisedDevice(advertisedDevice);
        doConnect = true;
        doScan = false;
      }

      else if (distanceType == "far")
      {
        BLEDevice::getScan()->stop();
        myDevice = new BLEAdvertisedDevice(advertisedDevice);
        doConnect = true;
        doScan = false;
      }
    } // Found our server
  }   // onResult
};    // MyAdvertisedDeviceCallbacks

void setup()
{
  Serial.begin(115200);
  Serial.println("Starting Arduino BLE Client application...");
  read();
  //Trigger for lock
  pinMode(14, OUTPUT);
  digitalWrite(14, HIGH);
  //Built-in LED
  pinMode(16, OUTPUT);
  digitalWrite(16, HIGH);
  //sleep timer 24 seconds
  esp_sleep_enable_timer_wakeup(24 * 1000000);
  readMAC();
  macString.replace("\n", "");
  // Serial.println(macString);
  //macString.replace(" ", "");
  split(mac, macString, ',');
  // printArray(mac);
  BLEDevice::init("");

  // Retrieve a Scanner and set the callback we want to use to be informed when we
  // have detected a new device.  Specify that we want active scanning and start the
  // scan to run for 5 seconds.
  BLEScan *pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setInterval(1349);
  pBLEScan->setWindow(449);
  pBLEScan->setActiveScan(true);
  pBLEScan->start(5, true);
  pClient = BLEDevice::createClient();
  doScan = true;
} // End of setup.

// This is the Arduino main loop function.
void loop()
{
  // If the flag "doConnect" is true then we have scanned for and found the desired
  // BLE Server with which we wish to connect.  Now we connect to it.  Once we are
  // connected we set the connected flag to be true.
  if (doConnect == true)
  {
    if (connectToServer())
      Serial.println("We are now connected to the BLE Server.");
    else
      Serial.println("We have failed to connect to the server; there is nothin more we will do.");
    doConnect = false;
  }

  // If we are connected to a peer BLE Server, update the characteristic each time we are reached
  // with the current time since boot.
  if (connected)
  {
    Serial.println("connected loop");
    if (pass)
    {
      Serial.println("Password is correct");
      if (distanceType == "tap")
      {
        if (rssi >= -65)
          open();
      }

      else if (distanceType == "touch")
      {
        if (rssi < -65 && rssi > -76)
          open();
      }

      else if (distanceType = "far")
        open();
      pass = false;
    }

    else
      Serial.println("Password is incorrect");

    delay(100);
    pClient->disconnect();
  }

  else if (doScan)
  {
    BLEDevice::getScan()->start(3);
    BLEDevice::getScan()->clearResults();
    //?
  }
  //esp_bt_mem_release(ESP_BT_MODE_BLE);
  // Serial.println(esp_get_free_heap_size());

  delay(200); // Delay a second between loops.
}
