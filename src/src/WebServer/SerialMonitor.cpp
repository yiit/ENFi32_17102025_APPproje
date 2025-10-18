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

// Global variables
String serialBuffer = "";
bool serialMonitorActive = false;
uint32_t serialBaudRate = 9600;
uint8_t serialDataBits = 8;
uint8_t serialStopBits = 1;
uint8_t serialParity = 0;

// Pattern detection variables
String detectedStartPattern = "";
String detectedEndPattern = "";
bool autoDetectMode = false;
uint8_t patternConfidence = 0;
String lastCompleteMessage = "";

// Character frequency analysis
uint16_t charFrequency[256] = {0};
uint8_t suspectedStartChars[10] = {0};
uint8_t suspectedEndChars[10] = {0};

// Circular buffer for serial data
#define SERIAL_BUFFER_SIZE 1000
SerialDataPacket serialPackets[SERIAL_BUFFER_SIZE];
uint16_t serialPacketIndex = 0;
uint16_t totalPackets = 0;

void handle_serial_monitor() {
  if (!isLoggedIn()) {
    return;
  }

  TXBuffer.startStream();
  navMenuIndex = MENU_INDEX_TOOLS;
  sendHeadandTail_stdtemplate(_HEAD);

  // Handle form submissions
  const bool startMonitor = hasArg(F("startmonitor"));
  const bool stopMonitor = hasArg(F("stopmonitor"));
  const bool clearBuffer = hasArg(F("clearbuffer"));
  const bool saveSettings = hasArg(F("savesettings"));
  const bool sendData = hasArg(F("senddata"));
  const bool markStart = hasArg(F("markstart"));
  const bool markEnd = hasArg(F("markend"));
  const bool autoDetect = hasArg(F("autodetect"));
  const bool savePattern = hasArg(F("savepattern"));

  if (saveSettings) {
    serialBaudRate = getFormItemInt(F("baudrate"));
    serialDataBits = getFormItemInt(F("databits"));
    serialStopBits = getFormItemInt(F("stopbits"));
    serialParity = getFormItemInt(F("parity"));
    
    Serial1.end();
    delay(100);
    Serial1.begin(serialBaudRate, SERIAL_8N1, 16, 17);
    
    // STRING CONCATENATION DÜZELTİLDİ
    String logMsg = String(F("Serial1 settings updated: ")) + String(serialBaudRate) + String(F(" baud"));
    addLog(LOG_LEVEL_INFO, logMsg);
  }

  if (startMonitor) {
    serialMonitorActive = true;
    serialBuffer = "";
    memset(charFrequency, 0, sizeof(charFrequency));
    addLog(LOG_LEVEL_INFO, String(F("Serial monitor started")));
  }

  if (stopMonitor) {
    serialMonitorActive = false;
    addLog(LOG_LEVEL_INFO, String(F("Serial monitor stopped")));
  }

  if (clearBuffer) {
    serialBuffer = "";
    totalPackets = 0;
    serialPacketIndex = 0;
    memset(charFrequency, 0, sizeof(charFrequency));
    addLog(LOG_LEVEL_INFO, String(F("Serial buffer cleared")));
  }

  // Pattern işaretleme
  if (markStart) {
    String selectedHex = webArg(F("selectedhex"));
    if (!selectedHex.isEmpty()) {
      detectedStartPattern = selectedHex;
      String logMsg = String(F("Start pattern marked: ")) + selectedHex;
      addLog(LOG_LEVEL_INFO, logMsg);
    }
  }

  if (markEnd) {
    String selectedHex = webArg(F("selectedhex"));
    if (!selectedHex.isEmpty()) {
      detectedEndPattern = selectedHex;
      String logMsg = String(F("End pattern marked: ")) + selectedHex;
      addLog(LOG_LEVEL_INFO, logMsg);
    }
  }

  // Otomatik pattern tespit
  if (autoDetect) {
    autoDetectMode = !autoDetectMode;
    if (autoDetectMode) {
      analyzePatterns();
      addLog(LOG_LEVEL_INFO, String(F("Auto pattern detection enabled")));
    } else {
      addLog(LOG_LEVEL_INFO, String(F("Auto pattern detection disabled")));
    }
  }

  // Pattern kaydetme - STRING CONCATENATION DÜZELTİLDİ
  if (savePattern) {
    String customStart = webArg(F("customstart"));
    String customEnd = webArg(F("customend"));
    if (!customStart.isEmpty()) detectedStartPattern = customStart;
    if (!customEnd.isEmpty()) detectedEndPattern = customEnd;
    
    String logMsg = String(F("Pattern saved - Start: ")) + detectedStartPattern + String(F(" End: ")) + detectedEndPattern;
    addLog(LOG_LEVEL_INFO, logMsg);
  }

  if (sendData) {
    String dataToSend = webArg(F("sendtext"));
    bool addCR = isFormItemChecked(F("addcr"));
    bool addLF = isFormItemChecked(F("addlf"));
    bool sendHex = isFormItemChecked(F("sendhex"));
    
    if (!dataToSend.isEmpty()) {
      if (sendHex) {
        for (int i = 0; i < dataToSend.length(); i += 2) {
          String hexByte = dataToSend.substring(i, i + 2);
          uint8_t byteValue = strtol(hexByte.c_str(), NULL, 16);
          Serial1.write(byteValue);
        }
      } else {
        Serial1.print(dataToSend);
      }
      
      if (addCR) Serial1.write('\r');
      if (addLF) Serial1.write('\n');
      
      // STRING CONCATENATION DÜZELTİLDİ
      String logMsg = String(F("Data sent to Serial1: ")) + dataToSend;
      addLog(LOG_LEVEL_INFO, logMsg);
    }
  }

  // Enhanced CSS (aynı kalıyor...)
  addHtml(F("<style>"));
  addHtml(F("*{box-sizing:border-box;margin:0;padding:0}"));
  addHtml(F("body{font-family:'Consolas','Monaco','Courier New',monospace;background:#1e1e1e;color:#d4d4d4;margin:0;padding:20px}"));
  addHtml(F(".monitor-container{max-width:1400px;margin:0 auto;background:#2d2d30;border-radius:8px;box-shadow:0 4px 20px rgba(0,0,0,0.3)}"));
  addHtml(F(".monitor-header{background:linear-gradient(135deg,#007acc,#005a9e);color:white;padding:20px;border-radius:8px 8px 0 0}"));
  addHtml(F(".monitor-header h1{margin:0;font-size:24px;font-weight:600}"));
  addHtml(F(".monitor-header p{margin:8px 0 0 0;opacity:0.9}"));
  addHtml(F(".main-content{display:grid;grid-template-columns:1fr 300px;gap:20px;padding:20px}"));
  addHtml(F(".left-panel{display:flex;flex-direction:column;gap:20px}"));
  addHtml(F(".right-panel{display:flex;flex-direction:column;gap:15px}"));
  addHtml(F(".settings-panel{background:#252526;padding:20px;border-radius:6px;border:1px solid #3e3e42}"));
  addHtml(F(".pattern-panel{background:#252526;padding:15px;border-radius:6px;border:1px solid #3e3e42}"));
  addHtml(F(".panel-title{color:#cccccc;font-weight:600;margin-bottom:12px;font-size:14px}"));
  addHtml(F(".settings-row{display:flex;gap:15px;align-items:center;margin-bottom:15px;flex-wrap:wrap}"));
  addHtml(F(".settings-group{display:flex;flex-direction:column;min-width:120px}"));
  addHtml(F(".settings-label{font-size:12px;color:#cccccc;margin-bottom:4px;font-weight:600}"));
  addHtml(F(".settings-input{background:#3c3c3c;border:1px solid #5a5a5a;color:#d4d4d4;padding:6px 10px;border-radius:4px;font-family:inherit;font-size:12px}"));
  addHtml(F(".settings-input:focus{outline:none;border-color:#007acc;box-shadow:0 0 0 2px rgba(0,122,204,0.3)}"));
  addHtml(F(".btn{background:#0e639c;color:white;border:none;padding:6px 12px;border-radius:4px;cursor:pointer;font-size:11px;font-weight:600;transition:all 0.2s;margin:2px}"));
  addHtml(F(".btn:hover{background:#1177bb;transform:translateY(-1px)}"));
  addHtml(F(".btn-success{background:#107c10}"));
  addHtml(F(".btn-success:hover{background:#0f7b0f}"));
  addHtml(F(".btn-danger{background:#d13438}"));
  addHtml(F(".btn-danger:hover{background:#c23237}"));
  addHtml(F(".btn-warning{background:#ff8c00}"));
  addHtml(F(".btn-warning:hover{background:#e67e00}"));
  addHtml(F(".btn-small{padding:4px 8px;font-size:10px}"));
  addHtml(F(".status-indicator{padding:4px 8px;border-radius:4px;font-size:11px;font-weight:bold;margin-left:10px}"));
  addHtml(F(".status-active{background:#107c10;color:white}"));
  addHtml(F(".status-inactive{background:#d13438;color:white}"));
  addHtml(F(".data-display{background:#1e1e1e;border-radius:6px;border:1px solid #3e3e42;max-height:600px;overflow-y:auto;flex:1}"));
  addHtml(F(".data-header{background:#252526;padding:10px 15px;border-bottom:1px solid #3e3e42;font-size:12px;color:#cccccc;font-weight:600}"));
  addHtml(F(".data-row{padding:8px 15px;border-bottom:1px solid #2d2d30;font-size:11px;line-height:1.4;position:relative}"));
  addHtml(F(".data-row:hover{background:#2a2a2a}"));
  addHtml(F(".timestamp{color:#569cd6;font-weight:600;margin-right:10px}"));
  addHtml(F(".data-type{color:#9cdcfe;font-weight:600;margin-right:8px}"));
  addHtml(F(".hex-data{color:#ce9178;font-family:'Courier New',monospace;font-size:10px;line-height:1.2}"));
  addHtml(F(".ascii-data{color:#d4d4d4;font-family:'Courier New',monospace;font-size:11px}"));
  addHtml(F(".special-char{background:#ff6b6b;color:white;padding:1px 4px;border-radius:2px;font-size:9px;font-weight:bold;margin:0 2px}"));
  addHtml(F(".hex-byte{display:inline-block;margin:1px;padding:2px 4px;background:#3c3c3c;border-radius:2px;cursor:pointer;transition:all 0.2s}"));
  addHtml(F(".hex-byte:hover{background:#4a90e2;color:white;transform:scale(1.1)}"));
  addHtml(F(".hex-byte.selected{background:#ff6b6b;color:white}"));
  addHtml(F(".hex-byte.start-pattern{background:#107c10;color:white}"));
  addHtml(F(".hex-byte.end-pattern{background:#d13438;color:white}"));
  addHtml(F(".pattern-input{width:100%;background:#3c3c3c;border:1px solid #5a5a5a;color:#d4d4d4;padding:8px;border-radius:4px;font-family:'Courier New',monospace;font-size:11px;margin-bottom:8px}"));
  addHtml(F(".pattern-detected{background:linear-gradient(135deg,rgba(16,124,16,0.2),rgba(56,239,125,0.2));border:1px solid #107c10;padding:8px;border-radius:4px;margin-bottom:8px}"));
  addHtml(F(".frequency-chart{background:#3c3c3c;padding:8px;border-radius:4px;margin-bottom:8px;max-height:150px;overflow-y:auto}"));
  addHtml(F(".char-freq{display:flex;justify-content:space-between;align-items:center;padding:2px 0;font-size:10px;border-bottom:1px solid #555}"));
  addHtml(F(".char-display{font-family:'Courier New',monospace;color:#ce9178}"));
  addHtml(F(".freq-bar{background:#4a90e2;height:4px;border-radius:2px;margin-left:8px;min-width:2px}"));
  addHtml(F(".send-panel{background:#252526;padding:15px;border-radius:6px;border:1px solid #3e3e42}"));
  addHtml(F(".send-row{display:flex;gap:8px;align-items:center;margin-bottom:8px;flex-wrap:wrap}"));
  addHtml(F(".send-input{flex:1;min-width:250px;background:#3c3c3c;border:1px solid #5a5a5a;color:#d4d4d4;padding:8px 12px;border-radius:4px;font-family:'Courier New',monospace}"));
  addHtml(F(".checkbox-group{display:flex;gap:12px;align-items:center;flex-wrap:wrap}"));
  addHtml(F(".checkbox-item{display:flex;align-items:center;gap:4px;color:#cccccc;font-size:11px}"));
  addHtml(F(".checkbox-item input{margin:0}"));
  addHtml(F(".stats-panel{background:#252526;padding:15px;border-radius:0 0 8px 8px;border-top:1px solid #3e3e42}"));
  addHtml(F(".stats-row{display:flex;gap:20px;font-size:12px;color:#cccccc;flex-wrap:wrap}"));
  addHtml(F(".stat-item{display:flex;flex-direction:column;align-items:center}"));
  addHtml(F(".stat-value{color:#569cd6;font-weight:bold;font-size:14px}"));
  addHtml(F(".stat-label{color:#999999;font-size:9px;margin-top:2px}"));
  addHtml(F("@media (max-width: 1200px) { .main-content { grid-template-columns: 1fr; } }"));
  addHtml(F("</style>"));

  addHtml(F("<div class='monitor-container'>"));
  
  // Header
  addHtml(F("<div class='monitor-header'>"));
  addHtml(F("<h1>📡 Serial Monitor Pro</h1>"));
  addHtml(F("<p>ESP32 Serial1 Port Monitor & Pattern Analyzer</p>"));
  addHtml(F("</div>"));

  addHtml(F("<div class='main-content'>"));
  addHtml(F("<div class='left-panel'>"));

  // Settings Panel
  renderSerialSettings();

  // Data Display
  renderSerialDisplay();

  // Send Panel
  renderSendPanel();

  addHtml(F("</div>"));
  addHtml(F("<div class='right-panel'>"));

  // Pattern Analysis Panel
  renderPatternPanel();

  // Character Frequency Panel
  renderFrequencyPanel();

  addHtml(F("</div>"));
  addHtml(F("</div>"));

  // Statistics
  renderStatsPanel();

  addHtml(F("</div>"));

  // Enhanced JavaScript
  addHtml(F("<script>"));
  addHtml(F("let selectedBytes = [];"));
  addHtml(F("function selectHexByte(element, hexValue) {"));
  addHtml(F("  if (element.classList.contains('selected')) {"));
  addHtml(F("    element.classList.remove('selected');"));
  addHtml(F("    selectedBytes = selectedBytes.filter(b => b !== hexValue);"));
  addHtml(F("  } else {"));
  addHtml(F("    element.classList.add('selected');"));
  addHtml(F("    selectedBytes.push(hexValue);"));
  addHtml(F("  }"));
  addHtml(F("  document.getElementById('selectedPattern').value = selectedBytes.join(' ');"));
  addHtml(F("}"));
  addHtml(F("function markAsStart() {"));
  addHtml(F("  if (selectedBytes.length > 0) {"));
  addHtml(F("    document.querySelector('[name=selectedhex]').value = selectedBytes.join(' ');"));
  addHtml(F("    document.querySelector('[name=markstart]').click();"));
  addHtml(F("  }"));
  addHtml(F("}"));
  addHtml(F("function markAsEnd() {"));
  addHtml(F("  if (selectedBytes.length > 0) {"));
  addHtml(F("    document.querySelector('[name=selectedhex]').value = selectedBytes.join(' ');"));
  addHtml(F("    document.querySelector('[name=markend]').click();"));
  addHtml(F("  }"));
  addHtml(F("}"));
  addHtml(F("function refreshData() {"));
  addHtml(F("  if ("));
  addHtml(serialMonitorActive ? F("true") : F("false"));
  addHtml(F(") {"));
  addHtml(F("    fetch('/serialdata').then(r => r.text()).then(data => {"));
  addHtml(F("      if (data.length > 0) location.reload();"));
  addHtml(F("    });"));
  addHtml(F("  }"));
  addHtml(F("}"));
  addHtml(F("setInterval(refreshData, 2000);"));
  addHtml(F("</script>"));

  sendHeadandTail_stdtemplate(_TAIL);
  TXBuffer.endStream();
}

