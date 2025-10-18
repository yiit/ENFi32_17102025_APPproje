#include "../Helpers/Memory.h"


#ifdef ESP8266
extern "C" {
# include <user_interface.h>
}
#endif // ifdef ESP8266

#include "../../ESPEasy_common.h"


#ifdef ESP32
# if ESP_IDF_VERSION_MAJOR < 5
#  include <soc/cpu.h>
# endif // if ESP_IDF_VERSION_MAJOR < 5
#endif  // ifdef ESP32

#include "../Helpers/Hardware_device_info.h"

/*********************************************************************************************\
   Memory management
\*********************************************************************************************/


// For keeping track of 'cont' stack
// See: https://github.com/esp8266/Arduino/issues/2557
//      https://github.com/esp8266/Arduino/issues/5148#issuecomment-424329183
//      https://github.com/letscontrolit/ESPEasy/issues/1824
#ifdef ESP32

// FIXME TD-er: For ESP32 you need to provide the task number, or nullptr to get from the calling task.
uint32_t getCurrentFreeStack() {
  return ((uint8_t *)esp_cpu_get_sp()) - pxTaskGetStackStart(nullptr);
}

uint32_t getFreeStackWatermark() {
  return uxTaskGetStackHighWaterMark(nullptr);
}

#else // ifdef ESP32

uint32_t getCurrentFreeStack() {
  // https://github.com/esp8266/Arduino/issues/2557
  register uint32_t *sp asm ("a1");

  return 4 * (sp - g_pcont->stack);
}

uint32_t getFreeStackWatermark() {
  return cont_get_free_stack(g_pcont);
}

bool allocatedOnStack(const void *address) {
  register uint32_t *sp asm ("a1");

  if (sp < address) { return false; }
  return g_pcont->stack < address;
}

#endif // ESP32


/********************************************************************************************\
   Get free system mem
 \*********************************************************************************************/
unsigned long FreeMem()
{
  #if defined(ESP8266)
  return system_get_free_heap_size();
  #endif // if defined(ESP8266)
  #if defined(ESP32)
  return ESP.getFreeHeap();
  #endif // if defined(ESP32)
}

#ifdef USE_SECOND_HEAP
unsigned long FreeMem2ndHeap()
{
  HeapSelectIram ephemeral;

  return ESP.getFreeHeap();
}

#endif // ifdef USE_SECOND_HEAP


unsigned long getMaxFreeBlock()
{
  const unsigned long freemem = FreeMem();

  // computing max free block is a rather extensive operation, so only perform when free memory is already low.
  if (freemem < 6144) {
  #if  defined(ESP32)
    return ESP.getMaxAllocHeap();
  #endif // if  defined(ESP32)
  #ifdef CORE_POST_2_5_0
    return ESP.getMaxFreeBlockSize();
  #endif // ifdef CORE_POST_2_5_0
  }
  return freemem;
}

/********************************************************************************************\
   Special alloc functions to allocate in PSRAM if available
   See: https://github.com/espressif/esp-idf/blob/master/components/heap/port/esp32s3/memory_layout.c
 \*********************************************************************************************/
void* special_malloc(uint32_t size) {
  void *res = nullptr;

#ifdef ESP32

  if (UsePSRAM()) {
    res = heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  }
#else // ifdef ESP32
  {
# ifdef USE_SECOND_HEAP

    // Try allocating on ESP8266 2nd heap
    HeapSelectIram ephemeral;
# endif // ifdef USE_SECOND_HEAP
    res = malloc(size);
  }
#endif  // ifdef ESP32

  if (res == nullptr) {
#ifdef USE_SECOND_HEAP

    // Not successful, try allocating on (ESP8266) main heap
    HeapSelectDram ephemeral;
#endif // ifdef USE_SECOND_HEAP
    res = malloc(size);
  }

  return res;
}

