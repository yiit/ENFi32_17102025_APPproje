#include "FileManager.h"

#ifdef WEBSERVER_SETUP

#include "../WebServer/ESPEasy_WebServer.h"
#include "../WebServer/HTML_wrappers.h"
#include "../WebServer/Markup.h"
#include "../WebServer/Markup_Buttons.h"
#include "../WebServer/Markup_Forms.h"

#include "../Globals/ESPEasy_Scheduler.h"
#include "../Helpers/ESPEasy_Storage.h"
#include "../Helpers/StringConverter.h"

// File system includes
#include <FS.h>
#include <LittleFS.h>

void handle_file_manager() {
  if (!isLoggedIn()) { return; }
  
  navMenuIndex = MENU_INDEX_FILES;
  TXBuffer.startStream();
  sendHeadandTail_stdtemplate(_HEAD);

  // CSS for file manager
  addHtml(F("<style>"));
  addHtml(F(".file-manager { margin: 20px 0; }"));
  addHtml(F(".file-list { border: 1px solid #ddd; border-radius: 8px; overflow: hidden; }"));
  addHtml(F(".file-item { display: flex; align-items: center; padding: 12px; border-bottom: 1px solid #eee; transition: background-color 0.2s; }"));
  addHtml(F(".file-item:hover { background-color: #f5f5f5; }"));
  addHtml(F(".file-item:last-child { border-bottom: none; }"));
  addHtml(F(".file-icon { margin-right: 12px; font-size: 18px; width: 24px; text-align: center; }"));
  addHtml(F(".file-info { flex: 1; }"));
  addHtml(F(".file-name { font-weight: bold; margin-bottom: 2px; }"));
  addHtml(F(".file-details { color: #666; font-size: 12px; }"));
  addHtml(F(".file-actions { margin-left: 12px; }"));
  addHtml(F(".btn-small { padding: 4px 8px; font-size: 12px; margin-left: 4px; }"));
  addHtml(F(".btn-edit { background-color: #007bff; color: white; }"));
  addHtml(F(".btn-delete { background-color: #dc3545; color: white; }"));
  addHtml(F(".create-section { margin: 20px 0; padding: 15px; border: 1px solid #ddd; border-radius: 8px; background-color: #f9f9f9; }"));
  addHtml(F("</style>"));

  addHtml(F("<div class='file-manager'>"));
  addHtml(F("<h2>📁 Dosya Sistem Yöneticisi</h2>"));
  addHtml(F("<p>Yazıcı işlemleri için .prn dosyalarınızı yönetin</p>"));

  // Create new file section
  addHtml(F("<div class='create-section'>"));
  addHtml(F("<h3>� Yeni PRN Dosyası Oluştur</h3>"));
  addHtml(F("<form method='post' action='/file_create'>"));
  addHtml(F("<table>"));
  addHtml(F("<tr><td>Dosya Adı:</td><td>"));
  addHtml(F("<input type='text' name='filename' placeholder='ornek.prn' style='width:200px;'> "));
  addHtml(F("<input type='submit' value='➕ Oluştur' class='button'>"));
  addHtml(F("</td></tr>"));
  addHtml(F("</table>"));
  addHtml(F("</form>"));
  addHtml(F("</div>"));

  // Upload file section
  addHtml(F("<div class='create-section'>"));
  addHtml(F("<h3>📤 PRN Dosyası Yükle</h3>"));
  addHtml(F("<table>"));
  addHtml(F("<tr><td>Dosya Seç:</td><td>"));
  addHtml(F("<a href='/file_upload' class='button' style='background-color: #007bff;'>📤 Dosya Yükle</a>"));
  addHtml(F("</td></tr>"));
  addHtml(F("</table>"));
  addHtml(F("</div>"));

  // File list
  addHtml(F("<div class='file-list'>"));
  
  // Scan directory for .prn files
  File root = LittleFS.open("/");
  if (root) {
    File file = root.openNextFile();
    bool hasFiles = false;
    
    while (file) {
      String filename = file.name();
      if (isPrnFile(filename)) {
        hasFiles = true;
        
        addHtml(F("<div class='file-item'>"));
        
        // File icon
        addHtml(F("<div class='file-icon'>"));
        addHtml(getFileIcon(filename));
        addHtml(F("</div>"));
        
        // File info
        addHtml(F("<div class='file-info'>"));
        addHtml(F("<div class='file-name'>"));
        addHtml(filename);
        addHtml(F("</div>"));
        addHtml(F("<div class='file-details'>"));
        addHtml(F("Boyut: "));
        addHtml(formatFileSize(file.size()));
        addHtml(F("</div>"));
        addHtml(F("</div>"));
        
        // File actions
        addHtml(F("<div class='file-actions'>"));
        addHtml(F("<a href='/file_edit?file="));
        addHtml(filename);
        addHtml(F("' class='btn-small btn-edit'>✏️ Düzenle</a>"));
        addHtml(F("<a href='/file_download?file="));
        addHtml(filename);
        addHtml(F("' class='btn-small' style='background-color: #28a745; color: white;'>💾 İndir</a>"));
        addHtml(F("<a href='/file_copy?file="));
        addHtml(filename);
        addHtml(F("' class='btn-small' style='background-color: #17a2b8; color: white;'>📋 Kopyala</a>"));
        addHtml(F("<a href='/file_delete?file="));
        addHtml(filename);
        addHtml(F("' class='btn-small btn-delete' onclick='return confirm(\""));
        addHtml(filename);
        addHtml(F(" dosyasını silmek istediğinizden emin misiniz?\")'>🗑️ Sil</a>"));
        addHtml(F("</div>"));
        
        addHtml(F("</div>"));
      }
      file.close();
      file = root.openNextFile();
    }
    root.close();
    
    if (!hasFiles) {
      addHtml(F("<div class='file-item'>"));
      addHtml(F("<div class='file-info'>"));
      addHtml(F("<div class='file-name'>❌ Hiç .prn dosyası bulunamadı</div>"));
      addHtml(F("<div class='file-details'>Başlamak için yeni bir dosya oluşturun</div>"));
      addHtml(F("</div>"));
      addHtml(F("</div>"));
    }
  } else {
    addHtml(F("<div class='file-item'>"));
    addHtml(F("<div class='file-info'>"));
    addHtml(F("<div class='file-name'>⚠️ Dosya sistemine erişim hatası</div>"));
    addHtml(F("</div>"));
    addHtml(F("</div>"));
  }
  
  addHtml(F("</div>")); // file-list
  addHtml(F("</div>")); // file-manager

  sendHeadandTail_stdtemplate(_TAIL);
  TXBuffer.endStream();
}

