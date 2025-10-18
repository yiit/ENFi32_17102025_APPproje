#include "../WebServer/EnhancedSetupPage.h"

#ifdef WEBSERVER_SETUP

#include "../WebServer/ESPEasy_WebServer.h"
#include "../WebServer/AccessControl.h"
#include "../WebServer/HTML_wrappers.h"
#include "../WebServer/Markup.h"
#include "../WebServer/Markup_Buttons.h"
#include "../WebServer/Markup_Forms.h"
#include "../WebServer/SysInfoPage.h"

#include "../ESPEasyCore/ESPEasyNetwork.h"
#include "../ESPEasyCore/ESPEasyWifi.h"

#include "../Globals/ESPEasyWiFiEvent.h"
#include "../Globals/NetworkState.h"
#include "../Globals/RTC.h"
#include "../Globals/Settings.h"
#include "../Globals/SecuritySettings.h"
#include "../Globals/WiFi_AP_Candidates.h"

#include "../Helpers/Misc.h"
#include "../Helpers/Networking.h"
#include "../Helpers/ESPEasy_Storage.h"
#include "../Helpers/StringConverter.h"

#define STAGE_SCAN          0
#define STAGE_SCANNING      1
#define STAGE_SELECT        2
#define STAGE_PASSWORD      3
#define STAGE_CONNECTING    4
#define STAGE_SUCCESS       5
#define STAGE_FAILED        6
#define STAGE_FAILSAFE      7

// Forward declarations - FONKSİYON TANIMLARI
void renderScanStage();
void renderScanningStage(uint8_t refreshCount);
void renderSelectStage();
void renderPasswordStage(const String& ssid);
void renderConnectingStage(const String& ssid, uint8_t refreshCount);
void renderSuccessStage();
void renderFailedStage(const String& ssid, const String& error);
void renderFailsafeStage();

// Global failsafe variables
static uint8_t wifiRetryCount = 0;
static unsigned long lastConnectionAttempt = 0;
static bool failsafeMode = false;

