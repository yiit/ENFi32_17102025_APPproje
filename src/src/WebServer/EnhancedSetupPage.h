#ifndef WEBSERVER_ENHANCEDSETUPPAGE_H
#define WEBSERVER_ENHANCEDSETUPPAGE_H

#include "../../ESPEasy_common.h"

#ifdef WEBSERVER_SETUP

// Advanced WiFi setup stages  
#define STAGE_SCAN          0
#define STAGE_SCANNING      1
#define STAGE_SELECT        2
#define STAGE_PASSWORD      3
#define STAGE_ADVANCED      4  // Yeni gelişmiş ayarlar aşaması
#define STAGE_CONFIRM       5  // Yeni onay aşaması
#define STAGE_CONNECTING    6
#define STAGE_SUCCESS       7
#define STAGE_FAILED        8
#define STAGE_FAILSAFE      9

// Advanced setup data structure
struct AdvancedWiFiSettings {
  String ssid;
  String password;
  bool useStaticIP = false;
  String staticIP;
  String subnet = "255.255.255.0";
  String gateway;
  String dns1 = "8.8.8.8";
  String dns2 = "8.8.4.4";
  bool hiddenSSID = false;
  bool emptyPassword = false;
  String errorMessage;
};

// Main enhanced setup page handler
void handle_enhanced_setup();

// Stage rendering functions
void renderScanStage();
void renderScanningStage(uint8_t refreshCount);
void renderSelectStage();
void renderPasswordStage(const String& ssid);
void renderAdvancedStage(const String& ssid);        // Yeni gelişmiş ayarlar
void renderConfirmStage(const AdvancedWiFiSettings& settings);  // Yeni onay aşaması
void renderConnectingStage(const String& ssid, uint8_t refreshCount);
void renderSuccessStage();
void renderFailedStage(const String& ssid, const String& error);
void renderFailsafeStage();

// Helper functions for validation
bool validateIPAddress(const String& ip);
bool isValidSubnet(const String& subnet);
String getDefaultGateway(const String& ip, const String& subnet);
String getDefaultDNS();

#endif // WEBSERVER_SETUP

#endif // WEBSERVER_ENHANCEDSETUPPAGE_H
