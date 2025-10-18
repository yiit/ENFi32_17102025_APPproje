#ifndef WEBSERVER_WIFISMARTMANAGER_H
#define WEBSERVER_WIFISMARTMANAGER_H

#include "../../ESPEasy_common.h"

#ifdef WEBSERVER_WIFI_MANAGER

// WiFi Smart Manager web sayfası
void handle_wifi_smart_manager();

// AJAX durumu için JSON response
void handle_wifi_smart_status_json();

#endif // WEBSERVER_WIFI_MANAGER

#endif // WEBSERVER_WIFISMARTMANAGER_H