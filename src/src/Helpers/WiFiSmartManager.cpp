#include "../Helpers/WiFiSmartManager.h"

#include "../../ESPEasy-Globals.h"
#include "../ESPEasyCore/ESPEasyWifi.h"
#include "../ESPEasyCore/ESPEasyNetwork.h"
#include "../Globals/ESPEasyWiFiEvent.h"
#include "../Globals/NetworkState.h"
#include "../Globals/RTC.h"
#include "../Helpers/Misc.h"
#include "../Helpers/Network.h"

// Static değişkenler
LongTermTimer WiFiSmartManager::lastHealthCheck;
LongTermTimer WiFiSmartManager::lastConnectionAttempt;
LongTermTimer WiFiSmartManager::stableConnectionStart;
LongTermTimer WiFiSmartManager::emergencyModeActivated;

uint8_t WiFiSmartManager::consecutiveFailures = 0;
uint8_t WiFiSmartManager::connectionRetries = 0;
uint32_t WiFiSmartManager::totalReconnectAttempts = 0;

bool WiFiSmartManager::emergencyModeActive = false;
bool WiFiSmartManager::forceAPMode = false;
bool WiFiSmartManager::connectionHealthy = false;

void WiFiSmartManager::init() {
    addLog(LOG_LEVEL_INFO, F("WiFi Smart Manager: Başlatılıyor..."));
    
    // Zamanlayıcıları resetle
    lastHealthCheck.clear();
    lastConnectionAttempt.clear();
    stableConnectionStart.clear();
    emergencyModeActivated.clear();
    
    // Sayaçları sıfırla
    resetCounters();
    
    // Durumu sıfırla
    emergencyModeActive = false;
    forceAPMode = false;
    connectionHealthy = false;
    
    addLog(LOG_LEVEL_INFO, F("WiFi Smart Manager: Başlatıldı - Akıllı mod aktif"));
}

void WiFiSmartManager::handle() {
    // Health check interval kontrolü
    if (!lastHealthCheck.isSet() || lastHealthCheck.timeoutReached(WIFI_SMART_HEALTH_CHECK_INTERVAL)) {
        performHealthCheck();
        lastHealthCheck.setNow();
    }
    
    // Uptime bazlı kontroller
    if (isEarlyUptime()) {
        handleEarlyUptime();
    } else if (isCriticalUptime()) {
        handleCriticalUptime();
    } else {
        handleStableUptime();
    }
    
    // Emergency mode kontrolü
    if (emergencyModeActive && emergencyModeActivated.timeoutReached(WIFI_SMART_EMERGENCY_MODE_DURATION)) {
        deactivateEmergencyMode();
    }
}

bool WiFiSmartManager::isEarlyUptime() {
    return getUptimeMinutes() < WIFI_SMART_EARLY_UPTIME_MINUTES;
}

bool WiFiSmartManager::isCriticalUptime() {
    uint32_t uptimeMin = getUptimeMinutes();
    return (uptimeMin >= WIFI_SMART_EARLY_UPTIME_MINUTES && 
            uptimeMin < WIFI_SMART_CRITICAL_UPTIME_MINUTES);
}

bool WiFiSmartManager::isStableUptime() {
    return getUptimeMinutes() >= WIFI_SMART_STABLE_UPTIME_MINUTES;
}

void WiFiSmartManager::handleEarlyUptime() {
    // İLK 3 DAKİKA: AGRESIF BAĞLANTI MODİ
    
    if (!WiFiConnected() && !emergencyModeActive) {
        // Hızlı retry - her 5 saniye
        if (!lastConnectionAttempt.isSet() || 
            lastConnectionAttempt.timeoutReached(WIFI_SMART_EARLY_RETRY_INTERVAL)) {
            
            connectionRetries++;
            totalReconnectAttempts++;
            
            addLog(LOG_LEVEL_INFO, String(F("WiFi Smart: Erken dönem bağlantı denemesi ")) + 
                   String(connectionRetries) + String(F("/")) + String(WIFI_SMART_MAX_CONSECUTIVE_FAILURES));
            
            // WiFi'ı yeniden başlat
            AttemptWiFiConnect();
            lastConnectionAttempt.setNow();
            
            // Maksimum deneme sayısına ulaştıysak AP mode aç
            if (connectionRetries >= WIFI_SMART_MAX_CONSECUTIVE_FAILURES) {
                addLog(LOG_LEVEL_ERROR, F("WiFi Smart: Erken dönem - maksimum deneme aşıldı, AP moda geçiş"));
                forceFailsafe();
            }
        }
    } else if (WiFiConnected()) {
        // Bağlantı başarılı, sayaçları sıfırla
        if (connectionRetries > 0) {
            addLog(LOG_LEVEL_INFO, F("WiFi Smart: Erken dönem bağlantı başarılı!"));
            resetCounters();
        }
    }
}

