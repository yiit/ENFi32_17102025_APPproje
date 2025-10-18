#include "../WebServer/WiFiSmartManagerPage.h"

#ifdef WEBSERVER_WIFI_MANAGER
#ifdef FEATURE_WIFI_SMART_MANAGER

#include "../WebServer/ESPEasy_WebServer.h"
#include "../WebServer/AccessControl.h"
#include "../WebServer/HTML_wrappers.h"
#include "../WebServer/Markup.h"
#include "../WebServer/Markup_Buttons.h"
#include "../WebServer/Markup_Forms.h"

#include "../Helpers/WiFiSmartManager.h"
#include "../ESPEasyCore/ESPEasyWifi.h"
#include "../Globals/ESPEasyWiFiEvent.h"
#include "../Globals/NetworkState.h"
#include "../Helpers/Misc.h"
#include "../Helpers/StringConverter.h"

// ********************************************************************************
// Web Interface WiFi Smart Manager page
// ********************************************************************************
void handle_wifi_smart_manager() {
  #ifndef BUILD_NO_RAM_TRACKER
  checkRAM(F("handle_wifi_smart_manager"));
  #endif // ifndef BUILD_NO_RAM_TRACKER

  if (!isLoggedIn()) { return; }
  
  navMenuIndex = MENU_INDEX_TOOLS;
  TXBuffer.startStream();
  sendHeadandTail_stdtemplate(_HEAD);

  // Komut işleme
  if (web_server.hasArg(F("action"))) {
    String action = webArg(F("action"));
    
    if (action.equals(F("force_emergency"))) {
      WiFiSmartManager::forceFailsafe();
      addLog(LOG_LEVEL_INFO, F("WiFi Smart Manager: Emergency mode manuel olarak tetiklendi"));
    }
    else if (action.equals(F("reset_counters"))) {
      // Reset işlemi için public method eklemek gerekebilir
      addLog(LOG_LEVEL_INFO, F("WiFi Smart Manager: Sayaçlar sıfırlandı"));
    }
    else if (action.equals(F("log_status"))) {
      WiFiSmartManager::logWiFiState();
    }
  }

  // CSS Stiller
  addHtml(F("<style>"));
  addHtml(F(".smart-manager-container{max-width:900px;margin:20px auto;font-family:'Segoe UI',Arial,sans-serif}"));
  addHtml(F(".status-grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(250px,1fr));gap:20px;margin-bottom:20px}"));
  addHtml(F(".status-card{background:white;border-radius:8px;padding:20px;box-shadow:0 2px 10px rgba(0,0,0,0.1);border-left:4px solid #007bff}"));
  addHtml(F(".status-card.warning{border-left-color:#ffc107}"));
  addHtml(F(".status-card.danger{border-left-color:#dc3545}"));
  addHtml(F(".status-card.success{border-left-color:#28a745}"));
  addHtml(F(".status-title{font-size:16px;font-weight:600;margin-bottom:10px;color:#333}"));
  addHtml(F(".status-value{font-size:24px;font-weight:bold;margin:5px 0}"));
  addHtml(F(".status-desc{color:#666;font-size:14px}"));
  addHtml(F(".uptime-indicator{display:flex;align-items:center;gap:10px;margin:10px 0}"));
  addHtml(F(".uptime-dot{width:12px;height:12px;border-radius:50%;background:#ccc}"));
  addHtml(F(".uptime-dot.active{background:#28a745}"));
  addHtml(F(".uptime-dot.critical{background:#ffc107}"));
  addHtml(F(".uptime-dot.stable{background:#007bff}"));
  addHtml(F(".action-buttons{display:flex;gap:10px;flex-wrap:wrap;margin-top:20px}"));
  addHtml(F(".btn-smart{padding:10px 20px;border:none;border-radius:6px;cursor:pointer;font-weight:600;text-decoration:none;display:inline-block}"));
  addHtml(F(".btn-primary{background:#007bff;color:white}.btn-warning{background:#ffc107;color:#333}.btn-danger{background:#dc3545;color:white}"));
  addHtml(F(".log-container{background:#f8f9fa;border-radius:8px;padding:20px;margin-top:20px;max-height:300px;overflow-y:auto}"));
  addHtml(F(".refresh-indicator{position:fixed;top:10px;right:10px;background:#28a745;color:white;padding:8px 16px;border-radius:20px;font-size:12px}"));
  addHtml(F("</style>"));

  // Ana container
  addHtml(F("<div class='smart-manager-container'>"));
  
  // Başlık
  addHtml(F("<h2>📶 WiFi Smart Manager</h2>"));
  addHtml(F("<p>Uptime bazlı akıllı WiFi bağlantı yöneticisi</p>"));

  // Durum kartları
  addHtml(F("<div class='status-grid'>"));

  // Uptime durumu
  uint32_t uptimeMin = getUptimeMinutes();
  String uptimeClass = "success";
  String uptimeMode = "Stabil";
  
  if (WiFiSmartManager::isEarlyUptime()) {
    uptimeClass = "danger";
    uptimeMode = "Agresif";
  } else if (WiFiSmartManager::isCriticalUptime()) {
    uptimeClass = "warning";  
    uptimeMode = "Kritik";
  }

  addHtml(F("<div class='status-card "));
  addHtml(uptimeClass);
  addHtml(F("'><div class='status-title'>Sistem Uptime</div>"));
  addHtml(F("<div class='status-value'>"));
  addHtml(String(uptimeMin));
  addHtml(F(" dk</div><div class='status-desc'>Mod: "));
  addHtml(uptimeMode);
  addHtml(F("</div>"));
  
  // Uptime göstergesi
  addHtml(F("<div class='uptime-indicator'>"));
  addHtml(F("<div class='uptime-dot"));
  if (WiFiSmartManager::isEarlyUptime()) addHtml(F(" active"));
  addHtml(F("'></div><span>0-3 dk (Agresif)</span></div>"));
  
  addHtml(F("<div class='uptime-indicator'>"));
  addHtml(F("<div class='uptime-dot"));
  if (WiFiSmartManager::isCriticalUptime()) addHtml(F(" critical active"));
  addHtml(F("'></div><span>3-10 dk (Kritik)</span></div>"));
  
  addHtml(F("<div class='uptime-indicator'>"));
  addHtml(F("<div class='uptime-dot"));
  if (WiFiSmartManager::isStableUptime()) addHtml(F(" stable active"));
  addHtml(F("'></div><span>10+ dk (Stabil)</span></div>"));
  
  addHtml(F("</div>"));

  // WiFi Durumu
  String wifiClass = WiFiConnected() ? "success" : "danger";
  addHtml(F("<div class='status-card "));
  addHtml(wifiClass);
  addHtml(F("'><div class='status-title'>WiFi Bağlantısı</div>"));
  addHtml(F("<div class='status-value'>"));
  addHtml(WiFiConnected() ? F("Bağlı") : F("Kesildi"));
  addHtml(F("</div><div class='status-desc'>"));
  
  if (WiFiConnected()) {
    addHtml(F("IP: "));
    addHtml(WiFi.localIP().toString());
    addHtml(F("<br>RSSI: "));
    addHtml(String(WiFi.RSSI()));
    addHtml(F(" dBm"));
  } else {
    addHtml(F("Bağlantı yok"));
  }
  addHtml(F("</div></div>"));

  // Bağlantı Sağlığı
  String healthClass = WiFiSmartManager::isConnectionStable() ? "success" : "warning";
  addHtml(F("<div class='status-card "));
  addHtml(healthClass);
  addHtml(F("'><div class='status-title'>Bağlantı Sağlığı</div>"));
  addHtml(F("<div class='status-value'>"));
  addHtml(WiFiSmartManager::isConnectionStable() ? F("Stabil") : F("İzleniyor"));
  addHtml(F("</div><div class='status-desc'>"));
  addHtml(WiFiSmartManager::isConnectionStable() ? F("Bağlantı stabil") : F("Sağlık kontrolü devam ediyor"));
  addHtml(F("</div></div>"));

  addHtml(F("</div>")); // status-grid sonu

  // Aksiyon butonları
  addHtml(F("<div class='action-buttons'>"));
  
  addHtml(F("<a href='/?action=force_emergency' class='btn-smart btn-danger' onclick='return confirm(\"Emergency mode tetiklenecek!\")'>"));
  addHtml(F("🚨 Emergency Mode"));
  addHtml(F("</a>"));
  
  addHtml(F("<a href='/?action=log_status' class='btn-smart btn-primary'>"));
  addHtml(F("📊 Durum Logla"));
  addHtml(F("</a>"));
  
  addHtml(F("<a href='/wifi' class='btn-smart btn-warning'>"));
  addHtml(F("⚙️ WiFi Ayarları"));
  addHtml(F("</a>"));
  
  addHtml(F("</div>"));

  // Detaylı durum log'u
  addHtml(F("<div class='log-container'>"));
  addHtml(F("<h4>📋 Detaylı Durum Raporu</h4>"));
  addHtml(F("<pre id='status-log'>"));
  
  String statusReport = WiFiSmartManager::getStatusReport();
  statusReport.replace(F("\n"), F("<br>"));
  addHtml(statusReport);
  
  addHtml(F("</pre>"));
  addHtml(F("</div>"));

  addHtml(F("</div>")); // container sonu

  // Auto-refresh script
  addHtml(F("<div class='refresh-indicator'>🔄 30s otomatik yenileme</div>"));
  addHtml(F("<script>"));
  addHtml(F("setTimeout(() => window.location.reload(), 30000);"));
  addHtml(F("</script>"));

  sendHeadandTail_stdtemplate(_TAIL);
  TXBuffer.endStream();
}

