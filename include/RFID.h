#ifndef RFID_H
#define RFID_H

#include <Arduino.h>
#include <SoftwareSerial.h>

extern byte RFIDData[8];

void initRFID();
void manageRFID();
boolean checkForRFID();

#endif // RFID_H
