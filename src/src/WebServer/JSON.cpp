#include "../WebServer/JSON.h"

#include "../WebServer/ESPEasy_WebServer.h"
#include "../WebServer/JSON.h"
#include "../WebServer/Markup_Forms.h"

#include "../CustomBuild/CompiletimeDefines.h"

#include "../DataStructs/TimingStats.h"

#include "../Globals/Cache.h"
#include "../Globals/Nodes.h"
#include "../Globals/Device.h"
#include "../Globals/Plugins.h"
#include "../Globals/NPlugins.h"

#include "../Helpers/_Plugin_init.h"
#include "../Helpers/ESPEasyStatistics.h"
#include "../Helpers/ESPEasy_Storage.h"
#include "../Helpers/Numerical.h"
#include "../Helpers/StringConverter.h"
#include "../Helpers/StringProvider.h"
#include "../Helpers/StringGenerator_System.h"

#include "../../_Plugin_Helper.h"
#include "../../ESPEasy-Globals.h"

void stream_comma_newline() {
  addHtml(',', '\n');
}


// ********************************************************************************
// Web Interface get CSV value from task
// ********************************************************************************
#ifdef WEBSERVER_CSVVAL
void handle_csvval()
{
  TXBuffer.startJsonStream();
  const int printHeader = getFormItemInt(F("header"), 1);
  bool printHeaderValid = true;
  if (printHeader != 1 && printHeader != 0)
  {
    addHtml(F("ERROR: Header not valid!\n"));
    printHeaderValid = false;
  }

  const taskIndex_t taskNr    = getFormItemInt(F("tasknr"), INVALID_TASK_INDEX);
  const bool taskValid = validTaskIndex(taskNr);
  if (!taskValid)
  {
    addHtml(F("ERROR: TaskNr not valid!\n"));
  }

  const int INVALID_VALUE_NUM = INVALID_TASKVAR_INDEX + 1;
  const taskVarIndex_t valNr    = getFormItemInt(F("valnr"), INVALID_VALUE_NUM);
  bool valueNumberValid = true;
  if (valNr != INVALID_VALUE_NUM && !validTaskVarIndex(valNr))
  {
    addHtml(F("ERROR: ValueId not valid!\n"));
    valueNumberValid = false;
  }

  if (taskValid && valueNumberValid && printHeaderValid)
  {
    const deviceIndex_t DeviceIndex = getDeviceIndex_from_TaskIndex(taskNr);

    if (validDeviceIndex(DeviceIndex))
    {
      const uint8_t taskValCount = getValueCountForTask(taskNr);

      if (printHeader)
      {
        for (uint8_t x = 0; x < taskValCount; x++)
        {
          if (valNr == INVALID_VALUE_NUM || valNr == x)
          {
            addHtml(Cache.getTaskDeviceValueName(taskNr, x));
            if (x != taskValCount - 1)
            {
              addHtml(';');
            }
          }
        }
        addHtml('\n');
      }

      struct EventStruct TempEvent(taskNr);

      for (uint8_t x = 0; x < taskValCount; x++)
      {
        if ((valNr == INVALID_VALUE_NUM) || (valNr == x))
        {
          addHtml(formatUserVarNoCheck(&TempEvent, x));

          if (x != taskValCount - 1)
          {
            addHtml(';');
          }
        }
      }
      addHtml('\n');
    }
  }
  TXBuffer.endStream();
}
#endif

