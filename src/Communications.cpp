// This module handles MQTT data transfers via wifi
// A priary and secondary network are defined.
// If the device fails to connect to the primary network after 30 seconds, it tries the secondary network and so forth
#include "Arduino.h"
#include "Communications.h"
#include "Settings.h"

#include <string>
#include <LittleFS.h>
#define WemosTest 

// Forward declarations for internal functions
boolean connectToWifi();
boolean connectClient();
//boolean subscribeTopics();
boolean MQTTPublishNext();
boolean serviceConnection();
boolean loadCommsSettings();
boolean readSettingFile(const char* path, char* target, size_t targetLen);

char ssid0[64] = "";
char pswd0[64] = "";

boolean useMqtt1 = true;
char mqtt1[64] = "";

// background publishing of topics is just a safety net in case MQTT events get missed
// it is intended to eventually pull everything back into sync over a minute or so....
#define MAX_TOPIC_TO_PUBLISH 15
#define BACKGROUNDPUBLISHRATE 4000

char* hostName = "node00";
char* reporterTopic = "track/reporter/nn00";

String payloadTrue = "ACTIVE";
String payloadFalse = "INACTIVE";

byte  nextTopicToPublish;
long  timeToPublish;

WiFiClient espClient;
PubSubClient client(espClient);
ESP8266WiFiMulti wifiMulti;

#define COMMS_WIFI_DISCONNECTED  4
#define COMMS_WIFI_CONNECTED     3
#define COMMS_CLIENT_CONNECTED   2
#define COMMS_LIVE               1

byte commsState;                                   // state machine starting state


void initComms() 
{
  commsState = COMMS_WIFI_DISCONNECTED;                // state machine starting state
  loadCommsSettings();
  client.setBufferSize(512);
  client.setServer(mqtt1, 1883);
  nextTopicToPublish = 0;
  if ((ssid0[0] != '\0') && (pswd0[0] != '\0'))
  {
    wifiMulti.addAP(ssid0, pswd0);       // add Wi-Fi networks you want to connect to
  }
  else
  {
    Serial.println("WiFi settings missing in /ssid.txt or /pass.txt");
  }
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  nodeID = 0;
}

boolean loadCommsSettings()
{
  if (!LittleFS.begin())
  {
    Serial.println("LittleFS mount failed");
    return(false);
  }

  boolean brokerLoaded = readSettingFile("/Broker.txt", mqtt1, sizeof(mqtt1));
  if (!brokerLoaded) brokerLoaded = readSettingFile("/broker.txt", mqtt1, sizeof(mqtt1));

  boolean ssidLoaded = readSettingFile("/ssid.txt", ssid0, sizeof(ssid0));
  if (!ssidLoaded) ssidLoaded = readSettingFile("/SSID.txt", ssid0, sizeof(ssid0));

  boolean passLoaded = readSettingFile("/pass.txt", pswd0, sizeof(pswd0));
  if (!passLoaded) passLoaded = readSettingFile("/Pass.txt", pswd0, sizeof(pswd0));

  if (!brokerLoaded)
  {
    Serial.println("Missing or empty /Broker.txt");
  }
  if (!ssidLoaded)
  {
    Serial.println("Missing or empty /ssid.txt");
  }
  if (!passLoaded)
  {
    Serial.println("Missing or empty /pass.txt");
  }

  Serial.print("MQTT Broker:");
  Serial.println(mqtt1);
  Serial.print("SSID:");
  Serial.println(ssid0);

  return(brokerLoaded && ssidLoaded && passLoaded);
}



boolean readSettingFile(const char* path, char* target, size_t targetLen)
{
  target[0] = '\0';
  File settingFile = LittleFS.open(path, "r");
  if (!settingFile)
  {
    Serial.print("Failed to open setting file:");
    Serial.println(path);
    return(false);
  }

  String value = settingFile.readStringUntil('\n');
  value.trim();
  settingFile.close();

  if (value.length() == 0)
  {
    return(false);
  }

  value.toCharArray(target, targetLen);
  return(target[0] != '\0');
}

