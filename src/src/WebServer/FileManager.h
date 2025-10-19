#ifndef WEBSERVER_FILEMANAGER_H
#define WEBSERVER_FILEMANAGER_H

#include "../../ESPEasy_common.h"

#ifdef WEBSERVER_SETUP

// File Manager functions
void handle_file_manager();
void handle_file_edit();
void handle_file_delete();
void handle_file_create();
void handle_file_copy();
void handle_file_upload();
void handle_file_upload_post();
void handle_file_download();
void handleFileUploadPRN();

// Helper functions
String getFileExtension(const String& filename);
bool isPrnFile(const String& filename);
String formatFileSize(size_t bytes);
String getFileIcon(const String& filename);

#endif // WEBSERVER_SETUP

#endif // WEBSERVER_FILEMANAGER_H