// ********************************************************************************
// Web Interface JSON page (no password!)
// ********************************************************************************
void handle_json()
{
  START_TIMER
  const taskIndex_t taskNr    = getFormItemInt(F("tasknr"), INVALID_TASK_INDEX);
  const bool showSpecificTask = validTaskIndex(taskNr);
  bool showSystem             = true;
  bool showWifi               = true;

  #if FEATURE_ETHERNET
  bool showEthernet = true;
  #endif // if FEATURE_ETHERNET
  bool showDataAcquisition = true;
  bool showTaskDetails     = true;
  #if FEATURE_ESPEASY_P2P
  bool showNodes           = true;
  #endif
  #if FEATURE_PLUGIN_STATS
  bool showPluginStats     = getFormItemInt(F("showpluginstats"), 0) != 0;
  #endif

  if (equals(webArg(F("view")), F("sensorupdate"))) {
    showSystem = false;
    showWifi   = false;
    #if FEATURE_ETHERNET
    showEthernet = false;
    #endif // if FEATURE_ETHERNET
    showDataAcquisition = false;
    showTaskDetails     = false;
    #if FEATURE_ESPEASY_P2P
    showNodes           = false;
    #endif
    #if FEATURE_PLUGIN_STATS
    showPluginStats     = hasArg(F("showpluginstats"));
    #endif
  }

  TXBuffer.startJsonStream();

  if (!showSpecificTask)
  {
    addHtml('{');

    if (showSystem) {
      addHtml(F("\"System\":{\n"));

      if (wdcounter > 0)
      {
        stream_next_json_object_value(LabelType::LOAD_PCT);
        stream_next_json_object_value(LabelType::LOOP_COUNT);
      }

      static const LabelType::Enum labels[] PROGMEM =
      {
        LabelType::BUILD_DESC,
        LabelType::GIT_BUILD,
        LabelType::SYSTEM_LIBRARIES,
#ifdef ESP32
        LabelType::ESP_IDF_SDK_VERSION,
#endif
        LabelType::PLUGIN_COUNT,
        LabelType::PLUGIN_DESCRIPTION,
        LabelType::BUILD_TIME,
        LabelType::BINARY_FILENAME,
        LabelType::LOCAL_TIME,
        #if FEATURE_EXT_RTC
        LabelType::EXT_RTC_UTC_TIME,
        #endif
        LabelType::TIME_SOURCE,
        LabelType::TIME_WANDER,
        LabelType::ISNTP,
        LabelType::UNIT_NR,
        LabelType::UNIT_NAME,
        LabelType::UPTIME,
        LabelType::UPTIME_MS,
#if FEATURE_INTERNAL_TEMPERATURE
        LabelType::INTERNAL_TEMPERATURE,
#endif
        LabelType::BOOT_TYPE,
        LabelType::RESET_REASON,
        LabelType::CPU_ECO_MODE,

    #if defined(CORE_POST_2_5_0) || defined(ESP32)
      #ifndef LIMIT_BUILD_SIZE
        LabelType::HEAP_MAX_FREE_BLOCK, // 7654
      #endif
    #endif // if defined(CORE_POST_2_5_0) || defined(ESP32)
    #if defined(CORE_POST_2_5_0)
      #ifndef LIMIT_BUILD_SIZE
        LabelType::HEAP_FRAGMENTATION,  // 12
      #endif
    #endif // if defined(CORE_POST_2_5_0)
        LabelType::FREE_MEM,
      #ifdef USE_SECOND_HEAP
        LabelType::FREE_HEAP_IRAM,
      #endif
        LabelType::FREE_STACK,

    #ifdef ESP32
        LabelType::HEAP_SIZE,
        LabelType::HEAP_MIN_FREE,
        #ifdef BOARD_HAS_PSRAM
        LabelType::PSRAM_SIZE,
        LabelType::PSRAM_FREE,
        LabelType::PSRAM_MIN_FREE,
        LabelType::PSRAM_MAX_FREE_BLOCK,
        #endif // BOARD_HAS_PSRAM
    #endif // ifdef ESP32
        LabelType::ESP_CHIP_MODEL,
    #ifdef ESP32
        LabelType::ESP_CHIP_REVISION,
    #endif // ifdef ESP32
        LabelType::FLASH_CHIP_ID,
        LabelType::FLASH_CHIP_VENDOR,
        LabelType::FLASH_CHIP_MODEL,
        LabelType::FLASH_CHIP_REAL_SIZE,
        LabelType::FLASH_CHIP_SPEED,
        LabelType::FLASH_IDE_MODE,
        LabelType::FS_SIZE,

        LabelType::SUNRISE,
        LabelType::SUNSET,
        LabelType::TIMEZONE_OFFSET,
        LabelType::LATITUDE,
        LabelType::LONGITUDE,
        LabelType::SYSLOG_LOG_LEVEL,
        LabelType::SERIAL_LOG_LEVEL,
        LabelType::WEB_LOG_LEVEL,
        #if FEATURE_SD
        LabelType::SD_LOG_LEVEL,
        #endif // if FEATURE_SD


        LabelType::MAX_LABEL
      };

      stream_json_object_values(labels);
      stream_comma_newline();
    }

    if (showWifi) {
      addHtml(F("\"WiFi\":{\n"));
      static const LabelType::Enum labels[] PROGMEM =
      {
        LabelType::HOST_NAME,
        #if FEATURE_MDNS
        LabelType::M_DNS,
        #endif // if FEATURE_MDNS
        LabelType::IP_CONFIG,
        LabelType::IP_ADDRESS,
#if FEATURE_USE_IPV6
        LabelType::IP6_LOCAL,
        LabelType::IP6_GLOBAL,
        LabelType::ENABLE_IPV6,
#endif
        LabelType::IP_SUBNET,
        LabelType::GATEWAY,
        LabelType::STA_MAC,
        LabelType::DNS_1,
        LabelType::DNS_2,
        LabelType::SSID,
        LabelType::BSSID,
        LabelType::CHANNEL,
        LabelType::ENCRYPTION_TYPE_STA,
        LabelType::CONNECTED_MSEC,
        LabelType::LAST_DISCONNECT_REASON,
        LabelType::LAST_DISC_REASON_STR,
        LabelType::NUMBER_RECONNECTS,
        LabelType::WIFI_STORED_SSID1,
        LabelType::WIFI_STORED_SSID2,
        LabelType::FORCE_WIFI_BG,
        LabelType::RESTART_WIFI_LOST_CONN,
        LabelType::FORCE_WIFI_NOSLEEP,
#ifdef SUPPORT_ARP
        LabelType::PERIODICAL_GRAT_ARP,
#endif // ifdef SUPPORT_ARP
#ifdef USES_ESPEASY_NOW
        LabelType::USE_ESPEASY_NOW,
        LabelType::FORCE_ESPEASY_NOW_CHANNEL,
#endif
        LabelType::CONNECTION_FAIL_THRESH,
#if FEATURE_SET_WIFI_TX_PWR
        LabelType::WIFI_TX_MAX_PWR,
        LabelType::WIFI_CUR_TX_PWR,
        LabelType::WIFI_SENS_MARGIN,
        LabelType::WIFI_SEND_AT_MAX_TX_PWR,
#endif
        LabelType::WIFI_NR_EXTRA_SCANS,
#ifdef ESP32
        LabelType::WIFI_PASSIVE_SCAN,
#endif
        LabelType::WIFI_USE_LAST_CONN_FROM_RTC,
        LabelType::WIFI_RSSI,
#ifndef ESP32
        LabelType::WAIT_WIFI_CONNECT,
#endif
        LabelType::HIDDEN_SSID_SLOW_CONNECT,
        LabelType::CONNECT_HIDDEN_SSID,
        LabelType::SDK_WIFI_AUTORECONNECT,

        LabelType::MAX_LABEL
      };

      stream_json_object_values(labels);

      // TODO: PKR: Add ETH Objects
      stream_comma_newline();
    }

    #if FEATURE_ETHERNET

    if (showEthernet) {
      addHtml(F("\"Ethernet\":{\n"));
      static const LabelType::Enum labels[] PROGMEM =
      {
        LabelType::ETH_WIFI_MODE,
        LabelType::ETH_CONNECTED,
        LabelType::ETH_CHIP,
        LabelType::ETH_DUPLEX,
        LabelType::ETH_SPEED,
        LabelType::ETH_STATE,
        LabelType::ETH_SPEED_STATE,


        LabelType::MAX_LABEL
      };

      stream_json_object_values(labels);
      stream_comma_newline();
    }
    #endif // if FEATURE_ETHERNET

  #if FEATURE_ESPEASY_P2P
    if (showNodes) {
      bool comma_between = false;

      for (auto it = Nodes.begin(); it != Nodes.end(); ++it)
      {
        if (it->second.ip[0] != 0)
        {
          if (comma_between) {
            addHtml(',');
          } else {
            comma_between = true;
            addHtml(F("\"nodes\":[\n")); // open json array if >0 nodes
          }

          addHtml('{');
          stream_next_json_object_value(F("nr"), it->first);
          stream_next_json_object_value(F("name"),
                                        (it->first != Settings.Unit) ? it->second.getNodeName() : Settings.getName());

          if (it->second.build) {
            stream_next_json_object_value(F("build"), formatSystemBuildNr(it->second.build));
          }

          if (it->second.nodeType) {
            stream_next_json_object_value(F("platform"), it->second.getNodeTypeDisplayString());
          }
          const int8_t rssi = it->second.getRSSI();
          if (rssi < 0) {
            stream_next_json_object_value(F("rssi"), rssi);
          }
          if (it->second.build >= 20107) {
            stream_next_json_object_value(F("load"), toString(it->second.getLoad(), 2));
            if (it->second.webgui_portnumber != 80) {
              stream_next_json_object_value(F("webport"), it->second.webgui_portnumber);
            }
          }
          stream_next_json_object_value(F("ip"), formatIP(it->second.IP()));
#if FEATURE_USE_IPV6
          if (it->second.hasIPv6_mac_based_link_local) {
            stream_next_json_object_value(F("ipv6local"), formatIP(it->second.IPv6_link_local(true), true));
          }
          if (it->second.hasIPv6_mac_based_link_global) {
            stream_next_json_object_value(F("ipv6global"), formatIP(it->second.IPv6_global()));
          }
#endif
          stream_last_json_object_value(F("age"), it->second.getAge());
        } // if node info exists
      }   // for loop

      if (comma_between) {
        addHtml(F("],\n")); // close array if >0 nodes
      }
    }
  #endif
  }

  taskIndex_t firstTaskIndex = 0;
  taskIndex_t lastTaskIndex  = TASKS_MAX - 1;

  if (showSpecificTask)
  {
    firstTaskIndex = taskNr - 1;
    lastTaskIndex  = taskNr - 1;
  }
  taskIndex_t lastActiveTaskIndex = 0;

  for (taskIndex_t TaskIndex = firstTaskIndex; TaskIndex <= lastTaskIndex; TaskIndex++) {
    if (validPluginID_fullcheck(Settings.getPluginID_for_task(TaskIndex))) {
      lastActiveTaskIndex = TaskIndex;
    }
  }

  if (!showSpecificTask) {
    addHtml(F("\"Sensors\":[\n"));
  }

  // Keep track of the lowest reported TTL and use that as refresh interval.
  unsigned long lowest_ttl_json = 60;

  for (taskIndex_t TaskIndex = firstTaskIndex; TaskIndex <= lastActiveTaskIndex && validTaskIndex(TaskIndex); TaskIndex++)
  {
    const deviceIndex_t DeviceIndex = getDeviceIndex_from_TaskIndex(TaskIndex);

    if (validDeviceIndex(DeviceIndex))
    {
      const unsigned long taskInterval = Settings.TaskDeviceTimer[TaskIndex];
      //LoadTaskSettings(TaskIndex);
      addHtml('{', '\n');

      unsigned long ttl_json = 60; // Default value

      // For simplicity, do the optional values first.
      const uint8_t valueCount = getValueCountForTask(TaskIndex);

      if (valueCount != 0) {
        if (Settings.TaskDeviceEnabled[TaskIndex]) {
          if (taskInterval == 0) {
            ttl_json = 1;
          } else {
            ttl_json = taskInterval;
          }

          if (ttl_json < lowest_ttl_json) {
            lowest_ttl_json = ttl_json;
          }
        }
        addHtml(F("\"TaskValues\": [\n"));

        struct EventStruct TempEvent(TaskIndex);

        for (uint8_t x = 0; x < valueCount; x++)
        {
          uint8_t nrDecimals = Cache.getTaskDeviceValueDecimals(TaskIndex, x);
          String  value      = formatUserVarNoCheck(&TempEvent, x);
          #if FEATURE_STRING_VARIABLES
          bool hasPresentation;
          const String presentation = formatUserVarForPresentation(&TempEvent, x, hasPresentation, value, DeviceIndex);
          #endif // if FEATURE_STRING_VARIABLES
          
          if (mustConsiderAsJSONString(value)) {
            // Flag as not to treat as a float
            nrDecimals = 255;
          }
          #if FEATURE_TASKVALUE_UNIT_OF_MEASURE
          String uom;
          const uint8_t uomIndex = Cache.getTaskVarUnitOfMeasure(TaskIndex, x);
          if (uomIndex != 0) {
            uom = toUnitOfMeasureName(uomIndex);
          }
          #else // if FEATURE_TASKVALUE_UNIT_OF_MEASURE
          const String uom;
          #endif // if FEATURE_TASKVALUE_UNIT_OF_MEASURE
          handle_json_stream_task_value_data(x + 1,
                                             Cache.getTaskDeviceValueName(TaskIndex, x),
                                             nrDecimals,
                                             value,
                                             #if FEATURE_STRING_VARIABLES
                                             presentation,
                                             #else // if FEATURE_STRING_VARIABLES
                                             EMPTY_STRING,
                                             #endif // if FEATURE_STRING_VARIABLES
                                             uom,
                                             x < (valueCount - 1));
        }
        #if FEATURE_STRING_VARIABLES
        if (Settings.ShowDerivedTaskValues(TaskIndex)) {
          int varNr = VARS_PER_TASK;
          String taskName = getTaskDeviceName(TaskIndex);
          taskName.toLowerCase();
          String postfix;
          const String search = getDerivedValueSearchAndPostfix(taskName, postfix);

          auto it = customStringVar.begin();
          while (it != customStringVar.end()) {
            if (it->first.startsWith(search) && it->first.endsWith(postfix)) {
              String valueName = it->first.substring(search.length(), it->first.indexOf('-'));
              String uom;
              String vType;
              const String vname2 = getDerivedValueNameUomAndVType(taskName, valueName, uom, vType);
              if (!vname2.isEmpty()) {
                valueName = vname2;
              }
              if (!it->second.isEmpty()) {
                String value(it->second);
                stripEscapeCharacters(value);
                value = parseTemplate(value);
                uint8_t nrDecimals = 255; // FIXME Use the minimal number of decimals needed
                bool hasPresentation;
                const String presentation = formatUserVarForPresentation(&TempEvent, INVALID_TASKVAR_INDEX, hasPresentation, value, DeviceIndex, valueName);

                stream_comma_newline(); // Push out a comma and newline
                handle_json_stream_task_value_data(varNr + 1,
                                                  valueName,
                                                  nrDecimals,
                                                  value,
                                                  presentation,
                                                  uom,
                                                  false); // No comma here
                ++varNr;
              }
            }
            else if (it->first.substring(0, search.length()).compareTo(search) > 0) {
              break;
            }
            ++it;
          }
        }
        #endif // if FEATURE_STRING_VARIABLES
        addHtml(F("],\n"));
      }

#if FEATURE_PLUGIN_STATS && FEATURE_CHART_JS
      if (showPluginStats && Device[DeviceIndex].PluginStats) {
        PluginTaskData_base *taskData = getPluginTaskDataBaseClassOnly(TaskIndex);
        if (taskData != nullptr && taskData->nrSamplesPresent() > 0) {
          addHtml(F("\"PluginStats\":\n"));
          taskData->plot_ChartJS(true);
          stream_comma_newline();
        }
      }
#endif


      if (showSpecificTask) {
        stream_next_json_object_value(F("TTL"), ttl_json * 1000);
        #if FEATURE_TASKVALUE_UNIT_OF_MEASURE
        stream_next_json_object_value(F("ShowUoM"), jsonBool(Settings.ShowUnitOfMeasureOnDevicesPage()));
        #endif // if FEATURE_TASKVALUE_UNIT_OF_MEASURE
      }

      if (showDataAcquisition) {
        addHtml(F("\"DataAcquisition\": [\n"));

        for (controllerIndex_t x = 0; x < CONTROLLER_MAX; x++)
        {
          addHtml('{');
          stream_next_json_object_value(F("Controller"), x + 1);
          stream_next_json_object_value(F("IDX"),        Settings.TaskDeviceID[x][TaskIndex]);
          stream_last_json_object_value(F("Enabled"), jsonBool(Settings.TaskDeviceSendData[x][TaskIndex]));

          if (x < (CONTROLLER_MAX - 1)) {
            stream_comma_newline();
          }
        }
        addHtml(F("],\n"));
      }

      if (showTaskDetails) {
        stream_next_json_object_value(F("TaskInterval"),     taskInterval);
        stream_next_json_object_value(F("Type"),             getPluginNameFromDeviceIndex(DeviceIndex));
        stream_next_json_object_value(F("TaskName"),         getTaskDeviceName(TaskIndex));
        stream_next_json_object_value(F("TaskDeviceNumber"), Settings.getPluginID_for_task(TaskIndex).value);
        for(int i = 0; i < 3; i++) {
          if (Settings.TaskDevicePin[i][TaskIndex] >= 0) {
            stream_next_json_object_value(concat(F("TaskDeviceGPIO"), i + 1) , static_cast<int>(Settings.TaskDevicePin[i][TaskIndex]));
          }
        }

        #if FEATURE_I2CMULTIPLEXER
        uint8_t i2cBus = 0;
        #if FEATURE_I2C_MULTIPLE
        i2cBus = Settings.getI2CInterface(TaskIndex);
        #endif
        if (Device[DeviceIndex].Type == DEVICE_TYPE_I2C && isI2CMultiplexerEnabled(i2cBus)) {
          #if FEATURE_I2C_MULTIPLE
          stream_next_json_object_value(F("I2C_Interface"), static_cast<int>(i2cBus + 1));
          #endif
          int8_t channel = Settings.I2C_Multiplexer_Channel[TaskIndex];
          if (bitRead(Settings.I2C_Flags[TaskIndex], I2C_FLAGS_MUX_MULTICHANNEL)) {
            addHtml(F("\"I2CBus\" : ["));
            uint8_t b = 0;
            for (uint8_t c = 0; c < I2CMultiplexerMaxChannels(i2cBus); ++c) {
              if (bitRead(channel, c)) {
                if (b > 0) { stream_comma_newline(); }
                b++;
                addHtml(F("\"Multiplexer channel "));
                addHtmlInt(c);
                addHtml('"');
              }
            }
            addHtml(F("],\n"));
          } else {
            if (channel == -1){
              stream_next_json_object_value(F("I2Cbus"),       F("Standard I2C bus"));
            } else {
              String i2cChannel = concat(F("Multiplexer channel "), channel);
              stream_next_json_object_value(F("I2Cbus"),       i2cChannel);
            }
          }
        }
        #endif // if FEATURE_I2CMULTIPLEXER
      }
      stream_next_json_object_value(F("TaskEnabled"), 
        // jsonBool(Settings.TaskDeviceEnabled[TaskIndex].enabled));
        jsonBool(Settings.TaskDeviceEnabled[TaskIndex]));

      stream_last_json_object_value(F("TaskNumber"), TaskIndex + 1);

      if (TaskIndex != lastActiveTaskIndex) {
        addHtml(',');
      }
      addHtml('\n');
    }
  }

  if (!showSpecificTask) {
    addHtml(F("],\n"));
    #if FEATURE_TASKVALUE_UNIT_OF_MEASURE
    stream_next_json_object_value(F("ShowUoM"), jsonBool(Settings.ShowUnitOfMeasureOnDevicesPage()));
    #endif // if FEATURE_TASKVALUE_UNIT_OF_MEASURE
    stream_last_json_object_value(F("TTL"), lowest_ttl_json * 1000);
  }

  TXBuffer.endStream();
  STOP_TIMER(HANDLE_SERVING_WEBPAGE_JSON);
}

