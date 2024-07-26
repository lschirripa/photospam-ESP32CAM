/*
* Capture ESP32 Cam JPEG images into a AVI file and store on SD
* AVI files stored on the SD card can also be selected and streamed to a browser as MJPEG.
*
* s60sc 2020 - 2024
*/

#include "appGlobals.h"
const char* serverIP = "192.168.1.51"; // Replace with your FastAPI server IP address
const int serverPort = 8000; // Replace with your FastAPI server port

const char* serverName = "http://192.168.0.100:8000/upload";

// Timer variables
unsigned long lastTime = 0;
unsigned long timerDelay = 1000;

void setup() {
  Serial.begin(115200); // Initialize Serial communication
  logSetup();

  // prep SD card storage
  if (startStorage()) {
    // Load saved user configuration
    if (loadConfig()) {
      // initialise camera
      if (psramFound()) {
        LOG_INF("PSRAM size: %s", fmtSize(ESP.getPsramSize()));
        if (ESP.getPsramSize() > 3 * ONEMEG) prepCam();
        else snprintf(startupFailure, SF_LEN, STARTUP_FAIL "Insufficient PSRAM for app");
      }
      else snprintf(startupFailure, SF_LEN, STARTUP_FAIL "Need PSRAM to be enabled");
    }
  }
  
#ifdef DEV_ONLY
  devSetup();
#endif

  // connect wifi or start config AP if router details not available
  startWifi();

  startWebServer();
  if (strlen(startupFailure)) LOG_WRN("%s", startupFailure);
  else {
    // start rest of services
    startSustainTasks(); 
#if INCLUDE_SMTP
    prepSMTP(); 
#endif
#if INCLUDE_FTP_HFS
    prepUpload();
#endif
    prepPeripherals();
#if INCLUDE_AUDIO
    prepAudio(); 
#endif
#if INCLUDE_TELEM
    prepTelemetry();
#endif
#if INCLUDE_TGRAM
    prepTelegram();
#endif
    prepRecording(); 
    checkMemory();
  } 
}

void loop() {
  if (Serial.available() > 0) {
    String command = Serial.readStringUntil('\n');
    command.trim();

    if (command == "ping") {
      if (pingServer()) {
        Serial.println("Ping successful!");
      } else {
        Serial.println("Ping failed.");
      }
    }
  }
    // Capture a picture
    camera_fb_t * fb = esp_camera_fb_get();
    if (!fb) {
        Serial.println("Camera capture failed");
        return;
    }

    // Prepare HTTP POST request
    if (WiFi.status() == WL_CONNECTED) {
        HTTPClient http;
        http.begin(serverName);
        http.addHeader("Content-Type", "image/jpeg");

        Serial.println("Sending POST request...");
        int httpResponseCode = http.POST(fb->buf, fb->len);

        if (httpResponseCode > 0) {
            String response = http.getString();
            Serial.println("HTTP Response code: " + String(httpResponseCode));
            Serial.println("Response: " + response);
        } else {
            Serial.println("Error on sending POST: " + String(httpResponseCode));
        }
        http.end();
    } else {
        Serial.println("WiFi Disconnected");
    }

    // Return the frame buffer to the driver for reuse
    esp_camera_fb_return(fb);

    // Wait for 2 second
    delay(2000);



}

bool pingServer() {
  WiFiClient client;
  if (!client.connect(serverIP, serverPort)) {
    return false; // Connection failed
  }

  client.stop();
  return true; // Connection successful
}