void renderSerialSettings() {
  addHtml(F("<div class='settings-panel'>"));
  addHtml(F("<div class='panel-title'>⚙️ Serial Ayarları"));
  addHtml(F("<span class='status-indicator "));
  addHtml(serialMonitorActive ? F("status-active'>AÇIK") : F("status-inactive'>KAPALI"));
  addHtml(F("</span></div>"));

  html_add_form();
  addHtml(F("<div class='settings-row'>"));
  
  // Baud Rate
  addHtml(F("<div class='settings-group'>"));
  addHtml(F("<div class='settings-label'>Baud Rate</div>"));
  addHtml(F("<select name='baudrate' class='settings-input'>"));
  const uint32_t baudRates[] = {1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200, 230400, 460800, 921600};
  for (uint8_t i = 0; i < 11; i++) {
    addHtml(F("<option value='"));
    addHtmlInt(baudRates[i]);
    addHtml(F("'"));
    if (serialBaudRate == baudRates[i]) addHtml(F(" selected"));
    addHtml(F(">"));
    addHtmlInt(baudRates[i]);
    addHtml(F("</option>"));
  }
  addHtml(F("</select>"));
  addHtml(F("</div>"));
  
  // Data Bits
  addHtml(F("<div class='settings-group'>"));
  addHtml(F("<div class='settings-label'>Format</div>"));
  addHtml(F("<select name='databits' class='settings-input'>"));
  addHtml(F("<option value='8'"));
  if (serialDataBits == 8) addHtml(F(" selected"));
  addHtml(F(">8N1</option>"));
  addHtml(F("<option value='7'"));
  if (serialDataBits == 7) addHtml(F(" selected"));
  addHtml(F(">7N1</option>"));
  addHtml(F("</select>"));
  addHtml(F("</div>"));
  
  addHtml(F("</div>"));

  // Control buttons
  addHtml(F("<div class='settings-row'>"));
  addHtml(F("<button type='submit' name='savesettings' value='1' class='btn btn-small'>💾 Ayarları Kaydet</button>"));
  if (serialMonitorActive) {
    addHtml(F("<button type='submit' name='stopmonitor' value='1' class='btn btn-danger btn-small'>⏹️ Durdur</button>"));
  } else {
    addHtml(F("<button type='submit' name='startmonitor' value='1' class='btn btn-success btn-small'>▶️ Başlat</button>"));
  }
  addHtml(F("<button type='submit' name='clearbuffer' value='1' class='btn btn-warning btn-small'>🗑️ Temizle</button>"));
  addHtml(F("</div>"));
  
  // Hidden form fields for pattern marking
  addHtml(F("<input type='hidden' name='selectedhex' value=''>"));
  addHtml(F("<input type='hidden' name='markstart' value='0'>"));
  addHtml(F("<input type='hidden' name='markend' value='0'>"));
  
  html_end_form();
  addHtml(F("</div>"));
}

