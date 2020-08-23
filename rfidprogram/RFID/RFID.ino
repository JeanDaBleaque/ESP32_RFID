#include "BluetoothSerial.h"
#include <MFRC522.h>
#include <SPI.h>
#include "FS.h"
#include <SD.h>
#include <ArduinoJson.h>
#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error Bluetooth is not enabled! Please run `make menuconfig` to and enable it
#endif

int RST_PIN = 22;
int SS_PIN = 21;
MFRC522 rfid(SS_PIN, RST_PIN);
byte ID[4] = {91, 94, 51, 9};
boolean inCommand = false;
boolean isAdmin = "false";
int userCount = 20;
const int capacityWrite = JSON_ARRAY_SIZE(3) + JSON_OBJECT_SIZE(2) + userCount*JSON_OBJECT_SIZE(7);
int cardIndex = 0;
BluetoothSerial SerialBT;
int LED_BUILTIN = 2;
String cmd = "";
void setup() {
  Serial.begin(115200);
  SerialBT.begin("ESP32test"); //Bluetooth device name
  Serial.println("The device started, now you can pair it with bluetooth!");
  SPI.begin();
  rfid.PCD_Init();
  pinMode(LED_BUILTIN, OUTPUT);
}

void splitToArgs(String cmd) {
  if (isAdmin) {
      String cmdArgs[5];
      int i = 0;
      int j = 0;
      int p = 0;
      while (i<=cmd.length()) {
      if (cmd.charAt(i) == ' ') {
        cmdArgs[p] = cmd.substring(j, i);
        j = i+1;
        p++;
      } else if (i == cmd.length()) {
        cmdArgs[p] = cmd.substring(j, i);
      }
      i++;
  }
  process(cmdArgs);
  } else {
    if (checkPerm()) {
      splitToArgs(cmd); 
    } else {
      return;
    }
  }
}

boolean checkAvailable(byte *card) {
  const int capacityRead = JSON_ARRAY_SIZE(3) + JSON_OBJECT_SIZE(2) + userCount*JSON_OBJECT_SIZE(7) + 182;
  DynamicJsonDocument doc(capacityRead);
  File file = SD.open("/data.json", FILE_READ);
  deserializeJson(doc, file);
  file.close();
  for (int i=0;i<userCount;i++) {
    byte ID[4] = {0, 0, 0, 0};
    for (int i=0;i<4;i++) {
      ID[i] = doc["users"][i]["id_"+i];
    }
    if (card[0] == ID[0] && card[1] == ID[1] && card[2] == ID[2] && card[3] == ID[3]) {
      cardIndex = i;
      return true;
    }
  }
  return false;
}

void deleteCard(byte cardID[]) {
  if (!checkAvailable(cardID)) {
    Serial.println("Card is not available.");
    return; 
  }
  const int capacityRead = JSON_ARRAY_SIZE(3) + JSON_OBJECT_SIZE(2) + userCount*JSON_OBJECT_SIZE(7) + 182;
  DynamicJsonDocument doc(capacityRead);
  File file = SD.open("/data.json", FILE_READ);
  deserializeJson(doc, file);
  file.close();
  JsonObject user = doc["users"];
  user.remove(cardIndex);
  userCount--;
  file = SD.open("/data.json", FILE_WRITE);
  serializeJson(doc, file);
  file.close();
}

void addCard(String uname, String surName, String permission, byte *cardID) {
  if (checkAvailable(cardID)) {
    Serial.println("Card available.");
    return; 
  }
  userCount++;
  const int capacityRead = JSON_ARRAY_SIZE(3) + JSON_OBJECT_SIZE(2) + userCount*JSON_OBJECT_SIZE(7) + 182;
  DynamicJsonDocument doc(capacityRead);
  File file = SD.open("/data.json", FILE_READ);
  deserializeJson(doc, file);
  file.close();
  doc["users"][userCount-1]["id_0"] = cardID[0];
  doc["users"][userCount-1]["id_1"] = cardID[1];
  doc["users"][userCount-1]["id_2"] = cardID[2];
  doc["users"][userCount-1]["id_3"] = cardID[3];
  doc["users"][userCount-1]["name"] = uname;
  doc["users"][userCount-1]["surname"] = surName;
  doc["users"][userCount-1]["perm"] = permission;
  file = SD.open("/data.json", FILE_WRITE);
  serializeJson(doc, file);
  file.close();
}

