#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include "esp_camera.h"
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>

// Kredensial WiFi
const char* ssid = "Ramadtya";
const char* password = "11111111";

// Kredensial Bot Telegram
const char* BOTtoken = "7407196224:AAHXwX6HdSsZbRfBFguQtLLaiccGcocwF-Y";
const char* CHAT_ID = "1271884899";

// URL Google Apps Script (URL aplikasi web yang sudah dideploy)
const char* googleAppsScriptURL = "https://script.google.com/macros/s/AKfycbyC1kux1UYcf_Ybi5CBwuWaU5UuoQ1vtA3CT1FfZhc_JLKI09jFFjfzUdsesiHVsTgF/exec";

// Definisi Pin
const int pirPin = 2;
const int buzzerPin = 13;
const int FLASH_LED_PIN = 4;

// Variabel Global
bool flashState = LOW;
bool sendPhoto = false;
bool buzzerEnabled = true;
bool saveToGDrive = true; // Variabel baru untuk mengontrol penyimpanan di Google Drive

// Konfigurasi Bot Telegram
WiFiClientSecure clientTCP;
UniversalTelegramBot bot(BOTtoken, clientTCP);

// Pengaturan Waktu Bot
const int botRequestDelay = 1000;
unsigned long lastTimeBotRan = 0;

// Konfigurasi Pin Kamera
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

void configInitCamera() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.grab_mode = CAMERA_GRAB_LATEST;

  if (psramFound()) {
    config.frame_size = FRAMESIZE_UXGA;
    config.jpeg_quality = 10;
    config.fb_count = 1;
  } else {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }
  
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Inisialisasi kamera gagal dengan error 0x%x", err);
    delay(1000);
    ESP.restart();
  }
}

// Fungsi baru untuk mengirim foto ke Google Drive
void sendPhotoToGoogleDrive(uint8_t* fbBuf, size_t fbLen) {
  if (!saveToGDrive) return;
  
  HTTPClient http;
  http.begin(googleAppsScriptURL);
  http.addHeader("Content-Type", "image/jpeg");
  
  // Timestamp untuk mengidentifikasi gambar
  String timestamp = String(millis());
  http.addHeader("X-Timestamp", timestamp);
  
  int httpResponseCode = http.POST(fbBuf, fbLen);
  
  if (httpResponseCode > 0) {
    String response = http.getString();
    Serial.println("Respon Upload Google Drive: " + response);
    bot.sendMessage(CHAT_ID, "Foto disimpan di Google Drive: " + response, "");
  } else {
    Serial.println("Error mengupload ke Google Drive");
    bot.sendMessage(CHAT_ID, "Gagal menyimpan foto ke Google Drive", "");
  }
  
  http.end();
}

String sendPhotoTelegram() {
  const char* myDomain = "api.telegram.org";
  String getAll = "";
  String getBody = "";

  camera_fb_t *fb = esp_camera_fb_get();
  esp_camera_fb_return(fb);
  
  fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Pengambilan gambar gagal");
    delay(1000);
    ESP.restart();
    return "Pengambilan gambar gagal";
  }  
  
  // Kirim ke Google Drive
  if (saveToGDrive) {
    sendPhotoToGoogleDrive(fb->buf, fb->len);
  }
  
  Serial.println("Menghubungkan ke " + String(myDomain));

  if (clientTCP.connect(myDomain, 443)) {
    Serial.println("Koneksi berhasil");
    
    String head = "--RandomNerdTutorials\r\nContent-Disposition: form-data; name=\"chat_id\"; \r\n\r\n" + String(CHAT_ID) + "\r\n--RandomNerdTutorials\r\nContent-Disposition: form-data; name=\"photo\"; filename=\"esp32-cam.jpg\"\r\nContent-Type: image/jpeg\r\n\r\n";
    String tail = "\r\n--RandomNerdTutorials--\r\n";

    size_t imageLen = fb->len;
    size_t extraLen = head.length() + tail.length();
    size_t totalLen = imageLen + extraLen;
  
    clientTCP.println("POST /bot" + String(BOTtoken) + "/sendPhoto HTTP/1.1");
    clientTCP.println("Host: " + String(myDomain));
    clientTCP.println("Content-Length: " + String(totalLen));
    clientTCP.println("Content-Type: multipart/form-data; boundary=RandomNerdTutorials");
    clientTCP.println();
    clientTCP.print(head);
  
    uint8_t *fbBuf = fb->buf;
    size_t fbLen = fb->len;
    for (size_t n = 0; n < fbLen; n += 1024) {
      if (n + 1024 < fbLen) {
        clientTCP.write(fbBuf, 1024);
        fbBuf += 1024;
      } else if (fbLen % 1024 > 0) {
        size_t remainder = fbLen % 1024;
        clientTCP.write(fbBuf, remainder);
      }
    }  
    
    clientTCP.print(tail);
    esp_camera_fb_return(fb);
    
    int waitTime = 10000;
    long startTimer = millis();
    bool state = false;
    
    while ((startTimer + waitTime) > millis()) {
      Serial.print(".");
      delay(100);      
      while (clientTCP.available()) {
        char c = clientTCP.read();
        if (state == true) getBody += String(c);
        if (c == '\n') {
          if (getAll.length() == 0) state = true; 
          getAll = "";
        } else if (c != '\r')
          getAll += String(c);
        startTimer = millis();
      }
      if (getBody.length() > 0) break;
    }
    clientTCP.stop();
    Serial.println(getBody);
  } else {
    getBody = "Koneksi ke api.telegram.org gagal.";
    Serial.println("Koneksi ke api.telegram.org gagal.");
  }
  return getBody;
}