void renderSerialDisplay() {
  addHtml(F("<div class='data-display'>"));
  addHtml(F("<div class='data-header'>📊 Gelen Veri - Hex Bytes'ları Seçebilirsiniz (Son "));
  addHtmlInt(min(totalPackets, (uint16_t)30));
  addHtml(F(" paket)</div>"));

  if (totalPackets == 0) {
    addHtml(F("<div class='data-row' style='text-align:center;color:#666;padding:40px;'>"));
    addHtml(F("Henüz veri alınmadı. Serial monitörü başlatın."));
    addHtml(F("</div>"));
  } else {
    // Show last 30 packets
    uint16_t startIndex = totalPackets > 30 ? (serialPacketIndex + SERIAL_BUFFER_SIZE - 30) % SERIAL_BUFFER_SIZE : 0;
    uint16_t count = min(totalPackets, (uint16_t)30);
    
    for (uint16_t i = 0; i < count; i++) {
      uint16_t index = (startIndex + i) % SERIAL_BUFFER_SIZE;
      SerialDataPacket& packet = serialPackets[index];
      
      addHtml(F("<div class='data-row'>"));
      
      // Timestamp ve length
      addHtml(F("<span class='timestamp'>"));
      addHtml(packet.timestamp);
      addHtml(F("</span>"));
      addHtml(F("<span class='data-type'>["));
      addHtmlInt(packet.dataLength);
      addHtml(F(" bytes]</span>"));
      
      // Special characters indicators
      if (packet.hasSTX) addHtml(F("<span class='special-char'>STX</span>"));
      if (packet.hasETX) addHtml(F("<span class='special-char'>ETX</span>"));
      if (packet.hasCR) addHtml(F("<span class='special-char'>CR</span>"));
      if (packet.hasLF) addHtml(F("<span class='special-char'>LF</span>"));
      
      addHtml(F("<br>"));
      
      // ASCII data
      addHtml(F("<span class='data-type'>ASCII:</span> <span class='ascii-data'>"));
      addHtml(packet.asciiData);
      addHtml(F("</span><br>"));
      
      // HEX data - SEÇİLEBİLİR BYTES
      addHtml(F("<span class='data-type'>HEX:</span><br>"));
      addHtml(F("<div class='hex-data'>"));
      
      // Her hex byte'ı ayrı span olarak göster
      for (int j = 0; j < packet.rawData.length(); j++) {
        uint8_t byte = packet.rawData.charAt(j);
        String hexStr = "";
        if (byte < 16) hexStr += "0";
        hexStr += String(byte, HEX);
        hexStr.toUpperCase();
        
        addHtml(F("<span class='hex-byte"));
        
        // Pattern matching kontrolü
        if (!detectedStartPattern.isEmpty() && packet.hexData.indexOf(detectedStartPattern) >= 0) {
          addHtml(F(" start-pattern"));
        }
        if (!detectedEndPattern.isEmpty() && packet.hexData.indexOf(detectedEndPattern) >= 0) {
          addHtml(F(" end-pattern"));
        }
        
        addHtml(F("' onclick='selectHexByte(this, \""));
        addHtml(hexStr);
        addHtml(F("\")' title='0x"));
        addHtml(hexStr);
        addHtml(F(" ("));
        addHtmlInt(byte);
        addHtml(F(")'>"));
        addHtml(hexStr);
        addHtml(F("</span>"));
        
        if ((j + 1) % 16 == 0) addHtml(F("<br>"));
      }
      
      addHtml(F("</div>"));
      addHtml(F("</div>"));
    }
  }
  
  addHtml(F("</div>"));
}

