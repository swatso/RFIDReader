//  RFID Tage Reader for MQTT
//  Intended for use with WEMOS D1 ProMini or similar ESP8266 module
//  Features
//    Interfaces with RFID Reader via a software serial port
//    When it recieves an RFID tag reference, it publishes it via MQTT
//
//  Heavily based on the WIFI2C source code, it follows the same configuration
//  logic;  creating a web page, initially http://node0.local it should be
//  configured to occupy a spare node address on the layout

#define WemosTest                         // Build for Wemos D1 mini (retired) with Serial debug

#include <Wire.h>
#include <Ticker.h>
#include "Settings.h"
#include "Communications.h"
#include "RFID.h"

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

// Settings
int lNodeID = 0;

// Wifi Connection Stats
unsigned long wifiConnectionTime;
unsigned long MQTTConnectionTime;

int nodeID = -1;
unsigned ID[] = {0x30, 0x30};
unsigned rBlockID[] = {0x30, 0x30};

unsigned long now;

void setup() 
{
  Serial.begin(115200);
  delay(100);
  initSettings();
  initComms();
  initRFID();
  I2CReadPoll = I2C_READ_RATE;
  I2CWritePoll = I2C_WRITE_RATE;

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
