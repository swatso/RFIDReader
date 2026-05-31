#ifndef COMMUNICATIONS_H
#define COMMUNICATIONS_H

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ESP8266mDNS.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>
#include <WiFiClient.h>
#include <ESP8266WiFiMulti.h>

// Globals defined in main.cpp, referenced by Communications.cpp
extern int nodeID;
extern unsigned ID[];
extern unsigned rBlockID[];
extern byte nodeC0;
extern byte nodeC1;
extern byte nodeS0;
extern byte nodeS1;
extern unsigned long wifiConnectionTime;
extern unsigned long MQTTConnectionTime;

void initComms();
void manageComms();
void callback(char* topic, byte* payload, unsigned int length);
boolean publishReporter(byte *message);
boolean publishBit(byte bitNo);
void setWifiNodeID(byte currentNodeID);
byte getCommsState();
boolean checkCommsState();
unsigned long getCommsUptime();
boolean checkMQTTState();
unsigned long getMQTTUptime();
String getIPAddress();
String getSSID();
int getRSSI();
String getMQTTAddress();

#endif // COMMUNICATIONS_H