void renderPatternPanel() {
  addHtml(F("<div class='pattern-panel'>"));
  addHtml(F("<div class='panel-title'>🎯 Pattern Analizi"));
  if (autoDetectMode) {
    addHtml(F("<span class='status-indicator status-active'>AUTO</span>"));
  }
  addHtml(F("</div>"));

  html_add_form();
  
  // Seçilen pattern gösterimi
  addHtml(F("<div style='margin-bottom:12px;'>"));
  addHtml(F("<div class='settings-label'>Seçilen Bytes:</div>"));
  addHtml(F("<input type='text' id='selectedPattern' class='pattern-input' placeholder='Hex bytes seçin...' readonly>"));
  addHtml(F("</div>"));
  
  // Pattern marking buttons
  addHtml(F("<div style='display:flex;gap:6px;margin-bottom:12px;'>"));
  addHtml(F("<button type='button' onclick='markAsStart()' class='btn btn-success btn-small'>📍 Başlangıç</button>"));
  addHtml(F("<button type='button' onclick='markAsEnd()' class='btn btn-danger btn-small'>🏁 Bitiş</button>"));
  addHtml(F("</div>"));
  
  // Current patterns
  if (!detectedStartPattern.isEmpty() || !detectedEndPattern.isEmpty()) {
    addHtml(F("<div class='pattern-detected'>"));
    addHtml(F("<div style='font-size:11px;color:#cccccc;margin-bottom:6px;'>Tespit Edilen:</div>"));
    if (!detectedStartPattern.isEmpty()) {
      addHtml(F("<div style='font-size:10px;color:#107c10;'>🟢 Start: "));
      addHtml(detectedStartPattern);
      addHtml(F("</div>"));
    }
    if (!detectedEndPattern.isEmpty()) {
      addHtml(F("<div style='font-size:10px;color:#d13438;'>🔴 End: "));
      addHtml(detectedEndPattern);
      addHtml(F("</div>"));
    }
    addHtml(F("</div>"));
  }
  
  // Manual pattern input
  addHtml(F("<div style='margin-bottom:8px;'>"));
  addHtml(F("<div class='settings-label'>Manuel Pattern:</div>"));
  addHtml(F("<input type='text' name='customstart' class='pattern-input' placeholder='Start hex (örn: 02 FF)' value='"));
  addHtml(detectedStartPattern);
  addHtml(F("'>"));
  addHtml(F("<input type='text' name='customend' class='pattern-input' placeholder='End hex (örn: 03 0D)' value='"));
  addHtml(detectedEndPattern);
  addHtml(F("'>"));
  addHtml(F("</div>"));
  
  // Action buttons
  addHtml(F("<div style='display:flex;gap:4px;flex-wrap:wrap;'>"));
  addHtml(F("<button type='submit' name='savepattern' value='1' class='btn btn-small'>💾 Kaydet</button>"));
  addHtml(F("<button type='submit' name='autodetect' value='1' class='btn btn-small'>"));
  addHtml(autoDetectMode ? F("⏹️ Auto") : F("🤖 Auto"));
  addHtml(F("</button>"));
  addHtml(F("</div>"));
  
  html_end_form();
  addHtml(F("</div>"));
}

