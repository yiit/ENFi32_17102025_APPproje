#include "_Plugin_Helper.h"

#ifdef USES_P120

#include "src/PluginStructs/P120_data_struct.h"
#include "src/Commands/InternalCommands.h"
#include "src/Commands/ExecuteCommand.h"

// P120 lokal değişkenler (ESPEasy-Globals'tan kaldırıldı)
float webapinettartim = 0.0;
float webapinettartim_son = 0.0;
float webapidaratartim = 0.0;
float webapibruttartim = 0.0;
float webapiadet = 0.0;
float webapibfiyat = 0.0;
float webapitutar = 0.0;
float top_net = 0.0;
float top_dara = 0.0;
float top_brut = 0.0;
long sno = 0;
long seri_no = 1;
int webapiurunno = 1;
int topla_i = 1;  // Default: art modu
int sec_URUN_buton = 0;
int sec_M_buton[8] = {0, 0, 0, 0, 0, 0, 0, 0};

// Button constants
const unsigned long LONGPRESS_TIME = 1000;

//#######################################################################################################
//##################################### Plugin 120: FYZ  ################################################
//#######################################################################################################

#define PLUGIN_120
#define PLUGIN_ID_120 120
#define PLUGIN_NAME_120 "Printer - FYZ"
#define PLUGIN_VALUENAME1_120 "NET"         // 3 char (max 12)
#define PLUGIN_VALUENAME2_120 "DARA"        // 4 char (max 12)  
#define PLUGIN_VALUENAME3_120 "BRUT"        // 4 char (max 12)
#define PLUGIN_VALUENAME4_120 "ADET"        // 4 char (max 12)
// Kullanılmayan değişken tanımları kaldırıldı (4 değişken ile sınırlandırıldı)
#define MAX_SRV_CLIENTS 5

// P120 uses PluginStruct for clean ESPEasy integration

#define CUSTOMTASK2_STR_SIZE_P120 46

#define HEDEF_ADDR_SIZE_P120 8
#define FIS_BASLIK1_ADDR_SIZE_P120 44
#define FIS_BASLIK2_ADDR_SIZE_P120 44
#define FIS_BASLIK3_ADDR_SIZE_P120 44
#define FIS_BASLIK4_ADDR_SIZE_P120 44

#define MES_BUFF_SIZE_P120 19
#define HEDEF_BUFF_SIZE_P120 9
#define FIS_BASLIK1_BUFF_SIZE_P120 45
#define FIS_BASLIK2_BUFF_SIZE_P120 45
#define FIS_BASLIK3_BUFF_SIZE_P120 45
#define FIS_BASLIK4_BUFF_SIZE_P120 45

#define FIS_BASLIK1_DEF_P120 "fisbasligi 1"
#define FIS_BASLIK2_DEF_P120 "fisbasligi 2"
#define FIS_BASLIK3_DEF_P120 "fisbasligi 3"
#define FIS_BASLIK4_DEF_P120 "fisbasligi 4"

#define FYZ_Model ExtraTaskSettings.TaskDevicePluginConfigLong[0]
#define FYZ_Indikator ExtraTaskSettings.TaskDevicePluginConfigLong[1]
#define FYZ_Mod ExtraTaskSettings.TaskDevicePluginConfigLong[2]
#define FYZ_Buton1 ExtraTaskSettings.TaskDevicePluginConfigLong[3]
#define FYZ_Buton2 ExtraTaskSettings.TaskDevicePluginConfigLong[4]
#define FYZ_Gecikme ExtraTaskSettings.TaskDevicePluginConfigLong[5]
#define FYZ_Sayac ExtraTaskSettings.TaskDevicePluginConfigLong[6]

#define FYZ_Kopya ExtraTaskSettings.TaskDevicePluginConfig[0]
#define FYZ_Logo ExtraTaskSettings.TaskDevicePluginConfig[1]

// Buffer Size Tanımları
#define MES_BUFF_SIZE_P120 19
#define HEDEF_BUFF_SIZE_P120 9
#define FIS_BASLIK1_BUFF_SIZE_P120 45
#define FIS_BASLIK2_BUFF_SIZE_P120 45
#define FIS_BASLIK3_BUFF_SIZE_P120 45
#define FIS_BASLIK4_BUFF_SIZE_P120 45