void handle_json_stream_task_value_data(uint16_t       valueNumber,
                                        const String & valueName,
                                        uint8_t        nrDecimals,
                                        const String & value,
                                        const String & presentation,
                                        const String & uom,
                                        bool           appendComma) {
  addHtml('{');
  stream_next_json_object_value(F("ValueNumber"), valueNumber);
  stream_next_json_object_value(F("Name"),        valueName);
  stream_next_json_object_value(F("NrDecimals"),  nrDecimals);
  #if FEATURE_STRING_VARIABLES
  if (!presentation.isEmpty()) {
    stream_next_json_object_value(F("Presentation"), presentation);
  }
  #endif // if FEATURE_STRING_VARIABLES
  if (!uom.isEmpty()) {
    stream_next_json_object_value(F("UoM"), uom);
  }
  stream_last_json_object_value(F("Value"),       value);

  if (appendComma) {
    stream_comma_newline();
  }
}
// ********************************************************************************
// JSON formatted timing statistics
// ********************************************************************************

#ifdef WEBSERVER_NEW_UI
void handle_timingstats_json() {
  TXBuffer.startJsonStream();
  json_init();
  json_open();
  # if FEATURE_TIMING_STATS
  jsonStatistics(false);
  # endif // if FEATURE_TIMING_STATS
  json_close();
  TXBuffer.endStream();
}

