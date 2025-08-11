#include "esp_camera.h"
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
camera_config_t config;

#define SDA 14
#define SCL 15

// --- I2C LCD Configuration ---
LiquidCrystal_I2C lcd(0x27, 16, 2);  // Change 0x27 to 0x3F if needed

// --- Camera Pin Configuration ---
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

// --- Serial Data Framing ---
String serialData = "";
bool receiving = false;

void setup() {
  Serial.begin(115200);
  Wire.begin(SDA, SCL);
  Serial.setDebugOutput(false);
  delay(1000);

  // --- LCD Initialization ---
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("ESP32-CAM Ready");
  lcd.setCursor(0, 1);
  lcd.print("Waiting.");

  // --- Camera Configuration ---
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
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.frame_size = FRAMESIZE_QVGA;
  config.jpeg_quality = 10;
  config.fb_count = 1;

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed 0x%x", err);
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Cam Init Failed");
    while (true);
  }
  Serial.println("ESP32-CAM Ready. Waiting for command...");
}

void loop() {
  // === Handle Image Capture ===
  if (Serial.available()) {
    String command = Serial.readStringUntil('\n');
    command.trim();  // removes \r and extra whitespace

    // If host sends CAPTURE command
    if (command == "CAPTURE") {
         
      //lcd.clear();
      //lcd.setCursor(0, 0);
      //lcd.print("Capturing...");


      camera_fb_t *fb = esp_camera_fb_get();
      if (!fb) {
        Serial.println("ERROR");
        lcd.setCursor(0, 1);
        lcd.print("Capture Failed");
        return;
      }
      // Send image length (4 bytes)
      uint32_t len = fb->len;
      Serial.write((uint8_t *)&len, 4);
      Serial.write(fb->buf, fb->len); // Send JPEG buffer
      
      esp_camera_fb_return(fb);  // Return buffer so next capture is fresh
    } else if (command.startsWith("<") && command.endsWith(">")) {
      String clean = command.substring(1, command.length() - 1);  // Strip <>
      displayToLCD(clean);
    }
  }
}

// === LCD Display Handler ===
void displayToLCD(String msg) {
  lcd.clear();

  int separator = msg.indexOf('|');
  String line1 = "", line2 = "";

  if (separator != -1) {
    line1 = msg.substring(0, separator);
    line2 = msg.substring(separator + 1);
  } else {
    line1 = "Invalid Msg";
    line2 = msg;
  }

  int maxScrollLen = max(line1.length(), line2.length());

  for (int i = 0; i <= maxScrollLen - 1; i++) {
    lcd.clear();

    String scroll1 = (line1.length() > 16)
      ? line1.substring(i, min((int)(i + 16), (int)line1.length()))
      : line1;

    String scroll2 = (line2.length() > 16)
      ? line2.substring(i, min((int)(i + 16), (int)line2.length()))
      : line2;

    lcd.setCursor(0, 0);
    lcd.print(scroll1);
    lcd.setCursor(0, 1);
    lcd.print(scroll2);

    delay(500);
  }

  // Final fixed lines (no min() confusion here)
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(line1.substring(0, (line1.length() > 16) ? 16 : line1.length()));
  lcd.setCursor(0, 1);
  lcd.print(line2.substring(0, (line2.length() > 16) ? 16 : line2.length()));
  delay(2000);
}








/*void displayToLCD(String msg) {
  lcd.clear();

  int separator = msg.indexOf('|');
  String line1 = "", line2 = "";

  if (separator != -1) {
    line1 = msg.substring(0, separator);
    line2 = msg.substring(separator + 1);
  } else {
    line1 = "Invalid Msg";
    line2 = msg;
  }

  int scrollCount1 = (line1.length() > 16) ? (line1.length() - 16 + 1) : 1;
  int scrollCount2 = (line2.length() > 16) ? (line2.length() - 16 + 1) : 1;
  int maxScrolls = max(scrollCount1, scrollCount2);

  for (int i = 0; i < maxScrolls; i++) {
    lcd.clear();

    String scroll1 = (line1.length() > 16 && i < scrollCount1)
      ? line1.substring(i, i + 16)
      : (line1.length() > 16 ? line1.substring(scrollCount1 - 1, scrollCount1 - 1 + 16) : line1);

    String scroll2 = (line2.length() > 16 && i < scrollCount2)
      ? line2.substring(i, i + 16)
      : (line2.length() > 16 ? line2.substring(scrollCount2 - 1, scrollCount2 - 1 + 16) : line2);

    lcd.setCursor(0, 0);
    lcd.print(scroll1);
    lcd.setCursor(0, 1);
    lcd.print(scroll2);

    delay(700); // SCROLL SPEED
  }

  // Final fixed display
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(line1.substring(0, min(16, (int)line1.length())));
  lcd.setCursor(0, 1);
  lcd.print(line2.substring(0, min(16, (int)line2.length())));
  delay(2000); // DISPLAY HOLD TIME
}*/