void handle_enhanced_setup() {
  #ifndef BUILD_NO_RAM_TRACKER
  checkRAM(F("handle_enhanced_setup"));
  #endif

  TXBuffer.startStream();
  const bool connected = NetworkConnected();
  navMenuIndex = MENU_INDEX_SETUP;
  sendHeadandTail_stdtemplate(_HEAD);

  static uint8_t currentStage = STAGE_SCAN;
  static uint8_t refreshCount = 0;
  static String selectedSSID = "";
  static String enteredPassword = "";
  static bool isHiddenNetwork = false;
  static String lastError = "";
  static unsigned long connectionStartTime = 0;

  // Check for user actions
  const bool startScan = hasArg(F("startscan"));
  const bool selectNetwork = hasArg(F("selectnetwork"));
  const bool enterPassword = hasArg(F("enterpassword"));
  const bool connectWifi = hasArg(F("connectwifi"));
  const bool rescan = hasArg(F("rescan"));
  const bool retry = hasArg(F("retry"));
  const bool resetWifi = hasArg(F("resetwifi"));
  const bool enableFailsafe = hasArg(F("failsafe"));

  String ssid = webArg(F("ssid"));
  String password;
  bool passwordGiven = getFormPassword(F("password"), password);

  // Failsafe mode check - DAHA AGRESIF
  if (enableFailsafe || 
      (wifiRetryCount >= 3 && 
       (millis() - lastConnectionAttempt) > 30000)) {  // 3 deneme, 30 saniye
    
    // STRING CONCATENATION DÜZELTİLDİ
    String logMsg = String(F("WiFi Setup: Entering failsafe mode - too many failed attempts"));
    addLog(LOG_LEVEL_ERROR, logMsg);
    
    // WiFi'yi tamamen durdur ve sıfırla
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    delay(1000);
    
    // WiFi credentials'ları temizle
    if (resetWifi) {
      SecuritySettings.WifiSSID[0] = 0;
      SecuritySettings.WifiKey[0] = 0;
      SecuritySettings.WifiSSID2[0] = 0;
      SecuritySettings.WifiKey2[0] = 0;
      SaveSettings();
      addLog(LOG_LEVEL_INFO, String(F("WiFi Setup: WiFi credentials cleared")));
    }
    
    // STA+AP mode'a geç
    WiFi.mode(WIFI_AP_STA);
    delay(500);
    
    // AP mode'u zorla aç ve sürekli açık tut
    WiFiEventData.timerAPstart.setMillisFromNow(1000);
    WiFiEventData.timerAPoff.clear();  // AP'yi hiç kapatma
    
    currentStage = STAGE_FAILSAFE;
    failsafeMode = true;
    wifiRetryCount = 0;
    lastError = "Çok fazla başarısız deneme - Güvenli mod aktif";
  }

  // Handle stage transitions
  if (startScan || rescan) {
    WiFiEventData.lastScanMoment.clear();
    WifiScan(false);
    currentStage = STAGE_SCANNING;
    refreshCount = 0;
    lastError = "";
    wifiRetryCount = 0;
  } else if (selectNetwork && !ssid.isEmpty()) {
    selectedSSID = ssid;
    isHiddenNetwork = (ssid == F("#Hidden#"));
    currentStage = STAGE_PASSWORD;
    lastError = "";
  } else if (connectWifi && passwordGiven) {
    enteredPassword = password;
    
    // Save WiFi credentials
    safe_strncpy(SecuritySettings.WifiKey, password.c_str(), sizeof(SecuritySettings.WifiKey));
    safe_strncpy(SecuritySettings.WifiSSID, selectedSSID.c_str(), sizeof(SecuritySettings.WifiSSID));
    
    Settings.IncludeHiddenSSID(isHiddenNetwork);
    addHtmlError(SaveSettings());
    
    WiFiEventData.wifiSetupConnect = true;
    WiFiEventData.wifiConnectAttemptNeeded = true;
    WiFi_AP_Candidates.force_reload();
    
    currentStage = STAGE_CONNECTING;
    refreshCount = 0;
    connectionStartTime = millis();
    lastConnectionAttempt = millis();
    lastError = "";
    AttemptWiFiConnect();
  } else if (retry) {
    currentStage = STAGE_PASSWORD;
    lastError = "";
    refreshCount = 0;
  }

  // Auto stage progression and error detection
  if (currentStage == STAGE_SCANNING) {
    const int scanResult = std::distance(WiFi_AP_Candidates.scanned_begin(), WiFi_AP_Candidates.scanned_end());
    if (scanResult > 0) {
      currentStage = STAGE_SELECT;
    } else if (refreshCount > 10) {
      currentStage = STAGE_SCAN;
      lastError = "Hiç WiFi ağı bulunamadı";
    }
  } else if (currentStage == STAGE_CONNECTING) {
    if (connected && NetworkConnected()) {
      currentStage = STAGE_SUCCESS;
      WiFiEventData.timerAPoff.setMillisFromNow(120000);
      lastError = "";
      wifiRetryCount = 0;
      failsafeMode = false;
      
      addLog(LOG_LEVEL_INFO, String(F("WiFi Setup: Connection successful")));
    } else if (refreshCount > 10) {  // 20'den 10'a düşürüldü - DAHA HIZLI FAİLSAFE
      wifiRetryCount++;
      currentStage = STAGE_FAILED;
      
      // STRING CONCATENATION DÜZELTİLDİ
      String logMsg = String(F("WiFi Setup: Connection failed - Attempt ")) + String(wifiRetryCount) + String(F(" of 3"));  // 5'ten 3'e düşürüldü
      addLog(LOG_LEVEL_ERROR, logMsg);
      
      // WiFi durumunu detaylı kontrol et
      wl_status_t wifiStatus = WiFi.status();
      
      // ANINDA FAİLSAFE koşulları - Kritik hatalar için
      bool criticalFailure = (wifiStatus == WL_CONNECT_FAILED || wifiStatus == WL_NO_SSID_AVAIL);
      
      if (criticalFailure || wifiRetryCount >= 3) {  // 5'ten 3'e düşürüldü - DAHA AGRESIF
        // FAILSAFE MODE AKTİF!
        failsafeMode = true;
        currentStage = STAGE_FAILSAFE;
        
        addLog(LOG_LEVEL_ERROR, String(F("FAILSAFE MODE AKTİF! WiFi Status: ")) + String(wifiStatus));
        
        // ESPEasy WiFi API kullanarak failsafe mod
        WifiDisconnect();  // ESPEasy disconnect
        delay(1000);
        setWifiMode(WIFI_AP_STA);  // ESPEasy setWifiMode
        setAP(true);  // ESPEasy AP enable
        
        addLog(LOG_LEVEL_INFO, String(F("Failsafe AP modu ESPEasy ile başlatıldı!")));
        lastError = String(F("Güvenli mod aktif! WiFi ayarlarını kontrol edin"));
        
        wifiRetryCount = 0;  // Reset counter
        return;
      }
      
      // Normal hata mesajları
      switch (wifiStatus) {
        case WL_CONNECT_FAILED:
          lastError = "Bağlantı başarısız - Şifre yanlış olabilir";
          break;
        case WL_NO_SSID_AVAIL:
          lastError = "WiFi ağı bulunamadı - Ağ kapalı olabilir";
          break;
        case WL_CONNECTION_LOST:
          lastError = "Bağlantı kesildi";
          break;
        case WL_DISCONNECTED:
          lastError = "WiFi bağlantısı reddedildi";
          break;
        default:
          lastError = "Bilinmeyen bağlantı hatası (Kod: " + String(wifiStatus) + ")";
          break;
      }
      
      lastError += " (Deneme: " + String(wifiRetryCount) + "/5)";
      
      if (wifiRetryCount >= 5) {
        lastError += " - Güvenli mod yakında aktif olacak!";
      }
    }
  }

  // Enhanced Material Design CSS
  addHtml(F("<style>"));
  addHtml(F("*{box-sizing:border-box;margin:0;padding:0}"));
  addHtml(F("body{font-family:'Segoe UI',Roboto,sans-serif;background:linear-gradient(135deg,#667eea 0%,#764ba2 100%);min-height:100vh;padding:20px}"));
  addHtml(F(".setup-container{max-width:400px;margin:0 auto;background:white;border-radius:16px;box-shadow:0 8px 32px rgba(0,0,0,0.12);overflow:hidden;animation:slideUp 0.5s ease}"));
  addHtml(F(".header{background:linear-gradient(135deg,#4facfe 0%,#00f2fe 100%);color:white;padding:24px;text-align:center;position:relative}"));
  addHtml(F(".header h1{font-size:24px;font-weight:600;margin-bottom:8px}"));
  addHtml(F(".header p{opacity:0.9;font-size:14px}"));
  addHtml(F(".stage-indicator{display:flex;justify-content:center;padding:16px;background:#f8f9fa;border-bottom:1px solid #e9ecef}"));
  addHtml(F(".stage-dot{width:12px;height:12px;border-radius:50%;background:#dee2e6;margin:0 4px;transition:all 0.3s ease}"));
  addHtml(F(".stage-dot.active{background:linear-gradient(45deg,#4facfe,#00f2fe);transform:scale(1.2)}"));
  addHtml(F(".stage-dot.completed{background:linear-gradient(45deg,#11998e,#38ef7d)}"));
  addHtml(F(".stage-dot.failed{background:linear-gradient(45deg,#ff416c,#ff4b2b)}"));
  addHtml(F(".content{padding:24px}"));
  addHtml(F(".step-title{font-size:20px;font-weight:600;color:#2c3e50;margin-bottom:16px;text-align:center}"));
  addHtml(F(".step-subtitle{color:#6c757d;text-align:center;margin-bottom:24px}"));
  addHtml(F(".wifi-item{background:#f8f9fa;border:2px solid #e9ecef;border-radius:12px;padding:16px;margin-bottom:12px;cursor:pointer;transition:all 0.3s ease;position:relative}"));
  addHtml(F(".wifi-item:hover{border-color:#4facfe;transform:translateY(-2px);box-shadow:0 4px 16px rgba(79,172,254,0.2)}"));
  addHtml(F(".wifi-item.selected{border-color:#38ef7d;background:linear-gradient(135deg,rgba(56,239,125,0.1) 0%,rgba(17,153,142,0.1) 100%)}"));
  addHtml(F(".wifi-name{font-weight:600;font-size:16px;color:#2c3e50;margin-bottom:4px}"));
  addHtml(F(".wifi-details{font-size:12px;color:#6c757d;display:flex;justify-content:space-between;align-items:center}"));
  addHtml(F(".signal-strength{display:flex;align-items:center;gap:2px}"));
  addHtml(F(".signal-bar{width:4px;height:16px;background:#dee2e6;border-radius:2px}"));
  addHtml(F(".signal-bar.filled{background:linear-gradient(to top,#4facfe,#00f2fe)}"));
  addHtml(F(".signal-bar.strong{background:linear-gradient(to top,#38ef7d,#11998e)}"));
  addHtml(F(".form-group{margin-bottom:20px}"));
  addHtml(F(".form-label{display:block;font-weight:600;color:#2c3e50;margin-bottom:8px}"));
  addHtml(F(".form-input{width:100%;padding:12px 16px;border:2px solid #e9ecef;border-radius:8px;font-size:16px;transition:all 0.3s ease}"));
  addHtml(F(".form-input:focus{outline:none;border-color:#4facfe;box-shadow:0 0 0 3px rgba(79,172,254,0.1)}"));
  addHtml(F(".btn{width:100%;padding:16px;border:none;border-radius:8px;font-size:16px;font-weight:600;cursor:pointer;transition:all 0.3s ease;text-decoration:none;display:inline-block;text-align:center}"));
  addHtml(F(".btn-primary{background:linear-gradient(135deg,#4facfe 0%,#00f2fe 100%);color:white}"));
  addHtml(F(".btn-primary:hover{transform:translateY(-2px);box-shadow:0 8px 25px rgba(79,172,254,0.4)}"));
  addHtml(F(".btn-success{background:linear-gradient(135deg,#11998e 0%,#38ef7d 100%);color:white}"));
  addHtml(F(".btn-danger{background:linear-gradient(135deg,#ff416c 0%,#ff4b2b 100%);color:white}"));
  addHtml(F(".btn-outline{background:transparent;border:2px solid #4facfe;color:#4facfe}"));
  addHtml(F(".btn-outline:hover{background:#4facfe;color:white}"));
  addHtml(F(".btn-outline-danger{background:transparent;border:2px solid #ff416c;color:#ff416c}"));
  addHtml(F(".btn-outline-danger:hover{background:#ff416c;color:white}"));
  addHtml(F(".loading{text-align:center;padding:40px 20px}"));
  addHtml(F(".spinner{width:40px;height:40px;border:4px solid #f3f3f3;border-top:4px solid #4facfe;border-radius:50%;animation:spin 1s linear infinite;margin:0 auto 16px}"));
  addHtml(F(".success-icon{width:80px;height:80px;background:linear-gradient(135deg,#11998e,#38ef7d);border-radius:50%;margin:0 auto 20px;display:flex;align-items:center;justify-content:center;color:white;font-size:32px}"));
  addHtml(F(".error-icon{width:80px;height:80px;background:linear-gradient(135deg,#ff416c,#ff4b2b);border-radius:50%;margin:0 auto 20px;display:flex;align-items:center;justify-content:center;color:white;font-size:32px}"));
  addHtml(F(".ip-display{background:linear-gradient(135deg,#667eea,#764ba2);color:white;padding:16px;border-radius:8px;text-align:center;font-family:monospace;font-size:18px;font-weight:bold;margin:16px 0}"));
  addHtml(F(".connection-details{background:#f8f9fa;border-radius:8px;padding:16px;margin-top:16px}"));
  addHtml(F(".detail-row{display:flex;justify-content:space-between;margin-bottom:8px;font-size:14px}"));
  addHtml(F(".detail-label{color:#6c757d;font-weight:500}"));
  addHtml(F(".detail-value{color:#2c3e50;font-weight:600}"));
  addHtml(F(".error-message{background:linear-gradient(135deg,rgba(255,65,108,0.1),rgba(255,75,43,0.1));border:2px solid #ff416c;color:#c53030;padding:16px;border-radius:8px;margin:16px 0;text-align:center;font-weight:600}"));
  addHtml(F(".progress-bar{width:100%;height:6px;background:#e9ecef;border-radius:3px;overflow:hidden;margin:16px 0}"));
  addHtml(F(".progress-fill{height:100%;background:linear-gradient(90deg,#4facfe,#00f2fe);animation:progress 20s linear forwards}"));
  addHtml(F(".failsafe-warning{background:linear-gradient(135deg,rgba(255,193,7,0.2),rgba(255,133,27,0.2));border:2px solid #ffc107;color:#856404;padding:16px;border-radius:8px;margin:16px 0;text-align:center;font-weight:600}"));
  addHtml(F(".retry-counter{background:#dc3545;color:white;padding:4px 8px;border-radius:12px;font-size:12px;font-weight:bold;margin-left:8px}"));
  addHtml(F(".failsafe-mode{background:linear-gradient(135deg,#ff9a9e 0%,#fecfef 100%);border:3px solid #ff6b6b;animation:pulse 2s infinite}"));
  addHtml(F("@keyframes spin{0%{transform:rotate(0deg)}100%{transform:rotate(360deg)}}"));
  addHtml(F("@keyframes slideUp{from{opacity:0;transform:translateY(30px)}to{opacity:1;transform:translateY(0)}}"));
  addHtml(F("@keyframes progress{0%{width:0%}100%{width:100%}}"));
  addHtml(F("@keyframes pulse{0%{border-color:#ff6b6b}50%{border-color:#ff9a9e}100%{border-color:#ff6b6b}}"));
  addHtml(F("</style>"));

  // Failsafe mode container
  if (failsafeMode) {
    addHtml(F("<div class='setup-container failsafe-mode'>"));
  } else {
    addHtml(F("<div class='setup-container'>"));
  }
  
  // Header with retry counter
  addHtml(F("<div class='header'>"));
  addHtml(F("<h1>WiFi Kurulum"));
  if (wifiRetryCount > 0 && !failsafeMode) {
    addHtml(F("<span class='retry-counter'>"));
    addHtmlInt(wifiRetryCount);
    addHtml(F("/5</span>"));
  }
  addHtml(F("</h1>"));
  if (failsafeMode) {
    addHtml(F("<p>🔒 Güvenli Mod Aktif</p>"));
  } else {
    addHtml(F("<p>ESP32'nizi WiFi ağına bağlayın</p>"));
  }
  addHtml(F("</div>"));

  // Stage Indicator
  addHtml(F("<div class='stage-indicator'>"));
  int maxStages = failsafeMode ? 6 : 5;
  for (int i = 0; i <= maxStages; i++) {
    addHtml(F("<div class='stage-dot"));
    if (i < currentStage || (currentStage == STAGE_FAILED && i <= STAGE_CONNECTING)) {
      addHtml(F(" completed"));
    }
    if (i == currentStage) {
      addHtml(F(" active"));
    }
    if (currentStage == STAGE_FAILED && i == STAGE_CONNECTING) {
      addHtml(F(" failed"));
    }
    addHtml(F("'></div>"));
  }
  addHtml(F("</div>"));

  addHtml(F("<div class='content'>"));
  html_add_form();

  // Failsafe warning
  if (wifiRetryCount >= 4 && !failsafeMode) {
    addHtml(F("<div class='failsafe-warning'>"));
    addHtml(F("⚠️ Uyarı: Bir deneme daha başarısız olursa güvenli mod aktif olacak!<br>"));
    addHtml(F("Güvenli modda WiFi ayarları sıfırlanacak ve AP modu açılacak."));
    addHtml(F("</div>"));
  }

  // Error message display
  if (!lastError.isEmpty() && (currentStage == STAGE_FAILED || currentStage == STAGE_SCAN)) {
    addHtml(F("<div class='error-message'>"));
    addHtml(F("❌ "));
    addHtml(lastError);
    addHtml(F("</div>"));
  }

  // Render current stage
  switch (currentStage) {
    case STAGE_SCAN:
      renderScanStage();
      break;
    case STAGE_SCANNING:
      renderScanningStage(refreshCount);
      break;
    case STAGE_SELECT:
      renderSelectStage();
      break;
    case STAGE_PASSWORD:
      renderPasswordStage(selectedSSID);
      break;
    case STAGE_CONNECTING:
      renderConnectingStage(selectedSSID, refreshCount);
      break;
    case STAGE_SUCCESS:
      renderSuccessStage();
      break;
    case STAGE_FAILED:
      renderFailedStage(selectedSSID, lastError);
      break;
    case STAGE_FAILSAFE:
      renderFailsafeStage();
      break;
  }

  html_end_form();
  addHtml(F("</div>"));
  addHtml(F("</div>"));

  // Auto-refresh for animated stages
  if (currentStage == STAGE_SCANNING || currentStage == STAGE_CONNECTING) {
    addHtml(F("<script>setTimeout(() => window.location.reload(), 1000);</script>"));
    refreshCount++;
  }

  sendHeadandTail_stdtemplate(_TAIL);
  TXBuffer.endStream();
}