void  manageComms()
{
  // mainroutine which operates the MQTT communications based on a statemachine
  // call this routine around every 100mS
  switch(commsState)
  {
    case COMMS_WIFI_DISCONNECTED:
#ifdef WemosTest
Serial.print(".");
#endif
      if(connectToWifi()==true)
      {
        commsState = COMMS_WIFI_CONNECTED;
        hostName[4] = ID[0];
        hostName[5]= ID[1];
        wifiConnectionTime = millis();
        #ifdef WemosTest
          Serial.print("connected:");
          Serial.println(hostName);
          Serial.print("IP:");
          Serial.println(getIPAddress());          
          Serial.print("SSID: ");
          Serial.println(getSSID());
          Serial.print("RSSI: ");
          Serial.println(getRSSI());
        #endif
        
        // start multicast domain nameserver (responder)
        if (MDNS.begin(hostName)) 
        {
          #ifdef WemosTest
          Serial.print("MDNS started:");
          Serial.println(hostName);
          #endif
        }
        else
        {
          #ifdef WemosTest
          Serial.print("MDNS failed:");
          Serial.println(hostName);
          #endif          
        }
        // start web server
        startWebServer();

        // Connect to the MQTT Server

        timeToPublish = millis() + BACKGROUNDPUBLISHRATE;
      }
      else delay(100);               // note yet connected to WIFI, don't overwhelm the network with requests
    break;

    case COMMS_WIFI_CONNECTED:
      if(wifiMulti.run() == WL_CONNECTED)
      {
        if(connectClient()==true)commsState = COMMS_CLIENT_CONNECTED;
        runWebServer();
      }
    break;

    case COMMS_CLIENT_CONNECTED:
      if(wifiMulti.run() == WL_CONNECTED)
      {
//        if(subscribeTopics()==true)commsState = COMMS_LIVE;
        commsState = COMMS_LIVE;
        MDNS.update();
        runWebServer();
      }
    break;
    
    case COMMS_LIVE:
      if(wifiMulti.run() == WL_CONNECTED)
      {
        if(serviceConnection() != true)commsState = COMMS_WIFI_CONNECTED;
        MDNS.update();
        runWebServer();
      }
    break;
  }
}


boolean connectToWifi() 
{
  // We start by connecting to a WiFi network
    
  if(wifiMulti.run() == WL_CONNECTED)
  {
    return(true);
  }
  else return(false);
}


boolean connectClient() 
{
#ifdef WemosTest
  Serial.println("connectMQTTClient");
#endif
  // Attempt to reconnect, return true if successful, otherwise false
  if (!client.connected()) 
  {
#ifdef WemosTest
    Serial.println("<Not connected>");
#endif
    // Build clientID based on the NodeID
    String clientId = String(ID[0]);
    clientId += String(ID[1]);
    clientId += "-";
    clientId += String(micros() & 0xff, 16); // to randomise. sort of
    
    // Attempt to connect
    if (client.connect(clientId.c_str())) 
    {
#ifdef WemosTest
      Serial.print("Connected to MQTT Broker at: ");
      Serial.println(mqtt1);
#endif
      MQTTConnectionTime = millis();
      return(true);     // Connected
    }
    else
    {
      // Failed to connect to MQTT server.    
      return(false);    // Not yet connected
    }
  } 
  return(true);         // already connected
}


boolean publishReporter(byte *message)
{
   if (client.connected())
   {
     // publishes the specified message on reporter Topic 00
     reporterTopic[19]=0x30;
     reporterTopic[20]=0x30;
     client.publish( reporterTopic , (char*)message, true );      
   }
   return(true);
}

boolean serviceConnection()
{
  // confirm still connected to mqtt server
  if (!client.connected()) 
  {
    // No, not connected
    Serial.print("Error:");
    Serial.println(client.state());
    return(false);
  }
  else
  {
    client.loop();
    return(true);
  }
}


void setWifiNodeID(byte currentNodeID)
{
  // Set the Wifi NodeID to the currentNodeID
  // also convert theNodeID into Hex (ASCII) for use in the MQTT Subscriptions
  if(nodeID != currentNodeID)
  {
    nodeID = currentNodeID;
    // convert and store nodeID as a two digit ACSIIHex 
    ID[0]= nodeID/16 + 0x30;
    if(ID[0] > 57)ID[0]+=7;
    ID[1]= nodeID % 16 + 0x30;
    if(ID[1] > 57)ID[1]+=7;

    // Build the basic subscription strings by inserting the node ID
    //turnoutTopic[15]=ID[0];
    //turnoutTopic[16]=ID[1];  
    //sensorTopic[14]=ID[0];
    //sensorTopic[15]=ID[1];
  }
}

byte  getCommsState()
{
  return(commsState);
}

boolean  checkCommsState()
{
  // returns true of communications is live
  return(commsState==COMMS_LIVE);
}

unsigned long getCommsUptime()
{
  if(wifiMulti.run() == WL_CONNECTED)return((millis()-wifiConnectionTime)/60000);
  else return(0);
}

boolean checkMQTTState()
{
  // returns the MQTT client connection state
  return(client.connected());
}

unsigned long getMQTTUptime()
{
  if(checkMQTTState() == true)return((millis()-MQTTConnectionTime)/60000);
  else return(0);
}

String getIPAddress()
{
  return(WiFi.localIP().toString());
}

String getSSID()
{
  return(WiFi.SSID());
}

int getRSSI()
{
  return(WiFi.RSSI());
}

String getMQTTAddress()
{
  return(mqtt1);
}
