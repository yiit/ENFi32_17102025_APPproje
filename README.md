# ENFi32 - Enhanced ESP32 IoT Firmware

Bu proje, ESPEasy tabanlı gelişmiş ESP32 IoT firmware'i için özel yapılandırma ve geliştirmelerdir.

## 🚀 Özellikler

### ✅ Temel Özellikler
- **ESP32 Desteği**: ESP32, ESP32-S3 ve diğer ESP32 serisi çipler
- **ESPEasy Tabanlı**: Güvenilir ESPEasy Core sistemi
- **Custom.h Konfigürasyonu**: Kişiselleştirilmiş ayarlar
- **P020 Serial Plugin**: Seri port veri okuma desteği

### 🔧 Geliştirilen Özellikler

#### 1. Enhanced WiFi Setup System
- **7-Stage Process**: SCAN → SCANNING → SELECT → PASSWORD → CONNECTING → SUCCESS/FAILED → FAILSAFE
- **Android-Style UI**: Modern Material Design arayüz
- **Otomatik Failsafe**: Başarısız bağlantılarda otomatik AP modu
- **Akıllı Retry Sistemi**: Agresif yeniden deneme mekanizması

#### 2. WiFi Smart Manager (UPTIME BAZLI)
- **Uptime Bazlı WiFi Yönetimi**: 
  - 0-3 dakika: Agresif mod (5 saniye retry)
  - 3-10 dakika: Kritik mod (15 saniye retry) 
  - 10+ dakika: Stabil mod (30 saniye retry)
- **Sağlık Kontrolü**: RSSI izleme ve bağlantı kalitesi analizi
- **Emergency Mode**: Otomatik AP modu aktivasyonu
- **Web Dashboard**: Gerçek zamanlı durum izleme

#### 3. Serial Monitor System
- **Advanced Data Analysis**: Hex/ASCII görüntüleme
- **Pattern Detection**: Tekrarlayan veri desenleri
- **Character Frequency**: Karakter frekans analizi
- **Real-time Monitoring**: Anlık seri port izleme

#### 4. Güvenlik Geliştirmeleri
- **WiFi Failsafe Settings**: Agresif bağlantı timeout'ları
- **Connection Monitoring**: Sürekli bağlantı durumu kontrolü
- **Auto AP Mode**: Başarısız bağlantılarda otomatik geri dönüş

## 🔧 Teknik Özellikler

### Hardware Support
- **ESP32**: 4MB Flash + 316KB RAM environments
- **ESP32-S3**: 16MB Flash + 8MB PSRAM + OPI + ETH
- **LittleFS**: File system support
- **Ethernet**: Hardware ethernet support (S3)

### Software Stack
- **Framework**: Arduino ESP32
- **Platform**: PlatformIO
- **Base**: ESPEasy Mega
- **Language**: C++17

## 🚀 Kurulum

### Gereksinimler
- PlatformIO Core 6.0+
- Git
- ESP32 Development Board

### Adımlar
```bash
# Repository'yi klonlayın
git clone [repository-url]
cd ENFi32_17102025_APPproje

# PlatformIO dependencies yükleyin
pio pkg install

# ESP32 için build edin
pio run -e custom_ESP32_4M316k_LittleFS

# ESP32-S3 için build edin  
pio run -e custom_ESP32s3_16M8M_LittleFS_OPI_PSRAM_ETH

# Upload edin
pio run -e [environment] --target upload
```

## ⚙️ Konfigürasyon

### Custom.h Ayarları
```cpp
#define DEFAULT_NAME        "ENFi32"
#define FEATURE_WIFI_SMART_MANAGER           1
#define WIFI_SMART_LOGGING                   1
#define WEBSERVER_WIFI_MANAGER               1
#define WEBSERVER_SERIALMONITOR              1
```

### WiFi Failsafe Settings
```cpp
#define WIFI_RECONNECT_MAX_RETRIES           3
#define WIFI_FAILSAFE_AP_TIMEOUT             30
#define WIFI_CONNECTION_STABLE_TIME          15
```

## 🌐 Web API Endpoints

### WiFi Management
- `/setup` - Enhanced WiFi setup page
- `/wifimanager` - Smart WiFi dashboard
- `/setup?action=force_emergency` - Trigger emergency mode

### Serial Monitoring  
- `/serialmonitor` - Serial data viewer
- `/serial_status.json` - JSON status API

