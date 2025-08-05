#include <WebServer.h>
#include <WiFi.h>
#include <esp32cam.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

const char* WIFI_SSID = "xxxxx";
const char* WIFI_PASS = "0000000";
 
WebServer server(80);

#define SDA 14
#define SCL 15

static auto loRes = esp32cam::Resolution::find(320, 240);
static auto midRes = esp32cam::Resolution::find(350, 530);
static auto hiRes = esp32cam::Resolution::find(800, 600);
 
LiquidCrystal_I2C lcd(0x27, 16, 2);  // Common I2C address: 0x27 or 0x3F
 
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
  Serial.printf("CAPTURE OK %dx%d %db\n", frame->getWidth(), frame->getHeight(),
                static_cast<int>(frame->size()));
 
  server.setContentLength(frame->size());
  server.send(200, "image/jpeg");
  WiFiClient client = server.client();
  frame->writeTo(client);
}
 
void handleJpgLo()
{
  if (!esp32cam::Camera.changeResolution(loRes)) {
    Serial.println("SET-LO-RES FAIL");
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
 
void handleJpgMid()
{
  if (!esp32cam::Camera.changeResolution(midRes)) {
    Serial.println("SET-MID-RES FAIL");
  }
  serveJpg();
}
 
 
void  setup(){
  Serial.begin(115200);
  Wire.begin(14,15);
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Waiting data...");
  Serial.println();
  {
    using namespace esp32cam;
    Config cfg;
    cfg.setPins(pins::AiThinker);
    cfg.setResolution(hiRes);
    cfg.setBufferCount(2);
    cfg.setJpeg(80); //JPEG compression quality for the ESP32-CAM when capturing images.
    
    bool ok = Camera.begin(cfg);
    Serial.println(ok ? "CAMERA OK" : "CAMERA FAIL");
  }
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }
  Serial.print("http://");
  Serial.println(WiFi.localIP());
  Serial.println("  /cam-lo.jpg");
  Serial.println("  /cam-hi.jpg");
  Serial.println("  /cam-mid.jpg");
 
  server.on("/cam-lo.jpg", handleJpgLo);
  server.on("/cam-hi.jpg", handleJpgHi);
  server.on("/cam-mid.jpg", handleJpgMid);
 
  server.begin();
}
 

void loop() {

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
 
void processMessage(String message) {
  lcd.clear();
 
  // Split message into label:count pairs
  int line = 0;
  int startIdx = 0;
 
  while (startIdx < message.length() && line < 2) {  // 2 lines max
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


 