#endif // WEBSERVER_NEW_UI

#ifdef WEBSERVER_NEW_UI

#if FEATURE_ESPEASY_P2P
void handle_nodes_list_json() {
  if (!isLoggedIn()) { return; }
  TXBuffer.startJsonStream();
  json_init();
  json_open(true);

  for (auto it = Nodes.begin(); it != Nodes.end(); ++it)
  {
    if (it->second.ip[0] != 0)
    {
      json_open();
      bool isThisUnit = it->first == Settings.Unit;

      if (isThisUnit) {
        json_number(F("thisunit"), String(1));
      }

      json_number(F("first"), String(it->first));
      json_prop(F("name"), isThisUnit ? Settings.getName() : it->second.getNodeName());

      if (it->second.build) { json_prop(F("build"), formatSystemBuildNr(it->second.build)); }
      json_prop(F("type"), it->second.getNodeTypeDisplayString());
      json_prop(F("ip"),   formatIP(it->second.ip));
      json_number(F("age"), String(it->second.getAge() / 1000)); // time in seconds
      json_close();
    }
  }
  json_close(true);
  TXBuffer.endStream();
}
#endif

void handle_buildinfo() {
  if (!isLoggedIn()) { return; }
  TXBuffer.startJsonStream();
  json_init();
  json_open();
  {
    json_open(true, F("plugins"));

    for (deviceIndex_t x; x <= getDeviceCount(); x++) {
      const pluginID_t pluginID = getPluginID_from_DeviceIndex(x);
      if (validPluginID(pluginID)) {
        json_open();
        json_number(F("id"), String(pluginID));
        json_prop(F("name"), getPluginNameFromDeviceIndex(x));
        json_close();
      }
    }
    json_close(true);
  }
  {
    json_open(true, F("controllers"));

    for (protocolIndex_t x = 0; x < getHighestIncludedCPluginID(); x++) {
      if (getCPluginID_from_ProtocolIndex(x) != INVALID_C_PLUGIN_ID) {
        json_open();
        json_number(F("id"), String(x + 1));
        json_prop(F("name"), getCPluginNameFromProtocolIndex(x));
        json_close();
      }
    }
    json_close(true);
  }
#if FEATURE_NOTIFIER
  {
    json_open(true, F("notifications"));

    for (uint8_t x = 0; x < NPLUGIN_MAX; x++) {
      if (validNPluginID(NPlugin_id[x])) {
        json_open();
        json_number(F("id"), String(x + 1));
        json_prop(F("name"), getNPluginNameFromNotifierIndex(x));
        json_close();
      }
    }
    json_close(true);
  }
#endif
  json_prop(LabelType::BUILD_DESC);
  json_prop(LabelType::GIT_BUILD);
  json_prop(LabelType::SYSTEM_LIBRARIES);
#ifdef ESP32
  json_prop(LabelType::ESP_IDF_SDK_VERSION);
#endif
  json_prop(LabelType::PLUGIN_COUNT);
  json_prop(LabelType::PLUGIN_DESCRIPTION);
  json_close();
  TXBuffer.endStream();
}

