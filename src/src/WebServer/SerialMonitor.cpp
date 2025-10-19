#include "../WebServer/SerialMonitor.h"

#ifdef WEBSERVER_SETUP

#include "../WebServer/ESPEasy_WebServer.h"
#include "../WebServer/AccessControl.h"
#include "../WebServer/HTML_wrappers.h"
#include "../WebServer/Markup.h"
#include "../WebServer/Markup_Buttons.h"
#include "../WebServer/Markup_Forms.h"

#include "../Helpers/StringConverter.h"
#include "../Helpers/ESPEasy_time_calc.h"
#include "../Helpers/_Plugin_Helper_serial.h"
#include "../Helpers/ESPEasy_Storage.h"
#include "../Globals/Settings.h"
#include <ESPeasySerial.h>

// Global variables - BASIT VERSİYON
String serialBuffer = "";
bool serialMonitorActive = false;
ESPeasySerial* currentSerial = nullptr;
uint32_t lastCharTime = 0; // Son karakter zamanı
uint32_t charTimeout = 500; // 500ms timeout

// Settings'den serial port bilgilerini al
ESPEasySerialPort getSelectedSerialPort() {
  return static_cast<ESPEasySerialPort>(Settings.serialmonitor_port);
}

int8_t getSelectedRxPin() {
  return Settings.serialmonitor_rxpin;
}

int8_t getSelectedTxPin() {
  return Settings.serialmonitor_txpin;
}

uint32_t getSelectedBaudRate() {
  return Settings.serialmonitor_baud;
}

uint8_t getSelectedDataBits() {
  return Settings.serialmonitor_databits;
}

uint8_t getSelectedStopBits() {
  return Settings.serialmonitor_stopbits;
}

uint8_t getSelectedParity() {
  return Settings.serialmonitor_parity;
}

// Yazıcı Serial için fonksiyonlar
ESPeasySerial* printerSerial = nullptr;

ESPEasySerialPort getPrinterSerialPort() {
  return static_cast<ESPEasySerialPort>(Settings.printer_port);
}

int8_t getPrinterRxPin() {
  return Settings.printer_rxpin;
}

int8_t getPrinterTxPin() {
  return Settings.printer_txpin;
}

uint32_t getPrinterBaudRate() {
  return Settings.printer_baud;
}

uint8_t getPrinterDataBits() {
  return Settings.printer_databits;
}

uint8_t getPrinterStopBits() {
  return Settings.printer_stopbits;
}

uint8_t getPrinterParity() {
  return Settings.printer_parity;
}

// Basit log sistemi
#define MAX_SERIAL_LOGS 50
String serialLogs[MAX_SERIAL_LOGS];
uint8_t serialLogIndex = 0;
uint16_t totalSerialLogs = 0;

// Karakter görünüm modu
enum DisplayMode {
  DISPLAY_ASCII = 0,
  DISPLAY_HEX = 1,
  DISPLAY_DEC = 2,
  DISPLAY_MIXED = 3
};
DisplayMode currentDisplayMode = DISPLAY_MIXED;