## 📊 Performance

### WiFi Connection Times
- **Normal Mode**: 5-10 saniye
- **Failsafe Mode**: 30 saniye maksimum
- **Recovery Time**: 15-30 saniye

### Memory Usage
- **Flash**: ~1.2MB (optimized)
- **RAM**: ~45KB (base) + Smart Manager overhead
- **PSRAM**: Available for large data operations

## 🛠️ Development

### Build Environments
- `custom_ESP32_4M316k_LittleFS`
- `custom_ESP32s3_16M8M_LittleFS_OPI_PSRAM_ETH`

### Debug Options
```cpp
#define WIFI_SMART_LOGGING                   1
#define SERIAL_MONITOR_DEBUG                 1
```

## 📝 Changelog

### v1.0.0 (Current)
- ✅ Enhanced WiFi Setup System
- ✅ WiFi Smart Manager (Uptime-based)
- ✅ Serial Monitor Interface
- ✅ Failsafe Mechanisms
- ✅ ESP32/ESP32-S3 Support

## 📄 License

Bu proje MIT License altında lisanslanmıştır.

## 🙏 Teşekkürler

- **ESPEasy Team**: Temel framework için
- **PlatformIO**: Build system için
- **Espressif**: ESP32 hardware için

---

**ENFi32** - Enhanced ESP32 IoT Firmware

---

# Original ESPEasy Documentation


Introduction https://espeasy.readthedocs.io/en/latest/ (and, mostly outdated, wiki: https://www.letscontrolit.com/wiki/index.php/ESPEasy#Introduction)

**MEGA**
This is the development branch of ESPEasy. All new features go into this branch, and it has become the current stable branch. If you want to do a bugfix, do it on this branch.


