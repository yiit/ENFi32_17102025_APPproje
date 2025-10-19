#include "../Commands/P120.h"

#ifdef USES_P120

#include "../Commands/Common.h"
#include "../Globals/Settings.h"
#include "../../ESPEasy-Globals.h"
#include "../Helpers/StringConverter.h"
#include "../Helpers/PrintToString.h"
#include "../Helpers/ESPEasy_Storage.h"
#include "../ESPEasyCore/ESPEasy_Log.h"
#include "../Globals/ESPEasy_time.h"

#include "../../ESPEasy_common.h"
#include "../../_Plugin_Helper.h"
#include "../PluginStructs/P120_data_struct.h"

#include <LittleFS.h>
#include "EEPROM.h"
#include <string.h>

// P120 helper fonksiyonu - EEPROM'a değerleri kaydet
static void saveToEEPROM(P120_data_struct *P120_data) {
  if (!P120_data) return;
  int address = 0;
  EEPROM.writeLong(address, P120_data->seri_no); address += sizeof(long);
  EEPROM.writeLong(address, P120_data->sno);     address += sizeof(long);
  EEPROM.writeFloat(address, P120_data->top_net);address += sizeof(float);
  EEPROM.commit();
}

// P120 helper fonksiyonu - PRN içeriğini işle  
static void processPrnContent(String& prnContent, P120_data_struct *P120_data) {
  if (!P120_data) return;
  
  // ExtraTaskSettings ile char array güncelleme (RAM efficient)
  char tempBuffer[12];
  
  // Float değerleri char array'lere çevir
  dtostrf(P120_data->webapinettartim, 7, 2, tempBuffer);
  prnContent.replace("[NET]", String(tempBuffer));
  prnContent.replace("%NET%", String(tempBuffer));
  
  dtostrf(P120_data->webapidaratartim, 7, 2, tempBuffer);
  prnContent.replace("[DARA]", String(tempBuffer));
  prnContent.replace("%DARA%", String(tempBuffer));
  
  dtostrf(P120_data->webapibruttartim, 7, 2, tempBuffer);
  prnContent.replace("[BRUT]", String(tempBuffer));
  prnContent.replace("%BRUT%", String(tempBuffer));
  
  snprintf(tempBuffer, sizeof(tempBuffer), "%.0f", P120_data->webapiadet);
  prnContent.replace("[ADET]", String(tempBuffer));
  prnContent.replace("%ADET%", String(tempBuffer));
  
  dtostrf(P120_data->top_net, 10, 2, tempBuffer);
  prnContent.replace("[TOP_NET]", String(tempBuffer));
  prnContent.replace("%TOP_NET%", String(tempBuffer));
  
  dtostrf(P120_data->top_dara, 10, 2, tempBuffer);
  prnContent.replace("[TOP_DARA]", String(tempBuffer));
  prnContent.replace("%TOP_DARA%", String(tempBuffer));
  
  dtostrf(P120_data->top_brut, 10, 2, tempBuffer);
  prnContent.replace("[TOP_BRUT]", String(tempBuffer));
  prnContent.replace("%TOP_BRUT%", String(tempBuffer));
  
  snprintf(tempBuffer, sizeof(tempBuffer), "%ld", P120_data->sno);
  prnContent.replace("[SNO]", String(tempBuffer));
  prnContent.replace("%SNO%", String(tempBuffer));
  
  snprintf(tempBuffer, sizeof(tempBuffer), "%ld", P120_data->seri_no);
  prnContent.replace("[SERI_NO]", String(tempBuffer));
  prnContent.replace("%SERI_NO%", String(tempBuffer));
  
  // Tarih/saat
  String tarih_str = node_time.getDateString('-');
  String saat_str = node_time.getTimeString(':');
  prnContent.replace("[TARIH]", tarih_str);
  prnContent.replace("%TARIH%", tarih_str);
  prnContent.replace("[SAAT]", saat_str);
  prnContent.replace("%SAAT%", saat_str);
  
  // Stabilite durumu
  const char* stabil_str = "US"; // Varsayılan
  prnContent.replace("[STABIL]", String(stabil_str));
  prnContent.replace("%STABIL%", String(stabil_str));
}


