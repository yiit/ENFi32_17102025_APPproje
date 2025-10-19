#ifndef WEBSERVER_SERIALMONITOR_H
#define WEBSERVER_SERIALMONITOR_H

#include "../../ESPEasy_common.h"

#ifdef WEBSERVER_SETUP

// Serial Monitor functions
void handle_serial_monitor();
void processSerialData();
void addSerialLog(const String& data);
void initializePrinterSerial();
void initPrinterSerialOnBoot();

// Yazıcı Serial global erişim fonksiyonları
class ESPeasySerial;
ESPeasySerial* getPrinterSerial();
bool isPrinterSerialActive();
void sendToPrinter(const String& data);
void sendToPrinterLn(const String& data);
void testPrinterSerial();

// Global serial variables
extern String serialBuffer;
extern bool serialMonitorActive;

// Basit log sistemi
#define MAX_SERIAL_LOGS 50
extern String serialLogs[MAX_SERIAL_LOGS];
extern uint8_t serialLogIndex;
extern uint16_t totalSerialLogs;

#endif // WEBSERVER_SETUP

#endif // WEBSERVER_SERIALMONITOR_H