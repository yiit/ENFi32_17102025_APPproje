# ENFi32 Proje Derleme Kılavuzu / Build Guide

## Türkçe

### Proje Durumu
Bu proje **zaten PlatformIO ile derlenebilir** şekilde yapılandırılmıştır. Tüm gerekli konfigürasyon dosyaları mevcuttur.

### Gereksinimler
- PlatformIO Core 6.0+ veya PlatformIO IDE
- Python 3.7+
- Git
- İnternet bağlantısı (ilk derleme için paketlerin indirilmesi gerekir)

### Kurulum ve Derleme

#### Yöntem 1: PlatformIO IDE (Önerilen - Başlangıç için en kolay)

1. **Visual Studio Code** veya **Atom** editörünü indirin ve kurun
2. **PlatformIO IDE** eklentisini yükleyin
3. Projeyi açın: `File > Open Folder` ve proje klasörünü seçin
4. PlatformIO, otomatik olarak gerekli bağımlılıkları indirecektir
5. Sol taraftaki PlatformIO menüsünden derleme ortamını seçin:
   - `custom_ESP32_4M316k_LittleFS` (varsayılan)
   - veya diğer ESP32 ortamları
6. **Build** butonuna tıklayın veya `Ctrl+Alt+B` (Windows/Linux) / `Cmd+Shift+B` (macOS)

#### Yöntem 2: Komut Satırı (PlatformIO CLI)

1. **PlatformIO Core'u yükleyin:**
   ```bash
   pip install -U platformio
   ```

2. **Proje klasörüne gidin:**
   ```bash
   cd /path/to/ENFi32_17102025_APPproje
   ```

3. **Tüm projeyi derleyin (varsayılan ortam):**
   ```bash
   pio run
   ```

4. **Belirli bir ortamı derleyin:**
   ```bash
   # ESP32 4MB Flash için
   pio run -e custom_ESP32_4M316k_LittleFS
   
   # ESP32-S3 16MB Flash + PSRAM için
   pio run -e custom_ESP32s3_16M8M_LittleFS_OPI_PSRAM_ETH
   ```

5. **Derleme ve yükleme:**
   ```bash
   pio run -e custom_ESP32_4M316k_LittleFS --target upload
   ```

6. **Seri port monitörü:**
   ```bash
   pio device monitor
   ```

### Kullanılabilir Derleme Ortamları

Proje birçok ESP32 varyantını destekler. `platformio.ini` dosyasındaki `default_envs` satırını düzenleyerek varsayılan ortamı değiştirebilirsiniz:

- **ESP32 Ortamları:**
  - `custom_ESP32_4M316k_LittleFS` - 4MB Flash, LittleFS (varsayılan)
  - `normal_ESP32_4M` - 4MB Flash, standart konfigürasyon
  - `max_ESP32_16M8M_LittleFS_ETH` - 16MB Flash, Ethernet desteği

- **ESP32-S3 Ortamları:**
  - `custom_ESP32s3_16M8M_LittleFS_OPI_PSRAM_ETH` - 16MB Flash, PSRAM, Ethernet

- **ESP32-C3, ESP32-S2, ESP32-C6** ortamları da mevcuttur

### Özel Ayarlar

Projenin özel ayarları `src/include/Custom.h` dosyasında tanımlanmıştır:

```cpp
#define DEFAULT_NAME        "ENFi32"
#define FEATURE_WIFI_SMART_MANAGER           1
#define WIFI_SMART_LOGGING                   1
#define WEBSERVER_WIFI_MANAGER               1
#define WEBSERVER_SERIALMONITOR              1
```

### Derleme Çıktıları

Başarılı derlemeden sonra firmware dosyaları şurada bulunur:
```
.pio/build/<environment_name>/firmware.bin
.pio/build/<environment_name>/firmware.factory.bin  # ESP32 için fabrika imajı
```

### Yükleme

1. **USB ile:**
   ```bash
   pio run -e custom_ESP32_4M316k_LittleFS --target upload
   ```

2. **OTA (Over-The-Air):**
   - Cihazın web arayüzüne gidin
   - Tools > Update Firmware
   - `.bin` dosyasını seçin ve yükleyin