void WiFiSmartManager::handleCriticalUptime() {
    // 3-10 DAKİKA: KRİTİK DÖNEm
    
    if (!WiFiConnected() && !emergencyModeActive) {
        // Orta hızlıkta retry - her 15 saniye
        if (!lastConnectionAttempt.isSet() || 
            lastConnectionAttempt.timeoutReached(WIFI_SMART_CRITICAL_RETRY_INTERVAL)) {
            
            connectionRetries++;
            totalReconnectAttempts++;
            
            addLog(LOG_LEVEL_INFO, String(F("WiFi Smart: Kritik dönem bağlantı denemesi ")) + 
                   String(connectionRetries) + String(F("/")) + String(WIFI_SMART_MAX_CONSECUTIVE_FAILURES));
            
            AttemptWiFiConnect();
            lastConnectionAttempt.setNow();
            
            // Kritik dönemde daha sabırlıyız ama yine de sınır var
            if (connectionRetries >= WIFI_SMART_MAX_CONSECUTIVE_FAILURES + 2) {  // 5 deneme
                addLog(LOG_LEVEL_ERROR, F("WiFi Smart: Kritik dönem - maksimum deneme aşıldı"));
                activateEmergencyMode();
            }
        }
    } else if (WiFiConnected()) {
        if (connectionRetries > 0) {
            addLog(LOG_LEVEL_INFO, F("WiFi Smart: Kritik dönem bağlantı başarılı!"));
            resetCounters();
        }
    }
}

void WiFiSmartManager::handleStableUptime() {
    // 10+ DAKİKA: STABİL DÖNEM
    
    if (!WiFiConnected() && !emergencyModeActive) {
        // Yavaş retry - her 30 saniye
        if (!lastConnectionAttempt.isSet() || 
            lastConnectionAttempt.timeoutReached(WIFI_SMART_STABLE_RETRY_INTERVAL)) {
            
            connectionRetries++;
            totalReconnectAttempts++;
            
            addLog(LOG_LEVEL_INFO, String(F("WiFi Smart: Stabil dönem bağlantı denemesi ")) + 
                   String(connectionRetries) + String(F("/10")));  // Stabil dönemde daha çok deneme
            
            AttemptWiFiConnect();
            lastConnectionAttempt.setNow();
            
            // Stabil dönemde çok sabırlıyız
            if (connectionRetries >= 10) {  // 10 deneme
                addLog(LOG_LEVEL_ERROR, F("WiFi Smart: Stabil dönem - uzun süreli bağlantı sorunu"));
                activateEmergencyMode();
            }
        }
    } else if (WiFiConnected()) {
        if (connectionRetries > 0) {
            addLog(LOG_LEVEL_INFO, F("WiFi Smart: Stabil dönem bağlantı başarılı!"));
            resetCounters();
        }
    }
}

void WiFiSmartManager::performHealthCheck() {
    bool wasHealthy = connectionHealthy;
    connectionHealthy = WiFiConnected() && NetworkConnected();
    
    if (WiFiConnected()) {
        // RSSI kontrolü
        int32_t rssi = WiFi.RSSI();
        if (rssi < WIFI_SMART_RSSI_THRESHOLD) {
            addLog(LOG_LEVEL_ERROR, String(F("WiFi Smart: Zayıf sinyal - RSSI: ")) + String(rssi));
            connectionHealthy = false;
        }
        
        // Bağlantı süresi kontrolü
        if (!stableConnectionStart.isSet()) {
            stableConnectionStart.setNow();
        } else if (stableConnectionStart.timeoutReached(WIFI_SMART_STABLE_CONNECTION_TIME)) {
            if (!wasHealthy && connectionHealthy) {
                addLog(LOG_LEVEL_INFO, F("WiFi Smart: Bağlantı stabil hale geldi"));
                resetCounters();  // Stabil bağlantı sonrası sayaçları sıfırla
            }
        }
    } else {
        stableConnectionStart.clear();
        connectionHealthy = false;
        consecutiveFailures++;
        
        if (consecutiveFailures >= WIFI_SMART_MAX_CONSECUTIVE_FAILURES && !emergencyModeActive) {
            addLog(LOG_LEVEL_ERROR, String(F("WiFi Smart: Ardışık ")) + String(consecutiveFailures) + 
                   String(F(" başarısızlık tespit edildi")));
            activateEmergencyMode();
        }
    }
}