void renderFrequencyPanel() {
  addHtml(F("<div class='pattern-panel'>"));
  addHtml(F("<div class='panel-title'>📈 Karakter Frekansı</div>"));
  
  addHtml(F("<div class='frequency-chart'>"));
  
  // En sık kullanılan 10 karakteri göster
  uint16_t maxFreq = 0;
  for (int i = 0; i < 256; i++) {
    if (charFrequency[i] > maxFreq) maxFreq = charFrequency[i];
  }
  
  if (maxFreq > 0) {
    uint8_t count = 0;
    for (int i = 0; i < 256 && count < 10; i++) {
      if (charFrequency[i] > 0) {
        addHtml(F("<div class='char-freq'>"));
        addHtml(F("<div style='display:flex;align-items:center;'>"));
        addHtml(F("<span class='char-display'>"));
        
        // Karakter gösterimi
        if (i >= 32 && i <= 126) {
          addHtml(String(char(i)));
        } else {
          addHtml(F("["));
          if (i < 16) addHtml(F("0"));
          addHtml(String(i, HEX));
          addHtml(F("]"));
        }
        
        addHtml(F("</span>"));
        addHtml(F("<span style='margin-left:6px;font-size:9px;color:#999;'>"));
        addHtmlInt(charFrequency[i]);
        addHtml(F("</span>"));
        addHtml(F("</div>"));
        
        // Frequency bar
        addHtml(F("<div class='freq-bar' style='width:"));
        addHtmlInt((charFrequency[i] * 50) / maxFreq);
        addHtml(F("px;'></div>"));
        addHtml(F("</div>"));
        count++;
      }
    }
  } else {
    addHtml(F("<div style='text-align:center;color:#666;font-size:10px;padding:20px;'>"));
    addHtml(F("Henüz veri analizi yok"));
    addHtml(F("</div>"));
  }
  
  addHtml(F("</div>"));
  addHtml(F("</div>"));
}

