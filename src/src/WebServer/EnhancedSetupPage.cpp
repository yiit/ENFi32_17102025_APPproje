#include "../WebServer/EnhancedSetupPage.h"

#ifdef WEBSERVER_SETUP

#include "../WebServer/ESPEasy_WebServer.h"
#include "../WebServer/AccessControl.h"
#include "../WebServer/HTML_wrappers.h"
#include "../WebServer/Markup.h"
#include "../WebServer/Markup_Buttons.h"
#include "../WebServer/Markup_Forms.h"
#include "../WebServer/SysInfoPage.h"
#include "../ESPEasyCore/ESPEasyWifi.h"
#include "../Helpers/StringConverter.h"

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
#define STAGE_ADVANCED      4  // Gelişmiş ayarlar aşaması
#define STAGE_CONFIRM       5  // Onay aşaması
#define STAGE_CONNECTING    6
#define STAGE_SUCCESS       7
#define STAGE_FAILED        8
#define STAGE_FAILSAFE      9

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

static AdvancedWiFiSettings advancedSettings;

// Helper functions
bool validateIPAddress(const String& ip) {
  int parts[4];
  int partCount = 0;
  String temp = ip;
  
  while (temp.length() > 0 && partCount < 4) {
    int dotIndex = temp.indexOf('.');
    String part = (dotIndex > 0) ? temp.substring(0, dotIndex) : temp;
    
    if (part.length() == 0) return false;
    
    parts[partCount] = part.toInt();
    if (parts[partCount] < 0 || parts[partCount] > 255) return false;
    
    partCount++;
    temp = (dotIndex > 0) ? temp.substring(dotIndex + 1) : "";
  }
  
  return partCount == 4 && temp.length() == 0;
}

bool isValidSubnet(const String& subnet) {
  if (!validateIPAddress(subnet)) return false;
  
  // Common subnet masks validation
  return (subnet == "255.255.255.0" || 
          subnet == "255.255.0.0" || 
          subnet == "255.0.0.0" ||
          subnet == "255.255.255.128" ||
          subnet == "255.255.255.192" ||
          subnet == "255.255.255.224" ||
          subnet == "255.255.255.240" ||
          subnet == "255.255.255.248" ||
          subnet == "255.255.255.252");
}