// FYZ komutları için TaskDeviceName yerine basit string değişkenler kullan
String fyz_art_komut = "";
String fyz_tek_komut = "";  
String fyz_top_komut = "";
#ifdef CAS_VERSION
#define FYZ_fisbaslik1 ExtraTaskSettings.TaskDeviceBaslik[0]
#define FYZ_fisbaslik2 ExtraTaskSettings.TaskDeviceBaslik[1]
#define FYZ_fisbaslik3 ExtraTaskSettings.TaskDeviceBaslik[2]
#define FYZ_fisbaslik4 ExtraTaskSettings.TaskDeviceBaslik[3]
#endif

#define FYZ_Hedef PCONFIG_FLOAT(0)

int fyz_stabil_sayisi = 0;

#if FEATURE_ETHERNET
#define FYZ_BUTTON1_PIN 14
#define FYZ_BUTTON2_PIN 15
#endif

// Button handling değişkenleri
static bool fyz_button1_last_state = HIGH;
static bool fyz_button2_last_state = HIGH;
static bool fyz_button1_pressed = HIGH;
static bool fyz_button2_pressed = HIGH;
static unsigned long fyz_button1_press_time = 0;
static unsigned long fyz_button2_press_time = 0;

// ---- Stabil tetikleyici parametreleri ----
static const uint16_t LOOP_HZ        = 10;      // 10 Hz
static const uint32_t COOLDOWN_MS    = 400;     // ms
static const int32_t  EPSs           = 1;       // 1 count deadband

static uint32_t last_cmd_ts = 0;

// Tek isim: last_fire_w (eski last_art_w yerine)
static float last_art_w = NAN;                // Son tetik ağırlığı
static const float REL_DELTA_THRESH = 0.30f;   // %30 eşik

// ---- FIFO (Ring Buffer) + O(1) kayan ortalama ----
static float    rb_buf[129];
static uint16_t rb_size  = 0;
static uint16_t rb_count = 0;
static uint16_t rb_index = 0;
static double   rb_sum   = 0.0;

// İlk tetik için isnan(last_fire_w) yeterliyse, rearm_ok false başlasın:
static bool rearm_ok = false;

// Pencereyi ayarlamak / sıfırlamak
static void RB_setWindow(uint16_t win)
{
  rb_size  = constrain(win, (uint16_t)1, (uint16_t)129);
  rb_count = 0;
  rb_index = 0;
  rb_sum   = 0.0;
}

// Her yeni örnekte çağır
static inline float RB_pushSample(float s)
{
  if (rb_size == 0) {
    // FYZ_Sayac tanımlı değilse derleme hatası almayın diye 8 gibi güvenli bir varsayılan atayabilirsiniz:
    const uint16_t def_win = 8; // veya mevcut FYZ_Sayac değeriniz
    RB_setWindow(def_win);
  }

  if (rb_count < rb_size) {
    rb_buf[rb_count++] = s;
    rb_sum += s;
  } else {
    float old = rb_buf[rb_index];
    rb_sum -= old;
    rb_buf[rb_index] = s;
    rb_sum += s;

    rb_index++;
    if (rb_index >= rb_size) rb_index = 0;
  }

  return (float)(rb_sum / (double)rb_count);
}

// ExecuteCommand_all alternatifi (basit) - doğru parametre sırası
void ExecuteCommand_all_P120(const String& cmd) {
  ExecuteCommand_all({EventValueSource::Enum::VALUE_SOURCE_WEB_FRONTEND, cmd});
}

// İndikator seçimi dummy fonksiyon
void indikator_secimi(struct EventStruct *event, int indicator, const String& webname) {
  // Basit implementasyon - sonra genişletilebilir
  addFormCheckBox(F("Indikator Aktif"), webname, indicator > 0);
}