#endif // WEBSERVER_NEW_UI


/*********************************************************************************************\
   Streaming versions directly to TXBuffer
\*********************************************************************************************/
void stream_to_json_object_value(const __FlashStringHelper *  object, const String& value) {
  stream_to_json_object_value(String(object), value);
}

void stream_to_json_object_value(const String& object, const String& value) {
  addHtml(wrap_String(object, '"', '"'));
  addHtml(':');
  addHtml(to_json_value(value));
}

void stream_to_json_object_value(const __FlashStringHelper *  object, int value) {
  stream_to_json_object_value(String(object), value);
}

void stream_to_json_object_value(const String& object, int value) {
  addHtml(wrap_String(object, '"', '"'));
  addHtml(':');
  addHtmlInt(value);
}

String jsonBool(bool value) {
  return boolToString(value);
}


// Add JSON formatted data directly to the TXbuffer, including a trailing comma.
void stream_next_json_object_value(const __FlashStringHelper * object, const String& value) {
  stream_to_json_object_value(object, value);
  stream_comma_newline();
}

void stream_next_json_object_value(const __FlashStringHelper * object, String&& value) {
  stream_to_json_object_value(object, value);
  stream_comma_newline();
}

void stream_next_json_object_value(const String& object, const String& value) {
  stream_to_json_object_value(object, value);
  stream_comma_newline();
}

