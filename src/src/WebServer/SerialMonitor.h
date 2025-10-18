#ifndef WEBSERVER_SERIALMONITOR_H
#define WEBSERVER_SERIALMONITOR_H

#include "../../ESPEasy_common.h"

#ifdef WEBSERVER_SETUP

// Serial Monitor functions
void handle_serial_monitor();

// Render functions - YENİ EKLENEN
void renderSerialSettings();
void renderSerialDisplay();
void renderPatternPanel();
void renderFrequencyPanel();
void renderSendPanel();
void renderStatsPanel();

// Processing functions - YENİ EKLENEN
void processSerialData();
void createSerialPacket(const String& packetData);
void analyzePatterns();

// Serial data structure
struct SerialDataPacket {
  String timestamp;
  String rawData;
  String hexData;
  String asciiData;
  String specialChars;
  bool hasSTX = false;
  bool hasETX = false;
  bool hasCR = false;
  bool hasLF = false;
  uint16_t dataLength = 0;
};

// Global serial buffer variables
extern String serialBuffer;
extern bool serialMonitorActive;
extern uint32_t serialBaudRate;
extern uint8_t serialDataBits;
extern uint8_t serialStopBits;
extern uint8_t serialParity;

// Pattern detection variables - YENİ EKLENEN
extern String detectedStartPattern;
extern String detectedEndPattern;
extern bool autoDetectMode;
extern uint8_t patternConfidence;
extern String lastCompleteMessage;

// Character frequency analysis - YENİ EKLENEN
extern uint16_t charFrequency[256];
extern uint8_t suspectedStartChars[10];
extern uint8_t suspectedEndChars[10];

// Serial packet buffer - YENİ EKLENEN
#define SERIAL_BUFFER_SIZE 1000
extern SerialDataPacket serialPackets[SERIAL_BUFFER_SIZE];
extern uint16_t serialPacketIndex;
extern uint16_t totalPackets;

#endif // WEBSERVER_SETUP

#endif // WEBSERVER_SERIALMONITOR_H