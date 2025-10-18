#include "../Commands/Tasks.h"


#include "../../ESPEasy_common.h"
#include "../../_Plugin_Helper.h"

#include "../Commands/Common.h"

#include "../DataStructs/TimingStats.h"

#include "../ESPEasyCore/Controller.h"
#include "../ESPEasyCore/Serial.h"

#include "../Globals/ESPEasy_Scheduler.h"
#include "../Globals/RulesCalculate.h"
#include "../Globals/RuntimeData.h"
#include "../Globals/Settings.h"

#include "../Helpers/Misc.h"
#include "../Helpers/Numerical.h"
#include "../Helpers/StringConverter.h"
#include "../Helpers/StringParser.h"

// taskIndex = (event->Par1 - 1);   Par1 is here for 1 ... TASKS_MAX
bool validTaskIndexVar(struct EventStruct *event, taskIndex_t& taskIndex)
{
  if (event == nullptr) { return false; }

  if (event->Par1 <= 0) { return false; }
  const taskIndex_t tmp_taskIndex = static_cast<taskIndex_t>(event->Par1 - 1);

  if (!validTaskIndex(tmp_taskIndex)) { return false; }

  taskIndex = tmp_taskIndex;

  return true;
}

// taskIndex = (event->Par1 - 1);   Par1 is here for 1 ... TASKS_MAX
// varNr = event->Par2 - 1;
bool validTaskVars(struct EventStruct *event, taskIndex_t& taskIndex, unsigned int& varNr)
{
  if (event == nullptr) { return false; }

  taskIndex_t tmp_taskIndex;
  if (!validTaskIndexVar(event, tmp_taskIndex)) { return false; }

  varNr = 0;

  if (event->Par2 > 0 && event->Par2 <= VARS_PER_TASK) {
    varNr = event->Par2 - 1;
    taskIndex = tmp_taskIndex;
    return true;
  }

  return false;
}

bool validateAndParseTaskIndexArguments(struct EventStruct * event, const char *Line, taskIndex_t &taskIndex)
{
  if (!validTaskIndexVar(event, taskIndex)) {
    String taskName;
    taskIndex_t tmpTaskIndex = taskIndex;
    if ((event->Par1 <= 0 || event->Par1 >= INVALID_TASK_INDEX) && GetArgv(Line, taskName, 2)) {
      tmpTaskIndex = findTaskIndexByName(taskName, true);
      if (tmpTaskIndex != INVALID_TASK_INDEX) {
        event->Par1 = tmpTaskIndex + 1;
      }
    }
    return validTaskIndexVar(event, taskIndex);
  }
  return true;
}

/**
 * parse TaskName/TaskValue when not numeric for task name and value name and validate values
 */
bool validateAndParseTaskValueArguments(struct EventStruct * event, const char *Line, taskIndex_t &taskIndex, unsigned int &varNr)
{
  if (!validTaskVars(event, taskIndex, varNr) || (event->Par2 <= 0 || event->Par2 >= VARS_PER_TASK))  // Extra check required because of shortcutting in validTaskVars()
  { 
    String taskName;
    taskIndex_t tmpTaskIndex = taskIndex;
    if ((event->Par1 <= 0 || event->Par1 >= INVALID_TASK_INDEX) && GetArgv(Line, taskName, 2)) {
      tmpTaskIndex = findTaskIndexByName(taskName, true);
      if (tmpTaskIndex != INVALID_TASK_INDEX) {
        event->Par1 = tmpTaskIndex + 1;
      }
    }
    String valueName;
    if ((event->Par2 <= 0 || event->Par2 >= VARS_PER_TASK) && event->Par1 - 1 != INVALID_TASK_INDEX && GetArgv(Line, valueName, 3))
    {
      uint8_t tmpVarNr = findDeviceValueIndexByName(valueName, event->Par1 - 1);
      if (tmpVarNr != VARS_PER_TASK) {
        event->Par2 = tmpVarNr + 1;
      }
    }
    if (!validTaskVars(event, taskIndex, varNr)) return false; 
  }

  return true;
}