// P120 ExtraTaskSettings Helper Functions
void P120_updateTaskValues(struct EventStruct *event) {
  // Float değerleri long'a çevir (1000 ile çarp)
  P120_CONFIG_DARA_VALUE = (long)(webapidaratartim * 1000);
  P120_CONFIG_BRUT_VALUE = (long)(webapibruttartim * 1000);
  P120_CONFIG_ADET_VALUE = (long)webapiadet;
  P120_CONFIG_TOP_NET_VALUE = (long)(top_net * 1000);
  P120_CONFIG_TOP_DARA_VALUE = (long)(top_dara * 1000);
  P120_CONFIG_TOP_BRUT_VALUE = (long)(top_brut * 1000);
  P120_CONFIG_SERI_NO = seri_no;
  P120_CONFIG_BIRIM_FIYAT = (long)(webapibfiyat * 1000);
  P120_CONFIG_TUTAR = (long)(webapitutar * 1000);
  
  // Dinamik value tipine göre UserVar'a ata
  for (uint8_t varIndex = 0; varIndex < 4; varIndex++) {
    byte valueType = (P120_CONFIG_NET_VALUE >> (varIndex * 2)) & 0x03;
    
    switch (varIndex) {
      case 0: // NET değeri
        switch (valueType) {
          case 0: UserVar.setFloat(event->TaskIndex, varIndex, webapinettartim); break; // Float
          case 1: UserVar.setInt32(event->TaskIndex, varIndex, (int32_t)webapinettartim); break; // Integer
          case 2: UserVar.setFloat(event->TaskIndex, varIndex, webapinettartim); break; // String (float olarak sakla)
          case 3: UserVar.setInt32(event->TaskIndex, varIndex, webapinettartim > 0.0 ? 1 : 0); break; // Boolean
        }
        break;
      case 1: // DARA değeri
        switch (valueType) {
          case 0: UserVar.setFloat(event->TaskIndex, varIndex, webapidaratartim); break;
          case 1: UserVar.setInt32(event->TaskIndex, varIndex, (int32_t)webapidaratartim); break;
          case 2: UserVar.setFloat(event->TaskIndex, varIndex, webapidaratartim); break;
          case 3: UserVar.setInt32(event->TaskIndex, varIndex, webapidaratartim > 0.0 ? 1 : 0); break;
        }
        break;
      case 2: // BRUT değeri
        switch (valueType) {
          case 0: UserVar.setFloat(event->TaskIndex, varIndex, webapibruttartim); break;
          case 1: UserVar.setInt32(event->TaskIndex, varIndex, (int32_t)webapibruttartim); break;
          case 2: UserVar.setFloat(event->TaskIndex, varIndex, webapibruttartim); break;
          case 3: UserVar.setInt32(event->TaskIndex, varIndex, webapibruttartim > 0.0 ? 1 : 0); break;
        }
        break;
      case 3: // ADET değeri
        switch (valueType) {
          case 0: UserVar.setFloat(event->TaskIndex, varIndex, webapiadet); break;
          case 1: UserVar.setInt32(event->TaskIndex, varIndex, (int32_t)webapiadet); break;
          case 2: UserVar.setFloat(event->TaskIndex, varIndex, webapiadet); break;
          case 3: UserVar.setInt32(event->TaskIndex, varIndex, webapiadet > 0.0 ? 1 : 0); break;
        }
        break;
    }
  }
}

String P120_getValueByName(const String& valueName) {
  // Value name'e göre değer döndür (%NAME% formatında kullanılabilir)
  if (valueName.equalsIgnoreCase("NET")) return String(webapinettartim, 2);
  if (valueName.equalsIgnoreCase("DARA")) return String(webapidaratartim, 2);
  if (valueName.equalsIgnoreCase("BRUT")) return String(webapibruttartim, 2);
  if (valueName.equalsIgnoreCase("ADET")) return String((int)webapiadet);
  if (valueName.equalsIgnoreCase("TOP_NET")) return String(top_net, 2);
  if (valueName.equalsIgnoreCase("TOP_DARA")) return String(top_dara, 2);
  if (valueName.equalsIgnoreCase("TOP_BRUT")) return String(top_brut, 2);
  if (valueName.equalsIgnoreCase("SERI_NO")) return String(seri_no);
  if (valueName.equalsIgnoreCase("STABIL")) return (P120_CONFIG_STABIL_MODE == 1) ? "ST" : "US";
  if (valueName.equalsIgnoreCase("TARIH")) return node_time.getDateString('-');
  if (valueName.equalsIgnoreCase("SAAT")) return node_time.getTimeString(':');
  if (valueName.equalsIgnoreCase("URUN_NO")) return String(webapiurunno);
  if (valueName.equalsIgnoreCase("BIRIM_FIYAT")) return String(webapibfiyat, 2);
  if (valueName.equalsIgnoreCase("TUTAR")) return String(webapitutar, 2);
  
  return "0.00"; // Varsayılan
}

