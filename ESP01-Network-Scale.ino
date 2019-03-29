#include <ESP8266WiFi.h>

#include <ESPAsyncTCP.h>
#include <AsyncEventSource.h>
#include <AsyncWebSocket.h>
#include <ESPAsyncWebServer.h>
#include <StringArray.h>
#include <WebAuthentication.h>
#include <WebHandlerImpl.h>
#include <WebResponseImpl.h>
#include <SPIFFSEditor.h>
#include <FS.h>

#include <HX711.h>
#include <Wire.h>

int connectionTime = 0;
File file;
char mac[18];
boolean tryConnection = false;

String ssid = "";
String pswd = "";

AsyncWebServer server(80);

const int LOADCELL_DATA_PIN = 12;
const int LOADCELL_CLK_PIN = 14;
HX711 scale;
float calibration_factor = -5480;
float scaleValue;
long zero_factor;

void setup() {
  Serial.begin(74880);
  Serial.println("ESP-01 Scale");

  if(!SPIFFS.begin()){
    Serial.println("An Error has occurred while mounting serial peripheral interface flash file system(SPIFFS).");
    return;
  }
  
  setMAC();
  Serial.print("MAC: ");
  Serial.println(mac);
  
  Serial.print("Connection Mode: ");
  Serial.println(wifi_mode_to_string(WiFi.getMode()));
  Serial.print("Connection Status: ");
  Serial.println(wl_status_to_string(WiFi.status()));
  Serial.println();

  if (WiFi.status() != WL_CONNECTED && strlen(WiFi.SSID().c_str()) > 0) {
    connectToNetwork();
  } else if (WiFi.status() != WL_CONNECTED && strlen(WiFi.SSID().c_str()) == 0) {
    listNetworks();
  }
}

void loop() {
  scale.set_scale(calibration_factor);
  scaleValue = scale.get_units() + 0.07F;
    
  if(Serial.available())
  {
    char temp = Serial.read();
    
    if(temp == '+') {
      Serial.print("Calibration factor increased to: ");
      Serial.println(calibration_factor);
      calibration_factor += 10;
    } else if(temp == '-') {
      calibration_factor -= 10;
      Serial.print("Calibration factor decreased to: ");
      Serial.println(calibration_factor);
    }
  }
  
  if (Serial.available() > 0) {
    String command = Serial.readStringUntil(' ');
    Serial.read();
    
    if (command == "connect") {
      ssid = Serial.readStringUntil(' ');
      Serial.read(); 
      pswd = Serial.readStringUntil('\n');
      connectToNetwork();
    }
    if (command == "disconnect") {
      WiFi.disconnect();
      Serial.println("Disconnected from the wiresless network.");
    }
    if (command == "list_networks") {
      listNetworks();
    }
    if (command == "reset") {
      hardwareReset();
    }
    if (command == "spiffs_format") {
      if(SPIFFS.format()){
        Serial.println("SPIFFS formatted successfully.");
      } else {
        Serial.println("SPIFFS format failiure.");
      }
    }
  }
}

void connectToNetwork() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), pswd.c_str());
  WiFi.setAutoConnect(true);
  Serial.print("Connecting to wireless access point with SSID ");
  Serial.print(WiFi.SSID().c_str());
  Serial.print(" ");
  while (WiFi.status() != WL_CONNECTED && connectionTime < 60) {
    delay(500);
    Serial.print(".");
    connectionTime++;
  }

  if (connectionTime >= 30) { Serial.println("ERROR"); Serial.println("Connection timed out. Access point may be unavailable."); connectionTime = 0; }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("OK");
    Serial.println();
    Serial.println("Network Configuration ");
    Serial.println("=============================================");
    Serial.print("Hostname: ");
    Serial.println(WiFi.hostname());
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
    Serial.printf("Gataway: %s\n", WiFi.gatewayIP().toString().c_str());
    Serial.print("Subnet: ");
    Serial.println(WiFi.subnetMask());
    Serial.print("DNS: ");
    WiFi.dnsIP().printTo(Serial);
    Serial.println();

    startWebServer();
    startScale();
  }
  else
  {
    listNetworks();
  }
}

/** Scale **/
void startScale() {
  scale.begin(LOADCELL_DATA_PIN, LOADCELL_CLK_PIN);
  scale.set_scale();
  scale.tare();
  zero_factor = scale.read_average();
}