const __FlashStringHelper * taskValueSet(struct EventStruct *event, const char *Line, taskIndex_t& taskIndex, bool& success)
{
  String TmpStr1;
  unsigned int varNr;

  if (!validateAndParseTaskValueArguments(event, Line, taskIndex, varNr)) {
    success = false;
    return F("INVALID_PARAMETERS");
  }
  if (!Settings.AllowTaskValueSetAllPlugins() && 
      getPluginID_from_TaskIndex(taskIndex).value != 33) { // PluginID 33 = Dummy Device
    success = false;
    return F("NOT_A_DUMMY_TASK");
  }
  if (!Settings.TaskDeviceEnabled[taskIndex]) {
    success = false;
    return F("TASK_NOT_ENABLED");
  }
  const uint8_t valueCount = getValueCountForTask(taskIndex);
  if (valueCount <= varNr) {
    success = false;
    return F("INVALID_VAR_INDEX");
  }

  EventStruct tmpEvent(taskIndex);
  if (GetArgv(Line, TmpStr1, 4)) {
    const Sensor_VType sensorType = tmpEvent.getSensorType();

    // FIXME TD-er: Must check if the value has to be computed and not convert to double when sensor type is 64 bit int.

    // Perform calculation with float result.
    ESPEASY_RULES_FLOAT_TYPE result{};

    if (isError(Calculate(TmpStr1, result))) {
      success = false;
      return F("CALCULATION_ERROR");
    }
    #ifndef BUILD_NO_DEBUG
    addLog(LOG_LEVEL_INFO, strformat(
      F("taskValueSet: %s  taskindex: %d varNr: %d result: %f type: %d"),
      Line,
      taskIndex,
      varNr, result, sensorType));
    #endif

    UserVar.set(taskIndex, varNr, result, sensorType);
  } else  {
    // TODO: Get Task description and var name
    serialPrintln(formatUserVarNoCheck(&tmpEvent, varNr));
  }
  success = true;
  return return_command_success_flashstr();
}

const __FlashStringHelper * Command_Task_Clear(struct EventStruct *event, const char *Line)
{
  taskIndex_t  taskIndex;

  if (!validateAndParseTaskIndexArguments(event, Line, taskIndex)) {
    return F("INVALID_PARAMETERS"); 
  }

  taskClear(taskIndex, true);
  return return_command_success_flashstr();
}

const __FlashStringHelper * Command_Task_ClearAll(struct EventStruct *event, const char *Line)
{
  for (taskIndex_t t = 0; t < TASKS_MAX; t++) {
    taskClear(t, false);
  }
  return return_command_success_flashstr();
}

const __FlashStringHelper * Command_Task_EnableDisable(struct EventStruct *event, bool enable, const char *Line)
{
  taskIndex_t  taskIndex;
  if (validateAndParseTaskIndexArguments(event, Line, taskIndex)) {
    // This is a command so no guarantee the taskIndex is correct in the event
    event->setTaskIndex(taskIndex);

    #if FEATURE_PLUGIN_PRIORITY
    if (Settings.isPriorityTask(event->TaskIndex)) {
      return return_command_failed_flashstr();
    }
    #endif // if FEATURE_PLUGIN_PRIORITY
    return return_command_boolean_result_flashstr(setTaskEnableStatus(event, enable));
  }
  return F("INVALID_PARAMETERS");
}

#if FEATURE_PLUGIN_PRIORITY
const __FlashStringHelper * Command_PriorityTask_DisableTask(struct EventStruct *event, const char *Line)
{
  taskIndex_t  taskIndex;

  if (validateAndParseTaskIndexArguments(event, Line, taskIndex)) {
    // This is a command so no guarantee the taskIndex is correct in the event
    event->setTaskIndex(taskIndex);

    if (Settings.isPowerManagerTask(event->TaskIndex))
    {
      Settings.setPowerManagerTask(event->TaskIndex, false);
      return return_command_success_flashstr();
    }
    // Handle other Priotiry task options
    Settings.setTaskEnableReadonly(event->TaskIndex, false);
    return return_command_failed_flashstr();
  }
  return F("INVALID_PARAMETERS");
}
#endif // if FEATURE_PLUGIN_PRIORITY

const __FlashStringHelper * Command_Task_Disable(struct EventStruct *event, const char *Line)
{
  return Command_Task_EnableDisable(event, false, Line);
}

const __FlashStringHelper * Command_Task_Enable(struct EventStruct *event, const char *Line)
{
  return Command_Task_EnableDisable(event, true, Line);
}

#if FEATURE_PLUGIN_PRIORITY
const __FlashStringHelper * Command_PriorityTask_Disable(struct EventStruct *event, const char *Line)
{
  return Command_PriorityTask_DisableTask(event, Line);
}
#endif

const __FlashStringHelper * Command_Task_ValueSet(struct EventStruct *event, const char *Line)
{
  taskIndex_t taskIndex;
  bool success;
  return taskValueSet(event, Line, taskIndex, success);
}