void stream_next_json_object_value(const __FlashStringHelper * object, int value) {
  stream_to_json_object_value(object, value);
  stream_comma_newline();
}

void stream_next_json_object_value(const String& object, int value) {
  stream_to_json_object_value(object, value);
  stream_comma_newline();
}

void stream_newline_close_brace() {
  addHtml('\n', '}');
}


// Add JSON formatted data directly to the TXbuffer, including a closing '}'
void stream_last_json_object_value(const __FlashStringHelper * object, const String& value) {
  stream_to_json_object_value(object, value);
  stream_newline_close_brace();
}

void stream_last_json_object_value(const __FlashStringHelper * object, String&& value) {
  stream_to_json_object_value(object, value);
  stream_newline_close_brace();
}

void stream_last_json_object_value(const String& object, const String& value) {
  stream_to_json_object_value(object, value);
  stream_newline_close_brace();
}

void stream_last_json_object_value(const __FlashStringHelper * object, int value) {
  stream_to_json_object_value(object, value);
  stream_newline_close_brace();
}

void stream_json_object_values(const LabelType::Enum labels[])
{
  size_t i = 0;
  LabelType::Enum cur  = static_cast<const LabelType::Enum>(pgm_read_byte(labels + i));

  while (true) {
    const LabelType::Enum next = static_cast<const LabelType::Enum>(pgm_read_byte(labels + i + 1));
    const bool nextIsLast      = next == LabelType::MAX_LABEL;

    if (nextIsLast) {
      stream_last_json_object_value(cur);
      return;
    } else {
      stream_next_json_object_value(cur);
    }
    ++i;
    cur = next;
  }
}

void stream_next_json_object_value(LabelType::Enum label) {
  stream_next_json_object_value(getLabel(label), getValue(label));
}

void stream_last_json_object_value(LabelType::Enum label) {
  stream_last_json_object_value(getLabel(label), getValue(label));
}