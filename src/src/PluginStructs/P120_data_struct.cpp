#include "../PluginStructs/P120_data_struct.h"

#ifdef USES_P120

#include "../Globals/ESPEasy_time.h"
#include "../Helpers/ESPEasy_Storage.h"
#include "../Commands/ExecuteCommand.h"
#include "../../ESPEasy-Globals.h"

// Constructor
P120_data_struct::P120_data_struct() {
  resetRingBuffer();
  current_mode = 1; // Start in ART mode
  rearm_ready = false;
  last_trigger_weight = NAN;
}

// Ring Buffer Methods
void P120_data_struct::setRingBufferSize(uint16_t size) {
  ring_size = constrain(size, (uint16_t)1, MAX_SAMPLES);
  resetRingBuffer();
}

void P120_data_struct::resetRingBuffer() {
  ring_count = 0;
  ring_index = 0;
  ring_sum = 0.0;
}

float P120_data_struct::addSample(float sample) {
  if (ring_size == 0) {
    setRingBufferSize(8); // Default window size
  }

  if (ring_count < ring_size) {
    ring_buffer[ring_count++] = sample;
    ring_sum += sample;
  } else {
    float old = ring_buffer[ring_index];
    ring_sum -= old;
    ring_buffer[ring_index] = sample;
    ring_sum += sample;

    ring_index++;
    if (ring_index >= ring_size) ring_index = 0;
  }

  return (float)(ring_sum / (double)ring_count);
}

// Button Handling
bool P120_data_struct::handleButtons(int pin1, int pin2) {
  static unsigned long debounce_time1 = 0;
  static unsigned long debounce_time2 = 0;
  static const unsigned long DEBOUNCE_DELAY = 50;
  static const unsigned long LONGPRESS_TIME = 1000;
  
  bool action_taken = false;
  
  // Button 1 handling
  bool button1_state = digitalRead(pin1);
  if (button1_state != button1_last_state) {
    debounce_time1 = millis();
  }
  
  if ((millis() - debounce_time1) > DEBOUNCE_DELAY) {
    if (button1_state != button1_pressed) {
      button1_pressed = button1_state;
      
      if (button1_pressed == LOW) { // Pressed
        button1_press_time = millis();
      } else { // Released
        unsigned long press_duration = millis() - button1_press_time;
        if (press_duration > LONGPRESS_TIME) {
          // Long press - toggle serial
          Settings.UseSerial = !Settings.UseSerial;
        } else {
          // Short press - ART command
          ExecuteCommand_all_config(ExecuteCommandArgs(EventValueSource::Enum::VALUE_SOURCE_WEB_FRONTEND, "fyzart"));
        }
        action_taken = true;
      }
    }
  }
  button1_last_state = button1_state;
  
  // Button 2 handling  
  bool button2_state = digitalRead(pin2);
  if (button2_state != button2_last_state) {
    debounce_time2 = millis();
  }
  
  if ((millis() - debounce_time2) > DEBOUNCE_DELAY) {
    if (button2_state != button2_pressed) {
      button2_pressed = button2_state;
      
      if (button2_pressed == LOW) { // Pressed
        button2_press_time = millis();
      } else { // Released
        unsigned long press_duration = millis() - button2_press_time;
        if (press_duration > LONGPRESS_TIME) {
          // Long press - KOPYA
          ExecuteCommand_all_config(ExecuteCommandArgs(EventValueSource::Enum::VALUE_SOURCE_WEB_FRONTEND, "fyzkop"));
        } else {
          // Short press - TOP
          ExecuteCommand_all_config(ExecuteCommandArgs(EventValueSource::Enum::VALUE_SOURCE_WEB_FRONTEND, "fyztop"));
        }
        action_taken = true;
      }
    }
  }
  button2_last_state = button2_state;
  
  return action_taken;
}

