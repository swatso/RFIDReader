#ifndef SETTINGS_H
#define SETTINGS_H

#include <Arduino.h>
#include <LittleFS.h>
#include <ESP8266WebServer.h>

#define Version "RFIDReader v30.12.2023"

// Globals defined in main.cpp, referenced by Settings.cpp
extern int lNodeID;

void initSettings();
void loadSettings();
void saveSettings();
void startWebServer();
void runWebServer();
void setReporterBlockID(int block);
String toHex(int val);

// Web request handler prototypes
void handleRoot();
void handleSettings();
void handleRestart();
void handleNotFound();

#endif // SETTINGS_H