void* special_realloc(void *ptr, size_t size) {
  void *res = nullptr;

#ifdef ESP32

  if (UsePSRAM()) {
    res = heap_caps_realloc(ptr, size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  }
#else // ifdef ESP32
  {
# ifdef USE_SECOND_HEAP

    // Try allocating on ESP8266 2nd heap
    HeapSelectIram ephemeral;
# endif // ifdef USE_SECOND_HEAP
    res = realloc(ptr, size);
  }
#endif  // ifdef ESP32

  if (res == nullptr) {
#ifdef USE_SECOND_HEAP

    // Not successful, try allocating on (ESP8266) main heap
    HeapSelectDram ephemeral;
#endif // ifdef USE_SECOND_HEAP
    res = realloc(ptr, size);
  }

  return res;
}

void* special_calloc(size_t num, size_t size) {
  void *res = nullptr;

#ifdef ESP32

  if (UsePSRAM()) {
    res = heap_caps_calloc(num, size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  }
#else // ifdef ESP32
# ifdef USE_SECOND_HEAP
  if (size > 64 || FreeMem() < 5000) {

    // Try allocating on ESP8266 2nd heap, only when sufficiently large data is needed
    HeapSelectIram ephemeral;
    res = calloc(num, size);
  }
# endif // ifdef USE_SECOND_HEAP
#endif  // ifdef ESP32

  if (res == nullptr) {
#ifdef USE_SECOND_HEAP

    // Not successful, try allocating on (ESP8266) main heap
    HeapSelectDram ephemeral;
#endif // ifdef USE_SECOND_HEAP
    res = calloc(num, size);
  }
#if defined(ESP8266) && defined(USE_SECOND_HEAP)
  if (res == nullptr) {
    // Not successful, try allocating on (ESP8266) 2nd heap
    HeapSelectIram ephemeral;
    res = calloc(num, size);
  }
#endif  
  return res;
}


#ifdef ESP8266
bool String_reserve_special(String& str, size_t size) {
  if (str.length() >= size) {
    // Nothing needs to be done
    return true;
  }
  #ifdef USE_SECOND_HEAP
  if (size >= 48 || FreeMem() < 5000) {
    // Only try to store larger strings here as those tend to be kept for a longer period.
    HeapSelectIram ephemeral;
    // String does round up to nearest multiple of 16 bytes, so no need to round up to multiples of 32 bit here
    if (str.reserve(size)) {
      return true;
    }
  }
  #endif
  return str.reserve(size);
}
#endif

#ifdef ESP32

// Special class to get access to the protected String functions
// This class only has a constructor which will perform 
// the requested allocation in PSRAM when possible
class PSRAM_String : public String {
  public:
  PSRAM_String(size_t size) : String() {
    sso.isSSO = 0;      // setSSO(false);
    ptr.buff = nullptr; // setBuffer(nullptr);
    ptr.cap = 0;        // setCapacity(0);
    ptr.len = 0;        // setLen(0);

    if (size != 0 && size > capacity() && UsePSRAM()) {
      size_t newSize = (size + 16) & (~0xf);
      void *ptr = special_calloc(1, newSize);
      if (ptr != nullptr) {
        setSSO(false);
        setBuffer((char *)ptr);
        setCapacity(newSize - 1);
        setLen(newSize - 1); // TD-er: Not sure if needed?
      }
    }
  }
};


bool String_reserve_special(String& str, size_t size) {
  if (size == 0) {
    return true;
  }
  if (!UsePSRAM()) {
    return str.reserve(size);
  }
  if (str.length() <= size) {
    // As we like to move this to PSRAM, it also makes sense 
    // to do this when the length equals size
    PSRAM_String psram_str(size);

    if (psram_str.length() == 0) {
      return str.reserve(size);
    }

    // Copy any existing content
    String tmp(std::move(str));

    // Move the newly allocated buffer to a tmp String object.
    // Needs to be empty, so the buffer is moved.
    // N.B. String::clear() = String::setlen(0))
    str = std::move(psram_str);
    str.clear();  

    if (tmp.length()) {
      str = tmp;
    }
  }
  return true;
}
#endif