3. **Web Flash Tool:**
   - Chrome veya Edge tarayıcısında [ESPEasy Flash Page](https://td-er.nl/ESPEasy/) adresine gidin
   - Cihazı USB ile bağlayın
   - Firmware dosyasını seçin ve flash edin

---

## English

### Project Status
This project is **already configured to compile with PlatformIO**. All necessary configuration files are present.

### Requirements
- PlatformIO Core 6.0+ or PlatformIO IDE
- Python 3.7+
- Git
- Internet connection (required for initial package downloads)

### Installation and Building

#### Method 1: PlatformIO IDE (Recommended - Easiest for beginners)

1. Download and install **Visual Studio Code** or **Atom** editor
2. Install the **PlatformIO IDE** extension
3. Open the project: `File > Open Folder` and select the project folder
4. PlatformIO will automatically download required dependencies
5. Select a build environment from the PlatformIO menu on the left:
   - `custom_ESP32_4M316k_LittleFS` (default)
   - or other ESP32 environments
6. Click the **Build** button or press `Ctrl+Alt+B` (Windows/Linux) / `Cmd+Shift+B` (macOS)

#### Method 2: Command Line (PlatformIO CLI)

1. **Install PlatformIO Core:**
   ```bash
   pip install -U platformio
   ```

2. **Navigate to project folder:**
   ```bash
   cd /path/to/ENFi32_17102025_APPproje
   ```

3. **Build the entire project (default environment):**
   ```bash
   pio run
   ```

4. **Build a specific environment:**
   ```bash
   # For ESP32 4MB Flash
   pio run -e custom_ESP32_4M316k_LittleFS
   
   # For ESP32-S3 16MB Flash + PSRAM
   pio run -e custom_ESP32s3_16M8M_LittleFS_OPI_PSRAM_ETH
   ```

5. **Build and upload:**
   ```bash
   pio run -e custom_ESP32_4M316k_LittleFS --target upload
   ```

6. **Serial monitor:**
   ```bash
   pio device monitor
   ```

### Available Build Environments

The project supports many ESP32 variants. You can change the default environment by editing the `default_envs` line in `platformio.ini`:

- **ESP32 Environments:**
  - `custom_ESP32_4M316k_LittleFS` - 4MB Flash, LittleFS (default)
  - `normal_ESP32_4M` - 4MB Flash, standard configuration
  - `max_ESP32_16M8M_LittleFS_ETH` - 16MB Flash, Ethernet support

- **ESP32-S3 Environments:**
  - `custom_ESP32s3_16M8M_LittleFS_OPI_PSRAM_ETH` - 16MB Flash, PSRAM, Ethernet

- **ESP32-C3, ESP32-S2, ESP32-C6** environments are also available

### Custom Settings

Project custom settings are defined in `src/include/Custom.h`:

```cpp
#define DEFAULT_NAME        "ENFi32"
#define FEATURE_WIFI_SMART_MANAGER           1
#define WIFI_SMART_LOGGING                   1
#define WEBSERVER_WIFI_MANAGER               1
#define WEBSERVER_SERIALMONITOR              1
```

### Build Outputs

After successful compilation, firmware files will be located at:
```
.pio/build/<environment_name>/firmware.bin
.pio/build/<environment_name>/firmware.factory.bin  # Factory image for ESP32
```

### Uploading Firmware

1. **Via USB:**
   ```bash
   pio run -e custom_ESP32_4M316k_LittleFS --target upload
   ```

2. **OTA (Over-The-Air):**
   - Go to device web interface
   - Tools > Update Firmware
   - Select the `.bin` file and upload

3. **Web Flash Tool:**
   - Open [ESPEasy Flash Page](https://td-er.nl/ESPEasy/) in Chrome or Edge browser
   - Connect device via USB
   - Select firmware file and flash

---

## Sorun Giderme / Troubleshooting

### SSL Certificate Hatası
Eğer paket indirme sırasında SSL sertifika hatası alırsanız:
1. İnternet bağlantınızı kontrol edin
2. Proxy ayarlarınızı kontrol edin
3. PlatformIO'yu güncelleyin: `pip install -U platformio`
4. Platform paketlerini manuel olarak yükleyin: `pio pkg install`

### If you get SSL certificate errors during package download:
1. Check your internet connection
2. Check your proxy settings
3. Update PlatformIO: `pip install -U platformio`
4. Manually install platform packages: `pio pkg install`

### Bellek Yetersizliği / Out of Memory
Eğer derleme sırasında bellek hatası alırsanız:
- Daha küçük bir build ortamı seçin
- `platformio.ini` dosyasında optimizasyon bayraklarını ayarlayın

### If you get out of memory errors during compilation:
- Select a smaller build environment
- Adjust optimization flags in `platformio.ini`

---

## Ek Kaynaklar / Additional Resources

- **PlatformIO Dökümantasyonu:** https://docs.platformio.org/
- **ESPEasy Dökümantasyonu:** https://espeasy.readthedocs.io/
- **ESP32 Arduino Core:** https://docs.espressif.com/projects/arduino-esp32/

---

**Not:** Bu proje ESPEasy tabanlıdır ve tüm ESPEasy özelliklerini destekler.

**Note:** This project is based on ESPEasy and supports all ESPEasy features.