// FONKSİYON İMPLEMENTASYONLARI

void renderScanStage() {
  addHtml(F("<div class='step-title'>WiFi Ağlarını Tara</div>"));
  addHtml(F("<div class='step-subtitle'>Çevredeki WiFi ağlarını bulmak için taramaya başlayın</div>"));
  
  addHtml(F("<div style='text-align: center; margin: 40px 0;'>"));
  addHtml(F("<div style='width: 120px; height: 120px; background: linear-gradient(135deg, #4facfe, #00f2fe); border-radius: 50%; margin: 0 auto 20px; display: flex; align-items: center; justify-content: center; color: white; font-size: 48px;'>"));
  addHtml(F("📶"));
  addHtml(F("</div>"));
  addHtml(F("</div>"));

  addHtml(F("<button type='submit' name='startscan' value='1' class='btn btn-primary'>"));
  addHtml(F("🔍 Ağları Tara"));
  addHtml(F("</button>"));
}

void renderScanningStage(uint8_t refreshCount) {
  addHtml(F("<div class='step-title'>Taranıyor...</div>"));
  addHtml(F("<div class='step-subtitle'>WiFi ağları aranıyor, lütfen bekleyin</div>"));
  
  addHtml(F("<div class='loading'>"));
  addHtml(F("<div class='spinner'></div>"));
  addHtml(F("<p style='color: #6c757d;'>Çevredeki ağlar taranıyor</p>"));
  addHtml(F("<p style='color: #6c757d; font-size: 12px; margin-top: 8px;'>"));
  addHtmlInt(refreshCount);
  addHtml(F(" saniye</p>"));
  addHtml(F("</div>"));
}