String do_command_fyzart(struct EventStruct *event, const char* Line)
{
  // FYZ Artı komutu - Tek ürün yazdır ve sayaç arttır
  if (loglevelActiveFor(LOG_LEVEL_INFO)) {
    addLog(LOG_LEVEL_INFO, F("P120: FYZ Art komutu çalışıyor"));
  }

  // P120 data struct'ı al
  P120_data_struct *P120_data = P120_getPluginData(event);
  if (!P120_data) {
    addLog(LOG_LEVEL_ERROR, F("P120: Plugin data bulunamadı"));
    return F("ERROR: Plugin data not found");
  }

  // webapinettartim değerini kontrol et
  if (P120_data->webapinettartim > 0.0001) {
    // Seçili PRN dosyasını oku ve yazdır
    if (strlen(Settings.urun_art_prn) > 0) {
      String filename = "/" + String(Settings.urun_art_prn);
      String prnContent = "";
      
      if (fileExists(filename)) {
        File prnFile = tryOpenFile(filename, "r");
        if (prnFile) {
          prnContent = prnFile.readString();
          prnFile.close();
          
          // Seri numarasını arttır
          P120_data->sno++;
          
          // PRN içeriğini işle
          processPrnContent(prnContent, P120_data);
          
          // Yazdır (Serial port üzerinden)
          if (Settings.UseSerial) {
            serialPrintln(prnContent);
          }
          
          // Serial1 (P120 Yazıcı) çıkışı
          Serial1.println(prnContent);
          
          // UDP çıkışı varsa
          if (Settings.UDPPort > 0) {
            // UDP gönderimi burada yapılabilir
          }
          
          // EEPROM'a kaydet
          saveToEEPROM(P120_data);
          
          // topla_i modunu kontrol et
          if (P120_data->topla_i == 1) {
            // Art modu devam ediyor
            addLog(LOG_LEVEL_INFO, concat(F("P120: Art fiş yazdırıldı (devam): "), filename));
          } else {
            // Tek mode'a geç
            P120_data->topla_i = 2;
            addLog(LOG_LEVEL_INFO, concat(F("P120: Art fiş yazdırıldı (tek): "), filename));
          }
          
          return return_command_success();
        } else {
          addLog(LOG_LEVEL_ERROR, concat(F("P120: PRN dosyası açılamadı: "), filename));
          return return_command_failed();
        }
      } else {
        addLog(LOG_LEVEL_ERROR, concat(F("P120: PRN dosyası bulunamadı: "), filename));
        return return_command_failed();
      }
    } else {
      addLog(LOG_LEVEL_ERROR, F("P120: Art PRN dosyası seçilmemiş"));
      return return_command_failed();
    }
  } else {
    // Hata beep çal
    const char* hata_beep = "\x1B\x42\x43\x03"; // ESC beep komutu
    Serial1.write((const uint8_t*)hata_beep, strlen(hata_beep));
    addLog(LOG_LEVEL_ERROR, F("P120: Net tartım değeri sıfır veya negatif"));
    return return_command_failed();
  }
}

String do_command_fyztop(struct EventStruct *event, const char* Line)
{
  // FYZ Toplam komutu - Toplam fiş yazdır ve sıfırla
  if (loglevelActiveFor(LOG_LEVEL_INFO)) {
    addLog(LOG_LEVEL_INFO, F("P120: FYZ Top komutu çalışıyor"));
  }

  // P120 data struct'ı al
  P120_data_struct *P120_data = P120_getPluginData(event);
  if (!P120_data) {
    addLog(LOG_LEVEL_ERROR, F("P120: Plugin data bulunamadı"));
    return F("ERROR: Plugin data not found");
  }

  // Seçili PRN dosyasını oku ve yazdır
  if (strlen(Settings.urun_top_prn) > 0) {
    String filename = "/" + String(Settings.urun_top_prn);
    String prnContent = "";
    
    if (fileExists(filename)) {
      File prnFile = tryOpenFile(filename, "r");
      if (prnFile) {
        prnContent = prnFile.readString();
        prnFile.close();
        
        // PRN içeriğini işle
        processPrnContent(prnContent, P120_data);
        
        // Yazdır (Serial port üzerinden)
        if (Settings.UseSerial) {
          serialPrintln(prnContent);
        }
        
        // Serial1 (P120 Yazıcı) çıkışı
        Serial1.println(prnContent);
        
        // Toplamları sıfırla
        P120_data->sno = 0;
        P120_data->top_net = 0.0;
        P120_data->top_dara = 0.0;
        P120_data->top_brut = 0.0;
        
        // topla_i modunu sıfırla (idle moda geç)
        P120_data->topla_i = 0;
        
        // EEPROM'a kaydet
        saveToEEPROM(P120_data);
        
        addLog(LOG_LEVEL_INFO, concat(F("P120: Toplam fiş yazdırıldı ve sıfırlandı: "), filename));
        return return_command_success();
      } else {
        addLog(LOG_LEVEL_ERROR, concat(F("P120: PRN dosyası açılamadı: "), filename));
        return return_command_failed();
      }
    } else {
      addLog(LOG_LEVEL_ERROR, concat(F("P120: PRN dosyası bulunamadı: "), filename));
      return return_command_failed();
    }
  } else {
    addLog(LOG_LEVEL_ERROR, F("P120: Toplam PRN dosyası seçilmemiş"));
    return return_command_failed();
  }
}

