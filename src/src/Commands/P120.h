#ifndef COMMANDS_P120_H
#define COMMANDS_P120_H

#include "../../ESPEasy_common.h"

#ifdef USES_P120

String do_command_fyzart(struct EventStruct *event, const char* Line);
String do_command_fyztop(struct EventStruct *event, const char* Line);  
String do_command_fyzkop(struct EventStruct *event, const char* Line);
String do_command_fyzuruntek(struct EventStruct *event, const char* Line);
String do_command_fyzurunart(struct EventStruct *event, const char* Line);

#endif // USES_P120

#endif // COMMANDS_P120_H