void renderSelectStage() {
  addHtml(F("<div class='step-title'>Ağ Seçin</div>"));
  addHtml(F("<div class='step-subtitle'>Bağlanmak istediğiniz WiFi ağını seçin</div>"));

  int networkCount = 0;
  for (auto it = WiFi_AP_Candidates.scanned_begin(); it != WiFi_AP_Candidates.scanned_end(); ++it) {
    if (networkCount >= 8) break;
    
    String networkSSID = it->bits.isHidden ? F("#Hidden#") : it->ssid;
    String displayName = it->bits.isHidden ? F("🔒 Gizli Ağ") : it->ssid;
    
    addHtml(F("<div class='wifi-item' onclick='selectWiFi(\""));
    addHtml(networkSSID);
    addHtml(F("\")'>"));
    
    addHtml(F("<div class='wifi-name'>"));
    addHtml(displayName);
    if (it->encryption_type() != 0) {
      addHtml(F(" 🔒"));
    }
    addHtml(F("</div>"));
    
    addHtml(F("<div class='wifi-details'>"));
    addHtml(F("<span>Kanal: "));
    addHtmlInt(it->channel);
    addHtml(F("</span>"));
    
    addHtml(F("<div class='signal-strength'>"));
    int rssi = it->rssi;
    for (int i = 0; i < 4; i++) {
      addHtml(F("<div class='signal-bar"));
      if (rssi > -80 + (i * 10)) {
        addHtml(F(" filled"));
        if (rssi > -60) addHtml(F(" strong"));
      }
      addHtml(F("'></div>"));
    }
    addHtml(F("<span style='margin-left: 4px; font-size: 11px;'>"));
    addHtmlInt(rssi);
    addHtml(F("</span>"));
    addHtml(F("</div>"));
    
    addHtml(F("</div>"));
    addHtml(F("</div>"));
    networkCount++;
  }

  addHtml(F("<button type='submit' name='rescan' value='1' class='btn btn-outline' style='margin-top: 16px;'>"));
  addHtml(F("🔄 Tekrar Tara"));
  addHtml(F("</button>"));

  // JavaScript for network selection
  addHtml(F("<script>"));
  addHtml(F("function selectWiFi(ssid) {"));
  addHtml(F("  const form = document.querySelector('form');"));
  addHtml(F("  const input = document.createElement('input');"));
  addHtml(F("  input.type = 'hidden';"));
  addHtml(F("  input.name = 'ssid';"));
  addHtml(F("  input.value = ssid;"));
  addHtml(F("  form.appendChild(input);"));
  addHtml(F("  const submit = document.createElement('input');"));
  addHtml(F("  submit.type = 'hidden';"));
  addHtml(F("  submit.name = 'selectnetwork';"));
  addHtml(F("  submit.value = '1';"));
  addHtml(F("  form.appendChild(submit);"));
  addHtml(F("  form.submit();"));
  addHtml(F("}"));
  addHtml(F("</script>"));
}