void renderSendPanel() {
  addHtml(F("<div class='send-panel'>"));
  addHtml(F("<div class='panel-title'>📤 Veri Gönder</div>"));
  html_add_form();
  addHtml(F("<div class='send-row'>"));
  addHtml(F("<input type='text' name='sendtext' class='send-input' placeholder='Göndermek istediğiniz veriyi girin...'>"));
  addHtml(F("<button type='submit' name='senddata' value='1' class='btn btn-success btn-small'>Gönder</button>"));
  addHtml(F("</div>"));
  addHtml(F("<div class='checkbox-group'>"));
  addHtml(F("<div class='checkbox-item'><input type='checkbox' name='addcr' id='addcr'><label for='addcr'>CR</label></div>"));
  addHtml(F("<div class='checkbox-item'><input type='checkbox' name='addlf' id='addlf' checked><label for='addlf'>LF</label></div>"));
  addHtml(F("<div class='checkbox-item'><input type='checkbox' name='sendhex' id='sendhex'><label for='sendhex'>Hex</label></div>"));
  addHtml(F("</div>"));
  html_end_form();
  addHtml(F("</div>"));
}

void renderStatsPanel() {
  addHtml(F("<div class='stats-panel'>"));
  addHtml(F("<div class='stats-row'>"));
  addHtml(F("<div class='stat-item'><div class='stat-value'>"));
  addHtmlInt(totalPackets);
  addHtml(F("</div><div class='stat-label'>Toplam Paket</div></div>"));
  addHtml(F("<div class='stat-item'><div class='stat-value'>"));
  addHtmlInt(serialBaudRate);
  addHtml(F("</div><div class='stat-label'>Baud Rate</div></div>"));
  addHtml(F("<div class='stat-item'><div class='stat-value'>"));
  addHtmlInt(serialBuffer.length());
  addHtml(F("</div><div class='stat-label'>Buffer Size</div></div>"));
  addHtml(F("<div class='stat-item'><div class='stat-value'>"));
  addHtml(serialMonitorActive ? F("AÇIK") : F("KAPALI"));
  addHtml(F("</div><div class='stat-label'>Durum</div></div>"));
  addHtml(F("</div>"));
  addHtml(F("</div>"));
}