#if FEATURE_STRING_VARIABLES
const __FlashStringHelper * Command_Task_ValueSetDerived(struct EventStruct *event, const char *Line)
{
  return taskValueSetString(event, Line, F(TASK_VALUE_DERIVED_PREFIX_TEMPLATE), F(TASK_VALUE_UOM_PREFIX_TEMPLATE), F(TASK_VALUE_VTYPE_PREFIX_TEMPLATE));
}

const __FlashStringHelper * Command_Task_ValueSetPresentation(struct EventStruct *event, const char *Line)
{
  return taskValueSetString(event, Line, F(TASK_VALUE_PRESENTATION_PREFIX_TEMPLATE));
}

const __FlashStringHelper * taskValueSetString(struct EventStruct *event, const char *Line, const __FlashStringHelper * storageTemplate, const __FlashStringHelper * uomTemplate, const __FlashStringHelper * vTypeTemplate)
{
  String taskName;
  taskIndex_t tmpTaskIndex = INVALID_TASK_INDEX;
  if ((event->Par1 <= 0 || event->Par1 >= INVALID_TASK_INDEX) && GetArgv(Line, taskName, 2)) {
    tmpTaskIndex = findTaskIndexByName(taskName, true);
    if (tmpTaskIndex != INVALID_TASK_INDEX) {
      event->Par1 = tmpTaskIndex + 1;
    }
  }
  String valueName;
  const bool hasValueName = GetArgv(Line, valueName, 3);
  if ((event->Par2 <= 0 || event->Par2 >= VARS_PER_TASK) && event->Par1 - 1 != INVALID_TASK_INDEX && hasValueName)
  {
    uint8_t tmpVarNr = findDeviceValueIndexByName(valueName, event->Par1 - 1);
    if (tmpVarNr != VARS_PER_TASK) {
      event->Par2 = tmpVarNr + 1;
    }
  }

  if (event->Par1 > 0 && validTaskIndex(event->Par1 - 1)) {
    taskName = getTaskDeviceName(event->Par1 - 1); // Taskname must be valid
  }

  if (!hasValueName || (event->Par1 > 0 && validTaskIndex(event->Par1 - 1) && event->Par2 > 0 && validTaskVarIndex(event->Par2 - 1))) {
    valueName = getTaskValueName(static_cast<taskIndex_t>(event->Par1 - 1), event->Par2 - 1); // Convert numeric var index into name
  }
  // addLog(LOG_LEVEL_INFO, strformat(F("TaskValueSetStorage: task: %s (%d), value: %s (%d), Line: %s"), taskName.c_str(), event->Par1, valueName.c_str(), event->Par2, Line)); // FIXME

  String argument;
  if (!taskName.isEmpty() && !valueName.isEmpty() && GetArgv(Line, argument, 4)) {
    taskName.toLowerCase();
    String orgValueName(valueName);
    valueName.toLowerCase();
    String key = strformat(storageTemplate, taskName.c_str(), valueName.c_str());
    const String key2 = strformat(F(TASK_VALUE_NAME_PREFIX_TEMPLATE), taskName.c_str(), valueName.c_str());
    setCustomStringVar(key, argument);
    if (getCustomStringVar(key2).isEmpty() || argument.isEmpty()) {
      setCustomStringVar(key2, argument.isEmpty() ? EMPTY_STRING : orgValueName);
    }
    if (uomTemplate != nullptr) { // We have a Unit of Measure template
      if (GetArgv(Line, argument, 5)) { // check for extra argument holding UoM
        key = strformat(uomTemplate, taskName.c_str(), valueName.c_str());
        if (!argument.isEmpty()) {
          #if FEATURE_TASKVALUE_UNIT_OF_MEASURE

          uint8_t i = 1; // Index 0 is empty/None
          String uom = toUnitOfMeasureName(i);
          while (i < 255 && !uom.isEmpty()) {
            if (argument.equalsIgnoreCase(uom)) {
              setCustomStringVar(key, uom);
              argument.clear();
              break;
            }
            ++i;
            uom = toUnitOfMeasureName(i);
          }
          if (!argument.isEmpty()) 
          #endif // if FEATURE_TASKVALUE_UNIT_OF_MEASURE
          {
            setCustomStringVar(key, argument);
          }
        } else {
          clearCustomStringVar(key);
        }
      }
    }
    if (vTypeTemplate != nullptr) { // We have a ValueType template
      if (GetArgv(Line, argument, 6)) { // check for extra argument holding VType
        key = strformat(vTypeTemplate, taskName.c_str(), valueName.c_str());
        if (!argument.isEmpty()) {
          constexpr uint8_t maxVType = static_cast<uint8_t>(Sensor_VType::SENSOR_TYPE_NOT_SET);

          for (uint8_t i = 0; i < maxVType; ++i) {
            const Sensor_VType vt = static_cast<Sensor_VType>(i);

            if ((getValueCountFromSensorType(vt, false) == 1) &&
                argument.equalsIgnoreCase(getSensorTypeLabel(vt))) {
              setCustomStringVar(key, getSensorTypeLabel(vt)); // Set 'official' VType label
              break;
            }
          }
        } else {
          clearCustomStringVar(key);
        }
      }
    }
    // addLog(LOG_LEVEL_INFO, strformat(F("TaskValueSetStorage: key: %s, argument: %s"), key.c_str(), argument.c_str())); // FIXME
    return return_command_success_flashstr();
  }
  return return_command_failed_flashstr(); // taskValueSet(event, Line, taskIndex, success);
}
#endif