void renderPasswordStage(const String& ssid) {
  addHtml(F("<div class='step-title'>Şifre Girin</div>"));
  addHtml(F("<div class='step-subtitle'>Seçilen ağ için şifrenizi girin</div>"));

  addHtml(F("<div style='background: #f8f9fa; border-radius: 8px; padding: 16px; margin-bottom: 24px; text-align: center;'>"));
  addHtml(F("<div style='color: #4facfe; font-size: 24px; margin-bottom: 8px;'>📶</div>"));
  addHtml(F("<div style='font-weight: 600; color: #2c3e50;'>"));
  addHtml(ssid == F("#Hidden#") ? F("Gizli Ağ") : ssid);
  addHtml(F("</div>"));
  addHtml(F("</div>"));

  if (ssid != F("#Hidden#")) {
    addHtml(F("<input type='hidden' name='ssid' value='"));
    addHtml(ssid);
    addHtml(F("'>"));
  } else {
    addHtml(F("<div class='form-group'>"));
    addHtml(F("<label class='form-label'>Ağ Adı (SSID)</label>"));
    addHtml(F("<input type='text' name='ssid' class='form-input' placeholder='WiFi ağ adını girin' required>"));
    addHtml(F("</div>"));
  }

  addHtml(F("<div class='form-group'>"));
  addHtml(F("<label class='form-label'>Şifre</label>"));
  addHtml(F("<input type='password' name='password' class='form-input' placeholder='WiFi şifresini girin' required>"));
  addHtml(F("</div>"));

  addHtml(F("<div style='margin-bottom: 16px;'>"));
  addHtml(F("<label style='display: flex; align-items: center; color: #6c757d;'>"));
  addHtml(F("<input type='checkbox' name='emptypass' style='margin-right: 8px;'>"));
  addHtml(F("Açık ağ (şifresiz)"));
  addHtml(F("</label>"));
  addHtml(F("</div>"));

  addHtml(F("<button type='submit' name='connectwifi' value='1' class='btn btn-primary'>"));
  addHtml(F("🚀 Bağlan"));
  addHtml(F("</button>"));
}