/** Web Server **/

void startWebServer() {
  server.serveStatic("/", SPIFFS, "/web/html").setDefaultFile("index.html").setTemplateProcessor(processor);
  server.serveStatic("/client", SPIFFS, "/web/client");
  
  server.on("/wireless_signal", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/plain", String(WiFi.RSSI()));
  });
  server.on("/scale_weight", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/plain", String(scaleValue));
  });
  server.on("/scale_calibration", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/plain", String(calibration_factor));
  });
   
  server.begin();
}

String processor(const String& var)
{
  if(var == "ESPVAL_MAC"){ return String(mac); }
  if(var == "ESPVAL_HOST"){ return String(WiFi.hostname()); }
  if(var == "ESPVAL_IP"){ return String(WiFi.localIP().toString().c_str()); }
  if(var == "ESPVAL_GATEWAY"){ return String(WiFi.gatewayIP().toString().c_str()); }
  if(var == "ESPVAL_SUBNET"){ return String(WiFi.subnetMask().toString().c_str()); }
  if(var == "ESPVAL_DNS"){ return String(WiFi.dnsIP().toString().c_str()); }
  if(var == "ESPVAL_RSSI"){ return String(WiFi.RSSI()); }
  if(var == "ESPVAL_BSSID"){ return String(WiFi.BSSIDstr()); }
  if(var == "ESPVAL_SSID"){ return String(WiFi.SSID().c_str()); }
  if(var == "ESPVAL_HTML_NAVIGATION"){
    String data = readFile("/web/html/navigation.html");
    data.replace("%ESP_IP%", WiFi.localIP().toString().c_str());
    
    return data;
  }
  if(var == "ESPVAL_ZEROFACTOR"){ return String(zero_factor); }
  
  return String();
}

String readFile(const String& filePath) {
  if (SPIFFS.exists(filePath)){
    File f = SPIFFS.open(filePath, "r");
    if (f && f.size()) {
      String data;
        
      while (f.available()){
        data += char(f.read());
      }
      f.close();
      return String(data);
    }
  } else {
    Serial.print("ERROR: File not found: ");
    Serial.println(filePath);
  }
  return String();
}

/** Wireless Networking **/

void hardwareReset() {
  Serial.println("Resetting...");
  ESP.eraseConfig();
  WiFi.disconnect();
  delay(3000);
  Serial.println("Device reset.");
}

void setMAC() {
  uint8_t macAddr[6];
  WiFi.macAddress(macAddr);
  sprintf(mac, "%2X:%2X:%2X:%2X:%2X:%2X", macAddr[0], macAddr[1], macAddr[2], macAddr[3], macAddr[4], macAddr[5]);
}

void listNetworks() {
  Serial.print("Scanning for wireless networks...");
  int numSsid = WiFi.scanNetworks();
  
  if (numSsid == -1) {
    Serial.println("No wireless networks in range.");
    while (true);
  }

  Serial.print(numSsid);
  Serial.println(" wireless networks in range.");
  Serial.println();

  for (int thisNet = 0; thisNet < numSsid; thisNet++) {
    Serial.print(thisNet);
    Serial.print(") ");
    Serial.print(WiFi.RSSI(thisNet));
    Serial.print(" dBm\t");
    Serial.println(WiFi.SSID(thisNet));
  }
}

const char* wl_status_to_string(wl_status_t status) {
  switch (status) {
    case WL_NO_SHIELD: return "WL_NO_SHIELD";
    case WL_IDLE_STATUS: return "WL_IDLE_STATUS";
    case WL_NO_SSID_AVAIL: return "WL_NO_SSID_AVAIL";
    case WL_SCAN_COMPLETED: return "WL_SCAN_COMPLETED";
    case WL_CONNECTED: return "WL_CONNECTED";
    case WL_CONNECT_FAILED: return "WL_CONNECT_FAILED";
    case WL_CONNECTION_LOST: return "WL_CONNECTION_LOST";
    case WL_DISCONNECTED: return "WL_DISCONNECTED";
  }
}

const char* wifi_mode_to_string(int id) {
  switch (id) {
    case 0: return "WIFI_OFF";
    case 1: return "WIFI_STA";
    case 2: return "WIFI_AP";
    case 3: return "WIFI_AP_STA";
  }
}