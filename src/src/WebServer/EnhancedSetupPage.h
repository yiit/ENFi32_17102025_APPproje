#ifndef WEBSERVER_ENHANCEDSETUPPAGE_H
#define WEBSERVER_ENHANCEDSETUPPAGE_H

#include "../../ESPEasy_common.h"

#ifdef WEBSERVER_SETUP

// Main enhanced setup page handler
void handle_enhanced_setup();

// Stage rendering functions
void renderScanStage();
void renderScanningStage(uint8_t refreshCount);
void renderSelectStage();
void renderPasswordStage(const String& ssid);
void renderConnectingStage(const String& ssid, uint8_t refreshCount);
void renderSuccessStage();

#endif // WEBSERVER_SETUP

#endif // WEBSERVER_ENHANCEDSETUPPAGE_H