void renderConnectingStage(const String& ssid, uint8_t refreshCount) {
  addHtml(F("<div class='step-title'>Bağlanıyor...</div>"));
  addHtml(F("<div class='step-subtitle'>WiFi ağına bağlantı kuruluyor</div>"));

  addHtml(F("<div class='loading'>"));
  addHtml(F("<div class='spinner'></div>"));
  addHtml(F("<p style='color: #2c3e50; font-weight: 600; margin-bottom: 8px;'>"));
  addHtml(ssid);
  addHtml(F("</p>"));
  addHtml(F("<p style='color: #6c757d;'>Bağlantı kuruluyor...</p>"));
  
  addHtml(F("<div class='progress-bar'>"));
  addHtml(F("<div class='progress-fill' style='animation-duration: "));
  addHtmlInt(20 - refreshCount);
  addHtml(F("s;'></div>"));
  addHtml(F("</div>"));
  
  addHtml(F("<p style='color: #6c757d; font-size: 12px; margin-top: 8px;'>"));
  addHtmlInt(refreshCount);
  addHtml(F(" / 20 saniye</p>"));
  addHtml(F("</div>"));
}

void renderSuccessStage() {
  addHtml(F("<div class='step-title'>Başarılı! 🎉</div>"));
  addHtml(F("<div class='step-subtitle'>WiFi bağlantısı başarıyla kuruldu</div>"));

  addHtml(F("<div style='text-align: center; margin: 20px 0;'>"));
  addHtml(F("<div class='success-icon'>✓</div>"));
  addHtml(F("</div>"));

  addHtml(F("<div class='ip-display'>"));
  addHtml(formatIP(NetworkLocalIP()));
  addHtml(F("</div>"));

  addHtml(F("<div class='connection-details'>"));
  addHtml(F("<div class='detail-row'>"));
  addHtml(F("<span class='detail-label'>Ağ:</span>"));
  addHtml(F("<span class='detail-value'>"));
  addHtml(WiFi.SSID());
  addHtml(F("</span>"));
  addHtml(F("</div>"));
  
  addHtml(F("<div class='detail-row'>"));
  addHtml(F("<span class='detail-label'>Sinyal:</span>"));
  addHtml(F("<span class='detail-value'>"));
  addHtmlInt(WiFi.RSSI());
  addHtml(F(" dBm</span>"));
  addHtml(F("</div>"));
  
  addHtml(F("<div class='detail-row'>"));
  addHtml(F("<span class='detail-label'>Gateway:</span>"));
  addHtml(F("<span class='detail-value'>"));
  addHtml(formatIP(NetworkGatewayIP()));
  addHtml(F("</span>"));
  addHtml(F("</div>"));
  
  addHtml(F("<div class='detail-row'>"));
  addHtml(F("<span class='detail-label'>Durum:</span>"));
  addHtml(F("<span class='detail-value' style='color: #28a745; font-weight: bold;'>🟢 Bağlandı</span>"));
  addHtml(F("</div>"));
  addHtml(F("</div>"));

  String configUrl = String(F("http://")) + formatIP(NetworkLocalIP()) + String(F("/config"));
  
  addHtml(F("<a href='"));
  addHtml(configUrl);
  addHtml(F("' class='btn btn-success' style='margin-top: 20px;'>"));
  addHtml(F("⚙️ Yapılandırmaya Git"));
  addHtml(F("</a>"));

  addHtml(F("<p style='color: #6c757d; font-size: 12px; text-align: center; margin-top: 16px;'>"));
  addHtml(F("✅ AP modu 2 dakika daha aktif kalacak"));
  addHtml(F("</p>"));
}