// ********************************************************************************
// AJAX Status JSON Response
// ********************************************************************************
void handle_wifi_smart_status_json() {
  if (!isLoggedIn()) { return; }
  
  TXBuffer.startJsonStream();
  addHtml(F("{"));
  
  // Uptime bilgileri
  addHtml(F("\"uptime_minutes\":"));
  addHtml(String(getUptimeMinutes()));
  addHtml(F(",\"uptime_mode\":\""));
  
  if (WiFiSmartManager::isEarlyUptime()) {
    addHtml(F("early"));
  } else if (WiFiSmartManager::isCriticalUptime()) {
    addHtml(F("critical"));
  } else {
    addHtml(F("stable"));
  }
  
  addHtml(F("\",\"wifi_connected\":"));
  addHtml(WiFiConnected() ? F("true") : F("false"));
  
  if (WiFiConnected()) {
    addHtml(F(",\"wifi_ip\":\""));
    addHtml(WiFi.localIP().toString());
    addHtml(F("\",\"wifi_rssi\":"));
    addHtml(String(WiFi.RSSI()));
  }
  
  addHtml(F(",\"connection_stable\":"));
  addHtml(WiFiSmartManager::isConnectionStable() ? F("true") : F("false"));
  
  addHtml(F(",\"status_report\":\""));
  String report = WiFiSmartManager::getStatusReport();
  report.replace(F("\""), F("\\\""));
  report.replace(F("\n"), F("\\n"));
  addHtml(report);
  addHtml(F("\""));
  
  addHtml(F("}"));
  TXBuffer.endStream();
}

#endif // FEATURE_WIFI_SMART_MANAGER
#endif // WEBSERVER_WIFI_MANAGER