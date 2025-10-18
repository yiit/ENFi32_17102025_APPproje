#include "../Helpers/Hardware_GPIO.h"

#ifdef ESP32P4
# include <driver/gpio.h>

# include "../Globals/Settings.h"
# include "../Helpers/Hardware_device_info.h"

// ********************************************************************************
// Get info of a specific GPIO pin
// ********************************************************************************

bool getGpioInfo(int gpio, int& pinnr, bool& input, bool& output, bool& warning) {
  pinnr = -1; // ESP32 does not label the pins, they just use the GPIO number.

  input   = GPIO_IS_VALID_GPIO(gpio);
  output  = GPIO_IS_VALID_OUTPUT_GPIO(gpio);
  warning = isBootStrapPin(gpio);

  if ((gpio < 0) || !(GPIO_IS_VALID_GPIO(gpio))) { return false; }

  // P4 does not use GPIO-pins for accessing flash or PSRAM

# if FEATURE_ETHERNET

  // Check pins used for RMII Ethernet PHY
  if (NetworkMedium_t::Ethernet == Settings.NetworkMedium) {
    switch (gpio) {
      case 28: // RMII_RXDV
      case 29: // RMII_RXD0
      case 30: // RMII_RXD1
      case 34: // RMII_TXD0
      case 35: // RMII_TXD1
      case 49: // RMII_TXEN
      case 50: // RMII_CLK
      case 51: // PHY_RSTN
      case 52: // MDIO
        warning = true;
        break;
    }


    // FIXME TD-er: Must we also check for pins used for MDC/MDIO and Eth PHY power?
  }
# endif // if FEATURE_ETHERNET

  return (input || output);
}

bool isBootStrapPin(int gpio)
{

  return false;
}

bool getGpioPullResistor(int gpio, bool& hasPullUp, bool& hasPullDown) {
  hasPullDown = false;
  hasPullUp   = false;

  int  pinnr;
  bool input;
  bool output;
  bool warning;

  if (!getGpioInfo(gpio, pinnr, input, output, warning)) {
    return false;
  }

  hasPullUp   = true;
  hasPullDown = true;

  return true;
}

bool validGpio(int gpio) {
  if (gpio < 0) { return false; }

  if (!GPIO_IS_VALID_GPIO(gpio)) { return false; }

  int  pinnr;
  bool input;
  bool output;
  bool warning;

  return getGpioInfo(gpio, pinnr, input, output, warning);
}

// Get ADC related info for a given GPIO pin
// @param gpio_pin   GPIO pin number
// @param adc        Number of ADC unit
// @param ch         Channel number on ADC unit
// @param t          index of touch pad ID
bool getADC_gpio_info(int gpio_pin, int& adc, int& ch, int& t)
{
  adc = -1;
  ch  = -1;
  t   = -1;

  switch (gpio_pin) {
    case 16: adc = 1; ch = 0; break;
    case 17: adc = 1; ch = 1; break;
    case 18: adc = 1; ch = 2; break;
    case 19: adc = 1; ch = 3; break;
    case 20: adc = 1; ch = 4; break;
    case 21: adc = 1; ch = 5; break;
    case 22: adc = 1; ch = 6; break;
    case 23: adc = 1; ch = 7; break;
    case 49: adc = 2; ch = 0; break;
    case 50: adc = 2; ch = 1; break;
    case 51: adc = 2; ch = 2; break;
    case 52: adc = 2; ch = 3; break;
    case 53: adc = 2; ch = 4; break;
    case 54: adc = 2; ch = 5; break;
    default:
      return false;
  }

  return true;
}

#if SOC_TOUCH_SENSOR_SUPPORTED
int touchPinToGpio(int touch_pin)
{
  if (touch_pin >= 0 && touch_pin < SOC_TOUCH_SENSOR_NUM) {
    return touch_pin + 2;
  }
  return -1;
}
#endif

#if SOC_DAC_SUPPORTED
// Get DAC related info for a given GPIO pin
// @param gpio_pin   GPIO pin number
// @param dac        Number of DAC unit
bool getDAC_gpio_info(int gpio_pin, int& dac)
{
  return false;
}
#endif

#endif