String getDefaultGateway(const String& ip, const String& subnet) {
  // Simple gateway calculation - usually .1 of the network
  if (!validateIPAddress(ip)) return "192.168.1.1";
  
  int dotIndex = ip.lastIndexOf('.');
  if (dotIndex > 0) {
    return ip.substring(0, dotIndex) + ".1";
  }
  return "192.168.1.1";
}

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
  const bool showAdvanced = hasArg(F("advanced"));
  const bool saveAdvanced = hasArg(F("saveadvanced"));
  const bool confirmConnection = hasArg(F("confirmconnect"));
  const bool connectWifi = hasArg(F("connectwifi"));
  const bool rescan = hasArg(F("rescan"));
  const bool retry = hasArg(F("retry"));
  const bool resetWifi = hasArg(F("resetwifi"));
  const bool enableFailsafe = hasArg(F("failsafe"));
  const bool goBack = hasArg(F("back"));

  String ssid = webArg(F("ssid"));
  String password;
  bool passwordGiven = getFormPassword(F("password"), password);

  // Local Status API endpoint (no internet needed)
  String statusCheck = webArg(F("status"));
  if (!statusCheck.isEmpty()) {
    // ESP32 local server response - JSON format
    web_server.sendHeader(F("Content-Type"), F("application/json"));
    web_server.sendHeader(F("Cache-Control"), F("no-cache"));
    web_server.send(200, F("application/json"), F(""));
    
    TXBuffer.startStream();
    
    if (statusCheck == F("scan")) {
      // WiFi scan status check
      const int scanResult = std::distance(WiFi_AP_Candidates.scanned_begin(), WiFi_AP_Candidates.scanned_end());
      if (scanResult > 0) {
        addHtml(F("{\"complete\":true,\"message\":\""));
        addHtmlInt(scanResult);
        addHtml(F(" ağ bulundu - tıklayın\",\"count\":"));
        addHtmlInt(scanResult);
        addHtml(F("}"));
      } else {
        addHtml(F("{\"complete\":false,\"message\":\"Ağlar aranıyor...\"}"));
      }
    } else if (statusCheck == F("connect")) {
      // WiFi connection status check
      bool wifiConnected = (WiFi.status() == WL_CONNECTED);
      bool hasValidIP = (WiFi.localIP() != IPAddress(0, 0, 0, 0));
      String ipStr = WiFi.localIP().toString();
      
      if (hasValidIP && (ipStr != "0.0.0.0")) {
        addHtml(F("{\"success\":true,\"ip\":\""));
        addHtml(ipStr);
        addHtml(F("\",\"message\":\"✅ Bağlantı Başarılı!\"}"));
      } else if (WiFi.status() == WL_CONNECT_FAILED) {
        addHtml(F("{\"failed\":true,\"message\":\"❌ Şifre Hatalı\"}"));
      } else if (WiFi.status() == WL_NO_SSID_AVAIL) {
        addHtml(F("{\"failed\":true,\"message\":\"❌ Ağ Bulunamadı\"}"));
      } else {
        addHtml(F("{\"success\":false,\"message\":\"🔄 Bağlanıyor...\",\"details\":\"Status: "));
        addHtmlInt(WiFi.status());
        addHtml(F("\"}"));
      }
    }
    
    TXBuffer.endStream();
    return;
  }

  // Failsafe mode check - DAHA AGRESIF
  if (enableFailsafe || 
      (wifiRetryCount >= 3 && 
       (millis() - lastConnectionAttempt) > 30000)) {  // 3 deneme, 30 saniye
    
    // STRING CONCATENATION DÜZELTİLDİ
    String logMsg = String(F("WiFi Setup: Entering failsafe mode - too many failed attempts"));
    addLog(LOG_LEVEL_ERROR, logMsg);
    
    // WiFi credentials'ları temizle
    if (resetWifi) {
      SecuritySettings.WifiSSID[0] = 0;
      SecuritySettings.WifiKey[0] = 0;
      SecuritySettings.WifiSSID2[0] = 0;
      SecuritySettings.WifiKey2[0] = 0;
      SaveSettings();
      addLog(LOG_LEVEL_INFO, String(F("WiFi Setup: WiFi credentials cleared")));
    }
    
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
    
    // Initialize advanced settings
    advancedSettings.ssid = ssid;
    advancedSettings.useStaticIP = false;
    advancedSettings.hiddenSSID = isHiddenNetwork;
    
  } else if (enterPassword && passwordGiven) {
    // Basic password entry - skip to connecting
    enteredPassword = password;
    advancedSettings.password = password;
    advancedSettings.emptyPassword = isFormItemChecked(F("emptypass"));
    
    // Clear static IP settings when using basic setup (defaults to DHCP)
    for (int i = 0; i < 4; i++) {
      Settings.IP[i] = 0;
      Settings.Gateway[i] = 0;
      Settings.Subnet[i] = 0;
      Settings.DNS[i] = 0;
    }
    
    // Save and connect immediately
    safe_strncpy(SecuritySettings.WifiKey, password.c_str(), sizeof(SecuritySettings.WifiKey));
    safe_strncpy(SecuritySettings.WifiSSID, selectedSSID.c_str(), sizeof(SecuritySettings.WifiSSID));
    Settings.IncludeHiddenSSID(isHiddenNetwork);
    addHtmlError(SaveSettings());
    
    // Apply DHCP configuration by clearing static IP
    setupStaticIPconfig();
    
    WiFiEventData.wifiSetupConnect = true;
    WiFiEventData.wifiConnectAttemptNeeded = true;
    WiFi_AP_Candidates.force_reload();
    
    // Force start adaptive AP system for setup process
    if (!WiFiEventData.firstConnectionFailure.isSet()) {
      WiFiEventData.firstConnectionFailure.setNow();
      addLog(LOG_LEVEL_INFO, F("WiFi Setup: Started adaptive AP timer (2 min backup)"));
    }
    
    currentStage = STAGE_CONNECTING;
    refreshCount = 0;
    connectionStartTime = millis();
    lastConnectionAttempt = millis();
    lastError = "";
    
    // Reset WiFi and start fresh with AP mode first
    addLog(LOG_LEVEL_INFO, F("WiFi Setup: Resetting WiFi for clean start"));
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    delay(500);  // Give time for clean disconnect
    
    // Start with AP mode for user access
    addLog(LOG_LEVEL_INFO, F("WiFi Setup: Starting AP mode first"));
    setWifiMode(WIFI_AP);
    setAP(true);
    delay(1000);  // Allow AP to stabilize
    
    // Now enable STA mode for connection attempt
    addLog(LOG_LEVEL_INFO, F("WiFi Setup: Starting basic setup connection"));
    setWifiMode(WIFI_AP_STA);
    addLog(LOG_LEVEL_INFO, F("WiFi Setup: STA+AP mode enabled with adaptive backup"));
    AttemptWiFiConnect();  } else if (showAdvanced) {
    // Go to advanced settings
    advancedSettings.password = password;
    advancedSettings.emptyPassword = isFormItemChecked(F("emptypass"));
    currentStage = STAGE_ADVANCED;
    lastError = "";
    
  } else if (saveAdvanced) {
    // Save advanced settings and show confirmation
    // Static IP mode - radio button ve hidden field'dan al
    String ipMode = webArg(F("ipmode"));  // radio button değeri
    String useStaticHidden = webArg(F("usestaticip"));  // hidden field değeri
    advancedSettings.useStaticIP = (ipMode == F("static")) || (useStaticHidden == F("1"));
    
    advancedSettings.staticIP = webArg(F("staticip"));
    advancedSettings.subnet = webArg(F("subnet"));
    advancedSettings.gateway = webArg(F("gateway"));
    advancedSettings.dns1 = webArg(F("dns1"));
    advancedSettings.dns2 = webArg(F("dns2"));
    
    // Debug: Form verilerini logla
    String debugMsg = String(F("WiFi Setup: Form data received - ")) + 
                     F("ipMode: ") + ipMode + 
                     F(" useStaticHidden: ") + useStaticHidden + 
                     F(" useStatic: ") + (advancedSettings.useStaticIP ? F("YES") : F("NO")) + 
                     F(" IP: ") + advancedSettings.staticIP + 
                     F(" Gateway: ") + advancedSettings.gateway + 
                     F(" Subnet: ") + advancedSettings.subnet + 
                     F(" DNS: ") + advancedSettings.dns1;
    addLog(LOG_LEVEL_INFO, debugMsg);
    
    // Enhanced validation with detailed error messages
    String validationError = "";
    if (advancedSettings.useStaticIP) {
      if (advancedSettings.staticIP.isEmpty()) {
        validationError = "IP adresi gerekli - Statik IP seçildiğinde IP adresi boş olamaz";
      } else if (!validateIPAddress(advancedSettings.staticIP)) {
        validationError = "Geçersiz IP adresi formatı - Doğru format: 192.168.1.100";
      } else if (advancedSettings.subnet.isEmpty()) {
        validationError = "Subnet mask gerekli - Genellikle 255.255.255.0 kullanılır";
      } else if (!isValidSubnet(advancedSettings.subnet)) {
        validationError = "Geçersiz subnet mask - Geçerli değerler: 255.255.255.0, 255.255.0.0, vs.";
      } else if (advancedSettings.gateway.isEmpty()) {
        validationError = "Gateway adresi gerekli - Genellikle router IP'si (ör: 192.168.1.1)";
      } else if (!validateIPAddress(advancedSettings.gateway)) {
        validationError = "Geçersiz gateway adresi - Router IP adresini kontrol edin";
      } else if (!advancedSettings.dns1.isEmpty() && !validateIPAddress(advancedSettings.dns1)) {
        validationError = "Geçersiz birincil DNS adresi - Örnek: 8.8.8.8";
      } else if (!advancedSettings.dns2.isEmpty() && !validateIPAddress(advancedSettings.dns2)) {
        validationError = "Geçersiz ikincil DNS adresi - Örnek: 8.8.4.4";
      }
      
      // Additional logical validations
      if (validationError.isEmpty()) {
        // Check if IP and gateway are in same network
        String ipNet = advancedSettings.staticIP.substring(0, advancedSettings.staticIP.lastIndexOf('.'));
        String gwNet = advancedSettings.gateway.substring(0, advancedSettings.gateway.lastIndexOf('.'));
        if (ipNet != gwNet && advancedSettings.subnet == "255.255.255.0") {
          validationError = "IP ve Gateway aynı ağda değil - IP: " + ipNet + ".x, Gateway: " + gwNet + ".x";
        }
        
        // Check if IP is not the same as gateway
        if (advancedSettings.staticIP == advancedSettings.gateway) {
          validationError = "IP adresi ve Gateway adresi aynı olamaz - Farklı adresler kullanın";
        }
      }
    }
    
    if (!validationError.isEmpty()) {
      advancedSettings.errorMessage = validationError;
      currentStage = STAGE_ADVANCED;
      lastError = "";
    } else {
      advancedSettings.errorMessage = "";
      
      // ✅ HEMEN SETTINGS'E KAYDET - Config sayfasında görünmesi için
      if (advancedSettings.useStaticIP) {
        // IP ayarlarını Settings'e kaydet (ConfigPage ile aynı yöntem)
        if (!advancedSettings.staticIP.isEmpty()) {
          str2ip(advancedSettings.staticIP, Settings.IP);
        }
        if (!advancedSettings.subnet.isEmpty()) {
          str2ip(advancedSettings.subnet, Settings.Subnet);
        }
        if (!advancedSettings.gateway.isEmpty()) {
          str2ip(advancedSettings.gateway, Settings.Gateway);
        }
        if (!advancedSettings.dns1.isEmpty()) {
          str2ip(advancedSettings.dns1, Settings.DNS);
        }
        
        // Debug: Kaydedilen IP değerlerini logla
        String logMsg = String(F("WiFi Setup: IP Settings saved - ")) + 
                       F("IP: ") + formatIP(Settings.IP) + 
                       F(" Gateway: ") + formatIP(Settings.Gateway) + 
                       F(" Subnet: ") + formatIP(Settings.Subnet) + 
                       F(" DNS: ") + formatIP(Settings.DNS);
        addLog(LOG_LEVEL_INFO, logMsg);
        
        // Settings'i kaydet
        SaveSettings();
      } else {
        // DHCP seçildiyse static IP'yi temizle
        for (byte i = 0; i < 4; ++i) {
          Settings.IP[i] = 0;
          Settings.Gateway[i] = 0;
          Settings.Subnet[i] = 0;
          Settings.DNS[i] = 0;
        }
        SaveSettings();
        addLog(LOG_LEVEL_INFO, F("WiFi Setup: DHCP mode selected - Static IP cleared"));
      }
      
      currentStage = STAGE_CONFIRM;
      lastError = "";
    }
    
  } else if (confirmConnection) {
    // User confirmed - proceed with connection
    enteredPassword = advancedSettings.password;
    // Save WiFi credentials
    safe_strncpy(SecuritySettings.WifiKey, advancedSettings.password.c_str(), sizeof(SecuritySettings.WifiKey));
    safe_strncpy(SecuritySettings.WifiSSID, selectedSSID.c_str(), sizeof(SecuritySettings.WifiSSID));
    
    // Apply advanced network settings if enabled
    if (advancedSettings.useStaticIP) {
      // IP ayarları zaten saveAdvanced durumunda kaydedildi
      // Sadece setupStaticIPconfig() uygula
      String logMsg = String(F("WiFi Setup: Applying pre-saved static IP config - ")) + advancedSettings.staticIP;
      addLog(LOG_LEVEL_INFO, logMsg);
      
      // Apply static IP configuration using ESPEasy's method
      setupStaticIPconfig();
    } else {
      // Clear static IP settings when DHCP is selected
      for (int i = 0; i < 4; i++) {
        Settings.IP[i] = 0;
        Settings.Gateway[i] = 0;
        Settings.Subnet[i] = 0;
        Settings.DNS[i] = 0;
      }
      addLog(LOG_LEVEL_INFO, F("WiFi Setup: DHCP enabled, static IP settings cleared"));
      
      // Apply DHCP configuration by clearing static IP
      setupStaticIPconfig();
    }
    
    Settings.IncludeHiddenSSID(advancedSettings.hiddenSSID);
    addHtmlError(SaveSettings());
    
    WiFiEventData.wifiSetupConnect = true;
    WiFiEventData.wifiConnectAttemptNeeded = true;
    WiFi_AP_Candidates.force_reload();
    
    currentStage = STAGE_CONNECTING;
    refreshCount = 0;
    connectionStartTime = millis();
    lastConnectionAttempt = millis();
    lastError = "";
    
    // Let existing WiFi system handle AP mode - don't interfere
    addLog(LOG_LEVEL_INFO, F("WiFi Setup: Starting connection attempt"));
    AttemptWiFiConnect();  } else if (connectWifi && passwordGiven) {
    // Direct connection (backward compatibility)
    enteredPassword = password;
    
    // Clear static IP settings when using direct connection (defaults to DHCP)
    for (int i = 0; i < 4; i++) {
      Settings.IP[i] = 0;
      Settings.Gateway[i] = 0;
      Settings.Subnet[i] = 0;
      Settings.DNS[i] = 0;
    }
    
    safe_strncpy(SecuritySettings.WifiKey, password.c_str(), sizeof(SecuritySettings.WifiKey));
    safe_strncpy(SecuritySettings.WifiSSID, selectedSSID.c_str(), sizeof(SecuritySettings.WifiSSID));
    
    Settings.IncludeHiddenSSID(isHiddenNetwork);
    addHtmlError(SaveSettings());
    
    // Apply DHCP configuration by clearing static IP
    setupStaticIPconfig();
    
    WiFiEventData.wifiSetupConnect = true;
    WiFiEventData.wifiConnectAttemptNeeded = true;
    WiFi_AP_Candidates.force_reload();
    
    currentStage = STAGE_CONNECTING;
    refreshCount = 0;
    connectionStartTime = millis();
    lastConnectionAttempt = millis();
    lastError = "";
    
    // Let existing WiFi system handle connection and AP mode
    addLog(LOG_LEVEL_INFO, F("WiFi Setup: Starting connection attempt"));
    setWifiMode(WIFI_AP_STA);
    setAP(true);
    addLog(LOG_LEVEL_INFO, F("WiFi Setup: STA+AP mode enabled with adaptive backup"));
    AttemptWiFiConnect();
    
  } else if (goBack) {
    // Handle back button navigation
    switch (currentStage) {
      case STAGE_PASSWORD:
        currentStage = STAGE_SELECT;
        break;
      case STAGE_ADVANCED:
        // Form verilerini al ve kaydet
        advancedSettings.useStaticIP = hasArg(F("usestaticip")) && (webArg(F("usestaticip")) == F("1"));
        if (advancedSettings.useStaticIP) {
          advancedSettings.staticIP = webArg(F("staticip"));
          advancedSettings.subnet = webArg(F("subnet"));
          advancedSettings.gateway = webArg(F("gateway"));
          advancedSettings.dns1 = webArg(F("dns1"));
          
          String logMsg = String(F("WiFi Setup: Advanced IP settings saved - Static: ")) + 
                         (advancedSettings.useStaticIP ? F("YES") : F("NO")) + 
                         F(" IP: ") + advancedSettings.staticIP;
          addLog(LOG_LEVEL_INFO, logMsg);
        }
        currentStage = STAGE_PASSWORD;
        break;
      case STAGE_CONFIRM:
        currentStage = STAGE_ADVANCED;
        break;
      case STAGE_FAILED:
        currentStage = STAGE_PASSWORD;
        break;
      default:
        currentStage = STAGE_SCAN;
        break;
    }
    lastError = "";
    refreshCount = 0;
    
  } else if (retry) {
    // Retry işlemi - mevcut aşamaya göre aksiyon al
    if (currentStage == STAGE_CONNECTING) {
      // Bağlantı durumunu kontrol et, aşamayı değiştirme
      addLog(LOG_LEVEL_INFO, F("WiFi Setup: Manual connection status check requested"));
    } else {
      // Diğer aşamalarda şifre ekranına dön
      currentStage = STAGE_PASSWORD;
    }
    lastError = "";
    refreshCount = 0;
  }

  // Auto stage progression and error detection
  if (currentStage == STAGE_SCANNING) {
    const int scanResult = std::distance(WiFi_AP_Candidates.scanned_begin(), WiFi_AP_Candidates.scanned_end());
    if (scanResult > 0) {
      currentStage = STAGE_SELECT;
    }
  } else if (currentStage == STAGE_CONNECTING) {
    // Enhanced connection detection - WiFi durumunu detaylı kontrol et
    bool wifiConnected = (WiFi.status() == WL_CONNECTED);
    bool networkConnected = NetworkConnected();
    bool hasValidIP = (WiFi.localIP() != IPAddress(0, 0, 0, 0));
    
    // Debug: Bağlantı durumunu logla
    String debugMsg = String(F("WiFi Setup: Connection check - ")) + 
                     F("connected: ") + (connected ? F("YES") : F("NO")) + 
                     F(" NetworkConnected(): ") + (networkConnected ? F("YES") : F("NO")) + 
                     F(" wifiConnected: ") + (wifiConnected ? F("YES") : F("NO")) + 
                     F(" hasValidIP: ") + (hasValidIP ? F("YES") : F("NO")) + 
                     F(" IP: ") + WiFi.localIP().toString() + 
                     F(" refreshCount: ") + String(refreshCount);
    addLog(LOG_LEVEL_INFO, debugMsg);
    
    // SUCCESS koşulu - IP varsa başarılı say (WiFi disconnect olsa bile)
    // Çünkü statik IP çalışıyor ve erişilebilir
    if (hasValidIP && (WiFi.localIP().toString() != "0.0.0.0")) {
      currentStage = STAGE_SUCCESS;
      WiFiEventData.timerAPoff.setMillisFromNow(120000);
      lastError = "";
      wifiRetryCount = 0;
      failsafeMode = false;
      
      String successMsg = String(F("WiFi Setup: Connection successful - IP: ")) + WiFi.localIP().toString() + 
                         String(F(" (Note: WiFi status may show disconnected but IP is accessible)"));
      addLog(LOG_LEVEL_INFO, successMsg);
    }
    // No timeout check needed since AP is already enabled from start
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
  addHtml(F(".form-input[type='text'],.form-input[type='password']{font-family:monospace}"));
  addHtml(F(".form-input:invalid{border-color:#ff416c;background:rgba(255,65,108,0.1)}"));
  addHtml(F(".form-input:valid{border-color:#38ef7d}"));
  addHtml(F(".form-input.error{border-color:#ff416c !important;background:rgba(255,65,108,0.1)}"));
  addHtml(F(".form-input.valid{border-color:#38ef7d !important;background:rgba(56,239,125,0.1)}"));
  addHtml(F(".toggle-section{transition:all 0.3s ease;overflow:hidden}"));
  addHtml(F(".confirm-warning{background:linear-gradient(135deg,rgba(255,193,7,0.2),rgba(255,133,27,0.2));border-left:4px solid #ffc107;padding:12px;margin:12px 0;border-radius:4px}"));
  addHtml(F(".advanced-dropdown{background:#f8f9fa;border-radius:12px;margin:16px 0;overflow:hidden;border:2px solid #e9ecef;transition:all 0.3s ease}"));
  addHtml(F(".dropdown-toggle{padding:16px 20px;cursor:pointer;display:flex;justify-content:space-between;align-items:center;font-weight:600;color:#4facfe;background:linear-gradient(135deg,rgba(79,172,254,0.1),rgba(0,242,254,0.1));transition:all 0.3s ease}"));
  addHtml(F(".dropdown-toggle:hover{background:linear-gradient(135deg,rgba(79,172,254,0.2),rgba(0,242,254,0.2))}"));
  addHtml(F(".dropdown-arrow{font-size:14px;transition:transform 0.3s ease;color:#6c757d}"));
  addHtml(F(".dropdown-panel{display:none;padding:20px;background:white;border-top:1px solid #e9ecef}"));
  addHtml(F(".radio-group{display:flex;gap:16px;margin-bottom:20px}"));
  addHtml(F(".radio-option{display:flex;align-items:center;cursor:pointer;padding:12px 16px;border:2px solid #e9ecef;border-radius:8px;transition:all 0.3s ease;font-weight:500}"));
  addHtml(F(".radio-option:hover{border-color:#4facfe;background:rgba(79,172,254,0.1)}"));
  addHtml(F(".radio-option input[type='radio']{display:none}"));
  addHtml(F(".radio-option input[type='radio']:checked + .radio-custom{background:#4facfe;border-color:#4facfe}"));
  addHtml(F(".radio-option input[type='radio']:checked + .radio-custom::after{opacity:1}"));
  addHtml(F(".radio-custom{width:18px;height:18px;border:2px solid #dee2e6;border-radius:50%;margin-right:8px;position:relative;transition:all 0.3s ease}"));
  addHtml(F(".radio-custom::after{content:'';width:8px;height:8px;background:white;border-radius:50%;position:absolute;top:50%;left:50%;transform:translate(-50%,-50%);opacity:0;transition:opacity 0.3s ease}"));
  addHtml(F(".input-with-example{position:relative}"));
  addHtml(F(".input-example{position:absolute;right:12px;top:50%;transform:translateY(-50%);color:#6c757d;font-size:12px;font-style:italic;pointer-events:none;background:white;padding:0 4px}"));
  addHtml(F(".ip-input{padding-right:140px}"));
  addHtml(F(".static-inputs{display:block;transition:all 0.3s ease}"));
  addHtml(F(".static-inputs.hidden{display:none !important}"));
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
  int maxStages = failsafeMode ? 9 : 7;  // Increased for new stages
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
    case STAGE_ADVANCED:
      renderAdvancedStage(selectedSSID);
      break;
    case STAGE_CONFIRM:
      renderConfirmStage(advancedSettings);
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

  // Auto-refresh removed - user can manually refresh if needed
  // Manual refresh button will be provided in each stage

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
  addHtml(F("<div class='step-subtitle'>WiFi ağları aranıyor</div>"));
  
  addHtml(F("<div class='loading'>"));
  addHtml(F("<div class='spinner'></div>"));
  addHtml(F("<p style='color: #6c757d;'>Çevredeki ağlar taranıyor</p>"));
  addHtml(F("<div id='scan-status' style='margin-top: 12px; color: #4facfe; font-weight: 500;'>Tarama başlatılıyor...</div>"));
  addHtml(F("</div>"));
  
  // Pure JavaScript (no CDN/internet needed)
  addHtml(F("<script>"));
  addHtml(F("var scanCheckInterval;"));
  addHtml(F("var checkCount = 0;"));
  
  addHtml(F("function updateScanMessage(text) {"));
  addHtml(F("  var statusEl = document.getElementById('scan-status');"));
  addHtml(F("  if(statusEl) statusEl.textContent = text;"));
  addHtml(F("}"));
  
  addHtml(F("function checkScanStatus() {"));
  addHtml(F("  checkCount++;"));
  addHtml(F("  var xhr = new XMLHttpRequest();"));
  addHtml(F("  xhr.open('GET', '/setup?status=scan', true);"));
  addHtml(F("  xhr.onreadystatechange = function() {"));
  addHtml(F("    if(xhr.readyState === 4 && xhr.status === 200) {"));
  addHtml(F("      try {"));
  addHtml(F("        var data = JSON.parse(xhr.responseText);"));
  addHtml(F("        updateScanMessage(data.message);"));
  addHtml(F("        if(data.complete) {"));
  addHtml(F("          clearInterval(scanCheckInterval);"));
  addHtml(F("          updateScanMessage('✅ Tarama tamamlandı - yönlendiriliyor...');"));
  addHtml(F("          setTimeout(function() { window.location.href='/setup'; }, 1500);"));
  addHtml(F("        }"));
  addHtml(F("      } catch(e) { updateScanMessage('Tarama devam ediyor...'); }"));
  addHtml(F("    }"));
  addHtml(F("  };"));
  addHtml(F("  xhr.send();"));
  addHtml(F("  if(checkCount > 15) { clearInterval(scanCheckInterval); updateScanMessage('⏱️ Zaman aşımı - sayfayı yenileyin'); }"));
  addHtml(F("}"));
  
  addHtml(F("setTimeout(function() {"));
  addHtml(F("  updateScanMessage('🔍 Tarama başlatılıyor...');"));
  addHtml(F("  scanCheckInterval = setInterval(checkScanStatus, 2000);"));
  addHtml(F("}, 1000);"));
  addHtml(F("</script>"));
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
  addHtml(F("<div class='step-title'>WiFi Bilgileri</div>"));
  addHtml(F("<div class='step-subtitle'>Ağ şifresi girin ve bağlantı türünü seçin</div>"));

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
  addHtml(F("<label class='form-label'>WiFi Şifresi</label>"));
  addHtml(F("<input type='password' name='password' class='form-input' placeholder='Ağ şifresini girin'>"));
  addHtml(F("</div>"));

  addHtml(F("<div style='margin-bottom: 24px;'>"));
  addHtml(F("<label style='display: flex; align-items: center; color: #6c757d; cursor: pointer;'>"));
  addHtml(F("<input type='checkbox' name='emptypass' style='margin-right: 8px;'>"));
  addHtml(F("🔓 Açık ağ (şifre yok)"));
  addHtml(F("</label>"));
  addHtml(F("</div>"));

  // Connection type selection
  addHtml(F("<div style='display: grid; grid-template-columns: 1fr 1fr; gap: 12px; margin-bottom: 16px;'>"));
  
  addHtml(F("<button type='submit' name='enterpassword' value='1' class='btn btn-success'>"));
  addHtml(F("🚀 Hızlı Bağlan"));
  addHtml(F("</button>"));
  
  addHtml(F("<button type='submit' name='advanced' value='1' class='btn btn-outline'>"));
  addHtml(F("⚙️ Gelişmiş"));
  addHtml(F("</button>"));
  
  addHtml(F("</div>"));

  addHtml(F("<button type='submit' name='back' value='1' class='btn btn-outline-danger'>"));
  addHtml(F("← Geri"));
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
  
  // Real-time connection status
  addHtml(F("<div id='connection-status' style='margin-top: 12px; padding: 12px; background: #f8f9fa; border-radius: 8px; text-align: center;'>"));
  addHtml(F("<div style='color: #4facfe; font-weight: 500;'>Bağlantı durumu kontrol ediliyor...</div>"));
  addHtml(F("<div id='ip-info' style='color: #6c757d; font-size: 12px; margin-top: 4px;'></div>"));
  addHtml(F("</div>"));
  addHtml(F("</div>"));
  
  // Progress bar
  addHtml(F("<div class='progress-bar' style='margin: 20px 0;'>"));
  addHtml(F("<div id='progress-fill' class='progress-fill' style='width: 0%; transition: width 0.5s ease;'></div>"));
  addHtml(F("</div>"));
  
  // Action buttons
  addHtml(F("<div style='display: flex; gap: 8px; margin-top: 16px;'>"));
  addHtml(F("<button type='button' onclick='checkConnectionNow()' class='btn btn-primary' style='flex: 1;'>"));
  addHtml(F("🔄 Hemen Kontrol Et"));
  addHtml(F("</button>"));
  addHtml(F("<button type='submit' name='back' value='1' class='btn btn-outline-danger' style='flex: 1;'>"));
  addHtml(F("← Şifreyi Değiştir"));
  addHtml(F("</button>"));
  addHtml(F("</div>"));
  
  // Pure JavaScript (internet-free)
  addHtml(F("<script>"));
  addHtml(F("var connectionCheckInterval;"));
  addHtml(F("var checkCount = 0;"));
  addHtml(F("var maxChecks = 20;"));
  
  addHtml(F("function updateProgress(percent) {"));
  addHtml(F("  var progressEl = document.getElementById('progress-fill');"));
  addHtml(F("  if(progressEl) progressEl.style.width = percent + '%';"));
  addHtml(F("}"));
  
  addHtml(F("function updateStatus(html) {"));
  addHtml(F("  var statusEl = document.getElementById('connection-status');"));
  addHtml(F("  if(statusEl) statusEl.innerHTML = html;"));
  addHtml(F("}"));
  
  addHtml(F("function updateIP(text) {"));
  addHtml(F("  var ipEl = document.getElementById('ip-info');"));
  addHtml(F("  if(ipEl) ipEl.textContent = text;"));
  addHtml(F("}"));
  
  addHtml(F("function checkConnectionStatus() {"));
  addHtml(F("  checkCount++;"));
  addHtml(F("  var progress = Math.min((checkCount / maxChecks) * 100, 95);"));
  addHtml(F("  updateProgress(progress);"));
  addHtml(F("  "));
  addHtml(F("  // Dynamic endpoint detection - get real IP from ESP32"));
  addHtml(F("  var endpoints = ["));
  addHtml(F("    window.location.origin + '/setup?status=connect',"));
  addHtml(F("    'http://192.168.4.1/setup?status=connect'"));
  
  // ESP32'den gerçek IP'yi JavaScript'e geç
  if (WiFi.status() == WL_CONNECTED && WiFi.localIP() != IPAddress(0,0,0,0)) {
    addHtml(F(","));
    addHtml(F("    'http://"));
    addHtml(WiFi.localIP().toString());
    addHtml(F("/setup?status=connect'"));
  }
  
  // Settings.IP varsa (statik IP) onu da dene
  if (Settings.IP[0] != 0) {
    addHtml(F(","));
    addHtml(F("    'http://"));
    addHtml(formatIP(Settings.IP));
    addHtml(F("/setup?status=connect'"));
  }
  
  addHtml(F("  ];"));
  addHtml(F("  "));
  addHtml(F("  console.log('📡 AJAX Endpoints:', endpoints);"));
  addHtml(F("  "));
  addHtml(F("  function tryEndpoint(index) {"));
  addHtml(F("    if(index >= endpoints.length) {"));
  addHtml(F("      updateStatus('<div style=\"color:#ffc107;\">⚠️ Durum kontrol edilemiyor - IP değişmiş olabilir</div>');"));
  addHtml(F("      return;"));
  addHtml(F("    }"));
  addHtml(F("    "));
  addHtml(F("    var xhr = new XMLHttpRequest();"));
  addHtml(F("    xhr.timeout = 2000;"));
  addHtml(F("    xhr.open('GET', endpoints[index], true);"));
  addHtml(F("    xhr.onreadystatechange = function() {"));
  addHtml(F("      if(xhr.readyState === 4) {"));
  addHtml(F("        if(xhr.status === 200) {"));
  addHtml(F("          try {"));
  addHtml(F("            var data = JSON.parse(xhr.responseText);"));
  addHtml(F("            if(data.success) {"));
  addHtml(F("              updateStatus('<div style=\"color:#28a745;font-weight:600;\">✅ ' + data.message + '</div>');"));
  addHtml(F("              updateIP('IP: ' + data.ip);"));
  addHtml(F("              updateProgress(100);"));
  addHtml(F("              clearInterval(connectionCheckInterval);"));
  addHtml(F("              setTimeout(function() { "));
  addHtml(F("                var newUrl = 'http://' + data.ip;"));
  addHtml(F("                updateStatus('<div style=\"color:#4facfe;\">🔄 Yeni IP adresine yönlendiriliyor: ' + data.ip + '</div>');"));
  addHtml(F("                setTimeout(function() { window.location.href = newUrl; }, 3000);"));
  addHtml(F("              }, 1000);"));
  addHtml(F("            } else if(data.failed) {"));
  addHtml(F("              updateStatus('<div style=\"color:#dc3545;font-weight:600;\">❌ ' + data.message + '</div>');"));
  addHtml(F("              clearInterval(connectionCheckInterval);"));
  addHtml(F("            } else {"));
  addHtml(F("              updateStatus('<div style=\"color:#4facfe;font-weight:500;\">🔄 ' + data.message + '</div>');"));
  addHtml(F("              updateIP(data.details || '');"));
  addHtml(F("            }"));
  addHtml(F("          } catch(e) {"));
  addHtml(F("            tryEndpoint(index + 1);"));
  addHtml(F("          }"));
  addHtml(F("        } else {"));
  addHtml(F("          tryEndpoint(index + 1);"));
  addHtml(F("        }"));
  addHtml(F("      }"));
  addHtml(F("    };"));
  addHtml(F("    xhr.onerror = function() { tryEndpoint(index + 1); };"));
  addHtml(F("    xhr.ontimeout = function() { tryEndpoint(index + 1); };"));
  addHtml(F("    xhr.send();"));
  addHtml(F("  }"));
  addHtml(F("  "));
  addHtml(F("  tryEndpoint(0);"));
  addHtml(F("  "));
  addHtml(F("  if(checkCount >= maxChecks) {"));
  addHtml(F("    clearInterval(connectionCheckInterval);"));
  addHtml(F("    updateStatus('<div style=\"color:#ffc107;font-weight:600;\">⏱️ Zaman Aşımı - Manuel kontrol edin</div>');"));
  addHtml(F("    updateIP('192.168.4.1 (AP) veya yeni IP adresini deneyin');"));
  addHtml(F("  }"));
  addHtml(F("}"));
  
  addHtml(F("function checkConnectionNow() {"));
  addHtml(F("  checkConnectionStatus();"));
  addHtml(F("}"));
  
  addHtml(F("setTimeout(function() {"));
  addHtml(F("  connectionCheckInterval = setInterval(checkConnectionStatus, 3000);"));
  addHtml(F("  checkConnectionStatus();"));
  addHtml(F("}, 2000);"));
  addHtml(F("</script>"));
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

  // Action buttons based on retry count
  if (wifiRetryCount >= 5) {
    // Max retries reached - force back to password
    addHtml(F("<div style='background: #f8d7da; border: 1px solid #f5c6cb; border-radius: 6px; padding: 12px; margin-bottom: 16px; text-align: center;'>"));
    addHtml(F("<strong style='color: #721c24;'>🚫 Maximum 5 deneme tamamlandı!</strong><br>"));
    addHtml(F("<small style='color: #721c24;'>Şifre yanlış olabilir. Lütfen şifreyi kontrol edin.</small>"));
    addHtml(F("</div>"));
    
    addHtml(F("<div style='display: flex; gap: 8px; margin-top: 20px;'>"));
    addHtml(F("<button type='submit' name='back' value='1' class='btn btn-warning' style='flex: 1;'>"));
    addHtml(F("� Şifreyi Değiştir"));
    addHtml(F("</button>"));
    addHtml(F("<button type='submit' name='startscan' value='1' class='btn btn-outline-secondary' style='flex: 1;'>"));
    addHtml(F("🔍 Yeni Ağ Seç"));
    addHtml(F("</button>"));
    addHtml(F("</div>"));
  } else {
    // Still have retries left
    addHtml(F("<div style='display: flex; gap: 8px; margin-top: 20px;'>"));
    addHtml(F("<button type='submit' name='retry' value='1' class='btn btn-primary' style='flex: 1;'>"));
    addHtml(F("🔄 Tekrar Dene ("));
    addHtml(String(5 - wifiRetryCount));
    addHtml(F(" kalan)"));
    addHtml(F("</button>"));
    addHtml(F("<button type='submit' name='back' value='1' class='btn btn-outline-warning' style='flex: 1;'>"));
    addHtml(F("🔑 Şifreyi Değiştir"));
    addHtml(F("</button>"));
    addHtml(F("</div>"));
  }
  
  // Emergency failsafe button
  addHtml(F("<button type='submit' name='failsafe' value='1' class='btn btn-danger' style='width: 100%; margin-top: 12px;' onclick='return confirm(\"Güvenli mod aktif edilecek ve WiFi ayarları sıfırlanacak. Emin misiniz?\")'>"));
  addHtml(F("🔒 Güvenli Modu Aktifleştir"));
  addHtml(F("</button>"));
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

void renderAdvancedStage(const String& ssid) {
  addHtml(F("<div class='step-title'>Gelişmiş Ayarlar</div>"));
  addHtml(F("<div class='step-subtitle'>Statik IP ve DNS ayarlarını yapılandırın</div>"));

  addHtml(F("<div style='background: #f8f9fa; border-radius: 8px; padding: 16px; margin-bottom: 24px; text-align: center;'>"));
  addHtml(F("<div style='color: #4facfe; font-size: 20px; margin-bottom: 8px;'>⚙️</div>"));
  addHtml(F("<div style='font-weight: 600; color: #2c3e50;'>"));
  addHtml(ssid == F("#Hidden#") ? F("Gizli Ağ") : ssid);
  addHtml(F("</div>"));
  addHtml(F("</div>"));

  // Hidden fields to preserve data
  addHtml(F("<input type='hidden' name='password' value='"));
  addHtml(advancedSettings.password);
  addHtml(F("'>"));
  
  if (advancedSettings.emptyPassword) {
    addHtml(F("<input type='hidden' name='emptypass' value='1'>"));
  }

  // Error display area
  if (!advancedSettings.errorMessage.isEmpty()) {
    addHtml(F("<div class='error-message'>"));
    addHtml(F("❌ "));
    addHtml(advancedSettings.errorMessage);
    addHtml(F("<br><small>Lütfen ayarları kontrol edin ve tekrar deneyin.</small>"));
    addHtml(F("</div>"));
  }

  // Dropdown toggle for advanced settings
  addHtml(F("<div class='advanced-dropdown'>"));
  addHtml(F("<div class='dropdown-toggle' onclick='toggleAdvancedSettings()'>"));
  addHtml(F("<span style='font-weight: 600; color: #4facfe;'>🌐 Statik IP Ayarları</span>"));
  addHtml(F("<span class='dropdown-arrow' id='dropdown-arrow'>▼</span>"));
  addHtml(F("</div>"));

  // Static IP settings (initially collapsed)
  addHtml(F("<div id='advanced-settings-panel' class='dropdown-panel'>"));
  
  // IP Configuration mode toggle with hidden checkbox for ESPEasy compatibility
  addHtml(F("<input type='hidden' name='usestaticip' id='usestaticip' value='"));
  addHtml(advancedSettings.useStaticIP ? F("1") : F("0"));
  addHtml(F("'>"));
  
  addHtml(F("<div class='form-group'>"));
  addHtml(F("<div class='radio-group'>"));
  
  addHtml(F("<label class='radio-option'>"));
  addHtml(F("<input type='radio' name='ipmode' value='dhcp' onchange='toggleIPMode(this.value)'"));
  if (!advancedSettings.useStaticIP) addHtml(F(" checked"));
  addHtml(F(">"));
  addHtml(F("<span class='radio-custom'></span>"));
  addHtml(F("📡 Otomatik IP (DHCP)"));
  addHtml(F("</label>"));

  addHtml(F("<label class='radio-option'>"));
  addHtml(F("<input type='radio' name='ipmode' value='static' onchange='toggleIPMode(this.value)'"));
  if (advancedSettings.useStaticIP) addHtml(F(" checked"));
  addHtml(F(">"));
  addHtml(F("<span class='radio-custom'></span>"));
  addHtml(F("🔧 Manuel IP (Statik)"));
  addHtml(F("</label>"));
  
  addHtml(F("</div>"));
  addHtml(F("</div>"));



  // Static IP input fields (initially hidden if DHCP selected)
  addHtml(F("<div id='static-ip-inputs' class='static-inputs"));
  if (!advancedSettings.useStaticIP) addHtml(F(" hidden"));
  addHtml(F("'>"));
  
  addHtml(F("<div class='form-group'>"));
  addHtml(F("<label class='form-label'>IP Adresi</label>"));
  addHtml(F("<div class='input-with-example'>"));
  addHtml(F("<input type='text' id='ip' name='staticip' class='form-input ip-input' placeholder='' pattern='\\d{1,3}\\.\\d{1,3}\\.\\d{1,3}\\.\\d{1,3}' oninput='validateIP(this)' value='"));
  addHtml(advancedSettings.staticIP);
  addHtml(F("'>"));
  addHtml(F("<span class='input-example'>// örn: 192.168.1.100</span>"));
  addHtml(F("</div>"));
  addHtml(F("</div>"));

  addHtml(F("<div class='form-group'>"));
  addHtml(F("<label class='form-label'>Subnet Mask</label>"));
  addHtml(F("<div class='input-with-example'>"));
  addHtml(F("<input type='text' id='subnet' name='subnet' class='form-input ip-input' placeholder='' pattern='\\d{1,3}\\.\\d{1,3}\\.\\d{1,3}\\.\\d{1,3}' oninput='validateSubnet(this)' value='"));
  addHtml(advancedSettings.subnet.isEmpty() ? F("255.255.255.0") : advancedSettings.subnet);
  addHtml(F("'>"));
  addHtml(F("<span class='input-example'>// örn: 255.255.255.0</span>"));
  addHtml(F("</div>"));
  addHtml(F("</div>"));

  addHtml(F("<div class='form-group'>"));
  addHtml(F("<label class='form-label'>Gateway (Router IP)</label>"));
  addHtml(F("<div class='input-with-example'>"));
  addHtml(F("<input type='text' id='gateway' name='gateway' class='form-input ip-input' placeholder='' pattern='\\d{1,3}\\.\\d{1,3}\\.\\d{1,3}\\.\\d{1,3}' oninput='validateGateway(this)' value='"));
  addHtml(advancedSettings.gateway);
  addHtml(F("'>"));
  addHtml(F("<span class='input-example'>// örn: 192.168.1.1</span>"));
  addHtml(F("</div>"));
  addHtml(F("</div>"));

  addHtml(F("<div class='form-group'>"));
  addHtml(F("<label class='form-label'>DNS 1 (Birincil DNS)</label>"));
  addHtml(F("<div class='input-with-example'>"));
  addHtml(F("<input type='text' id='dns' name='dns1' class='form-input ip-input' placeholder='' pattern='\\d{1,3}\\.\\d{1,3}\\.\\d{1,3}\\.\\d{1,3}' oninput='validateDNS(this)' value='"));
  addHtml(advancedSettings.dns1.isEmpty() ? F("8.8.8.8") : advancedSettings.dns1);
  addHtml(F("'>"));
  addHtml(F("<span class='input-example'>// örn: 8.8.8.8 (Google)</span>"));
  addHtml(F("</div>"));
  addHtml(F("</div>"));

  addHtml(F("<div class='form-group'>"));
  addHtml(F("<label class='form-label'>DNS 2 (İkincil DNS) - İsteğe bağlı</label>"));
  addHtml(F("<div class='input-with-example'>"));
  addHtml(F("<input type='text' name='dns2' class='form-input ip-input' placeholder='' pattern='\\d{1,3}\\.\\d{1,3}\\.\\d{1,3}\\.\\d{1,3}' oninput='validateDNS(this)' value='"));
  addHtml(advancedSettings.dns2.isEmpty() ? F("8.8.4.4") : advancedSettings.dns2);
  addHtml(F("'>"));
  addHtml(F("<span class='input-example'>// örn: 8.8.4.4 (Google)</span>"));
  addHtml(F("</div>"));
  addHtml(F("</div>"));
  
  addHtml(F("</div>"));
  addHtml(F("</div>")); // End dropdown panel

  // Hidden field for static IP mode
  addHtml(F("<input type='hidden' name='usestaticip' id='usestaticip' value='"));
  addHtml(advancedSettings.useStaticIP ? F("1") : F("0"));
  addHtml(F("'>"));

  // Buttons
  addHtml(F("<div style='display: flex; gap: 8px; margin-top: 24px;'>"));
  addHtml(F("<button type='submit' name='saveadvanced' value='1' class='btn btn-primary' style='flex: 1;'>"));
  addHtml(F("💾 Ayarları Kaydet"));
  addHtml(F("</button>"));
  addHtml(F("<button type='submit' name='back' value='1' class='btn btn-outline-danger' style='flex: 1;'>"));
  addHtml(F("← Geri"));
  addHtml(F("</button>"));
  addHtml(F("</div>"));

  // Enhanced JavaScript for UI interaction and validation
  addHtml(F("<script>"));
  
  addHtml(F("function toggleAdvancedSettings() {"));
  addHtml(F("  const panel = document.getElementById('advanced-settings-panel');"));
  addHtml(F("  const arrow = document.getElementById('dropdown-arrow');"));
  addHtml(F("  if (panel.style.display === 'none' || panel.style.display === '') {"));
  addHtml(F("    panel.style.display = 'block';"));
  addHtml(F("    arrow.innerHTML = '▲';"));
  addHtml(F("    arrow.style.transform = 'rotate(180deg)';"));
  addHtml(F("  } else {"));
  addHtml(F("    panel.style.display = 'none';"));
  addHtml(F("    arrow.innerHTML = '▼';"));
  addHtml(F("    arrow.style.transform = 'rotate(0deg)';"));
  addHtml(F("  }"));
  addHtml(F("}"));

  addHtml(F("function toggleIPMode(mode) {"));
  addHtml(F("  const staticInputs = document.getElementById('static-ip-inputs');"));
  addHtml(F("  const useStaticField = document.getElementById('usestaticip');"));
  addHtml(F("  if (mode === 'static') {"));
  addHtml(F("    staticInputs.classList.remove('hidden');"));
  addHtml(F("    useStaticField.value = '1';"));
  addHtml(F("  } else {"));
  addHtml(F("    staticInputs.classList.add('hidden');"));
  addHtml(F("    useStaticField.value = '0';"));
  addHtml(F("  }"));
  addHtml(F("}"));

  // Sayfa yüklendiğinde static seçiliyse alanları göster
  addHtml(F("window.onload = function() {"));
  addHtml(F("  const staticRadio = document.querySelector('input[value=\"static\"]:checked');"));
  addHtml(F("  if (staticRadio) toggleIPMode('static');"));
  addHtml(F("};"));





  addHtml(F("function validateIP(input) {"));
  addHtml(F("  const value = input.value;"));
  addHtml(F("  const parts = value.split('.');"));
  addHtml(F("  if (parts.length === 4 && parts.every(part => part >= 0 && part <= 255 && part !== '')) {"));
  addHtml(F("    input.classList.remove('error');"));
  addHtml(F("    input.classList.add('valid');"));
  addHtml(F("  } else {"));
  addHtml(F("    input.classList.add('error');"));
  addHtml(F("    input.classList.remove('valid');"));
  addHtml(F("  }"));
  addHtml(F("}"));

  addHtml(F("function validateSubnet(input) {"));
  addHtml(F("  const validSubnets = ['255.255.255.0','255.255.0.0','255.0.0.0','255.255.255.128','255.255.255.192','255.255.255.224','255.255.255.240','255.255.255.248','255.255.255.252'];"));
  addHtml(F("  if (validSubnets.includes(input.value)) {"));
  addHtml(F("    input.classList.remove('error');"));
  addHtml(F("    input.classList.add('valid');"));
  addHtml(F("  } else {"));
  addHtml(F("    input.classList.add('error');"));
  addHtml(F("    input.classList.remove('valid');"));
  addHtml(F("  }"));
  addHtml(F("}"));

  addHtml(F("function validateGateway(input) { validateIP(input); }"));
  addHtml(F("function validateDNS(input) { if(input.value) validateIP(input); }"));





  addHtml(F("</script>"));
}

void renderConfirmStage(const AdvancedWiFiSettings& settings) {
  addHtml(F("<div class='step-title'>Ayarları Onayla</div>"));
  addHtml(F("<div class='step-subtitle'>Bağlantı bilgilerini kontrol edin ve onaylayın</div>"));

  addHtml(F("<div style='background: #f8f9fa; border-radius: 8px; padding: 16px; margin-bottom: 24px;'>"));
  
  addHtml(F("<div style='text-align: center; margin-bottom: 16px;'>"));
  addHtml(F("<div style='color: #28a745; font-size: 24px; margin-bottom: 8px;'>✅</div>"));
  addHtml(F("<div style='font-weight: 600; color: #2c3e50;'>Bağlantı Hazır</div>"));
  addHtml(F("</div>"));

  // Network details
  addHtml(F("<div class='detail-row' style='border-bottom: 1px solid #e9ecef; padding-bottom: 8px; margin-bottom: 8px;'>"));
  addHtml(F("<span class='detail-label'>WiFi Ağı:</span>"));
  addHtml(F("<span class='detail-value'>📶 "));
  addHtml(settings.ssid == F("#Hidden#") ? F("Gizli Ağ") : settings.ssid);
  addHtml(F("</span>"));
  addHtml(F("</div>"));

  addHtml(F("<div class='detail-row' style='margin-bottom: 8px;'>"));
  addHtml(F("<span class='detail-label'>Şifre:</span>"));
  addHtml(F("<span class='detail-value'>"));
  if (settings.emptyPassword) {
    addHtml(F("🔓 Açık Ağ"));
  } else {
    addHtml(F("🔒 "));
    for (int i = 0; i < settings.password.length(); i++) {
      addHtml(F("•"));
    }
  }
  addHtml(F("</span>"));
  addHtml(F("</div>"));

  if (settings.useStaticIP) {
    addHtml(F("<div style='border-top: 1px solid #e9ecef; padding-top: 12px; margin-top: 12px;'>"));
    addHtml(F("<div style='font-weight: 600; color: #4facfe; margin-bottom: 8px;'>🌐 Statik IP Ayarları</div>"));
    
    addHtml(F("<div class='detail-row'>"));
    addHtml(F("<span class='detail-label'>IP Adresi:</span>"));
    addHtml(F("<span class='detail-value'>"));
    addHtml(settings.staticIP);
    addHtml(F("</span>"));
    addHtml(F("</div>"));
    
    addHtml(F("<div class='detail-row'>"));
    addHtml(F("<span class='detail-label'>Subnet:</span>"));
    addHtml(F("<span class='detail-value'>"));
    addHtml(settings.subnet);
    addHtml(F("</span>"));
    addHtml(F("</div>"));
    
    addHtml(F("<div class='detail-row'>"));
    addHtml(F("<span class='detail-label'>Gateway:</span>"));
    addHtml(F("<span class='detail-value'>"));
    addHtml(settings.gateway);
    addHtml(F("</span>"));
    addHtml(F("</div>"));
    
    addHtml(F("<div class='detail-row'>"));
    addHtml(F("<span class='detail-label'>DNS:</span>"));
    addHtml(F("<span class='detail-value'>"));
    addHtml(settings.dns1);
    if (!settings.dns2.isEmpty()) {
      addHtml(F(", "));
      addHtml(settings.dns2);
    }
    addHtml(F("</span>"));
    addHtml(F("</div>"));
    
    addHtml(F("</div>"));
  } else {
    addHtml(F("<div style='border-top: 1px solid #e9ecef; padding-top: 12px; margin-top: 12px;'>"));
    addHtml(F("<div style='color: #6c757d; font-style: italic;'>📡 Otomatik IP (DHCP)</div>"));
    addHtml(F("</div>"));
  }
  
  addHtml(F("</div>"));

  // Confirmation warning
  addHtml(F("<div style='background: linear-gradient(135deg, rgba(255,193,7,0.1), rgba(255,133,27,0.1)); border: 2px solid #ffc107; border-radius: 8px; padding: 16px; margin: 16px 0; text-align: center;'>"));
  addHtml(F("⚠️ <strong>Bu ayarlarla WiFi ağına bağlanmak istediğinizden emin misiniz?</strong><br>"));
  addHtml(F("<small style='color: #856404;'>Hatalı ayarlar durumunda cihaza erişim zorlaşabilir.</small>"));
  addHtml(F("</div>"));

  // Buttons
  addHtml(F("<div style='display: flex; gap: 8px; margin-top: 24px;'>"));
  addHtml(F("<button type='submit' name='confirmconnect' value='1' class='btn btn-success' style='flex: 1;'>"));
  addHtml(F("✅ Evet, Bağlan"));
  addHtml(F("</button>"));
  addHtml(F("<button type='submit' name='back' value='1' class='btn btn-outline-danger' style='flex: 1;'>"));
  addHtml(F("← Düzenle"));
  addHtml(F("</button>"));
  addHtml(F("</div>"));
}

#endif // WEBSERVER_SETUP