void WiFiSmartManager::activateEmergencyMode() {
    if (emergencyModeActive) return;
    
    emergencyModeActive = true;
    forceAPMode = true;
    emergencyModeActivated.setNow();
    
    addLog(LOG_LEVEL_ERROR, F("WiFi Smart: EMERGENCY MODE AKTİF!"));
    addLog(LOG_LEVEL_INFO, String(F("WiFi Smart: Toplam bağlantı denemesi: ")) + String(totalReconnectAttempts));
    
    // AP modunu zorla
    setAP(true);
    
    // WiFi'ı resetle
    resetWiFi();
    
    addLog(LOG_LEVEL_INFO, F("WiFi Smart: AP modu açıldı, 5 dakika emergency mode"));
}

void WiFiSmartManager::deactivateEmergencyMode() {
    if (!emergencyModeActive) return;
    
    emergencyModeActive = false;
    forceAPMode = false;
    
    addLog(LOG_LEVEL_INFO, F("WiFi Smart: Emergency mode sona erdi"));
    resetCounters();
    
    // Normal WiFi işlemlerine geri dön
    AttemptWiFiConnect();
}

void WiFiSmartManager::forceFailsafe() {
    addLog(LOG_LEVEL_ERROR, F("WiFi Smart: FORCE FAILSAFE TETİKLENDİ!"));
    activateEmergencyMode();
}

void WiFiSmartManager::resetCounters() {
    consecutiveFailures = 0;
    connectionRetries = 0;
    // totalReconnectAttempts'i sıfırlamıyoruz - istatistik için sakla
}

bool WiFiSmartManager::shouldSwitchToAP() {
    if (forceAPMode || emergencyModeActive) return true;
    
    // Uptime bazlı kararlar
    if (isEarlyUptime() && consecutiveFailures >= WIFI_SMART_MAX_CONSECUTIVE_FAILURES) {
        return true;
    }
    
    if (isCriticalUptime() && consecutiveFailures >= WIFI_SMART_MAX_CONSECUTIVE_FAILURES + 2) {
        return true;
    }
    
    return false;
}

bool WiFiSmartManager::shouldRetryConnection() {
    if (emergencyModeActive) return false;
    if (WiFiConnected()) return false;
    
    uint32_t retryInterval;
    
    if (isEarlyUptime()) {
        retryInterval = WIFI_SMART_EARLY_RETRY_INTERVAL;
    } else if (isCriticalUptime()) {
        retryInterval = WIFI_SMART_CRITICAL_RETRY_INTERVAL;
    } else {
        retryInterval = WIFI_SMART_STABLE_RETRY_INTERVAL;
    }
    
    return !lastConnectionAttempt.isSet() || lastConnectionAttempt.timeoutReached(retryInterval);
}

bool WiFiSmartManager::isConnectionStable() {
    return connectionHealthy && 
           stableConnectionStart.isSet() && 
           stableConnectionStart.timeoutReached(WIFI_SMART_STABLE_CONNECTION_TIME);
}

String WiFiSmartManager::getStatusReport() {
    String report = F("WiFi Smart Manager Status:\n");
    report += String(F("- Uptime: ")) + String(getUptimeMinutes()) + String(F(" min\n"));
    
    if (isEarlyUptime()) {
        report += F("- Mode: Early (Agresif)\n");
    } else if (isCriticalUptime()) {
        report += F("- Mode: Critical (Orta)\n");
    } else {
        report += F("- Mode: Stable (Sakin)\n");
    }
    
    report += String(F("- WiFi Connected: ")) + (WiFiConnected() ? F("Yes") : F("No")) + String(F("\n"));
    report += String(F("- Connection Healthy: ")) + (connectionHealthy ? F("Yes") : F("No")) + String(F("\n"));
    report += String(F("- Emergency Mode: ")) + (emergencyModeActive ? F("Active") : F("Inactive")) + String(F("\n"));
    report += String(F("- Consecutive Failures: ")) + String(consecutiveFailures) + String(F("\n"));
    report += String(F("- Current Retries: ")) + String(connectionRetries) + String(F("\n"));
    report += String(F("- Total Attempts: ")) + String(totalReconnectAttempts) + String(F("\n"));
    
    if (WiFiConnected()) {
        report += String(F("- RSSI: ")) + String(WiFi.RSSI()) + String(F(" dBm\n"));
        report += String(F("- IP: ")) + WiFi.localIP().toString() + String(F("\n"));
    }
    
    return report;
}

void WiFiSmartManager::logWiFiState() {
    addLog(LOG_LEVEL_INFO, getStatusReport());
}