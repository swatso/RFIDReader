// This module handles MQTT data transfers via wifi
// A priary and secondary network are defined.
// If the device fails to connect to the primary network after 30 seconds, it tries the secondary network and so forth

#include "Communications.h"
#include "Settings.h"

#include <string>

// Forward declarations for internal functions
boolean connectToWifi();
boolean connectClient();
boolean subscribeTopics();
boolean MQTTPublishNext();
boolean serviceConnection();

//const char* mqtt_server = "mqttserver";
const char* mqtt_server = "192.168.0.184";
const char* ssid0 = "WLR1";
const char* pswd0 = "RailwayModel";

const char* ssid1 = "WLR2";
const char* pswd1 = "RailwayModel";

const char* ssid2 = "VM7320635";
const char* pswd2 = "kfcr7fsXxcs8";

boolean useMqtt1 = true;
const char* mqtt1 = "192.168.0.1";          // Shed network
const char* mqtt2 = "192.168.0.184";        // Garage network

// background publishing of topics is just a safety net in case MQTT events get missed
// it is intended to eventually pull everything back into sync over a minute or so....
#define MAX_TOPIC_TO_PUBLISH 15
#define BACKGROUNDPUBLISHRATE 4000

char* hostName = "node00";
char* sensorTopic = "iot/io/sensor/nn00";
char* turnoutTopic = "iot/io/turnout/nn00";
char* reporterTopic = "iot/io/reporter/nn00";

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
  client.setBufferSize(512);
  client.setServer(mqtt1, 1883);
  client.setCallback(callback);
  nextTopicToPublish = 0;
  wifiMulti.addAP(ssid0, pswd0);       // add Wi-Fi networks you want to connect to
  wifiMulti.addAP(ssid1, pswd1);       // add Wi-Fi networks you want to connect to
  #ifdef WemosTest
//  wifiMulti.addAP(ssid2, pswd2);       // add Wi-Fi networks you want to connect to
  #endif
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  nodeID = 0;
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
        if(subscribeTopics()==true)commsState = COMMS_LIVE;
        MDNS.update();
        runWebServer();
      }
    break;
    
    case COMMS_LIVE:
      if(wifiMulti.run() == WL_CONNECTED)
      {
        if(serviceConnection() != true)commsState = COMMS_WIFI_CONNECTED;
        if(MQTTPublishNext() != true)commsState = COMMS_WIFI_CONNECTED;
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
    // Build the basic subscription strings by inserting the node ID
    turnoutTopic[15]=ID[0];
    turnoutTopic[16]=ID[1];  
    sensorTopic[14]=ID[0];
    sensorTopic[15]=ID[1];
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
      if(useMqtt1==true)Serial.println(mqtt1);
      else Serial.println(mqtt2);
#endif
      MQTTConnectionTime = millis();
      return(true);     // Connected
    }
    else
    {
      // Failed to connect to MQTT server.
      // There are different IP addresses for the two potential networks (Garage and Shed)
      // Ideally would use MDNS to resolve using a common hostname, bit there is a known issue
      // in pubsub client not resolving MDNS hostname so the compromise is to declare two fixed IP
      // addresses and find the one that works.....
      if(useMqtt1 == true)
      {
        client.setServer(mqtt2, 1883);
        useMqtt1= false;
      }
      else
      {
        client.setServer(mqtt1, 1883);
        useMqtt1= true;
      }      
      return(false);    // Not yet connected
    }
  } 
  return(true);         // already connected
}




boolean  subscribeTopics()
{
  //Serial.println("<subscribe>");
  // ******************************************
  return(true);       // No topics to subscribe for the RFID Node
  //*******************************************

  
  // subscribe to the 16 (turnOut) topics
  std::string subscription;
  byte i;
  for(i=0; i<16; i++)
  {
    // We are subscribing to turnout topics 00 to 0F hex
    turnoutTopic[17]= 0x30;
    turnoutTopic[18]= i + 0x30;
    if(turnoutTopic[18] > 57)turnoutTopic[18]+=7;

    subscription.assign(turnoutTopic,19);
    client.subscribe(subscription.c_str());
    delay(1);
    serviceConnection();
    yield();
  }
  return(true);
}



boolean MQTTPublishNext()
{
  //**************************************************
  return(true);        // no background topics to publish for RFID node
  //**************************************************
}

boolean publishBit(byte bitNo)
{
   if (client.connected())
   {
    // publishes the specified bit (0 to 15) on the appropriate Sensor Topic
    // build the sensor topic number in the range 00 to 0F hex
    sensorTopic[16]=0x30;
    sensorTopic[17]= bitNo + 0x30;
    if(sensorTopic[17] > 57)sensorTopic[17]+=7;

    if(bitNo < 8)
    {
     // bit is in nodeS0
     if(bitRead(nodeS0,bitNo)==true)client.publish( sensorTopic , (char*) payloadTrue.c_str(), true );
     else client.publish( sensorTopic , (char*) payloadFalse.c_str(), true );
    }
    else
    {
     // bit is in nodeS1
     if(bitRead(nodeS1,bitNo-8)==true)client.publish( sensorTopic , (char*) payloadTrue.c_str(), true );
     else client.publish( sensorTopic , (char*) payloadFalse.c_str(), true );
    }
   }
yield();
   return(true);
}


boolean publishReporter(byte *message)
{
   if (client.connected())
   {
     // publishes the specified message on reporter Topic 00
     reporterTopic[18]=0x30;
     reporterTopic[19]= 0x30;
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



void callback(char* topic, byte* payload, unsigned int length) 
{
  // Called when one of the subscribed topics is recieved
  // ie a Turnout has changed in value...
  // Toggle bits in C0 and C1
  // Event will be defined by topic[17] and topic[18] in the range 00 to 0F hex
  byte event = (topic[18]-0x30);
  if (event > 9)event -= 7;

  if(event < 8)
  {
    if ((char)payload[0] == 'T')bitWrite(nodeC0,event,true);
    else bitWrite(nodeC0,event,false); 
  }
  else
  {
    event = event - 8;
    if ((char)payload[0] == 'T')bitWrite(nodeC1,event,true);
    else bitWrite(nodeC1,event,false);     
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
    turnoutTopic[15]=ID[0];
    turnoutTopic[16]=ID[1];  
    sensorTopic[14]=ID[0];
    sensorTopic[15]=ID[1];
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
  if(checkMQTTState()==true)
  {
    if(useMqtt1 == true)return(mqtt1);
    return(mqtt2);
  }
  else return("Not Connected");
}
