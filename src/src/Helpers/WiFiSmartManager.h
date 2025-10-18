#ifndef HELPERS_WIFISMARTMANAGER_H
#define HELPERS_WIFISMARTMANAGER_H

#include "../../ESPEasy_common.h"
#include "../DataTypes/ESPEasyTimeSource.h"
#include "../Helpers/LongTermTimer.h"

// UPTIME BAZLI WiFi YÖNETİM SİSTEMİ
// Akıllı WiFi mod değişim sistemi

class WiFiSmartManager {
public:
    // Başlatma
    static void init();
    
    // Ana kontrol döngüsü - her saniye çağrılır
    static void handle();
    
    // WiFi durumu kontrolleri
    static bool shouldSwitchToAP();
    static bool shouldRetryConnection();
    static bool isConnectionStable();
    
    // Uptime bazlı kontroller
    static bool isEarlyUptime();        // İlk 3 dakika
    static bool isCriticalUptime();     // 3-10 dakika arası
    static bool isStableUptime();       // 10+ dakika
    
    // Güvenlik kontrolleri
    static void checkConnectionHealth();
    static void forceFailsafe();
    
    // Durum raporlama
    static String getStatusReport();
    static void logWiFiState();
    
private:
    // Zamanlayıcılar
    static LongTermTimer lastHealthCheck;
    static LongTermTimer lastConnectionAttempt;
    static LongTermTimer stableConnectionStart;
    static LongTermTimer emergencyModeActivated;
    
    // Sayaçlar
    static uint8_t consecutiveFailures;
    static uint8_t connectionRetries;
    static uint32_t totalReconnectAttempts;
    
    // Durum bayrakları
    static bool emergencyModeActive;
    static bool forceAPMode;
    static bool connectionHealthy;
    
    // İç fonksiyonlar
    static void performHealthCheck();
    static void handleEarlyUptime();
    static void handleCriticalUptime();
    static void handleStableUptime();
    static void activateEmergencyMode();
    static void deactivateEmergencyMode();
    static void resetCounters();
};

// UPTIME BAZLI KONFIGÜRASYON SABITLERI
#define WIFI_SMART_EARLY_UPTIME_MINUTES     3      // İlk 3 dakika agresif bağlantı
#define WIFI_SMART_CRITICAL_UPTIME_MINUTES  10     // 3-10 dakika arası kritik dönem
#define WIFI_SMART_STABLE_UPTIME_MINUTES    10     // 10+ dakika stabil dönem

#define WIFI_SMART_HEALTH_CHECK_INTERVAL    10000  // Her 10 saniye health check
#define WIFI_SMART_EARLY_RETRY_INTERVAL     5000   // İlk dönem: 5 saniye retry
#define WIFI_SMART_CRITICAL_RETRY_INTERVAL  15000  // Kritik dönem: 15 saniye retry
#define WIFI_SMART_STABLE_RETRY_INTERVAL    30000  // Stabil dönem: 30 saniye retry

#define WIFI_SMART_MAX_CONSECUTIVE_FAILURES 3      // Maksimum ardışık başarısızlık
#define WIFI_SMART_EMERGENCY_MODE_DURATION  300000 // 5 dakika emergency mode
#define WIFI_SMART_STABLE_CONNECTION_TIME   30000  // 30 saniye stabil bağlantı

// HATA DURUMU YÖNETİMİ
#define WIFI_SMART_CONNECTION_TIMEOUT       20000  // 20 saniye bağlantı timeout
#define WIFI_SMART_RSSI_THRESHOLD          -80     // Minimum sinyal kalitesi
#define WIFI_SMART_PING_CHECK_INTERVAL     60000   // 1 dakikada bir ping check

#endif // HELPERS_WIFISMARTMANAGER_H