// Basit button handling fonksiyonları (OneButton yerine)
void handleButtons() {
  static unsigned long debounceTime1 = 0;
  static unsigned long debounceTime2 = 0;
  static const unsigned long DEBOUNCE_DELAY = 50;  // 50ms debounce
  static const unsigned long LONGPRESS_TIME = 1000; // 1 saniye long press
  
  // Button 1 handling
  bool button1_state = digitalRead(FYZ_BUTTON1_PIN);
  if (button1_state != fyz_button1_last_state) {
    debounceTime1 = millis();
  }
  
  if ((millis() - debounceTime1) > DEBOUNCE_DELAY) {
    if (button1_state != fyz_button1_pressed) {
      fyz_button1_pressed = button1_state;
      
      if (fyz_button1_pressed == LOW) { // Button pressed (pull-up)
        fyz_button1_press_time = millis();
      } else { // Button released
        unsigned long press_duration = millis() - fyz_button1_press_time;
        if (press_duration > LONGPRESS_TIME) {
          // Long press
          Settings.UseSerial = !Settings.UseSerial; // Toggle serial
        } else {
          // Short press - Art yazdırma
          ExecuteCommand_all_P120(F("fyzart"));
        }
      }
    }
  }
  fyz_button1_last_state = button1_state;
  
  // Button 2 handling  
  bool button2_state = digitalRead(FYZ_BUTTON2_PIN);
  if (button2_state != fyz_button2_last_state) {
    debounceTime2 = millis();
  }
  
  if ((millis() - debounceTime2) > DEBOUNCE_DELAY) {
    if (button2_state != fyz_button2_pressed) {
      fyz_button2_pressed = button2_state;
      
      if (fyz_button2_pressed == LOW) { // Button pressed
        fyz_button2_press_time = millis();
      } else { // Button released
        unsigned long press_duration = millis() - fyz_button2_press_time;
        if (press_duration > LONGPRESS_TIME) {
          // Long press - Kopya yazdırma
          ExecuteCommand_all_P120(F("fyzkop"));
        } else {
          // Short press - Toplam yazdırma
          ExecuteCommand_all_P120(F("fyztop"));
          if (FYZ_Kopya) {
            ExecuteCommand_all_P120(F("fyzkop"));
          }
        }
      }
    }
  }
  fyz_button2_last_state = button2_state;
}

int fyzurun_stabil_sayisi = 0;