void handleNewMessages(int numNewMessages) {
  Serial.print("Menangani Pesan Baru: ");
  Serial.println(numNewMessages);

  for (int i = 0; i < numNewMessages; i++) {
    String chat_id = String(bot.messages[i].chat_id);
    if (chat_id != CHAT_ID) {
      bot.sendMessage(chat_id, "Pengguna tidak terotorisasi", "");
      continue;
    }
    
    String text = bot.messages[i].text;
    String from_name = bot.messages[i].from_name;
    
    if (text == "/start") {
      String welcome = "Selamat datang, " + from_name + "\n";
      welcome += "Perintah yang tersedia:\n";
      welcome += "/photo : ambil foto baru\n";
      welcome += "/flash : Toggel LED flash\n";
      welcome += "/buzzer_off : Matikan buzzer\n";
      welcome += "/buzzer_on : Nyalakan buzzer\n";
      welcome += "/gdrive_off : Matikan penyimpanan Google Drive\n";
      welcome += "/gdrive_on : Nyalakan penyimpanan Google Drive\n";
      welcome += "/status : Periksa status perangkat\n";
      bot.sendMessage(CHAT_ID, welcome, "");
    }
    else if (text == "/flash") {
      flashState = !flashState;
      digitalWrite(FLASH_LED_PIN, flashState);
    }
    else if (text == "/photo") {
      sendPhoto = true;
    }
    else if (text == "/buzzer_off") {
      buzzerEnabled = false;
      digitalWrite(buzzerPin, LOW);
      bot.sendMessage(CHAT_ID, "Buzzer dimatikan", "");
    }
    else if (text == "/buzzer_on") {
      buzzerEnabled = true;
      bot.sendMessage(CHAT_ID, "Buzzer dinyalakan", "");
    }
    else if (text == "/gdrive_off") {
      saveToGDrive = false;
      bot.sendMessage(CHAT_ID, "Penyimpanan Google Drive dinonaktifkan", "");
    }
    else if (text == "/gdrive_on") {
      saveToGDrive = true;
      bot.sendMessage(CHAT_ID, "Penyimpanan Google Drive diaktifkan", "");
    }
    else if (text == "/status") {
      String status = "Status Saat Ini:\n";
      status += "Buzzer: " + String(buzzerEnabled ? "Diaktifkan" : "Dinonaktifkan") + "\n";
      status += "Google Drive: " + String(saveToGDrive ? "Diaktifkan" : "Dinonaktifkan") + "\n";
      status += "Flash: " + String(flashState ? "ON" : "OFF") + "\n";
      bot.sendMessage(CHAT_ID, status, "");
    }
  }
}

void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
  Serial.begin(115200);
  
  pinMode(FLASH_LED_PIN, OUTPUT);
  digitalWrite(FLASH_LED_PIN, flashState);
  pinMode(pirPin, INPUT);
  pinMode(buzzerPin, OUTPUT);
  
  configInitCamera();

  WiFi.mode(WIFI_STA);
  Serial.println();
  Serial.print("Menghubungkan ke ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  clientTCP.setCACert(TELEGRAM_CERTIFICATE_ROOT);
  
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  
  Serial.println();
  Serial.print("Alamat IP ESP32-CAM: ");
  Serial.println(WiFi.localIP());
}

void loop() {
  if (sendPhoto) {
    Serial.println("Menyiapkan foto");
    sendPhotoTelegram();
    sendPhoto = false;
  }
  
  int pirValue = digitalRead(pirPin);
  if (pirValue == HIGH) {
    Serial.println("Gerakan terdeteksi!");
    sendPhoto = true;
    
    if (buzzerEnabled) {
      digitalWrite(buzzerPin, HIGH);
    }
    
    String message = "Gerakan terdeteksi! Foto akan dikirim";
    if (saveToGDrive) {
      message += " dan disimpan di Google Drive";
    }
    message += ".";
    bot.sendMessage(CHAT_ID, message, "");
  }

  if (millis() - lastTimeBotRan > botRequestDelay) {
    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    while(numNewMessages) {
      Serial.println("Mendapatkan respon");
      handleNewMessages(numNewMessages);
      numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    }
    lastTimeBotRan = millis();
  }
}