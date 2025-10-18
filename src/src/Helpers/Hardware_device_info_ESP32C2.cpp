#include "../Helpers/Hardware_device_info.h"

#ifdef ESP32C2

# include <soc/soc.h>
# include <soc/efuse_reg.h>
# include <soc/spi_reg.h>
# include <soc/spi_pins.h>
# include <soc/rtc.h>
# include <esp_chip_info.h>
# include <bootloader_common.h>


bool isFlashInterfacePin_ESPEasy(int gpio) {
  // GPIO-11: Flash voltage selector
  // For chip variants with a SiP flash built in, GPIO11~ GPIO17 are dedicated to connecting SiP flash, not for other uses
  //  return (gpio) >= 12 && (gpio) <= 17;
  switch (gpio) {
#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 4, 0)
    case SPI_IOMUX_PIN_NUM_HD:
    case SPI_IOMUX_PIN_NUM_CS:
    case SPI_IOMUX_PIN_NUM_MOSI:
    case SPI_IOMUX_PIN_NUM_CLK:
    case SPI_IOMUX_PIN_NUM_MISO:
    case SPI_IOMUX_PIN_NUM_WP:
#else
    case MSPI_IOMUX_PIN_NUM_HD:
    case MSPI_IOMUX_PIN_NUM_WP:
    case MSPI_IOMUX_PIN_NUM_CS0:
    case MSPI_IOMUX_PIN_NUM_CLK:
    case MSPI_IOMUX_PIN_NUM_MOSI:
    case MSPI_IOMUX_PIN_NUM_MISO:
#endif
      return true;
  }
  return false;
}

bool flashVddPinCanBeUsedAsGPIO()
{
  return false;
}

int32_t getEmbeddedFlashSize()
{
  // ESP32-C2 doesn't have eFuse field FLASH_CAP.
  // Can't get info about the flash chip.
  return 0;
}

int32_t getEmbeddedPSRAMSize()
{
  // Doesn't have PSRAM
  return 0;
}

# ifndef isPSRAMInterfacePin
bool isPSRAMInterfacePin(int gpio) {
  return false;
}

# endif // ifndef isPSRAMInterfacePin


const __FlashStringHelper* getChipModel(uint32_t chip_model, uint32_t chip_revision, uint32_t pkg_version, bool single_core)
{
  switch (pkg_version) {
    case 0: return F("ESP32-C2");
    case 1: return F("ESP32-C2");
  }
  return F("ESP32-C2");
}

#endif // ifdef ESP32C2