boolean Plugin_120(byte function, struct EventStruct *event, String &string) {
  boolean success = false;
  switch (function) {
    case PLUGIN_DEVICE_ADD:
      {
        Device[++deviceCount].Number = PLUGIN_ID_120;
        Device[deviceCount].Type = DEVICE_TYPE_DUMMY;
        Device[deviceCount].FormulaOption = true;
        Device[deviceCount].ValueCount = 4;                    // Task sayısı: 4'e düşürüldü
        Device[deviceCount].SendDataOption = false;
        Device[deviceCount].TimerOption = false;
        Device[deviceCount].GlobalSyncOption = false;
        Device[deviceCount].Custom = true;                     // 32 değişken için CustomTaskSettings aktif
        Device[deviceCount].VType = Sensor_VType::SENSOR_TYPE_ULONG; // ULONG değer tipi (ESPEasy standard)
        Device[deviceCount].DecimalsOnly = false;              // Ondalık sınırlaması yok
        Device[deviceCount].ExitTaskBeforeSave = false;        // Kayıt öncesi çıkış gerekmiyor
        break;
      }
    case PLUGIN_GET_DEVICENAME:
      {
        string = F(PLUGIN_NAME_120);
        break;
      }
    case PLUGIN_GET_DEVICEVALUENAMES:
      {
        // 4 değişken ile sınırlandırıldı (12 karakter max)
        strcpy_P(ExtraTaskSettings.TaskDeviceValueNames[0], PSTR(PLUGIN_VALUENAME1_120));  // NET (max 12 char)
        strcpy_P(ExtraTaskSettings.TaskDeviceValueNames[1], PSTR(PLUGIN_VALUENAME2_120));  // DARA (max 12 char)
        strcpy_P(ExtraTaskSettings.TaskDeviceValueNames[2], PSTR(PLUGIN_VALUENAME3_120));  // BRUT (max 12 char)
        strcpy_P(ExtraTaskSettings.TaskDeviceValueNames[3], PSTR(PLUGIN_VALUENAME4_120));  // ADET (max 12 char)
        break;
      }
    case PLUGIN_WEBFORM_LOAD:
      {
        // ========== 1. TASK NAME ==========
        addFormSubHeader(F("📝 Görev Ayarları"));
        
        // ========== 2. DEVICE ENABLE/DISABLE ==========  
        addFormSubHeader(F("⚙️ Device Durumu"));
        addFormCheckBox(F("Device Aktif"), F("plugin_120_enabled"), Settings.TaskDeviceEnabled[event->TaskIndex]);
        addFormNote(F("Device'ı aktif/pasif yapabilirsiniz"));
        
        // ========== 3. PRINTER TYPE SELECTION ==========
        addFormSubHeader(F("🖨️ Yazıcı Türü"));
        
        byte printerType = FYZ_Indikator; // 0=Fiş, 1=Etiket
        const __FlashStringHelper *typeOptions[] = {
          F("📄 Fiş Yazıcı"),
          F("🏷️ Etiket Yazıcı")
        };
        constexpr int typeCount = NR_ELEMENTS(typeOptions);
        const FormSelectorOptions typeSelector(typeCount, typeOptions);
        typeSelector.addFormSelector(F("Yazıcı Türü"), F("plugin_120_type"), printerType);
        
        // ========== 4. MODEL SELECTION (Dinamik) ==========
        addFormSubHeader(F("📱 Model Seçimi"));
        
        // Model seçim notları
        addFormNote(F("Model 0-3: Fiş Yazıcıları, Model 4-7: Etiket Yazıcıları"));
        addFormNote(F("💡 Yazıcı türüne uygun model seçin"));
        
        // Model dropdown (ESPEasy standard selector kullan)
        byte modelChoice = FYZ_Model;
        
        // Tüm model seçenekleri
        const __FlashStringHelper *allModels[] = {
          F("FYZ58"),    // 0 - Fiş
          F("FYZ80"),    // 1 - Fiş  
          F("FYZ58M"),   // 2 - Fiş
          F("FYZ80M"),   // 3 - Fiş
          F("EYZ72"),    // 4 - Etiket
          F("EYZ100"),   // 5 - Etiket
          F("EYZ72Mobil"), // 6 - Etiket
          F("EYZ100Mobil") // 7 - Etiket
        };
        constexpr int modelCount = NR_ELEMENTS(allModels);
        const FormSelectorOptions modelSelector(modelCount, allModels);
        modelSelector.addFormSelector(F("Model"), F("plugin_120_model"), modelChoice);
        
        // ========== 5. YÖNETİM AYARLARI ==========
        addFormSubHeader(F("🔧 Yönetim"));
        addFormCheckBox(F("Kopya Aktif"), F("plugin_120_kopya"), FYZ_Kopya);
        byte choice1 = FYZ_Mod;
        const __FlashStringHelper *options1[] = {
          F("Sürekli Veri"),
          F("Terazi Otomatik"),
          F("Dengeli Otomatik"),
          F("Terazi Tuş"),
          F("Yazıcı Tuş"),
          F("Kumanda"),
          F("Veri Paketi"),
        };
        constexpr int optionCount1 = NR_ELEMENTS(options1);
        const FormSelectorOptions selector120mod(optionCount1, options1);
        selector120mod.addFormSelector(F("Yazdırma Mod"), F("plugin_120_mod"), choice1);
        
        // FS root tanımı eksik - basitleştirelim
        String options2[10];
        
        addFormTextBox(F("Hedef Kilogram"), F("plugin_120_hedef"), String(FYZ_Hedef), HEDEF_BUFF_SIZE_P120);
        addFormNumericBox(F("Gecikme Saniye"), F("plugin_120_gecikme"), FYZ_Gecikme, 0, 999);
        addFormNumericBox(F("Veri Sayaç"), F("plugin_120_sayac"), FYZ_Sayac, 2, 129);
        
        if (FYZ_Mod == 5) {
          addFormTextBox(F("Artı Komutu"), getPluginCustomArgName(0), fyz_art_komut, MES_BUFF_SIZE_P120);
          addFormTextBox(F("Toplam Komutu"), getPluginCustomArgName(1), fyz_top_komut, MES_BUFF_SIZE_P120);
          addFormTextBox(F("Tek Komutu"), getPluginCustomArgName(2), fyz_tek_komut, MES_BUFF_SIZE_P120);
        }

#ifdef CAS_VERSION
        addFormTextBox(F("Fiş Başlığı 1"), getPluginCustomArgName(5), FYZ_fisbaslik1, FIS_BASLIK1_ADDR_SIZE_P120);
        addFormTextBox(F("Fiş Başlığı 2"), getPluginCustomArgName(6), FYZ_fisbaslik2, FIS_BASLIK2_ADDR_SIZE_P120);
        addFormTextBox(F("Fiş Başlığı 3"), getPluginCustomArgName(7), FYZ_fisbaslik3, FIS_BASLIK3_ADDR_SIZE_P120);
        addFormTextBox(F("Fiş Başlığı 4"), getPluginCustomArgName(8), FYZ_fisbaslik4, FIS_BASLIK4_ADDR_SIZE_P120);
#endif
        
        addFormSubHeader(F("İndikatör Ayarları"));
        // İndikator seçimi
        indikator_secimi(event, FYZ_Indikator, F("plugin_120_indikator"));
        addFormCheckBox(F("İndikatör Data Düzenleme"), F("duzenle"), PCONFIG(4));
        addFormNote(F("<font color='red'>Başlangıç-Bitiş Datasının Değişimine İzin Verir.</font>"));
        
        // P120 Dinamik Value Name Sistemi
        addFormSubHeader(F("Değişken İsimleri (Rules için %NAME% formatında)"));
        addFormNote(F("Bu isimler Rules'da %ISIM% şeklinde kullanılabilir. Boş bırakırsanız varsayılan isimler kullanılır."));
        
        // 4 ana değişken için custom isimler
        for (uint8_t varIndex = 0; varIndex < 4; varIndex++) {
          String label = F("Değişken ");
          label += (varIndex + 1);
          label += F(" İsmi");
          
          String fieldName = F("p120_varname_");
          fieldName += varIndex;
          
          String currentName = ExtraTaskSettings.TaskDeviceValueNames[varIndex];
          if (currentName.isEmpty()) {
            // Varsayılan isimler
            const char* defaults[] = {"NET", "DARA", "BRUT", "ADET"};
            currentName = defaults[varIndex];
          }
          
          addFormTextBox(label, fieldName, currentName, 15);
          
          // Value tipi seçimi (integer/float/string)
          String typeLabel = F("Değişken ");
          typeLabel += (varIndex + 1);
          typeLabel += F(" Tipi");
          
          String typeFieldName = F("p120_vartype_");
          typeFieldName += varIndex;
          
          byte currentType = (P120_CONFIG_NET_VALUE >> (varIndex * 2)) & 0x03; // 2 bit per variable
          const __FlashStringHelper *typeOptions[] = {
            F("Float (ondalık sayı)"),
            F("Integer (tam sayı)"),
            F("String (metin)"),
            F("Boolean (true/false)")
          };
          constexpr int typeOptionCount = NR_ELEMENTS(typeOptions);
          const FormSelectorOptions typeSelector(typeOptionCount, typeOptions);
          typeSelector.addFormSelector(typeLabel, typeFieldName, currentType);
        }
        
        success = true;
        break;
      }
    case PLUGIN_WEBFORM_SAVE:
      {
        // Optimize web form kayıt sistemi
        Settings.TaskDeviceEnabled[event->TaskIndex] = isFormItemChecked(F("plugin_120_enabled"));
        
        // Yazıcı türü ve model kaydet
        FYZ_Indikator = getFormItemInt(F("plugin_120_type")); // 0=Fiş, 1=Etiket  
        FYZ_Model = getFormItemInt(F("plugin_120_model"));
        FYZ_Kopya = isFormItemChecked(F("plugin_120_kopya"));
        
        // Eski alanlar için varsayılan değerler
        FYZ_Mod = getFormItemInt(F("plugin_120_mod"));
        FYZ_Hedef = getFormItemFloat(F("plugin_120_hedef"));
        FYZ_Gecikme = getFormItemInt(F("plugin_120_gecikme"));
        FYZ_Sayac = getFormItemInt(F("plugin_120_sayac"));
        
        if (FYZ_Mod == 5) {
          fyz_art_komut = webArg(getPluginCustomArgName(0));
          fyz_top_komut = webArg(getPluginCustomArgName(1));
          fyz_tek_komut = webArg(getPluginCustomArgName(2));
        }
        
#ifdef CAS_VERSION
        strncpy_webserver_arg(FYZ_fisbaslik1, getPluginCustomArgName(5));
        strncpy_webserver_arg(FYZ_fisbaslik2, getPluginCustomArgName(6));
        strncpy_webserver_arg(FYZ_fisbaslik3, getPluginCustomArgName(7));
        strncpy_webserver_arg(FYZ_fisbaslik4, getPluginCustomArgName(8));
#endif
        
        PCONFIG(4) = isFormItemChecked(F("duzenle"));
        
        // P120 Dinamik Value Name'leri kaydet
        for (uint8_t varIndex = 0; varIndex < 4; varIndex++) {
          String fieldName = F("p120_varname_");
          fieldName += varIndex;
          String newName = webArg(fieldName);
          
          if (newName.length() > 0 && newName.length() <= 15) {
            ExtraTaskSettings.setTaskDeviceValueName(varIndex, newName);
          }
          
          // Value tipini kaydet (2 bit per variable in long[0])
          String typeFieldName = F("p120_vartype_");
          typeFieldName += varIndex;
          byte valueType = getFormItemInt(typeFieldName);
          
          // Clear current type bits and set new ones
          long currentConfig = P120_CONFIG_NET_VALUE;
          currentConfig &= ~(0x03L << (varIndex * 2)); // Clear 2 bits
          currentConfig |= ((long)(valueType & 0x03)) << (varIndex * 2); // Set new 2 bits
          P120_CONFIG_NET_VALUE = currentConfig;
        }
        
        RB_setWindow((uint16_t)FYZ_Sayac);
        success = true;
        break;
      }
    case PLUGIN_INIT:
      {
        // Initialize P120 PluginStruct (ESPEasy pattern)
        initPluginTaskData(event->TaskIndex, new (std::nothrow) P120_data_struct());
        
        // Get PluginStruct ve ring buffer size ayarla
        P120_data_struct* pluginData = static_cast<P120_data_struct*>(getPluginTaskData(event->TaskIndex));
        if (pluginData != nullptr) {
          pluginData->setRingBufferSize(FYZ_Sayac > 0 ? FYZ_Sayac : 10);
        }
        
#ifdef CAS_VERSION
        XML_FIS_BASLIK1_S = FYZ_fisbaslik1;
        XML_FIS_BASLIK2_S = FYZ_fisbaslik2;
        XML_FIS_BASLIK3_S = FYZ_fisbaslik3;
        XML_FIS_BASLIK4_S = FYZ_fisbaslik4;
#endif
        // Printer Serial1 başlat (Settings.printer_txpin kullan)
        Serial1.begin(9600, SERIAL_8N1, -1, Settings.printer_txpin); // TX-only mode
        delay(50);
        
        // Button pins ESPEasy'de plugin initialize ediliyor - PluginStruct'ta init yok
        
        success = true;
        break;
      }

    case PLUGIN_EXIT:
      {
        // Clean up P120 PluginStruct (ESPEasy pattern)
        clearPluginTaskData(event->TaskIndex);
        success = true;
        break;
      }

    case PLUGIN_FIFTY_PER_SECOND:
      {
        // Get P120 PluginStruct instance (ESPEasy pattern)
        P120_data_struct* pluginData = static_cast<P120_data_struct*>(getPluginTaskData(event->TaskIndex));
        if (pluginData == nullptr) return success;
        
        // --- Button handling (basit GPIO) ---
        handleButtons();
        
        // --- İLK AÇILIŞTA ART MODU (topla_i=1) ---
        static bool topla_boot_init = false;
        if (!topla_boot_init) { topla_i = 1; topla_boot_init = true; }

        // --- 1) Görsel alanlar & sayısal kararlılık ölçümü ---
        // Eski XML char array'ler artık gerekli değil - ExtraTaskSettings kullanıyoruz

        const uint8_t dec = ExtraTaskSettings.TaskDeviceValueDecimals[0];
        static const int32_t SCALE_TAB[7] = { 1,10,100,1000,10000,100000,1000000 };
        const int32_t SCALE = (dec <= 6) ? SCALE_TAB[dec] : 1000;

        // Add current sample to ring buffer and check stability via PluginStruct
        pluginData->addSample(webapinettartim_son);
        
        // Check stability with current settings (using decimal scaling)
        bool stable = pluginData->isStable(webapinettartim_son, webapinettartim, dec, FYZ_Gecikme);
        
        // Handle target and rearm logic
        const int32_t target_sc = (int32_t)lroundf(FYZ_Hedef * SCALE);
        const int32_t a = (int32_t)lroundf(webapinettartim_son * SCALE);
        
        // --- RE-ARM: hedefin ALTINA inildiğinde aktif et + stabil sayacı resetle ---
        if (a < target_sc) {
          rearm_ok = true;
          // Stability counter reset - PluginStruct içinde isStable ile yönetiliyor
        }

        // Stabil durumunu ExtraTaskSettings'e kaydet
        P120_CONFIG_STABIL_MODE = stable ? 1 : 0;

        // STABİL olur OLMAZ ve topla_i 0'a düşmüşse (tek/kopya sonrası) otomatik ART'a dön
        if (stable && topla_i == 0) {
          topla_i = 1;
        }

        const bool cooldown_ok = (millis() - last_cmd_ts) >= COOLDOWN_MS;

        // --- 2) Manuel TEK/KOPYA: butona basılınca HEMEN, hedef/stabil bakmadan ---
        if (cooldown_ok) {
          if (topla_i == 2) {                // fyzuruntek
            String cmd = F("fyzuruntek&");
            cmd += String(sec_URUN_buton);
            cmd += F("&0&0");
            for (uint8_t k = 0; k < 8; k++) { cmd += '&'; cmd += String(sec_M_buton[k]); }
            ExecuteCommand_all_P120(cmd);
            last_cmd_ts = millis();
            topla_i = 0;                      // geçici olarak mod dışı
            success = true;
            break;                            // bu turda başka iş yapma
          }
          if (topla_i == 3) {                // fyzkop
            ExecuteCommand_all_P120(F("fyzkop&0"));
            last_cmd_ts = millis();
            topla_i = 0;                      // geçici olarak mod dışı
            success = true;
            break;
          }
        }

        // --- 3) Otomatik ART (fyzurunart): yalnızca hedef ÜSTÜ/EŞİT + STABİL + rearm/ilk tetik ---
        if (stable && (a >= target_sc) && cooldown_ok) {
          if (topla_i == 1 && (isnan(last_art_w) || rearm_ok)) {
            String cmd = F("fyzurunart&");
            cmd += String(sec_URUN_buton);
            cmd += F("&0&0");
            for (uint8_t k = 0; k < 8; k++) { cmd += '&'; cmd += String(sec_M_buton[k]); }
            ExecuteCommand_all_P120(cmd);

            last_cmd_ts = millis();
            last_art_w = webapinettartim_son;   // son tetik ağırlığını kaydet
            rearm_ok    = false;                 // bir sonraki tetik için tekrar hedef ALTINI bekle
            success     = true;
            // topla_i = 1 kalır (ART modu açık)
          }
        }

        // P120 dinamik değişkenleri güncelle (Rules için) via PluginStruct
        pluginData->updateTaskValues(event);
        
        success = true;
        break;
      }

    case PLUGIN_ONCE_A_SECOND:
      {
        // serial_error fonksiyonu eksik - basit log yapalım
        if (loglevelActiveFor(LOG_LEVEL_INFO)) {
          String log = F("P120: FYZ Mode ");
          log += FYZ_Mod;
          addLogMove(LOG_LEVEL_INFO, log);
        }
        success = true;
        break;
      }

    case PLUGIN_WRITE:
      {
        // Commands sistemi ile çalışıyor - bu plugin sadece monitoring yapar
        success = false; // Commands systemine bırak
        break;
      }

    case PLUGIN_SERIAL_IN:
      {
        // Serial handling - Commands sistem ile entegre
        success = true;
        break;
      }
  }
  return success;
}

#endif // USES_P120