String formatCharacter(char c, DisplayMode mode, int index = -1, bool isLast = false) {
  String result = "";
  
  switch(mode) {
    case DISPLAY_ASCII:
      if (c >= 32 && c <= 126) {
        result = String(c);
      } else {
        result = "[" + String((int)c) + "]";
      }
      break;
      
    case DISPLAY_HEX:
      result = "0x" + String((int)c, HEX);
      break;
      
    case DISPLAY_DEC:
      result = String((int)c);
      break;
      
    case DISPLAY_MIXED:
    default:
      if (c >= 32 && c <= 126) {
        result = "<span class='char-box ascii'>";
        if (index >= 0) result += "<div class='char-index'>" + String(index) + "</div>";
        if (isLast) result += "<div class='last-byte'>son byte</div>";
        result += String(c) + "<br><small>" + String((int)c) + "</small></span>";
      } else {
        // Kontrol karakterleri için isimler - char cast ile güvenli karşılaştırma
        String charName = "";
        uint8_t byteValue = (uint8_t)c;
        switch(byteValue) {
          case 0:   charName = "NUL"; break;
          case 1:   charName = "SOH"; break;
          case 2:   charName = "STX"; break;
          case 3:   charName = "ETX"; break;
          case 4:   charName = "EOT"; break;
          case 5:   charName = "ENQ"; break;
          case 6:   charName = "ACK"; break;
          case 7:   charName = "BEL"; break;
          case 8:   charName = "BS"; break;
          case 9:   charName = "TAB"; break;
          case 10:  charName = "LF"; break;   // Line Feed - \n
          case 11:  charName = "VT"; break;
          case 12:  charName = "FF"; break;
          case 13:  charName = "CR"; break;   // Carriage Return - \r
          case 14:  charName = "SO"; break;
          case 15:  charName = "SI"; break;
          case 16:  charName = "DLE"; break;
          case 17:  charName = "DC1"; break;
          case 18:  charName = "DC2"; break;
          case 19:  charName = "DC3"; break;
          case 20:  charName = "DC4"; break;
          case 21:  charName = "NAK"; break;
          case 22:  charName = "SYN"; break;
          case 23:  charName = "ETB"; break;
          case 24:  charName = "CAN"; break;
          case 25:  charName = "EM"; break;
          case 26:  charName = "SUB"; break;
          case 27:  charName = "ESC"; break;
          case 28:  charName = "FS"; break;
          case 29:  charName = "GS"; break;
          case 30:  charName = "RS"; break;
          case 31:  charName = "US"; break;
          case 127: charName = "DEL"; break;
          default:  
            // Bilinmeyen kontrol karakterleri için [sayı] formatı
            charName = "[" + String(byteValue) + "]"; 
            break;
        }
        
        result = "<span class='char-box special'>";
        if (index >= 0) result += "<div class='char-index'>" + String(index) + "</div>";
        if (isLast) result += "<div class='last-byte'>son byte</div>";
        result += charName + "<br><small>(" + String(byteValue) + ")</small></span>";
      }
      break;
  }
  
  return result;
}

void addSerialLog(const String& data) {
  String timestamp = String(millis() / 1000) + "s";
  String logEntry = "";
  
  // Debug log kaldırıldı - performans için
  
  // Karakter sayısı bilgisi
  logEntry += "<div class='log-header-info'>" + timestamp + " - " + String(data.length()) + " bytes</div>";
  logEntry += "<div class='char-container'>";
  
  // Her karakteri formatla
  for (int i = 0; i < data.length(); i++) {
    bool isLastChar = (i == data.length() - 1);
    char currentChar = data.charAt(i);
    logEntry += formatCharacter(currentChar, currentDisplayMode, i + 1, isLastChar);
  }
  
  logEntry += "</div>";
  
  serialLogs[serialLogIndex] = logEntry;
  serialLogIndex = (serialLogIndex + 1) % MAX_SERIAL_LOGS;
  totalSerialLogs++;
  
  // ESPEasy log'a da ekle - sadece printable karakterler
  String cleanData = "";
  for (int i = 0; i < data.length(); i++) {
    char c = data.charAt(i);
    if (c >= 32 && c <= 126) {
      cleanData += c;
    } else {
      cleanData += "[" + String((uint8_t)c) + "]";
    }
  }
  String logMsg = String(F("Serial1 RX: ")) + cleanData;
  addLog(LOG_LEVEL_DEBUG, logMsg);
}