String do_command_fyzkop(struct EventStruct *event, const char* Line)
{
  // FYZ Kopya komutu - Son fişin kopyasını yazdır
  if (loglevelActiveFor(LOG_LEVEL_INFO)) {
    addLog(LOG_LEVEL_INFO, F("P120: FYZ Kopya komutu çalışıyor"));
  }

  // P120 data struct'ı al
  P120_data_struct *P120_data = P120_getPluginData(event);
  if (!P120_data) {
    addLog(LOG_LEVEL_ERROR, F("P120: Plugin data bulunamadı"));
    return F("ERROR: Plugin data not found");
  }

  // Son yazdırılan PRN'i tekrar yazdır (tek PRN dosyası)
  if (strlen(Settings.urun_tek_prn) > 0) {
    String filename = "/" + String(Settings.urun_tek_prn);
    String prnContent = "";
    
    if (fileExists(filename)) {
      File prnFile = tryOpenFile(filename, "r");
      if (prnFile) {
        prnContent = prnFile.readString();
        prnFile.close();
        
        // PRN içeriğini işle
        processPrnContent(prnContent, P120_data);
        
        // KOPYA etiketi ekle
        prnContent = "*** KOPYA ***\n" + prnContent;
        
        // Yazdır
        if (Settings.UseSerial) {
          serialPrintln(prnContent);
        }
        
        // Serial1 (P120 Yazıcı) çıkışı
        Serial1.println(prnContent);
        
        // topla_i = 3 (kopya modu)
        P120_data->topla_i = 3;
        
        addLog(LOG_LEVEL_INFO, concat(F("P120: Kopya fiş yazdırıldı: "), filename));
        return return_command_success();
      }
    }
  }
  
  addLog(LOG_LEVEL_ERROR, F("P120: Kopya yazdırılamadı"));
  return return_command_failed();
}

String do_command_fyzuruntek(struct EventStruct *event, const char* Line)
{
  // FYZ Ürün Tek komutu - Tek ürün yazdırma (toplamları sıfırla)
  if (loglevelActiveFor(LOG_LEVEL_INFO)) {
    addLog(LOG_LEVEL_INFO, F("P120: FYZ Ürün Tek komutu"));
  }

  // P120 data struct'ı al
  P120_data_struct *P120_data = P120_getPluginData(event);
  if (!P120_data) {
    addLog(LOG_LEVEL_ERROR, F("P120: Plugin data bulunamadı"));
    return F("ERROR: Plugin data not found");
  }

  // Tek ürün PRN dosyasını oku
  if (strlen(Settings.urun_tek_prn) > 0) {
    String filename = "/" + String(Settings.urun_tek_prn);
    
    if (fileExists(filename)) {
      File prnFile = tryOpenFile(filename, "r");
      if (prnFile) {
        String prnContent = prnFile.readString();
        prnFile.close();
        
        // PRN içeriğini işle
        processPrnContent(prnContent, P120_data);
        
        // Yazdır
        if (Settings.UseSerial) {
          serialPrintln(prnContent);
        }
        
        // Serial1 (P120 Yazıcı) çıkışı
        Serial1.println(prnContent);
        
        // Toplamları sıfırla (tek yazdırım)
        P120_data->sno = 0;
        P120_data->top_net = 0.0;
        P120_data->top_dara = 0.0;
        P120_data->top_brut = 0.0;
        
        // topla_i = 2 moduna geç (tek yazdırım tamamlandı)
        P120_data->topla_i = 2;
        
        addLog(LOG_LEVEL_INFO, concat(F("P120: Ürün tek yazdırıldı: "), filename));
        return return_command_success();
      }
    }
  }
  
  addLog(LOG_LEVEL_ERROR, F("P120: Ürün tek yazdırılamadı"));
  return return_command_failed();
}

String do_command_fyzurunart(struct EventStruct *event, const char* Line)
{
  // FYZ Ürün Art komutu - Art yazdırma (devam eden toplama)
  if (loglevelActiveFor(LOG_LEVEL_INFO)) {
    addLog(LOG_LEVEL_INFO, F("P120: FYZ Ürün Art komutu"));
  }

  // P120 data struct'ı al
  P120_data_struct *P120_data = P120_getPluginData(event);
  if (!P120_data) {
    addLog(LOG_LEVEL_ERROR, F("P120: Plugin data bulunamadı"));
    return F("ERROR: Plugin data not found");
  }

  // Art PRN dosyasını kullan
  if (strlen(Settings.urun_art_prn) > 0) {
    String filename = "/" + String(Settings.urun_art_prn);
    
    if (fileExists(filename)) {
      File prnFile = tryOpenFile(filename, "r");
      if (prnFile) {
        String prnContent = prnFile.readString();
        prnFile.close();
        
        // Seri numarasını arttır
        P120_data->sno++;
        
        // Toplamları güncelle
        P120_data->top_net += P120_data->webapinettartim;
        P120_data->top_dara += P120_data->webapidaratartim;  
        P120_data->top_brut += P120_data->webapibruttartim;
        
        // PRN içeriğini işle
        processPrnContent(prnContent, P120_data);
        
        // Yazdır
        if (Settings.UseSerial) {
          serialPrintln(prnContent);
        }
        
        // Serial1 (P120 Yazıcı) çıkışı  
        Serial1.println(prnContent);
        
        // EEPROM'a kaydet
        saveToEEPROM(P120_data);
        
        // topla_i = 1 moduna geç (art modu devam)
        P120_data->topla_i = 1;
        
        addLog(LOG_LEVEL_INFO, concat(F("P120: Ürün art yazdırıldı: "), filename));
        return return_command_success();
      }
    }
  }
  
  addLog(LOG_LEVEL_ERROR, F("P120: Ürün art yazdırılamadı"));
  return return_command_failed();
}

#endif // USES_P120