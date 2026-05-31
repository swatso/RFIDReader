//  RFID Tage Reader for MQTT
//  Intended for use with WEMOS D1 ProMini or similar ESP8266 module
//  Features
//    Interfaces with RFID Reader via a software serial port
//    When it recieves an RFID tag reference, it publishes it via MQTT
//
//  Heavily based on the WIFI2C source code, it follows the same configuration
//  logic;  creating a web page, initially http://node0.local it should be
//  configured to occupy a spare node address on the layout

//#define WemosTest                         // Build for Wemos D1 mini (retired) with Serial debug

#define Version "RFIDReader v30.12.2023"
  // Initial versionbasedon the WIFI2C source code
  
#include <Wire.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ESP8266mDNS.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>
#include <WiFiClient.h>
#include <ESP8266WiFiMulti.h> 
#include <Ticker.h>

// Polling ratesin multiples of 10mS
#define I2C_READ_RATE     2
#define I2C_WRITE_RATE    10
#define IR_POLLING_RATE   50

int I2CReadPoll;
int I2CWritePoll;

byte LEDStatus;

// Module controland status registers
byte nodeC0;
byte nodeC1;
byte nodeS0;            // Published copy of S0
byte nodeS1;            // Published copy of S1
byte S0;                // I2C recieved value of S0
byte S1;                // I2C reveived value of S1

// Settings from EEPROM
int lNodeID = 0;
int settingsChecksum;

// Wifi Connection Stats
unsigned long wifiConnectionTime;
unsigned long MQTTConnectionTime;

int nodeID = -1;
unsigned ID[] = {0x30, 0x30};
unsigned rBlockID[] = {0x30, 0x30};

unsigned long now;

void setup() 
{
  initSettings();
  initComms();
  initRFID();
  I2CReadPoll = I2C_READ_RATE;
  I2CWritePoll = I2C_WRITE_RATE;
  Serial.begin(115200);
  Serial.println(Version);
  now = millis();
}

void loop() 
{ 
  //Main Operational Loop, Polls the MQTT, I2C and IR runtimes at the defined rates
  manageRFID();
  manageComms();                          // run the serviceroutines for WIFI and MQTT
  yield();
}
