#ifndef PLUGINSTRUCTS_P120_DATA_STRUCT_H
#define PLUGINSTRUCTS_P120_DATA_STRUCT_H

#include "../../_Plugin_Helper.h"

#ifdef USES_P120

#include <ESPeasySerial.h>
#include "../Helpers/ESPEasy_time.h"

#ifndef PLUGIN_120_DEBUG
  #define PLUGIN_120_DEBUG            false // when true: extra logging in serial out
#endif // ifndef PLUGIN_120_DEBUG

// P120 ExtraTaskSettings Macros (ESPEasy standard)
#define P120_CONFIG_NET_VALUE         ExtraTaskSettings.TaskDevicePluginConfigLong[0]   // NET değeri + type config bits
#define P120_CONFIG_DARA_VALUE        ExtraTaskSettings.TaskDevicePluginConfigLong[1]   // DARA değeri (float * 1000)
#define P120_CONFIG_BRUT_VALUE        ExtraTaskSettings.TaskDevicePluginConfigLong[2]   // BRUT değeri (float * 1000)
#define P120_CONFIG_ADET_VALUE        ExtraTaskSettings.TaskDevicePluginConfigLong[3]   // ADET değeri
#define P120_CONFIG_TOP_NET_VALUE     ExtraTaskSettings.TaskDevicePluginConfigLong[4]   // Toplam NET (float * 1000)
#define P120_CONFIG_TOP_DARA_VALUE    ExtraTaskSettings.TaskDevicePluginConfigLong[5]   // Toplam DARA (float * 1000)
#define P120_CONFIG_TOP_BRUT_VALUE    ExtraTaskSettings.TaskDevicePluginConfigLong[6]   // Toplam BRUT (float * 1000)
#define P120_CONFIG_SERI_NO           ExtraTaskSettings.TaskDevicePluginConfigLong[7]   // Seri No
#define P120_CONFIG_FIS_NO            ExtraTaskSettings.TaskDevicePluginConfigLong[8]   // Fiş No
#define P120_CONFIG_URUN_NO           ExtraTaskSettings.TaskDevicePluginConfigLong[9]   // Ürün No
#define P120_CONFIG_STABIL_MODE       ExtraTaskSettings.TaskDevicePluginConfigLong[10]  // Stabilite modu (0=US, 1=ST)
#define P120_CONFIG_PRINT_MODE        ExtraTaskSettings.TaskDevicePluginConfigLong[11]  // Yazdırma modu
#define P120_CONFIG_GECIKME           ExtraTaskSettings.TaskDevicePluginConfigLong[12]  // Gecikme saniye
#define P120_CONFIG_SAYAC             ExtraTaskSettings.TaskDevicePluginConfigLong[13]  // Veri sayaç
#define P120_CONFIG_BIRIM_FIYAT       ExtraTaskSettings.TaskDevicePluginConfigLong[14]  // Birim fiyat (float * 1000)
#define P120_CONFIG_TUTAR             ExtraTaskSettings.TaskDevicePluginConfigLong[15]  // Tutar (float * 1000)

// P120 int16_t ExtraTaskSettings (16 values)
#define P120_CONFIG_BUTTON1_PIN       ExtraTaskSettings.TaskDevicePluginConfig[0]       // Button 1 Pin
#define P120_CONFIG_BUTTON2_PIN       ExtraTaskSettings.TaskDevicePluginConfig[1]       // Button 2 Pin
#define P120_CONFIG_HEDEF_KG          ExtraTaskSettings.TaskDevicePluginConfig[2]       // Hedef kilogram
#define P120_CONFIG_TOPLA_MODE        ExtraTaskSettings.TaskDevicePluginConfig[3]       // Topla modu (0=azalt, 1=arttır)
#define P120_CONFIG_PRN_MODE          ExtraTaskSettings.TaskDevicePluginConfig[4]       // PRN dosya modu

// P120 PCONFIG Macros (standard ESPEasy)
#define P120_MODEL                    PCONFIG(0)      // FYZ Model (0-3)
#define P120_INDIKATOR                PCONFIG(1)      // İndikator seçimi
#define P120_KOPYA                    PCONFIG(2)      // Kopya aktif
#define P120_MOD                      PCONFIG(3)      // Yazdırma modu
#define P120_LOGO                     PCONFIG(4)      // Logo ayarı
#define P120_DATA_EDIT                PCONFIG(5)      // Data düzenleme izni

// P120 PCONFIG_FLOAT Macros
#define P120_HEDEF                    PCONFIG_FLOAT(0)  // Hedef kilogram

// P120 Value Types (2 bits per variable in CONFIG_NET_VALUE)
#define P120_TYPE_FLOAT               0
#define P120_TYPE_INTEGER             1
#define P120_TYPE_STRING              2
#define P120_TYPE_BOOLEAN             3

// Buffer Size Definitions
#define P120_MES_BUFF_SIZE            19
#define P120_HEDEF_BUFF_SIZE          9
#define P120_FIS_BASLIK_BUFF_SIZE     45
#define P120_CUSTOM_ARG_MAX           10

// P120 Data Structure (ESPEasy PluginStruct pattern)
struct P120_data_struct : PluginTaskData_base {
  // Ring buffer for averaging
  static const uint16_t MAX_SAMPLES = 129;
  float    ring_buffer[MAX_SAMPLES];
  uint16_t ring_size  = 0;
  uint16_t ring_count = 0;
  uint16_t ring_index = 0;
  double   ring_sum   = 0.0;
  
  // Button handling
  bool button1_last_state = HIGH;
  bool button2_last_state = HIGH;
  bool button1_pressed = HIGH;
  bool button2_pressed = HIGH;
  unsigned long button1_press_time = 0;
  unsigned long button2_press_time = 0;
  
  // Stability tracking
  uint16_t stabil_counter = 0;
  bool rearm_ready = false;
  float last_trigger_weight = NAN;
  unsigned long last_command_time = 0;
  
  // Current mode
  int8_t current_mode = 1; // 1=ART, 2=TEK, 3=KOPYA, 0=IDLE
  
  // Custom command strings
  String art_command = "";
  String top_command = "";
  String tek_command = "";
  
  // P120 değişkenleri (globals'tan taşınan)
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
  
  // Constructor
  P120_data_struct();
  
  // Methods
  void setRingBufferSize(uint16_t size);
  float addSample(float sample);
  void resetRingBuffer();
  
  bool handleButtons(int pin1, int pin2);
  bool isStable(float current, float previous, uint8_t decimals, uint16_t required_samples);
  
  void updateTaskValues(struct EventStruct *event);
  String getValueByName(const String& valueName);
  
  uint8_t getValueType(uint8_t varIndex);
  void setValueType(uint8_t varIndex, uint8_t valueType);
  
  bool isInitialized() const { return ring_size > 0; }
};

// Global access functions
P120_data_struct* P120_getPluginData(struct EventStruct *event);
void P120_releasePluginData(struct EventStruct *event);

#endif // USES_P120
#endif // PLUGINSTRUCTS_P120_DATA_STRUCT_H