void renderFailedStage(const String& ssid, const String& error) {
  addHtml(F("<div class='step-title'>Bağlantı Başarısız ❌</div>"));
  addHtml(F("<div class='step-subtitle'>WiFi ağına bağlanılamadı</div>"));

  addHtml(F("<div style='text-align: center; margin: 20px 0;'>"));
  addHtml(F("<div class='error-icon'>✗</div>"));
  addHtml(F("</div>"));

  addHtml(F("<div class='error-message'>"));
  addHtml(error);
  addHtml(F("</div>"));

  addHtml(F("<div style='background: #f8f9fa; border-radius: 8px; padding: 16px; margin: 16px 0; text-align: center;'>"));
  addHtml(F("<div style='color: #6c757d; font-size: 14px; margin-bottom: 8px;'>Denenen Ağ:</div>"));
  addHtml(F("<div style='font-weight: 600; color: #2c3e50;'>"));
  addHtml(F("📶 "));
  addHtml(ssid);
  addHtml(F("</div>"));
  addHtml(F("</div>"));

  addHtml(F("<div style='display: flex; gap: 8px; margin-top: 20px;'>"));
  addHtml(F("<button type='submit' name='retry' value='1' class='btn btn-primary' style='flex: 1;'>"));
  addHtml(F("🔄 Tekrar Dene"));
  addHtml(F("</button>"));
  addHtml(F("<button type='submit' name='startscan' value='1' class='btn btn-outline-danger' style='flex: 1;'>"));
  addHtml(F("🔍 Yeni Tarama"));
  addHtml(F("</button>"));
  addHtml(F("</div>"));
  
  if (wifiRetryCount >= 2) {
    addHtml(F("<button type='submit' name='failsafe' value='1' class='btn btn-danger' style='width: 100%; margin-top: 8px;' onclick='return confirm(\"Güvenli mod aktif edilecek ve WiFi ayarları sıfırlanacak. Emin misiniz?\")'>"));
    addHtml(F("🔒 Güvenli Modu Aktifleştir"));
    addHtml(F("</button>"));
  }
}

