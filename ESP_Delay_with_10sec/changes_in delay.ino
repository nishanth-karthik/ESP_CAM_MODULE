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
static auto loRes = esp32cam::Resolution::find(320, 240);
static auto midRes = esp32cam::Resolution::find(530, 350);  // Width, then height
static auto hiRes = esp32cam::Resolution::find(800, 600);

// --- LCD Configuration ---
LiquidCrystal_I2C lcd(0x27, 16, 2);  // 16x2 LCD at I2C address 0x27

// --- Serial Input Parsing ---
String inputBuffer = "";
bool receiving = false;

// --- Setup Function ---
void setup()
{
  Serial.begin(115200);
  Wire.begin(SDA, SCL);

  // --- LCD Setup ---
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Waiting data...");

  // --- Camera Setup ---
  {
    using namespace esp32cam;
    Config cfg;
    cfg.setPins(pins::AiThinker);
    cfg.setResolution(midRes);
    cfg.setBufferCount(2);
    cfg.setJpeg(80);  // JPEG quality

    bool ok = Camera.begin(cfg);
    Serial.println(ok ? "CAMERA OK" : "CAMERA FAIL");
  }

  // --- Connect to Wi-Fi ---
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.print("ESP32-CAM IP: http://");
  Serial.println(WiFi.localIP());
  Serial.println("  /cam-lo.jpg");
  Serial.println("  /cam-mid.jpg");
  Serial.println("  /cam-hi.jpg");

  // --- HTTP Endpoints ---
  server.on("/cam-lo.jpg", handleJpgLo);
  server.on("/cam-mid.jpg", handleJpgMid);
  server.on("/cam-hi.jpg", handleJpgHi);
  server.begin();
}

// --- Main Loop ---
void loop()
{
  server.handleClient();

  // --- Handle incoming serial data ---
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

// --- Serve JPEG ---
void serveJpg()
{
  auto frame = esp32cam::capture();
  if (frame == nullptr) {
    Serial.println("CAPTURE FAIL");
    server.send(503, "", "");
    return;
  }

  Serial.printf("CAPTURE OK %dx%d %db\n", frame->getWidth(), frame->getHeight(),
                static_cast<int>(frame->size()));

  server.setContentLength(frame->size());
  server.send(200, "image/jpeg");
  WiFiClient client = server.client();
  frame->writeTo(client);
}

// --- Handlers for Different Resolutions ---
void handleJpgLo()
{
  if (!esp32cam::Camera.changeResolution(loRes)) {
    Serial.println("SET-LO-RES FAIL");
  }
  serveJpg();
}

void handleJpgMid()
{
  if (!esp32cam::Camera.changeResolution(midRes)) {
    Serial.println("SET-MID-RES FAIL");
  }
  serveJpg();
}

void handleJpgHi()
{
  if (!esp32cam::Camera.changeResolution(hiRes)) {
    Serial.println("SET-HI-RES FAIL");
  }
  serveJpg();
}

// --- Display Serial Data on LCD ---
void processMessage(String message)
{
  lcd.clear();

  int line = 0;
  int startIdx = 0;

  while (startIdx < message.length() && line < 2) {
    int commaIdx = message.indexOf(',', startIdx);
    String pair;

    if (commaIdx == -1) {
      pair = message.substring(startIdx);
      startIdx = message.length();
    } else {
      pair = message.substring(startIdx, commaIdx);
      startIdx = commaIdx + 1;
    }

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
