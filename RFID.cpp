// RFID reciever is connected to GPIO pins on the Wemos device
// It transmits a string of bytes each time that it successfully reads an RFID device
// The output is 9600 baud
//
// It consists
//    <STX>
//    14 Bytes of Data
//    <CR>
//    <LF>
//    <ETX>

//    Of the 14 bytes of data, I regard the last 8 of being useful for unique identification
//    So it is these 8 byteswhich are captured for onward transmission via MQTT

#include "RFID.h"
#include "Communications.h"

SoftwareSerial swSer(14, 12, false, 256);
byte  RFIDData[8];
byte  RFIDState;
byte  RFIDByteCnt;
byte  RFIDPtr;

#define RFID_START      0
#define RFID_PREAMBLE   1
#define RFID_DATA       2
#define RFID_END        3
#define STX 2
#define ETX 3
      
void initRFID() 
{
  swSer.begin(9600);
  RFIDState = RFID_START;
}

void manageRFID() 
{
  byte i;
  if(checkForRFID() == true)
  {
    //we have a new RFID packet
    Serial.print("Packet Data:");
    for(i=0; i<8; i++)
    {
      Serial.print(RFIDData[i]);
      Serial.print(" ");
    }
    Serial.println();

    // Publish the RFID data via MQTT
    publishReporter(RFIDData);
  }
}


boolean checkForRFID()
{
  // call this function repeatedly and often to check for a new RFID datapacket
  // it processes any bytes recieved and returns True is a new data packet is available
  // The data packet is stored in the byte array RFIDBuffer
   while(swSer.available() > 0)
   {
      byte RFIDByte = swSer.read();
      switch (RFIDState)
      {
        case RFID_START:
          // waiting for start of packet
          if(RFIDByte == STX)RFIDState = RFID_PREAMBLE;
          RFIDByteCnt = 0;
          break;

      case RFID_PREAMBLE:
          // all the preamble bytes can be thrown away
          if(RFIDByte == STX)RFIDByteCnt = -1;          // something has gone wrong if we recieve an STX char, but we can deal with it
          RFIDByteCnt++;
          if(RFIDByteCnt > 5)
          {
            RFIDState = RFID_DATA;
            RFIDPtr = 0;
          }
          break;
          
      case  RFID_DATA:
          // this is the unique ID we want
          if(RFIDByte == STX)
          {
            // somethingis wrong, this is the start of a new packet
            if(RFIDByte == STX)RFIDState = RFID_PREAMBLE;
            RFIDByteCnt = 0;
            break;            
          }
          if(RFIDByte == ETX)
          {
            // somethingis wrong with the packet - start over
            RFIDState = RFID_START;
            break;
          }
          RFIDData[RFIDPtr++] = RFIDByte;
          if(RFIDPtr > 7)RFIDState = RFID_END;
          break;

      case RFID_END:
          if(RFIDByte == STX)
          {
            RFIDState = RFID_PREAMBLE;
            RFIDByteCnt = 0;
          }
          if(RFIDByte == ETX)
          {
            RFIDState = RFID_START;
            return(true);
          }
          break;
      }
   }
   return(false);
}