// Temizlenmiş serial data işleme - CR/LF kombinasyonu düzeltildi
void processSerialData() {
  if (!serialMonitorActive || !currentSerial) return;
  
  // Timeout kontrolü - 100ms boyunca yeni karakter gelmezse paketi gönder
  uint32_t currentTime = millis();
  if (serialBuffer.length() > 0 && (currentTime - lastCharTime) > 100) {
    addSerialLog(serialBuffer);
    serialBuffer = "";
  }
  
  // Statik değişkenler - CR beklemede mi?
  static bool expectingLF = false;
  static uint32_t crTime = 0;
  
  while (currentSerial->available()) {
    char c = currentSerial->read();
    lastCharTime = currentTime;
    
    // CR bekleme durumu kontrolü
    if (expectingLF) {
      if (c == '\n' && (currentTime - crTime) < 50) {
        // CR+LF kombinasyonu tamamlandı
        serialBuffer += c;
        expectingLF = false;
        addSerialLog(serialBuffer);
        serialBuffer = "";
        continue;
      } else {
        // CR'den sonra LF gelmedi veya çok geç geldi, önceki paketi gönder
        expectingLF = false;
        if (!serialBuffer.isEmpty()) {
          addSerialLog(serialBuffer);
          serialBuffer = "";
        }
        // Şu anki karakteri yeni pakete ekle (aşağıda eklenir)
      }
    }
    
    // Karakteri buffer'a ekle
    serialBuffer += c;
    
    // Paket bitiş durumları
    if (c == '\n') {
      // LF geldi, paketi hemen gönder
      addSerialLog(serialBuffer);
      serialBuffer = "";
    } else if (c == '\r') {
      // CR geldi, LF bekle
      expectingLF = true;
      crTime = currentTime;
    } else if (serialBuffer.length() >= 200) {
      // Çok uzun paket, zorla gönder
      addSerialLog(serialBuffer);
      serialBuffer = "";
    }
  }
}

// Serial port başlatma fonksiyonu
void initializeSelectedSerial() {
  if (currentSerial) {
    delete currentSerial;
    currentSerial = nullptr;
  }
  
  ESPEasySerialPort selectedPort = getSelectedSerialPort();
  int8_t rxPin = getSelectedRxPin();
  int8_t txPin = getSelectedTxPin();
  uint32_t baudRate = getSelectedBaudRate();
  uint8_t dataBits = getSelectedDataBits();
  uint8_t stopBits = getSelectedStopBits();
  uint8_t parity = getSelectedParity();
  
  // Serial config oluştur - ESP32 için basitleştirilmiş
  uint32_t config = SERIAL_8N1; // Default 8N1
  
  // ESP32 için config değerleri
  if (dataBits == 7 && stopBits == 1 && parity == 0) config = SERIAL_7N1;
  else if (dataBits == 7 && stopBits == 1 && parity == 1) config = SERIAL_7O1;
  else if (dataBits == 7 && stopBits == 1 && parity == 2) config = SERIAL_7E1;
  else if (dataBits == 7 && stopBits == 2 && parity == 0) config = SERIAL_7N2;
  else if (dataBits == 7 && stopBits == 2 && parity == 1) config = SERIAL_7O2;
  else if (dataBits == 7 && stopBits == 2 && parity == 2) config = SERIAL_7E2;
  else if (dataBits == 8 && stopBits == 1 && parity == 0) config = SERIAL_8N1;
  else if (dataBits == 8 && stopBits == 1 && parity == 1) config = SERIAL_8O1;
  else if (dataBits == 8 && stopBits == 1 && parity == 2) config = SERIAL_8E1;
  else if (dataBits == 8 && stopBits == 2 && parity == 0) config = SERIAL_8N2;
  else if (dataBits == 8 && stopBits == 2 && parity == 1) config = SERIAL_8O2;
  else if (dataBits == 8 && stopBits == 2 && parity == 2) config = SERIAL_8E2;
  
  // Debug: Settings değerlerini kontrol et
  addLog(LOG_LEVEL_INFO, String(F("Serial Config - Port:")) + String(Settings.serialmonitor_port) + 
                        String(F(" RX:")) + String(rxPin) + String(F(" TX:")) + String(txPin) +
                        String(F(" Baud:")) + String(baudRate) + String(F(" Data:")) + String(dataBits) +
                        String(F(" Stop:")) + String(stopBits) + String(F(" Parity:")) + String(parity));
  
  currentSerial = new ESPeasySerial(selectedPort, rxPin, txPin);
  if (currentSerial) {
    currentSerial->begin(baudRate, config);
    String logMsg = String(F("Serial Monitor started on ")) + 
                   serialHelper_getSerialTypeLabel(selectedPort) + 
                   String(F(" RX:")) + String(rxPin) + 
                   String(F(" TX:")) + String(txPin) +
                   String(F(" @")) + String(baudRate) +
                   String(F(" ")) + String(dataBits) + 
                   ((parity == 0) ? F("N") : (parity == 1) ? F("O") : F("E")) + 
                   String(stopBits);
    addLog(LOG_LEVEL_INFO, logMsg);
  }
}