byte* readCard() {
  byte error = 0;
  byte* newCard = new byte[4];
  Serial.println("Reading...");
  Serial.println("Present...");
  if (!rfid.PICC_IsNewCardPresent()) {
    delay(500);
    Serial.println("Present in...");
    newCard = readCard();
    return 0;
  }
  Serial.println("Printing...");
  for (int i=0;i<4;i++) {
    newCard[i] = rfid.uid.uidByte[i];
  }
    String id = "";
    for (int i=0;i<4;i++) {
      id += " " + (String) newCard[i];
    }
    Serial.println("ID:" + id);
    return newCard;
}

void process(String *cmdArgs) {
  String invoke = cmdArgs[0];
  Serial.println(invoke);
  if (invoke == "led") {
    if (cmdArgs[1] == "true") {
      digitalWrite(LED_BUILTIN, HIGH);
    } else if (cmdArgs[1] == "false") {
      digitalWrite(LED_BUILTIN, LOW);
    }
  }

  if (invoke == "addCard") {
    inCommand = true;
    Serial.println("Waiting...");
    byte* newCard = readCard();
    addCard(cmdArgs[1], cmdArgs[2], cmdArgs[3], newCard);
    inCommand = false;
    Serial.println("Finished!");
  }

  if (invoke == "deleteCard") {
    inCommand = true;
    byte* cardToDelete = readCard();
    deleteCard(cardToDelete);
    inCommand = false;
  }

  if (invoke == "updateCard") {
    inCommand = true;
    byte* cardToUpdate = readCard();
    inCommand = false;
    updateCard(cmdArgs[1], cmdArgs[2], cmdArgs[3], cardToUpdate);
  }
  
}

void updateCard(String uname, String surName, String permission, byte *cardID) {
  if (!checkAvailable(cardID)) {
    Serial.println("Card is not available.");
    return; 
  }
  const int capacityRead = JSON_ARRAY_SIZE(3) + JSON_OBJECT_SIZE(2) + userCount*JSON_OBJECT_SIZE(7) + 182;
  DynamicJsonDocument doc(capacityRead);
  File file = SD.open("/data.json", FILE_READ);
  deserializeJson(doc, file);
  file.close();
  doc["users"][cardIndex]["name"] = uname;
  doc["users"][cardIndex]["surname"] = surName;
  doc["users"][cardIndex]["perm"] = permission;
  file = SD.open("/data.json", FILE_WRITE);
  serializeJson(doc, file);
  file.close();
}

void checkCard() {
  if (!rfid.PICC_IsNewCardPresent()) {
    return;
  }
  if (!rfid.PICC_ReadCardSerial()) {
    return;
  }
  Serial.println("Reading...");
  byte *newCard = new byte[4];
  for (int i=0;i<4;i++) {
    rfid.uid.uidByte[i] == newCard[i];
  }
  if (checkAvailable(newCard)) {
    SerialBT.println("True");
  } else {
    SerialBT.println("False");
    String id = "";
    for (int i=0;i<4;i++) {
      byte readByte = rfid.uid.uidByte[i];
      id += " " + (String) readByte;
    }
    SerialBT.println("ID:" + id);
  }
  rfid.PICC_HaltA();
  Serial.println("Finished!");
}

void loop() {
  if (inCommand == false) {
    checkCard();
  }
  if (SerialBT.available()) {
    cmd = SerialBT.readStringUntil('\n');
    if (cmd != "") {
      splitToArgs(cmd);
    }
  }
  cmd = "";
  delay(100);
}
