#include "../Helpers/Hardware_GPIO.h"

#ifdef ESP32C6
# include <driver/gpio.h>

# include "../Helpers/Hardware_device_info.h"

// ********************************************************************************
// Get info of a specific GPIO pin
// ********************************************************************************

bool getGpioInfo(int gpio, int& pinnr, bool& input, bool& output, bool& warning) {
  pinnr = -1; // ESP32 does not label the pins, they just use the GPIO number.

  input   = GPIO_IS_VALID_GPIO(gpio);
  output  = GPIO_IS_VALID_OUTPUT_GPIO(gpio);
  warning = isBootStrapPin(gpio);

  if (!(GPIO_IS_VALID_GPIO(gpio))) { return false; }

  if (gpio == 27) {
    // By default VDD_SPI is the power supply pin for embedded flash or external flash. It can only be used as GPIO
    // only when the chip is connected to an external flash, and this flash is powered by an external power supply
    input   = false;
    output  = false;
    warning = true;
  }

  if (isFlashInterfacePin_ESPEasy(gpio)) {
    // Connected to the integrated SPI flash.
    input   = false;
    output  = false;
    warning = true;
  }

  if ((gpio == PIN_USB_D_MIN) || (gpio == PIN_USB_D_PLUS)) {
    // USB OTG and USB Serial/JTAG function. USB signal is a differential
    // signal transmitted over a pair of D+ and D- wires.
    warning = true;
  }

  return (input || output);
}

bool isBootStrapPin(int gpio) 
{ 
  // See: https://www.espressif.com/sites/default/files/documentation/esp32-c6_datasheet_en.pdf 
  // Chapter 3 - Boot Configurations 
  if (gpio == 8) { 
    // Strapping pin which must be high during flashing 
    // ROM message printing 
    return true; 
  } 
 
  if (gpio == 9) { 
    // Strapping pin to force download mode (like GPIO-0 on ESP8266/ESP32-classic) 
    // internal Weak pull-up, must be pulled down to enter download boot mode. 
    return true; 
  } 
 
  if (gpio == 4 || gpio == 5) { 
    // SDIO Sampling and Driving Clock Edge 
    //  MTMS = GPIO-4 
    //  MTDI = Gpio-5 
    return true; 
  } 
 
  if (gpio == 15) { 
    // JTAG signal source 
    return true; 
  } 
 
  return false; 
} 

bool getGpioPullResistor(int gpio, bool& hasPullUp, bool& hasPullDown) {
  hasPullDown = false;
  hasPullUp   = false;

  if (validGpio(gpio)) {
    hasPullUp   = true;
    hasPullDown = true;
  }

  return true;
}

bool validGpio(int gpio) {
  if (!GPIO_IS_VALID_GPIO(gpio)) { return false; }

  int  pinnr;
  bool input;
  bool output;
  bool warning;

  return getGpioInfo(gpio, pinnr, input, output, warning);
}

// Get ADC related info for a given GPIO pin
// @param gpio_pin   GPIO pin number
// @param adc        Number of ADC unit (0 == Hall effect)
// @param ch         Channel number on ADC unit
// @param t          index of touch pad ID
bool getADC_gpio_info(int gpio_pin, int& adc, int& ch, int& t)
{
  adc = -1;
  ch  = -1;
  t   = -1;

  if ((gpio_pin >= 0) && (gpio_pin <= 6)) {
    adc = 1;
    ch  = gpio_pin;
    return true;
  }
  return false;
}

#if SOC_TOUCH_SENSOR_SUPPORTED
int touchPinToGpio(int touch_pin)
{
  // No touch pin support
  return -1;
}
#endif

#if SOC_DAC_SUPPORTED
// Get DAC related info for a given GPIO pin
// @param gpio_pin   GPIO pin number
// @param dac        Number of DAC unit
bool getDAC_gpio_info(int gpio_pin, int& dac)
{
  // ESP32-C3, ESP32-S3, ESP32-C2, ESP32-C6 and ESP32-H2 don't have a DAC onboard
  return false;
}
#endif

#endif // ifdef ESP32C6