void handle_file_edit() {
  if (!isLoggedIn()) { return; }
  
  navMenuIndex = MENU_INDEX_FILES;
  String filename = webArg(F("file"));
  if (filename.isEmpty() || !isPrnFile(filename)) {
    handle_file_manager();
    return;
  }
  
  String error;
  String fileContent = "";
  
  // Save file if POST request
  if (web_server.method() == HTTP_POST) {
    fileContent = webArg(F("content"));
    File file = LittleFS.open("/" + filename, "w");
    if (file) {
      file.print(fileContent);
      file.close();
      error = F("✅ Dosya başarıyla kaydedildi!");
    } else {
      error = F("❌ Dosya kaydedilirken hata oluştu!");
    }
  } else {
    // Load file content
    File file = LittleFS.open("/" + filename, "r");
    if (file) {
      fileContent = file.readString();
      file.close();
    } else {
      error = F("❌ Dosya okunurken hata oluştu!");
    }
  }
  
  TXBuffer.startStream();
  sendHeadandTail_stdtemplate(_HEAD);
  
  // CSS for editor
  addHtml(F("<style>"));
  addHtml(F(".editor-container { margin: 20px 0; }"));
  addHtml(F(".editor-header { display: flex; justify-content: space-between; align-items: center; margin-bottom: 15px; }"));
  addHtml(F(".editor-textarea { width: 100%; height: 400px; font-family: 'Courier New', monospace; font-size: 14px; padding: 10px; border: 1px solid #ddd; border-radius: 4px; }"));
  addHtml(F(".editor-actions { margin-top: 15px; }"));
  addHtml(F(".status-message { padding: 10px; border-radius: 4px; margin-bottom: 15px; }"));
  addHtml(F(".status-success { background-color: #d4edda; color: #155724; border: 1px solid #c3e6cb; }"));
  addHtml(F(".status-error { background-color: #f8d7da; color: #721c24; border: 1px solid #f5c6cb; }"));
  addHtml(F("</style>"));

  addHtml(F("<div class='editor-container'>"));
  
  // Header
  addHtml(F("<div class='editor-header'>"));
  addHtml(F("<h2>✏️ Dosya Düzenle: "));
  addHtml(filename);
  addHtml(F("</h2>"));
  addHtml(F("<a href='/file_manager' class='button'>⬅️ Dosya Yöneticisine Dön</a>"));
  addHtml(F("</div>"));
  
  // Status message
  if (!error.isEmpty()) {
    addHtml(F("<div class='status-message "));
    if (error.startsWith("✅")) {
      addHtml(F("status-success"));
    } else {
      addHtml(F("status-error"));
    }
    addHtml(F("'>"));
    addHtml(error);
    addHtml(F("</div>"));
  }
  
  // Editor form
  addHtml(F("<form method='post'>"));
  addHtml(F("<textarea name='content' class='editor-textarea' placeholder='Yazıcı komutlarınızı buraya girin...'>"));
  addHtml(fileContent);
  addHtml(F("</textarea>"));
  
  addHtml(F("<div class='editor-actions'>"));
  addHtml(F("<input type='submit' value='💾 Dosyayı Kaydet' class='button' style='background-color: #28a745;'>"));
  addHtml(F("<span style='margin-left: 15px; color: #666;'>Kaydetmek için Ctrl+S</span>"));
  addHtml(F("</div>"));
  addHtml(F("</form>"));
  
  addHtml(F("</div>"));

  // Add keyboard shortcut for save
  html_add_script(F("document.addEventListener('keydown', function(e) { if (e.ctrlKey && e.key === 's') { e.preventDefault(); document.querySelector('form').submit(); } });"), false);

  sendHeadandTail_stdtemplate(_TAIL);
  TXBuffer.endStream();
}