// Otomatik pattern analizi - STRING CONCATENATION DÜZELTİLDİ
void analyzePatterns() {
  // En sık kullanılan karakterleri bul
  uint16_t maxFreq = 0;
  uint8_t mostFrequentChar = 0;
  
  for (int i = 0; i < 256; i++) {
    if (charFrequency[i] > maxFreq) {
      maxFreq = charFrequency[i];
      mostFrequentChar = i;
    }
  }
  
  // Başlangıç karakteri tespiti
  for (int i = 0; i < 32; i++) {
    if (charFrequency[i] > 5 && charFrequency[i] < maxFreq / 2) {
      String hexStr = "";
      if (i < 16) hexStr += "0";
      hexStr += String(i, HEX);
      hexStr.toUpperCase();
      
      if (detectedStartPattern.isEmpty()) {
        detectedStartPattern = hexStr;
        String logMsg = String(F("Auto-detected start pattern: ")) + hexStr;
        addLog(LOG_LEVEL_INFO, logMsg);
      }
    }
  }
  
  // Bitiş karakteri tespiti
  if (charFrequency[0x0D] > 0 || charFrequency[0x0A] > 0) {
    if (charFrequency[0x0D] > 0) detectedEndPattern = "0D";
    else if (charFrequency[0x0A] > 0) detectedEndPattern = "0A";
  }
}