Check here to learn how to use this branch and help us improving ESPEasy: [Starter guide for (local) development on ESPEasy](https://espeasy.readthedocs.io/en/latest/Participate/PlatformIO.html#starter-guide-for-local-development-on-espeasy)

## Web based flasher (experimental)

To make it easier to get started, one may flash a build directly to the ESP from your browser.
Currently only Chrome and Edge are supported.

See [this flash page](https://td-er.nl/ESPEasy/) to try the new web flash feature.

The web flasher is using [ESP Web Tools](https://esphome.github.io/esp-web-tools/) made by the people behind ESPHome and Home Assistant.


## Binary releases

On demand, controlled by the repo owner, our build-bot will build a new binary release: https://github.com/letscontrolit/ESPEasy/releases

The releases are named something like 'mega-20220626' (last number is the build date)

Depending on your needs, we release different types of files:

The name is built up from a few key parts:

ESPEasy_mega\_*[releasedate]*\_*[build-type]*\_*[opt-arduino-library]*\_*[hardware-type]*\_*[flash-size][filesystem-size]*\_*[opt-build-features]*.bin

*[build-type]* can be any of:
Build type   | Description                               | included plugins                 |
-------------|-------------------------------------------|----------------------------------|
climate      | All plugins related to climate measurement| Stable + Climate                 |
custom       | Custom predefined set/Defined in Custom.h | Specific                         |
normal       | Standard plugins                          | Stable                           |
collection_A | Normal + plugin collection A              | Stable + Collection base + set A |
collection_B | Normal + plugin collection B              | Stable + Collection base + set B |
collection_C | Normal + plugin collection C              | Stable + Collection base + set C |
collection_D | Normal + plugin collection D              | Stable + Collection base + set D |
collection_E | Normal + plugin collection E              | Stable + Collection base + set E |
collection_F | Normal + plugin collection F              | Stable + Collection base + set F |
collection_G | Normal + plugin collection G              | Stable + Collection base + set G |
max          | All available plugins                     | All available                    |
energy       | All plugins related to energy measurement | Stable + Energy measurement      |
display      | All plugins related to displays           | Stable + Displays                |
neopixel     | All plugins related to neopixel           | Stable + Neopixel                |
hard         | hardware specific builds                  | Minimal                          |
minimal      | minimal plugins for specific use-cases    | Switch and Controller            |
spec_*       | specialized technical builds              | Not intended for regular use     |
IRext        | Infra-red hardware specific               | Sending and receiving IR cmd     |
safeboot     | (Experimental) `safeboot` build to enable<br>most/all plugins on 4MB Flash boards | None                             |


*[opt-arduino-library]* (optional) can be any of:
Arduino library | Description                        |
----------------|------------------------------------|
alt_wifi        | Alternative WiFi configuration     |
beta            | Arduino Beta release               |
sdk3            | Arduino SDK v.3                    |
core_274        | Arduino Core 2.7.4 release         |
core_312        | Arduino Core 3.1.2 release         |
core_274_sdk3   | Arduino Core 2.7.4 SDK v.3 release |


*[hardware-type]* can be any of:
Hardware type    | Description                                 |
-----------------|---------------------------------------------|
ESP8266          | Espressif ESP8266/ESP8285 generic boards    |
WROOM02          | Espressif ESP8266 WRoom02 boards            |
ESP32            | Espressif ESP32 generic boards              |
ESP32solo1       | Espressif ESP32-Solo1 generic boards        |
ESP32s2          | Espressif ESP32-S2 generic boards           |
ESP32c3          | Espressif ESP32-C3 generic boards           |
ESP32s3          | Espressif ESP32-S3 generic boards           |
ESP32c2          | Espressif ESP32-C2 generic boards           |
ESP32c6          | Espressif ESP32-C6 generic boards           |
ESP32-wrover-kit | Espressif ESP32 wrover-kit boards           |
SONOFF           | Sonoff hardware specific                    |
other_POW        | Switch with power measurement               |
Shelly_1         | Shelly 1 switch                             |
Shelly_PLUG_S    | Shelly plug S switch with power measurement |
Ventus           | Ventus W266 weather station                 |
LCtech_relay     | LC-tech serial switch                       |

N.B. Starting 2022/07/23, 1M ESP8266 builds can also be used on ESP8285 units and thus there is no longer a specific ESP8285 build anymore.


*[flash-size]* can be any of:
Flash size | Description                 |
-----------|-----------------------------|
1M         | 1 MB with 128 kB filesystem |
2M         | 2 MB with 128 kB filesystem |
2M256      | 2 MB with 256 kB filesystem |
2M320k     | 2 MB with 320 kB filesystem |
4M1M       | 4 MB with 1 MB filesystem   |
4M2M       | 4 MB with 2 MB filesystem   |
16M        | 16 MB with 14 MB filesystem |
4M316k     | 4 MB with 316 kB filesystem |
8M1M       | 8 MB with 1 MB filesystem   |
16M1M      | 16 MB with 1 MB filesystem  |
16M8M      | 16 MB with 8 MB filesystem  |

N.B. Starting with release 2023/12/25, All ESP32 LittleFS builds use IDF 5.3, to support newer ESP32 chips like ESP32-C2 and ESP32-C6, and SPI Ethernet. Other SPIFFS based ESP32 builds will be migrated to LittleFS as SPIFFS is no longer officially available in IDF 5 and later. As a temporary solution, a specially crafted IDF 5.1 build that still includes SPIFFS, is used for the SPIFFS builds. A migration plan will be made available in 2025.

*[opt-build-features]* can be any of:
Build features  | Description                                                                                               |
----------------|-----------------------------------------------------------------------------------------------------------|
LittleFS        | Use LittleFS instead of SPIFFS filesystem (SPIFFS is unstable \> 2 MB, and no longer supported in IDF \> 5) |
VCC             | Analog input configured to measure VCC voltage (ESP8266 only)                                             |
OTA             | Arduino OTA (Over The Air) update feature enabled                                                         |
Domoticz        | Only Domoticz controllers (HTTP) and plugins included                                                     |
Domoticz_MQTT   | Only Domoticz controllers (MQTT) and plugins included                                                     |
FHEM_HA         | Only FHEM/OpenHAB/Home Assistant (MQTT) controllers and plugins included                                  |
ETH             | Ethernet support enabled (ESP32 and IDF 5.x based builds)                                                 |
OPI_PSRAM       | Specific configuration to enable PSRAM detection, ESP32-S3 only                                           |
CDC             | Support USBCDC/HWCDC-serial console on ESP32-C3, ESP32-S2, ESP32-S3 and ESP32-C6                          |
noOTA/NO_OTA    | Does not support OTA (Over The Air-updating of the firmware) Use [the flash page](https://td-er.nl/ESPEasy/) or ESPTool via USB Serial |

N.B. Starting ca. 2025/02/27, many ESP32 builds are *only* available with _ETH suffix, indicating that Ethernet support is enabled, to reduce the (rather high) number of builds.

Some example firmware names:
Firmware name                                                         | Hardware                                        | Included plugins                 |
----------------------------------------------------------------------|-------------------------------------------------|----------------------------------|
ESPEasy_mega-20230822_normal_ESP8266_1M.bin                           | ESP8266/ESP8285 with 1MB flash                  | Stable                           |
ESPEasy_mega-20230822_normal_ESP8266_4M1M.bin                         | ESP8266 with 4MB flash                          | Stable                           |
ESPEasy_mega-20230822_collection_A_ESP8266_4M1M.bin                   | ESP8266 with 4MB flash                          | Stable + Collection base + set A |
ESPEasy_mega-20230822_normal_ESP32_4M316k_ETH.bin                     | ESP32 with 4MB flash                            | Stable                           |
ESPEasy_mega-20230822_collection_A_ESP32_4M316k_ETH.bin               | ESP32 with 4MB flash                            | Stable + Collection base + set A |
ESPEasy_mega-20230822_collection_B_ESP32_4M316k_ETH.bin               | ESP32 with 4MB flash                            | Stable + Collection base + set B |
ESPEasy_mega-20230822_max_ESP32s3_8M1M_LittleFS_ETH.bin           | ESP32-S3 with 8MB flash, CDC-serial, Ethernet   | All available plugins            |
ESPEasy_mega-20230822_max_ESP32s3_8M1M_LittleFS_OPI_PSRAM_ETH.bin | ESP32-S3 8MB flash, PSRAM, CDC-serial, Ethernet | All available plugins            |
ESPEasy_mega-20230822_max_ESP32_16M1M_ETH.bin                         | ESP32 with 16MB flash, SPIFFS, Ethernet         | All available plugins            |
ESPEasy_mega-20230822_max_ESP32_16M8M_LittleFS_ETH.bin                | ESP32 with 16MB flash, LittleFS, Ethernet       | All available plugins            |

The binary files for the different ESP32 variants (S2, C3, S3, C2, C6, Solo1, 'Classic') are available in separate archives.

To see what plugins are included in which collection set, you can find that on the [ESPEasy Plugin overview page](https://espeasy.readthedocs.io/en/latest/Plugin/_Plugin.html)

For ESP32 builds (all models) there are 2 ``.bin`` files available:
1) a ``.factory.bin`` to be used when flashing the firmware via external tools, like the Espressif Flash Download tool, to be loaded at address ``0x0``, that includes the bootloader and flash-partitioning data. This is the second of 2 recommended methods to install ESPEasy on a new device.<BR/>
The main recommended method for flashing ESPEasy onto a (new) device is via [this flash page](https://td-er.nl/ESPEasy/) (using Chrome or Edge webbrowser), where you connect the device via USB-serial, and select what firmware-build to install.
2) a 'regular' ``.bin`` file, that can be used to update a running board via OTA (Over The Air), using the Update Firmware button on the Tools page. The .bin file then has to be available for the device the webbrowser is running from (local or via a network), as the file is uploaded via the webbrowser.<BR/>
The firmware to upload via OTA must match both the ESP32 model, and current partitioning & formatting, LittleFS as indicated in the name, or SPIFFS.

## Documentation & more info

Our new, in-depth documentation can be found at [ESPEasy.readthedocs.io](https://espeasy.readthedocs.io/en/latest/). Automatically built, so always up-to-date according to the contributed contents. The old Wiki documentation can be found at [letscontrolit.com/wiki](https://www.letscontrolit.com/wiki/index.php?title=ESPEasy).

Additional details and discussion are on the "Experimental" section of the forum: https://www.letscontrolit.com/forum/viewforum.php?f=18

[![Gitpod Ready-to-Code](https://img.shields.io/badge/Gitpod-Ready--to--Code-blue?logo=gitpod)](https://gitpod.io/#https://github.com/letscontrolit/ESPEasy) 

## SAST Tools

[PVS-Studio](https://pvs-studio.com/en/pvs-studio/?utm_source=website&utm_medium=github&utm_campaign=open_source) - static analyzer for C, C++, C#, and Java code.

## Icons used

Icons on courtesy of [ICONS8](https://icons8.com/).