void handle_file_create() {
  if (!isLoggedIn()) { return; }
  
  String filename = webArg(F("filename"));
  
  if (filename.isEmpty()) {
    handle_file_manager();
    return;
  }
  
  // Ensure .prn extension
  if (!filename.endsWith(F(".prn"))) {
    filename += F(".prn");
  }
  
  // Create empty file
  File file = LittleFS.open("/" + filename, "w");
  if (file) {
    file.print(F("# Yeni PRN dosyası\n# Yazıcı komutlarınızı buraya ekleyin\n"));
    file.close();
    
    // Redirect to edit
    String redirectUrl = F("/file_edit?file=");
    redirectUrl += filename;
    web_server.sendHeader(F("Location"), redirectUrl);
    web_server.send(302, F("text/plain"), F(""));
  } else {
    handle_file_manager();
  }
}

void handle_file_copy() {
  if (!isLoggedIn()) { return; }
  
  String filename = webArg(F("file"));
  
  if (!filename.isEmpty() && isPrnFile(filename)) {
    // Create copy name
    String baseName = filename;
    baseName.replace(F(".prn"), F(""));
    String copyName = baseName + F("_kopya.prn");
    
    // Check if copy already exists, add number if needed
    int copyNumber = 1;
    while (LittleFS.exists("/" + copyName)) {
      copyName = baseName + F("_kopya") + String(copyNumber) + F(".prn");
      copyNumber++;
    }
    
    // Read source file
    File sourceFile = LittleFS.open("/" + filename, "r");
    if (sourceFile) {
      String content = sourceFile.readString();
      sourceFile.close();
      
      // Create copy file
      File copyFile = LittleFS.open("/" + copyName, "w");
      if (copyFile) {
        copyFile.print(content);
        copyFile.close();
      }
    }
  }
  
  // Redirect back to file manager
  web_server.sendHeader(F("Location"), F("/file_manager"));
  web_server.send(302, F("text/plain"), F(""));
}

void handle_file_delete() {
  if (!isLoggedIn()) { return; }
  
  String filename = webArg(F("file"));
  
  if (!filename.isEmpty() && isPrnFile(filename)) {
    LittleFS.remove("/" + filename);
  }
  
  // Redirect back to file manager
  web_server.sendHeader(F("Location"), F("/file_manager"));
  web_server.send(302, F("text/plain"), F(""));
}

