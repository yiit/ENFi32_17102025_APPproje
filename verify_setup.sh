#!/bin/bash
# ENFi32 PlatformIO Kurulum Doğrulama Scripti
# ENFi32 PlatformIO Setup Verification Script

echo "=============================================="
echo "ENFi32 PlatformIO Kurulum Doğrulaması"
echo "ENFi32 PlatformIO Setup Verification"
echo "=============================================="
echo ""

# Check if we're in the right directory
if [ ! -f "platformio.ini" ]; then
    echo "❌ HATA: platformio.ini bulunamadı!"
    echo "❌ ERROR: platformio.ini not found!"
    echo "   Lütfen proje klasöründe olduğunuzdan emin olun."
    echo "   Please make sure you are in the project folder."
    exit 1
fi

echo "✅ platformio.ini bulundu / found"

# Check if PlatformIO is installed
if ! command -v pio &> /dev/null; then
    echo "❌ HATA: PlatformIO CLI bulunamadı!"
    echo "❌ ERROR: PlatformIO CLI not found!"
    echo ""
    echo "   Kurulum / Installation:"
    echo "   pip install -U platformio"
    echo ""
    exit 1
fi

echo "✅ PlatformIO CLI bulundu / found"

# Get PlatformIO version
PIO_VERSION=$(pio --version 2>&1 | grep -oP 'version \K[0-9.]+' || echo "unknown")
echo "   Sürüm / Version: $PIO_VERSION"

# Check Python version
PYTHON_VERSION=$(python3 --version 2>&1 | grep -oP 'Python \K[0-9.]+' || echo "unknown")
echo "✅ Python sürümü / version: $PYTHON_VERSION"

echo ""
echo "=============================================="
echo "Kullanılabilir Derleme Ortamları"
echo "Available Build Environments"
echo "=============================================="

# List some key environments
echo ""
echo "ESP32 Ortamları / Environments:"
echo "  - custom_ESP32_4M316k_LittleFS (varsayılan/default)"
echo "  - custom_ESP32_4M316k_LittleFS_ETH"
echo "  - custom_ESP32_16M8M_LittleFS_ETH"

echo ""
echo "ESP32-S3 Ortamları / Environments:"
echo "  - custom_ESP32s3_4M316k_LittleFS_ETH"
echo "  - custom_ESP32s3_8M1M_LittleFS_OPI_PSRAM_ETH"
echo "  - custom_ESP32s3_16M8M_LittleFS_OPI_PSRAM_ETH"

echo ""
echo "=============================================="
echo "Hızlı Test / Quick Test"
echo "=============================================="
echo ""
echo "Varsayılan ortamı derlemek için / To build default environment:"
echo "  pio run"
echo ""
echo "Belirli bir ortamı derlemek için / To build specific environment:"
echo "  pio run -e custom_ESP32_4M316k_LittleFS"
echo ""
echo "Derleme ve yükleme / Build and upload:"
echo "  pio run -e custom_ESP32_4M316k_LittleFS --target upload"
echo ""

# Check if we can parse the platformio.ini
echo "Konfigürasyon kontrolü / Configuration check..."
if pio project config >/dev/null 2>&1; then
    echo "✅ platformio.ini konfigürasyonu geçerli / valid"
else
    echo "⚠️  UYARI: platformio.ini parse edilemedi"
    echo "⚠️  WARNING: Could not parse platformio.ini"
    echo "   Bu normal olabilir, bazı platformlar henüz yüklenmemiş olabilir."
    echo "   This may be normal, some platforms may not be installed yet."
fi

echo ""
echo "=============================================="
echo "✅ Kurulum doğrulaması tamamlandı!"
echo "✅ Setup verification completed!"
echo "=============================================="
echo ""
echo "Daha fazla bilgi için / For more information:"
echo "  BUILD_GUIDE.md dosyasını okuyun / Read BUILD_GUIDE.md"
echo ""
