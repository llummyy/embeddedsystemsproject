#include <WiFiNINA.h>
#include <PubSubClient.h>
#include <WiFiSSLClient.h>
#include "WifiCreds.h"
#include <Wire.h>
#include <rgb_lcd.h>
#include <Servo.h>
#include <Keypad.h>


#define ROWS 4
#define COLS 3
String PinInput = "";

// WiFi Credentials
const char* ssid = WiFiSSID;
const char* password = WiFiPassword;

// MQTT broker details
const char* mqttServer = "49dcc968ea4a46e28a0488449edcbb8d.s1.eu.hivemq.cloud";
const int mqttPort = 8883;
const char* mqttUser = "hivemq.webclient.1747723393121";
const char* mqttPassword = "5l&$gs0IF3qySRZ?9;aC";
const char* mqttTopic = "DoorLockAccess";

WiFiSSLClient wifiClient; // Secure client for port 8883
PubSubClient client(wifiClient);

rgb_lcd lcd;
Servo lockServo;
const int servoPin = 9;  

int failedAttempts = 0;
const int maxAttempts = 3;
bool lockedOut = false;
unsigned long lockoutTime = 0;
const unsigned long lockoutDuration = 10000; // 10 seconds
const int buzzerPin = 10;
const int switchPin = 12;

bool isLocked = true; // Start locked
bool wifiConnected = false;



struct Account {
  String name;
  String pin;
};

Account users[] = {
  {"liam", "1432"},
  {"linuki", "2844"},
  {"test", "1111"}
};

void callback(char* topic, byte* payload, unsigned int length) {
  String message;
  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  message.trim();

  Serial.print("MQTT message received on topic ");
  Serial.print(topic);
  Serial.print(": ");
  Serial.println(message);

  if (message == "unlock" && isLocked) {
    Serial.println("Unlock command received");
    lockServo.write(120); // Unlock
    isLocked = false;
    Serial.println("Door unlocked");
  } else if (message == "lock" && !isLocked) {
    Serial.println("Lock command received");
    lockServo.write(0); // Lock
    isLocked = true;
    Serial.println("Door locked");
  }
}



void openDoor(int userIndex) {
  lockServo.write(120);
  isLocked = false;
  Serial.println("Door unlocked");

  String logMsg = "Access: " + users[userIndex].name;
  client.publish("DoorLockAccess", logMsg.c_str());

  delay(5000);

  lockServo.write(0);
  isLocked = true;
  Serial.println("Door relocked");
  
}

void lockDoor() {
  lockServo.write(0);  // Lock position
  isLocked = true;
  Serial.println("Door locked");
}

bool AccessCheck(String pin) {
  for (int i = 0; i < sizeof(users)/sizeof(users[0]); i++) {
    if (users[i].pin == pin) {
      failedAttempts = 0;
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Access granted");
      lcd.setCursor(0, 1);
      lcd.print("Welcome, ");
      lcd.print(users[i].name);
      Serial.print("Access granted: ");
      Serial.println(users[i].name);
      openDoor(i);  // Unlock door
      return true;
    }
  }

  failedAttempts++;
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Access denied");
  Serial.println("Access denied");

  if (failedAttempts >= maxAttempts) {
    lockedOut = true;
    lockoutTime = millis();
    lcd.setCursor(0, 1);
    lcd.print("LOCKED OUT");
    String lockdownLog = ("LOCKDOWN OCCURED!!");
    client.publish("DoorLockAccess", lockdownLog.c_str());

    for (int i = 0; i < 5; i++) {
      tone(buzzerPin, 2000);  // 2kHz tone
      delay(500);
      noTone(buzzerPin);
      delay(250);
    }
  }

  return false;
}  

char keys[ROWS][COLS] = {
  {'9','6','3'},
  {'8','5','2'},
  {'7','4','1'},
  {'*','0','#'}
};

byte rowPins[ROWS] = {2, 3, 4, 5 };
byte colPins[COLS] = {6, 7, 8};

Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

