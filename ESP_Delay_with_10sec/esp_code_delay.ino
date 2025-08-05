#include <WebServer.h>
#include <WiFi.h>
#include <esp32cam.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// --- Wi-Fi Credentials ---
const char* ssid = "One Plus Nord 4";
const char* password = "nishanth";

// --- Web Server Setup ---
WebServer server(80);

// --- LCD I2C Pins ---
#define SDA 14
#define SCL 15

// --- Camera Resolutions ---
static auto camRes = esp32cam::Resolution::find(320, 240); // Use loRes for faster transmission

// --- LCD Configuration ---
LiquidCrystal_I2C lcd(0x27, 16, 2);  // 16x2 LCD at I2C address 0x27

// --- Serial Input Parsing ---
String inputBuffer = "";
bool receiving = false;

void serveJpg()
{
  auto frame = esp32cam::capture();
  if (frame == nullptr) {
    Serial.println("CAPTURE FAIL");
    server.send(503, "", "");
    return;
  }

  // Avoid browser or script caching
  server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  server.sendHeader("Pragma", "no-cache");
  server.sendHeader("Expires", "-1");

  server.setContentLength(frame->size());
  server.send(200, "image/jpeg");
  WiFiClient client = server.client();
  frame->writeTo(client);
}

// --- Setup ---
void setup()
{
  Serial.begin(115200);
  Wire.begin(SDA, SCL);

  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Waiting data...");

  {
    using namespace esp32cam;
    Config cfg;
    cfg.setPins(pins::AiThinker);
    cfg.setResolution(camRes);
    cfg.setBufferCount(2);
    cfg.setJpeg(80);
    bool ok = Camera.begin(cfg);
    Serial.println(ok ? "CAMERA OK" : "CAMERA FAIL");
  }

  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.print("ESP32-CAM Ready: http://");
  Serial.println(WiFi.localIP());
  Serial.println("Use /snap for fresh image");

  server.on("/snap", []() {
    serveJpg();  // Fresh capture on demand
  });

  server.begin();
}

// --- Loop ---
void loop()
{
  server.handleClient();

  while (Serial.available()) {
    char incomingChar = Serial.read();

    if (incomingChar == '<') {
      inputBuffer = "";
      receiving = true;
    } else if (incomingChar == '>') {
      receiving = false;
      processMessage(inputBuffer);
      inputBuffer = "";
    } else if (receiving) {
      inputBuffer += incomingChar;
    }
  }
}

// --- Display on LCD ---
void processMessage(String message)
{
  lcd.clear();
  int line = 0;
  int startIdx = 0;

  while (startIdx < message.length() && line < 2) {
    int commaIdx = message.indexOf(',', startIdx);
    String pair = (commaIdx == -1) ? message.substring(startIdx) : message.substring(startIdx, commaIdx);
    startIdx = (commaIdx == -1) ? message.length() : commaIdx + 1;

    int colonIdx = pair.indexOf(':');
    if (colonIdx != -1) {
      String label = pair.substring(0, colonIdx);
      String count = pair.substring(colonIdx + 1);
      lcd.setCursor(0, line);
      lcd.print(label);
      lcd.print(":");
      lcd.print(count);
      line++;
    }
  }
}