// Process incoming serial data
void processSerialData() {
  if (!serialMonitorActive) return;
  
  while (Serial1.available()) {
    uint8_t receivedByte = Serial1.read();
    
    // Karakter frekansını güncelle
    charFrequency[receivedByte]++;
    
    static String currentPacket = "";
    static unsigned long lastByteTime = 0;
    unsigned long currentTime = millis();
    
    // Timeout kontrolü (500ms sessizlik = yeni paket)
    if (currentTime - lastByteTime > 500 && !currentPacket.isEmpty()) {
      createSerialPacket(currentPacket);
      currentPacket = "";
    }
    
    currentPacket += char(receivedByte);
    lastByteTime = currentTime;
    
    // Check for packet end
    if (receivedByte == '\r' || receivedByte == '\n' || receivedByte == 0x03 || currentPacket.length() > 200) {
      if (!currentPacket.isEmpty()) {
        createSerialPacket(currentPacket);
        currentPacket = "";
      }
    }
    
    // Auto-pattern detection
    if (autoDetectMode && totalPackets % 10 == 0) {
      analyzePatterns();
    }
  }
}

void createSerialPacket(const String& packetData) {
  SerialDataPacket packet;
  
  // TIMESTAMP DÜZELTİLDİ - basit timestamp
  packet.timestamp = String(millis() / 1000) + "s";
  
  packet.rawData = packetData;
  packet.dataLength = packetData.length();
  
  // Analyze special characters
  packet.hasSTX = packetData.indexOf('\x02') >= 0;
  packet.hasETX = packetData.indexOf('\x03') >= 0;
  packet.hasCR = packetData.indexOf('\r') >= 0;
  packet.hasLF = packetData.indexOf('\n') >= 0;
  
  // Generate ASCII and HEX representations
  packet.asciiData = "";
  packet.hexData = "";
  
  for (int i = 0; i < packetData.length(); i++) {
    uint8_t byte = packetData.charAt(i);
    
    // ASCII representation - TÜM KARAKTERLER
    if (byte >= 32 && byte <= 126) {
      packet.asciiData += char(byte);
    } else if (byte == 0) {
      packet.asciiData += F("[NULL]");
    } else {
      switch (byte) {
        case '\r': packet.asciiData += F("\\r"); break;
        case '\n': packet.asciiData += F("\\n"); break;
        case '\t': packet.asciiData += F("\\t"); break;
        case '\x02': packet.asciiData += F("[STX]"); break;
        case '\x03': packet.asciiData += F("[ETX]"); break;
        case '\x04': packet.asciiData += F("[EOT]"); break;
        case '\x06': packet.asciiData += F("[ACK]"); break;
        case '\x15': packet.asciiData += F("[NAK]"); break;
        case '\x1B': packet.asciiData += F("[ESC]"); break;
        default: 
          packet.asciiData += F("[0x");
          if (byte < 16) packet.asciiData += "0";
          packet.asciiData += String(byte, HEX);
          packet.asciiData += F("]");
          break;
      }
    }
    
    // HEX representation
    if (byte < 16) packet.hexData += "0";
    packet.hexData += String(byte, HEX);
    packet.hexData.toUpperCase();
    packet.hexData += " ";
  }
  
  // Store packet
  serialPackets[serialPacketIndex] = packet;
  serialPacketIndex = (serialPacketIndex + 1) % SERIAL_BUFFER_SIZE;
  totalPackets++;
  
  // Add to log - STRING CONCATENATION DÜZELTİLDİ
  String logMsg = String(F("Serial1 RX [")) + String(packet.dataLength) + String(F("]: ")) + packet.asciiData;
  addLog(LOG_LEVEL_DEBUG, logMsg);
}

// AJAX endpoint for real-time data
void handle_serial_data_ajax() {
  if (!isLoggedIn()) {
    web_server.send(401, F("text/plain"), F("Unauthorized"));
    return;
  }
  
  String response = "";
  
  if (totalPackets > 0) {
    response += F("{\"packets\":");
    response += String(totalPackets);
    response += F(",\"active\":");
    response += serialMonitorActive ? F("true") : F("false");
    response += F("}");
  }
  
  web_server.send(200, F("application/json"), response);
}

#endif // WEBSERVER_SETUP