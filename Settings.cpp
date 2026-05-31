
#include "Settings.h"
#include "Communications.h"
#include "RFID.h"

//Constants
#define EEPROM_SIZE 12

ESP8266WebServer server(80);    // Create a webserver object that listens for HTTP request on port 80

void  initSettings()
{
  EEPROM.begin(EEPROM_SIZE);
  loadSettings();
}

void  loadSettings()
{
  // Loads the settings from EEPROM/Flash
  EEPROM.get(0, lNodeID);
  EEPROM.get(4,settingsChecksum);

#ifdef WemosTest
  Serial.println("EEPROM");
  Serial.print("TransponderBlockID:");
  Serial.println(lNodeID,HEX);
  Serial.print("checksum:");
  Serial.println(settingsChecksum,HEX);  
#endif

  if(settingsChecksum != (lNodeID+1))
  {
    // Bad checksum - restore defaults
    lNodeID = 0;
    saveSettings();
    #ifdef WemosTest
       Serial.println("Bad Checksum - restoring defaults");
    #endif
  }
  setWifiNodeID(lNodeID);
}

void  saveSettings()
{
  //Saves the settings to EEPROM/Flash
  EEPROM.put(0, lNodeID);
  // calculate the checksum
  settingsChecksum = lNodeID+1;
  EEPROM.put(4, settingsChecksum);
  EEPROM.commit();
}

void startWebServer(void)
{
  server.on("/", HTTP_GET, handleRoot);        // Call the 'handleRoot' function when a client requests URI "/"
  server.on("/settings", HTTP_POST, handleSettings); // Call the 'settings' function when a POST request is made to URI "/settings"
  server.on("/restart", HTTP_GET, handleRestart);
  server.onNotFound(handleNotFound);           // When a client requests an unknown URI (i.e. something other than "/"), call function "handleNotFound"

  server.begin();                            // Actually start the server
  Serial.println("HTTP server started");
}



String toHex(int val)
{
  // Convert given val into 2 digit Hex (ASCII) string
    String valStr = "";
    char ch;
    ch = val/16 + 0x30;
    if(ch > 57)ch+=7;
    valStr+=ch;
    ch = val % 16 + 0x30;
    if(ch > 57)ch+=7;
    valStr+=ch;
    return(valStr);
}


void runWebServer()
{
  server.handleClient();                     // Listen for HTTP requests from clients
}


void handleRoot() 
{                          
  // When URI / is requested, send a web page
  String postForm = "<html><head><title>";
  postForm+="Node";
  postForm+=toHex(lNodeID);
  postForm+="</title></head>";
  postForm+="<body><h1>Settings for ";
  postForm+=Version;
  postForm+="</h1><br>";
  postForm+="<form action=\"/settings\" method=\"POST\">";
  postForm+="<label for=\"idNode\">NodeID (00h to FFh):</label><br>";
  postForm+="<input type=\"text\" id=\"idNode\" name=\"NodeID\" placeholder=\"";
  postForm+=toHex(lNodeID);
  postForm+="\"><br>";
  postForm+="<input type=\"submit\" value=\"submit\"></form></body><html>";
  postForm+="<h2>RFID</h2>";
  postForm+="<br>";  
  postForm+="Last Packet: ";
  postForm+=toHex(RFIDData[0]);  
  postForm+=" ";  
  postForm+=toHex(RFIDData[1]);  
  postForm+=" ";   
  postForm+=toHex(RFIDData[2]);  
  postForm+=" "; 
  postForm+=toHex(RFIDData[3]);  
  postForm+=" "; 
  postForm+=toHex(RFIDData[4]);  
  postForm+=" "; 
  postForm+=toHex(RFIDData[5]);  
  postForm+=" "; 
  postForm+=toHex(RFIDData[6]);  
  postForm+=" "; 
  postForm+=toHex(RFIDData[7]);  
  postForm+="<br>";  
  postForm+="<h2>WIFI Properties:</h2>";
  postForm+="<br>";
  postForm+="SSID: ";
  postForm+=getSSID();
  postForm+="<br>";
  postForm+="RSSI: ";
  postForm+=getRSSI();
  postForm+="<br>";
  postForm+="IP: ";
  postForm+=getIPAddress();
  postForm+="<br>";
  postForm+="MQTT Broker: ";
  postForm+=getMQTTAddress();
  postForm+="<br>";
  postForm+="<h2>UpTime (Minutes)</h2>";
  postForm+="<br>";
  postForm+="WIFI: ";
  postForm+=getCommsUptime();
  postForm+="<br>";
  postForm+="MQTT: ";
  postForm+=getMQTTUptime();
  postForm+="<br>"; 
  postForm+="<br>";
  postForm+="<meta http-equiv='refresh' content='30'/>";
  server.send(200, "text/html", postForm);
}

void handleSettings() 
{                         // If a POST request is made to URI /login
  String input;
  
  if( ! server.hasArg("NodeID"))
  { 
    // If the POST request doesn't have required arguments
    server.send(400, "text/plain", "400: Invalid Request");         // The request is invalid, so send HTTP status 400
    return;
  }

   if(server.arg("NodeID") != NULL)
  {
    input = server.arg("NodeID");
    lNodeID = (int)strtol(&input[0],NULL,16);
    setWifiNodeID(lNodeID);
  }
  saveSettings();
  server.send(200, "text/html", "<h1>Values Saved</h1>");
}

void handleRestart()
{
  // restart the ESP
  ESP.restart();
}


void handleNotFound(){
  server.send(404, "text/plain", "404: Not found"); // Send HTTP status 404 (Not Found) when there's no handler for the URI in the request
}



void setReporterBlockID(int block)
{
    // convert and store nodeID as a two digit ACSIIHex 
    rBlockID[0]= block/16 + 0x30;
    if(rBlockID[0] > 57)rBlockID[0]+=7;
    rBlockID[1]= block % 16 + 0x30;
    if(rBlockID[1] > 57)rBlockID[1]+=7;

    reporterTopic[16] = rBlockID[0];
    reporterTopic[17] = rBlockID[1];
}