const __FlashStringHelper * Command_Task_ValueToggle(struct EventStruct *event, const char *Line)
{
  taskIndex_t  taskIndex;
  unsigned int varNr;

  if (!validateAndParseTaskValueArguments(event, Line, taskIndex, varNr)) {
    return F("INVALID_PARAMETERS");
  }
  if (!Settings.TaskDeviceEnabled[taskIndex]) {
    return F("TASK_NOT_ENABLED");
  }

  EventStruct tmpEvent(taskIndex);
  const Sensor_VType sensorType = tmpEvent.getSensorType();
  const int    result       = lround(UserVar.getAsDouble(taskIndex, varNr, sensorType));
  if ((result == 0) || (result == 1)) {
    UserVar.set(taskIndex, varNr, (result == 0) ? 1.0 : 0.0, sensorType);
  }
  return return_command_success_flashstr();
}

const __FlashStringHelper * Command_Task_ValueSetAndRun(struct EventStruct *event, const char *Line)
{
  taskIndex_t taskIndex;
  bool success;
  const __FlashStringHelper * returnvalue = taskValueSet(event, Line, taskIndex, success);
  if (success)
  {
    struct EventStruct TempEvent(taskIndex);
    TempEvent.Source = event->Source;
    SensorSendTask(&TempEvent);

    return return_command_success_flashstr();
  }
  return returnvalue;
}

const __FlashStringHelper * Command_ScheduleTask_Run(struct EventStruct *event, const char* Line)
{
  taskIndex_t  taskIndex;

  if (!validateAndParseTaskIndexArguments(event, Line, taskIndex) || event->Par2 < 0) {
    return F("INVALID_PARAMETERS");
  }
  if (!Settings.TaskDeviceEnabled[taskIndex]) {
    return F("TASK_NOT_ENABLED");
  }

  uint32_t msecFromNow = 0;
  String par3;
  if (GetArgv(Line, par3, 3)) {
    if (validUIntFromString(par3, msecFromNow)) {
      Scheduler.schedule_task_device_timer(taskIndex, millis() + msecFromNow);
      return return_command_success_flashstr();
    }
  }
  return F("INVALID_PARAMETERS");  
}

const __FlashStringHelper * Command_Task_Run(struct EventStruct *event, const char *Line)
{
  taskIndex_t  taskIndex;

  if (!validateAndParseTaskIndexArguments(event, Line, taskIndex) || event->Par2 < 0) {
    return F("INVALID_PARAMETERS");
  }
  if (!Settings.TaskDeviceEnabled[taskIndex]) {
    return F("TASK_NOT_ENABLED");
  }
  uint32_t unixTime = 0;
  String par3;
  if (GetArgv(Line, par3, 3)) {
    validUIntFromString(par3, unixTime);
  }

  struct EventStruct TempEvent(taskIndex);
  TempEvent.Source = event->Source;
  SensorSendTask(&TempEvent, unixTime);

  return return_command_success_flashstr();
}

const __FlashStringHelper * Command_Task_RemoteConfig(struct EventStruct *event, const char *Line)
{
  struct EventStruct TempEvent(event->TaskIndex);
  String request = Line;

  // FIXME TD-er: Should we call ExecuteCommand here? The command is not parsed like any other call.
  remoteConfig(&TempEvent, request);
  return return_command_success_flashstr();
}