void handle_file_upload() {
  if (!isLoggedIn()) { return; }
  
  navMenuIndex = MENU_INDEX_FILES;
  TXBuffer.startStream();
  sendHeadandTail_stdtemplate(_HEAD);

  addHtml(F("<div style='margin: 20px 0;'>"));
  addHtml(F("<h2>📤 PRN Dosyası Yükle</h2>"));
  addHtml(F("<p>Bilgisayarınızdan .prn dosyası yükleyin</p>"));
  
  addHtml(F("<form method='post' enctype='multipart/form-data'>"));
  addHtml(F("<table>"));
  addHtml(F("<tr><td>Dosya Seç:</td><td>"));
  addHtml(F("<input type='file' name='datafile' accept='.prn' required>"));
  addHtml(F("</td></tr>"));
  addHtml(F("<tr><td></td><td>"));
  addHtml(F("<input type='submit' value='📤 Yükle' class='button' style='background-color: #007bff;'>"));
  addHtml(F(" <a href='/file_manager' class='button'>❌ İptal</a>"));
  addHtml(F("</td></tr>"));
  addHtml(F("</table>"));
  addHtml(F("</form>"));
  addHtml(F("</div>"));

  sendHeadandTail_stdtemplate(_TAIL);
  TXBuffer.endStream();
}

void handle_file_upload_post() {
  if (!isLoggedIn()) { return; }
  
  // This function is called when file upload completes
  // The actual file handling is done by handleFileUpload() callback
  
  // Redirect back to file manager
  web_server.sendHeader(F("Location"), F("/file_manager"));
  web_server.send(302, F("text/plain"), F(""));
}

void handle_file_download() {
  if (!isLoggedIn()) { return; }
  
  String filename = webArg(F("file"));
  
  if (!filename.isEmpty() && isPrnFile(filename)) {
    File file = LittleFS.open("/" + filename, "r");
    if (file) {
      web_server.sendHeader(F("Content-Type"), F("application/octet-stream"));
      String dispositionValue = String(F("attachment; filename=\"")) + filename + String(F("\""));
      web_server.sendHeader(F("Content-Disposition"), dispositionValue);
      web_server.streamFile(file, F("application/octet-stream"));
      file.close();
      return;
    }
  }
  
  // If file not found, redirect to file manager
  web_server.sendHeader(F("Location"), F("/file_manager"));
  web_server.send(302, F("text/plain"), F(""));
}

// File upload handler for PRN files
void handleFileUploadPRN() {
  HTTPUpload& upload = web_server.upload();
  static File uploadFile;
  
  switch (upload.status) {
    case UPLOAD_FILE_START:
      {
        String filename = upload.filename;
        if (filename.isEmpty()) {
          return;
        }
        
        // Ensure .prn extension
        if (!isPrnFile(filename)) {
          if (!filename.endsWith(F(".prn"))) {
            filename += F(".prn");
          }
        }
        
        // Open file for writing
        uploadFile = LittleFS.open("/" + filename, "w");
        break;
      }
      
    case UPLOAD_FILE_WRITE:
      if (uploadFile) {
        uploadFile.write(upload.buf, upload.currentSize);
      }
      break;
      
    case UPLOAD_FILE_END:
      if (uploadFile) {
        uploadFile.close();
      }
      break;
      
    case UPLOAD_FILE_ABORTED:
      if (uploadFile) {
        uploadFile.close();
      }
      break;
  }
}

// Helper functions
String getFileExtension(const String& filename) {
  int dotIndex = filename.lastIndexOf('.');
  if (dotIndex != -1 && dotIndex < filename.length() - 1) {
    return filename.substring(dotIndex + 1);
  }
  return "";
}

bool isPrnFile(const String& filename) {
  String ext = getFileExtension(filename);
  ext.toLowerCase();
  return ext == "prn";
}

String formatFileSize(size_t bytes) {
  if (bytes < 1024) {
    return String(bytes) + F(" B");
  } else if (bytes < 1024 * 1024) {
    return String(bytes / 1024.0, 1) + F(" KB");
  } else {
    return String(bytes / (1024.0 * 1024.0), 1) + F(" MB");
  }
}

String getFileIcon(const String& filename) {
  if (isPrnFile(filename)) {
    return F("🖨️");
  }
  return F("📄");
}

#endif // WEBSERVER_SETUP