// Yazıcı Serial başlatma fonksiyonu
void initializePrinterSerial() {
  if (printerSerial) {
    delete printerSerial;
    printerSerial = nullptr;
  }
  
  ESPEasySerialPort selectedPort = getPrinterSerialPort();
  int8_t rxPin = getPrinterRxPin();
  int8_t txPin = getPrinterTxPin();
  uint32_t baudRate = getPrinterBaudRate();
  uint8_t dataBits = getPrinterDataBits();
  uint8_t stopBits = getPrinterStopBits();
  uint8_t parity = getPrinterParity();
  
  // Serial config oluştur - ESP32 için basitleştirilmiş
  uint32_t config = SERIAL_8N1; // Default 8N1
  
  // ESP32 için config değerleri
  if (dataBits == 7 && stopBits == 1 && parity == 0) config = SERIAL_7N1;
  else if (dataBits == 7 && stopBits == 1 && parity == 1) config = SERIAL_7O1;
  else if (dataBits == 7 && stopBits == 1 && parity == 2) config = SERIAL_7E1;
  else if (dataBits == 7 && stopBits == 2 && parity == 0) config = SERIAL_7N2;
  else if (dataBits == 7 && stopBits == 2 && parity == 1) config = SERIAL_7O2;
  else if (dataBits == 7 && stopBits == 2 && parity == 2) config = SERIAL_7E2;
  else if (dataBits == 8 && stopBits == 1 && parity == 0) config = SERIAL_8N1;
  else if (dataBits == 8 && stopBits == 1 && parity == 1) config = SERIAL_8O1;
  else if (dataBits == 8 && stopBits == 1 && parity == 2) config = SERIAL_8E1;
  else if (dataBits == 8 && stopBits == 2 && parity == 0) config = SERIAL_8N2;
  else if (dataBits == 8 && stopBits == 2 && parity == 1) config = SERIAL_8O2;
  else if (dataBits == 8 && stopBits == 2 && parity == 2) config = SERIAL_8E2;
  
  printerSerial = new ESPeasySerial(selectedPort, rxPin, txPin);
  if (printerSerial) {
    printerSerial->begin(baudRate, config);
    String logMsg = String(F("Printer Serial started on ")) + 
                   serialHelper_getSerialTypeLabel(selectedPort) + 
                   String(F(" RX:")) + String(rxPin) + 
                   String(F(" TX:")) + String(txPin) +
                   String(F(" @")) + String(baudRate) +
                   String(F(" ")) + String(dataBits) + 
                   ((parity == 0) ? F("N") : (parity == 1) ? F("O") : F("E")) + 
                   String(stopBits);
    addLog(LOG_LEVEL_INFO, logMsg);
  }
}

// Yazıcı Serial'e global erişim fonksiyonları
ESPeasySerial* getPrinterSerial() {
  return printerSerial;
}

bool isPrinterSerialActive() {
  return (printerSerial != nullptr);
}

void sendToPrinter(const String& data) {
  if (printerSerial) {
    printerSerial->print(data);
  }
}

void sendToPrinterLn(const String& data) {
  if (printerSerial) {
    printerSerial->println(data);
  }
}

void testPrinterSerial() {
  if (printerSerial) {
    printerSerial->println(F("=== Printer Serial Test ==="));
    printerSerial->println(F("Test mesajı gönderiliyor..."));
    printerSerial->print(F("Saat: "));
    printerSerial->print(millis() / 1000);
    printerSerial->println(F(" saniye"));
    printerSerial->println(F("========================"));
    addLog(LOG_LEVEL_INFO, F("Test mesajı Printer Serial'e gönderildi"));
  } else {
    addLog(LOG_LEVEL_ERROR, F("Printer Serial aktif değil - test başarısız"));
  }
}