// Stability Check
bool P120_data_struct::isStable(float current, float previous, uint8_t decimals, uint16_t required_samples) {
  static const int32_t SCALE_TAB[7] = { 1,10,100,1000,10000,100000,1000000 };
  const int32_t SCALE = (decimals <= 6) ? SCALE_TAB[decimals] : 1000;
  
  const int32_t a = (int32_t)lroundf(current * SCALE);
  const int32_t b = (int32_t)lroundf(previous * SCALE);
  const int32_t diff = a - b;
  
  // Deadband check
  if ((diff >= -1) && (diff <= 1)) {
    if (stabil_counter < 0xFFFF) ++stabil_counter;
  } else {
    stabil_counter = 0;
  }
  
  return (stabil_counter >= required_samples);
}

// Task Values Update
void P120_data_struct::updateTaskValues(struct EventStruct *event) {
  // Update ExtraTaskSettings values
  P120_CONFIG_DARA_VALUE = (long)(webapidaratartim * 1000);
  P120_CONFIG_BRUT_VALUE = (long)(webapibruttartim * 1000);
  P120_CONFIG_ADET_VALUE = (long)webapiadet;
  P120_CONFIG_TOP_NET_VALUE = (long)(top_net * 1000);
  P120_CONFIG_TOP_DARA_VALUE = (long)(top_dara * 1000);
  P120_CONFIG_TOP_BRUT_VALUE = (long)(top_brut * 1000);
  P120_CONFIG_SERI_NO = seri_no;
  P120_CONFIG_BIRIM_FIYAT = (long)(webapibfiyat * 1000);
  P120_CONFIG_TUTAR = (long)(webapitutar * 1000);
  
  // Update UserVar based on value types
  for (uint8_t varIndex = 0; varIndex < 4; varIndex++) {
    uint8_t valueType = getValueType(varIndex);
    
    float sourceValue = 0.0;
    switch (varIndex) {
      case 0: sourceValue = webapinettartim; break;
      case 1: sourceValue = webapidaratartim; break;
      case 2: sourceValue = webapibruttartim; break;
      case 3: sourceValue = webapiadet; break;
    }
    
    switch (valueType) {
      case P120_TYPE_FLOAT:
        UserVar.setFloat(event->TaskIndex, varIndex, sourceValue);
        break;
      case P120_TYPE_INTEGER:
        UserVar.setInt32(event->TaskIndex, varIndex, (int32_t)sourceValue);
        break;
      case P120_TYPE_STRING:
        UserVar.setFloat(event->TaskIndex, varIndex, sourceValue); // Store as float for string conversion
        break;
      case P120_TYPE_BOOLEAN:
        UserVar.setInt32(event->TaskIndex, varIndex, sourceValue > 0.0 ? 1 : 0);
        break;
    }
  }
}

// Value Type Management
uint8_t P120_data_struct::getValueType(uint8_t varIndex) {
  if (varIndex >= 4) return P120_TYPE_FLOAT;
  return (P120_CONFIG_NET_VALUE >> (varIndex * 2)) & 0x03;
}

void P120_data_struct::setValueType(uint8_t varIndex, uint8_t valueType) {
  if (varIndex >= 4) return;
  
  long currentConfig = P120_CONFIG_NET_VALUE;
  currentConfig &= ~(0x03L << (varIndex * 2)); // Clear 2 bits
  currentConfig |= ((long)(valueType & 0x03)) << (varIndex * 2); // Set new 2 bits
  P120_CONFIG_NET_VALUE = currentConfig;
}

// Get Value By Name
String P120_data_struct::getValueByName(const String& valueName) {
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
  
  return "0.00"; // Default
}

// Global access functions
P120_data_struct* P120_getPluginData(struct EventStruct *event) {
  return static_cast<P120_data_struct*>(getPluginTaskData(event->TaskIndex));
}

void P120_releasePluginData(struct EventStruct *event) {
  delete static_cast<P120_data_struct*>(getPluginTaskData(event->TaskIndex));
  initPluginTaskData(event->TaskIndex, nullptr);
}

#endif // USES_P120