void backupSwitch() {
  static bool lastSwitchState = HIGH;
  bool switchState = digitalRead(switchPin);

  if (switchState != lastSwitchState) {
    delay(50);  
    switchState = digitalRead(switchPin);

    if (switchState == LOW && isLocked) {
      Serial.println("Backup switch: Unlocking door");
      lockServo.write(120);
      isLocked = false;
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Door Open");
    } 
    else if (switchState == HIGH && !isLocked) {
      Serial.println("Backup switch: Locking door");
      lockServo.write(0);
      isLocked = true;
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Door Closed");
    }
  }

  lastSwitchState = switchState;
}

void checkWiFi() {
  if (WiFi.status() != WL_CONNECTED) {
    if (wifiConnected) {
      Serial.println("WiFi lost, switching to offline mode.");
      lcd.clear();
      lcd.print("Offline mode");
      wifiConnected = false;
      client.disconnect();
    }

  
    static unsigned long lastAttemptTime = 0;
    unsigned long now = millis();

    if (now - lastAttemptTime > 5000) { // Try every 5 seconds
      Serial.println("Attempting to reconnect to WiFi...");
      WiFi.begin(ssid, password);
      lastAttemptTime = now;
    }

    // Allow door to work in offline mode
    backupSwitch();
  } 
  else {
    if (!wifiConnected) {
      Serial.println("WiFi restored, reconnecting MQTT...");
      lcd.clear();
      lcd.print("WiFi connected");
      wifiConnected = true;

      client.setServer(mqttServer, mqttPort);
      client.setCallback(callback);

      while (!client.connected()) {
        Serial.print("Connecting to MQTT...");
        if (client.connect("NanoClient", mqttUser, mqttPassword)) {
          Serial.println("connected");
          client.subscribe(mqttTopic);
        } else {
          Serial.print("failed, rc=");
          Serial.print(client.state());
          delay(2000);
        }
      }
    }

    client.loop(); // Handle MQTT communication
  }
}


void setup() {
  Serial.begin(9600);

  pinMode(switchPin, INPUT_PULLUP);
  pinMode(buzzerPin, OUTPUT);

  lcd.begin(16, 2);
  lcd.setRGB(0, 128, 64);
  lcd.setCursor(0, 0);
  lcd.print("Connecting WiFi");

  WiFi.begin(ssid, password);
  unsigned long startAttemptTime = millis();

  while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 10000) {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected.");
    lcd.clear();
    lcd.print("WiFi connected");
    wifiConnected = true;

    client.setServer(mqttServer, mqttPort);
    client.setCallback(callback);

    while (!client.connected()) {
      Serial.print("Connecting to MQTT...");
      if (client.connect("NanoClient", mqttUser, mqttPassword)) {
        Serial.println("connected");
        client.subscribe(mqttTopic);
      } else {
        Serial.print("failed");
        Serial.print(client.state());
        delay(2000);
      }
    }
  } else {
    Serial.println("\nWiFi connection failed, entering offline mode.");
    lcd.clear();
    lcd.print("Offline mode");
    wifiConnected = false;
  }

  

  lockServo.attach(servoPin);
  lockDoor();

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Enter PIN:");
}

void loop() {
  checkWiFi();
  client.loop();

  if (lockedOut) {
    if (millis() - lockoutTime >= lockoutDuration) {
      lockedOut = false;
      failedAttempts = 0;
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Enter PIN:");
      Serial.println("Lockout expired. Try again.");
    }
    return;
  }

  if (!lockedOut && wifiConnected) {

    char key = keypad.getKey();
    if (key) {
      PinInput += key;
      Serial.println(PinInput);
      lcd.setCursor(0, 1);
      lcd.print(PinInput);

      if (PinInput.length() == 4) {
        AccessCheck(PinInput);
        PinInput = "";
        delay(2000);
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Enter PIN:");
      }
    }
  }
}