// Boot sırasında Yazıcı Serial'i başlat
void initPrinterSerialOnBoot() {
  // Sadece geçerli pinler varsa başlat
  if (Settings.printer_rxpin >= 0 && Settings.printer_txpin >= 0) {
    initializePrinterSerial();
    
    // Başlangıç mesajını direkt printer serial'e gönder
    if (printerSerial) {
      delay(200); // Serial tamamen başlaması için bekle
      printerSerial->println(F("Printer Start"));
      printerSerial->println(F("ESPEasy Ready"));
    }
    
    addLog(LOG_LEVEL_INFO, F("Printer Serial initialized on boot"));
  }
}

void handle_serial_monitor() {
  if (!isLoggedIn()) {
    return;
  }

  navMenuIndex = MENU_INDEX_TOOLS;
  TXBuffer.startStream();
  sendHeadandTail_stdtemplate(_HEAD);

  // Form işlemleri
  const bool startMonitor = hasArg(F("startmonitor"));
  const bool stopMonitor = hasArg(F("stopmonitor"));
  const bool clearBuffer = hasArg(F("clearbuffer"));
  const bool sendData = hasArg(F("senddata"));
  const bool saveSettings = hasArg(F("savesettings"));

  // Serial ayarları kaydetme
  if (saveSettings) {
    if (hasArg(F("baudrate"))) {
      Settings.serialmonitor_baud = getFormItemInt(F("baudrate"));
    }
    if (hasArg(F("databits"))) {
      Settings.serialmonitor_databits = getFormItemInt(F("databits"));
    }
    if (hasArg(F("stopbits"))) {
      Settings.serialmonitor_stopbits = getFormItemInt(F("stopbits"));
    }
    if (hasArg(F("parity"))) {
      Settings.serialmonitor_parity = getFormItemInt(F("parity"));
    }
    
    // Settings'i kaydet
    SaveSettings();
    addLog(LOG_LEVEL_INFO, String(F("Serial Monitor settings saved")));
  }

  if (startMonitor) {
    // Serial'i her zaman yeniden başlat (pin değişiklikleri için)
    initializeSelectedSerial();
    serialMonitorActive = true;
    serialBuffer = "";
    totalSerialLogs = 0;
    serialLogIndex = 0;
    // Tüm log array'ini temizle
    for (int i = 0; i < MAX_SERIAL_LOGS; i++) {
      serialLogs[i] = "";
    }
    addLog(LOG_LEVEL_INFO, String(F("Serial monitor started")));
  }

  if (stopMonitor) {
    serialMonitorActive = false;
    addLog(LOG_LEVEL_INFO, String(F("Serial monitor stopped")));
  }

  if (clearBuffer) {
    serialBuffer = "";
    totalSerialLogs = 0;
    serialLogIndex = 0;
    // Tüm log array'ini temizle
    for (int i = 0; i < MAX_SERIAL_LOGS; i++) {
      serialLogs[i] = "";
    }
    addLog(LOG_LEVEL_INFO, String(F("Serial buffer cleared")));
  }

  if (sendData) {
    String dataToSend = webArg(F("sendtext"));
    bool addCR = isFormItemChecked(F("addcr"));
    bool addLF = isFormItemChecked(F("addlf"));
    
    if (!dataToSend.isEmpty() && currentSerial) {
      currentSerial->print(dataToSend);
      if (addCR) currentSerial->print('\r');
      if (addLF) currentSerial->print('\n');
      
      String logMsg = String(F("Serial TX: ")) + dataToSend;
      addLog(LOG_LEVEL_INFO, logMsg);
    }
  }

  // CSS - BASİT VERSİYON
  addHtml(F("<style>"));
  addHtml(F("html,body{overflow:hidden;margin:0;padding:0}"));
  addHtml(F("body{background:#1e1e1e;color:#ccc;font-family:Arial,sans-serif;padding:15px;height:100vh;box-sizing:border-box}"));
  addHtml(F(".container{max-width:1000px;margin:0 auto;background:#252526;border-radius:6px;padding:20px;height:calc(100vh - 30px);overflow-y:auto}"));
  addHtml(F(".header{text-align:center;margin-bottom:20px;border-bottom:2px solid #007acc;padding-bottom:15px}"));
  addHtml(F(".header h1{color:#007acc;margin:0;font-size:28px}"));
  addHtml(F(".header p{margin:8px 0;font-size:14px;color:#9cdcfe}"));
  addHtml(F(".panel{background:#2d2d30;border:1px solid #444;border-radius:6px;margin:20px 0;padding:20px}"));
  addHtml(F(".panel-title{color:#569cd6;font-weight:bold;margin-bottom:15px;font-size:18px}"));
  addHtml(F(".form-row{display:flex;gap:15px;margin:15px 0;flex-wrap:wrap}"));
  addHtml(F(".form-group{display:flex;flex-direction:column;min-width:140px}"));
  addHtml(F("label{color:#9cdcfe;font-size:13px;margin-bottom:5px}"));
  addHtml(F("select,input{background:#3c3c3c;border:1px solid #666;color:#ccc;padding:10px;border-radius:4px;font-size:14px}"));
  addHtml(F("button{background:#007acc;color:white;border:none;padding:12px 18px;border-radius:4px;cursor:pointer;font-size:14px;margin:5px}"));
  addHtml(F("button:hover{background:#106ebe}"));
  addHtml(F("button.success{background:#16825d}"));
  addHtml(F("button.danger{background:#f14c4c}"));
  addHtml(F("button.warning{background:#fc9403}"));
  addHtml(F(".log-container{background:#1e1e1e;border:1px solid #444;border-radius:6px;margin:20px 0}"));
  addHtml(F(".log-header{background:#252526;padding:15px;border-bottom:1px solid #444;color:#ccc;font-weight:bold;font-size:16px}"));
  addHtml(F(".log-content{height:400px;overflow-y:auto;padding:15px;font-family:Consolas,monospace;font-size:12px;line-height:1.5;scrollbar-width:none;-ms-overflow-style:none}"));
  addHtml(F(".log-content::-webkit-scrollbar{display:none}"));
  addHtml(F(".container::-webkit-scrollbar{display:none}"));
  addHtml(F(".container{scrollbar-width:none;-ms-overflow-style:none}"));
  addHtml(F(".log-entry{padding:5px 0;border-bottom:1px solid #333;color:#ddd}"));
  addHtml(F(".log-time{color:#569cd6;margin-right:10px}"));
  addHtml(F("@media (max-width:768px){.form-row{flex-direction:column}button{width:100%;margin:3px 0}.log-content{height:300px}}"));
  addHtml(F(".char-container{display:flex;flex-wrap:wrap;gap:3px;margin:5px 0}"));
  addHtml(F(".char-box{display:inline-block;border:1px solid #666;border-radius:3px;padding:3px 5px;text-align:center;font-family:monospace;font-size:11px;line-height:1.2;min-width:20px;background:#2d2d30}"));
  addHtml(F(".char-box.ascii{background:#0d7377;color:#fff;border-color:#14a085}"));
  addHtml(F(".char-box.special{background:#fc9403;color:#000;border-color:#ffa500;font-weight:bold}"));
  addHtml(F(".char-box.control{background:#f14c4c;color:#fff;border-color:#ff6b6b}"));
  addHtml(F(".char-box small{display:block;font-size:9px;opacity:0.8;margin-top:1px}"));
  addHtml(F(".char-index{position:absolute;top:-8px;left:50%;transform:translateX(-50%);font-size:8px;color:#666;background:#1e1e1e;padding:0 2px;border-radius:2px;line-height:1}"));
  addHtml(F(".last-byte{position:absolute;bottom:-8px;left:50%;transform:translateX(-50%);font-size:8px;color:#ff6b6b;background:#1e1e1e;padding:0 2px;border-radius:2px;line-height:1;font-weight:bold}"));
  addHtml(F(".char-box{position:relative}"));
  addHtml(F(".log-header-info{color:#9cdcfe;font-size:11px;margin-bottom:5px;font-style:italic}"));
  addHtml(F("</style>"));

  addHtml(F("<div class='container'>"));

  // Header
  addHtml(F("<div class='header'>"));
  addHtml(F("<h1>📡 Serial Monitor</h1>"));
  addHtml(F("<p>ESP32S3 Serial1 Monitor (RX=15, TX=16)</p>"));
  addHtml(F("</div>"));

  // Settings Panel
  addHtml(F("<div class='panel'>"));
  addHtml(F("<div class='panel-title'>⚙️ Serial Ayarları</div>"));
  html_add_form();
  
  addHtml(F("<div class='form-row'>"));
  
  // Mevcut Serial Port Bilgisi (Hardware sayfasından ayarlanır)
  addHtml(F("<div class='form-group'>"));
  addHtml(F("<label>Active Serial Port</label>"));
  addHtml(F("<div style='padding:10px; background:#2d2d30; border:1px solid #444; border-radius:4px;'>"));
  addHtml(serialHelper_getSerialTypeLabel(getSelectedSerialPort()));
  addHtml(F(" - RX:"));
  addHtmlInt(getSelectedRxPin());
  addHtml(F(" TX:"));
  addHtmlInt(getSelectedTxPin());
  addHtml(F("<br><small style='color:#9cdcfe;'>Pin ayarları Hardware sayfasından değiştirilebilir</small>"));
  addHtml(F("</div>"));
  addHtml(F("</div>"));
  
  addHtml(F("</div>"));
  
  addHtml(F("<div class='form-row'>"));
  
  // Baud Rate
  addHtml(F("<div class='form-group'>"));
  addHtml(F("<label>Baud Rate</label>"));
  addHtml(F("<select name='baudrate'>"));
  uint32_t baudRates[] = {9600, 19200, 38400, 57600, 115200, 230400, 460800, 921600};
  for (int i = 0; i < 8; i++) {
    addHtml(F("<option value='"));
    addHtmlInt(baudRates[i]);
    addHtml(F("'"));
    if (getSelectedBaudRate() == baudRates[i]) addHtml(F(" selected"));
    addHtml(F(">"));
    addHtmlInt(baudRates[i]);
    addHtml(F("</option>"));
  }
  addHtml(F("</select></div>"));
  
  // Data Bits
  addHtml(F("<div class='form-group'>"));
  addHtml(F("<label>Data Bits</label>"));
  addHtml(F("<select name='databits'>"));
  addHtml(F("<option value='8'"));
  if (getSelectedDataBits() == 8) addHtml(F(" selected"));
  addHtml(F(">8</option>"));
  addHtml(F("<option value='7'"));
  if (getSelectedDataBits() == 7) addHtml(F(" selected"));
  addHtml(F(">7</option>"));
  addHtml(F("</select></div>"));
  
  // Stop Bits
  addHtml(F("<div class='form-group'>"));
  addHtml(F("<label>Stop Bits</label>"));
  addHtml(F("<select name='stopbits'>"));
  addHtml(F("<option value='1'"));
  if (getSelectedStopBits() == 1) addHtml(F(" selected"));
  addHtml(F(">1</option>"));
  addHtml(F("<option value='2'"));
  if (getSelectedStopBits() == 2) addHtml(F(" selected"));
  addHtml(F(">2</option>"));
  addHtml(F("</select></div>"));
  
  // Parity
  addHtml(F("<div class='form-group'>"));
  addHtml(F("<label>Parity</label>"));
  addHtml(F("<select name='parity'>"));
  addHtml(F("<option value='0'"));
  if (getSelectedParity() == 0) addHtml(F(" selected"));
  addHtml(F(">None</option>"));
  addHtml(F("<option value='1'"));
  if (getSelectedParity() == 1) addHtml(F(" selected"));
  addHtml(F(">Odd</option>"));
  addHtml(F("<option value='2'"));
  if (getSelectedParity() == 2) addHtml(F(" selected"));
  addHtml(F(">Even</option>"));
  addHtml(F("</select></div>"));

  // Durum
  addHtml(F("<div class='form-group'>"));
  addHtml(F("<label>Durum</label>"));
  addHtml(F("<span style='color:"));
  addHtml(serialMonitorActive ? F("#16825d'>AÇIK") : F("#f14c4c'>KAPALI"));
  addHtml(F("</span></div>"));

  addHtml(F("</div>"));
  
  // Control buttons
  addHtml(F("<div class='form-row'>"));
  addHtml(F("<button type='submit' name='savesettings' value='1'>💾 Ayarları Kaydet</button>"));
  if (serialMonitorActive) {
    addHtml(F("<button type='submit' name='stopmonitor' value='1' class='danger'>⏹️ Durdur</button>"));
  } else {
    addHtml(F("<button type='submit' name='startmonitor' value='1' class='success'>▶️ Başlat</button>"));
  }
  addHtml(F("<button type='submit' name='clearbuffer' value='1' class='warning'>🗑️ Temizle</button>"));
  addHtml(F("<button type='submit' name='refresh' value='1'>🔄 Yenile</button>"));
  addHtml(F("</div>"));
  
  html_end_form();
  addHtml(F("</div>"));

  // Data Display
  addHtml(F("<div class='log-container'>"));
  addHtml(F("<div class='log-header'>"));
  addHtml(F("📊 Serial Monitor Log (Son "));
  addHtmlInt(totalSerialLogs > MAX_SERIAL_LOGS ? MAX_SERIAL_LOGS : totalSerialLogs);
  addHtml(F(" paket)"));
  addHtml(F("</div>"));
  addHtml(F("<div class='log-content' id='logContent'>"));
  
  // Logları göster - en yeniden eskiye
  if (totalSerialLogs > 0) {
    int displayCount = totalSerialLogs > MAX_SERIAL_LOGS ? MAX_SERIAL_LOGS : totalSerialLogs;
    int startIndex = totalSerialLogs > MAX_SERIAL_LOGS ? serialLogIndex : 0;
    
    for (int i = 0; i < displayCount; i++) {
      int index = (startIndex + i) % MAX_SERIAL_LOGS;
      if (!serialLogs[index].isEmpty()) {
        addHtml(F("<div class='log-entry'>"));
        addHtml(serialLogs[index]);
        addHtml(F("</div>"));
      }
    }
  } else {
    addHtml(F("<div class='log-entry' style='color:#999;text-align:center;padding:20px'>"));
    addHtml(F("Henüz veri yok. Serial monitor'ü başlatın ve RX (pin 15) üzerinden veri gönderin."));
    addHtml(F("</div>"));
  }
  
  addHtml(F("</div>"));
  addHtml(F("</div>"));

  // Send Panel
  addHtml(F("<div class='panel'>"));
  addHtml(F("<div class='panel-title'>📤 Veri Gönder</div>"));
  html_add_form();
  
  addHtml(F("<div class='form-row' style='align-items:stretch'>"));
  addHtml(F("<div class='form-group' style='flex:2;min-width:200px'>"));
  addHtml(F("<label>Göndermek istediğiniz veri</label>"));
  addHtml(F("<input type='text' name='sendtext' placeholder='Buraya yazın...'>"));
  addHtml(F("</div>"));
  
  addHtml(F("<div class='form-group' style='flex:0 0 auto;min-width:120px'>"));
  addHtml(F("<label style='margin-bottom:6px'>Satır Sonu</label>"));
  addHtml(F("<div style='display:flex;gap:8px;align-items:center'>"));
  addHtml(F("<label style='display:flex;align-items:center;font-size:10px;margin:0'>"));
  addHtml(F("<input type='checkbox' name='addcr' value='1' style='width:auto;margin-right:4px'> CR"));
  addHtml(F("</label>"));
  addHtml(F("<label style='display:flex;align-items:center;font-size:10px;margin:0'>"));
  addHtml(F("<input type='checkbox' name='addlf' value='1' checked style='width:auto;margin-right:4px'> LF"));
  addHtml(F("</label>"));
  addHtml(F("</div>"));
  addHtml(F("</div>"));
  
  addHtml(F("<button type='submit' name='senddata' value='1'>📤 Gönder</button>"));
  addHtml(F("</div>"));
  
  html_end_form();
  addHtml(F("</div>"));

  // Auto scroll JavaScript
  addHtml(F("<script>"));
  addHtml(F("document.addEventListener('DOMContentLoaded', function() {"));
  addHtml(F("  var logContent = document.getElementById('logContent');"));
  addHtml(F("  if (logContent) {"));
  addHtml(F("    logContent.scrollTop = logContent.scrollHeight;"));
  addHtml(F("  }"));
  addHtml(F("});"));
  addHtml(F("</script>"));

  addHtml(F("</div>"));

  sendHeadandTail_stdtemplate(_TAIL);
  TXBuffer.endStream();
}

#endif // WEBSERVER_SETUP