void renderFailsafeStage() {
  addHtml(F("<div class='step-title'>🔒 Güvenli Mod</div>"));
  addHtml(F("<div class='step-subtitle'>Çok fazla başarısız deneme tespit edildi</div>"));

  addHtml(F("<div style='text-align: center; margin: 20px 0;'>"));
  addHtml(F("<div style='width: 80px; height: 80px; background: linear-gradient(135deg, #ff9a9e, #fecfef); border-radius: 50%; margin: 0 auto 20px; display: flex; align-items: center; justify-content: center; color: #721c24; font-size: 32px;'>"));
  addHtml(F("🔒"));
  addHtml(F("</div>"));
  addHtml(F("</div>"));

  addHtml(F("<div class='failsafe-warning'>"));
  addHtml(F("Güvenlik nedeniyle sistem güvenli moda geçmiştir.<br>"));
  addHtml(F("AP modu otomatik olarak açılmıştır."));
  addHtml(F("</div>"));

  addHtml(F("<div style='display: flex; flex-direction: column; gap: 8px; margin-top: 20px;'>"));
  addHtml(F("<button type='submit' name='startscan' value='1' class='btn btn-primary'>"));
  addHtml(F("🔍 Yeni WiFi Taraması Yap"));
  addHtml(F("</button>"));
  addHtml(F("<button type='submit' name='resetwifi' value='1' class='btn btn-danger' onclick='return confirm(\"WiFi ayarları sıfırlanacak. Emin misiniz?\")'>"));
  addHtml(F("🗑️ WiFi Ayarlarını Sıfırla"));
  addHtml(F("</button>"));
  addHtml(F("</div>"));
}

#endif // WEBSERVER_SETUP