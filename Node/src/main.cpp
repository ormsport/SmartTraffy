#include <Arduino.h>
#include <string.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <ESPmDNS.h>
#define mDNSUpdate(c)  do {} while(0)
#include <WebServer.h>
#include <HTTPUpdateServer.h>
#include <SPIFFS.h>
#include <AutoConnect.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <esp_camera.h>
#include "SimStreamer.h"
#include "OV2640Streamer.h"
#include "OV2640.h"
#include "CRtspSession.h"
#include <Ticker.h>
#include "time.h"
#include <Adafruit_GFX.h>
#include <Adafruit_NeoMatrix.h>
#include <Adafruit_NeoPixel.h>
#include <Fonts/TomThumb.h>

Ticker ticker;
WebServer server(80);
WiFiServer rtspServer(554);
WiFiClient espClient;
PubSubClient mqttClient(espClient);
HTTPUpdateServer httpUpdater;
AutoConnect portal(server);
OV2640 cam;
CStreamer* streamer;
CRtspSession* session;
WiFiClient client;

#include "FSBrowse.h"
#include "definitions.h"
#include "camera_pins.h"

AutoConnectConfig Config;
AutoConnectAux auxLanding;
AutoConnectAux auxUpload;
AutoConnectAux auxBrowse;
AutoConnectAux auxUpdate(update_path, "Update");
Adafruit_NeoMatrix matrix = Adafruit_NeoMatrix(8, 8, 1, 1, MATRIX_PIN,
  NEO_MATRIX_BOTTOM + NEO_MATRIX_RIGHT,
  NEO_GRB + NEO_KHZ800);
const uint16_t colors[] = { matrix.Color(255,0,0), matrix.Color(219,123,43), matrix.Color(0,255,0) };

#include "functions.h"

void setup() {
  Serial.begin(115200);
  Serial.println("\nBooting...");

  // ***************************************************************************
  // Setup: Camera
  // ***************************************************************************
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
  config.frame_size = FRAMESIZE_SVGA;
  config.pixel_format = PIXFORMAT_JPEG;
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.jpeg_quality = 12;
  config.fb_count = 2;

  Serial.print("Camera init...");
  if (!cam.init(config)) {
    Serial.println("done");
  } else {
    Serial.println("failed");
    while (true) { yield(); } // If cam init failed, do nothing
  }

  // ***************************************************************************
  // Setup: GPIO
  // ***************************************************************************
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(R_PIN, OUTPUT);
  pinMode(Y_PIN, OUTPUT);
  pinMode(G_PIN, OUTPUT);
  pinMode(MATRIX_PIN, OUTPUT);

  // ***************************************************************************
  // Setup: WS2812b Matrix LED
  // ***************************************************************************
  matrix.begin();
  matrix.setTextWrap(false);
  matrix.setFont(&TomThumb);
  matrix.setBrightness(20);
  matrix.setTextColor(colors[0]);

  clearLight();

  // ***************************************************************************
  // Setup: SPIFFS
  // ***************************************************************************
  if (!SPIFFS.begin()) {
    Serial.println("SPIFFS Mount failed, Forrmatting...");
    SPIFFS.format();
    ESP.restart();
  } else {
    File root = SPIFFS.open("/");
    File file = root.openNextFile();
    while (file) {
      String fileName = file.name();
      size_t fileSize = file.size();
      Serial.printf("FS File: %s, size: %s\n", fileName.c_str(), formatBytes(fileSize).c_str());
      file = root.openNextFile();
    }
    Serial.printf("\n");
  }

  // ***************************************************************************
  // Setup: Load config file
  // ***************************************************************************
    Serial.print("Load config from FS...");
    if (readConfigFS()) {
        Serial.println("done");
    } else {
        Serial.println(", using default config");
        _mqtt_host = String(mqtt_host);
        _mqtt_port = String(mqtt_port);
        _mqtt_user = String(mqtt_user);
        _mqtt_pass = String(mqtt_pass);
        mqtt_intopic = mqtt_intopic + "/in";
        _mqtt_qos = mqtt_qos;
        nickname = nickname + "-" + nodeid;
        litBri = LIGHT_BRIGHTNESS;
        matBri = MATRIX_BRIGHTNESS;
        blkTiming = BLINK_TIMING;
        mode = R_BLK;
        updateConfigFS = true;
    }

  // ***************************************************************************
  // Setup: Set node ID
  // ***************************************************************************
  uint64_t chipid64 = ESP.getEfuseMac();
  snprintf(chipid, 16, "%04X%08X", (uint16_t)(chipid64 >> 32), (uint32_t)chipid64);
#if defined(use_chip_id)
  //snprintf(nickname, 32, "%s-%s", nickname, nodeid);
  snprintf(mqtt_clientid, 64, "%s_%s", nickname.c_str(), chipid);
#endif
  Serial.printf("Node name: %s\n", nickname.c_str());



  // ***************************************************************************
  // Setup: HTTP Update
  // ***************************************************************************
  httpUpdater.setup(&server, update_path, update_username, update_password);

  // ***************************************************************************
  // Setup: WebServer/FSBrowseHandle
  // ***************************************************************************
  //reboot
  server.on("/reboot", []() {
    Serial.printf("/reboot\n");
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200, "text/plain", "rebooting..." );
    ESP.restart();
  });
  //access edit page
  //load editor
  server.on("/edit", HTTP_GET, []() {
    if (!handleFileRead("/edit.htm")) server.send(404, "text/plain", "FileNotFound. Upload edit.htm to FS");
  });
  //list directory
  server.on("/list", HTTP_GET, handleFileList);
  //create file
  server.on("/edit", HTTP_PUT, handleFileCreate);
  //delete file
  server.on("/edit", HTTP_DELETE, handleFileDelete);
  //first callback is called after the request has ended with all parsed arguments
  //second callback handles file uploads at that location
  server.on("/edit", HTTP_POST, []() {
      server.send(200, "text/plain", "");
    }, handleFileUpload);
  //get heap status, analog input value and all GPIO statuses in one json call
  server.on("/all", HTTP_GET, []() {
    String json = "{";
    json += "\"heap\":" + String(ESP.getFreeHeap());
    json += ", \"analog\":" + String(analogRead(A0));
    json += ", \"gpio\":" + String((uint32_t)(0));
    json += "}";
    server.send(200, "text/json", json);
    json = String();
  });
  //not found
  server.onNotFound([]() {
    if (!handleFileRead(server.uri()))
      handleNotFound();
  });

  server.on("/esp_status", HTTP_GET, []() {
    DynamicJsonDocument jsonBuffer(JSON_OBJECT_SIZE(15) + 150);
    JsonObject json = jsonBuffer.to<JsonObject>();
    json["heap"] = ESP.getFreeHeap();
    json["sketch_size"] = ESP.getSketchSize();
    json["free_sketch_space"] = ESP.getFreeSketchSpace();
    json["flash_chip_size"] = ESP.getFlashChipSize();
    json["flash_chip_speed"] = ESP.getFlashChipSpeed();
    json["sdk_version"] = ESP.getSdkVersion();
    json["chip_rev"] = ESP.getChipRevision();
    json["cpu_freq"] = ESP.getCpuFreqMHz();
    json["mqtt"] = mqttClient.connected();

    String json_str;
    serializeJson(json, json_str);
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200, "application/json", json_str);
  });

  // ***************************************************************************
  // Setup: AutoConnect
  // ***************************************************************************
  //config softAP ssid
  Config.apid = mqtt_clientid;
  Config.psk = "";
  Config.autoReconnect = true;
  Config.hostName = mqtt_clientid;
  portal.config(Config);
  portal.onDetect(atDetect);
  //append FSBrowse path
  portal.append("/edit", "FSBrowse");
  // Load a custom web page 
  portal.load(NODE_CONFIG_PAGE);
  if (portal.load(NODE_CONFIG_EXEC)) portal.on(_applyUrl, applyConfig);
  auxLanding.load(PAGE_LANDING);
  auxUpload.load(PAGE_UPLOAD);
  auxBrowse.load(PAGE_BROWSE);
  auxBrowse.on(postUpload);
  portal.onNotFound(_handleFileRead);
  portal.join({ auxLanding, auxUpload, auxBrowse, auxUpdate });
  WiFi.setSleep(WIFI_PS_NONE);  //prevent wifi disconnect
  if (portal.begin()) {
    if (MDNS.begin(nickname.c_str())) {
        MDNS.addService("http", "tcp", 80);
        MDNS.addService("rtsp", "udp", 554);
        rtspServer.begin();
        offline = false;
        Serial.printf("Server ready!\nTo config node open http://%s.local/ in your browser.\nRTSP stream url: rtsp://%s:554/mjpeg/1\n", nickname.c_str(), WiFi.localIP().toString());
    } else Serial.println("Error setting up MDNS responder");
  } else {
    Serial.println("Connection failed.");
    while (true) { yield(); }
  }

  // ***************************************************************************
  // Setup: NTP
  // ***************************************************************************
  configTime(0, 0, ntpServer);

  // ***************************************************************************
  // Setup: MQTT(PubSupClient)
  // ***************************************************************************
  if ((_mqtt_host != "") && (_mqtt_port.toInt() > 0) && (_mqtt_port.toInt() < 65535)) {
    mqtt_config_invaild = false;
    mqtt_outtopic = "SmartTraffy/" + nickname + "/out";

    Serial.printf("MQTT active: %s:%d\n", _mqtt_host, _mqtt_port.toInt());

    mqttClient.setServer(_mqtt_host.c_str(), _mqtt_port.toInt());
    mqttClient.setCallback(mqtt_callback);

    while (!offline && !mqttClient.connected()) {
      Serial.print("Connecting to MQTT Server...");

      if (mqttClient.connect(mqtt_clientid, _mqtt_user.c_str(), _mqtt_pass.c_str())) {
        Serial.println("Connected");
        mqttClient.subscribe(mqtt_intopic.c_str(), _mqtt_qos.toInt());
        Serial.printf("Command topic(in): %s\nStatus topic(out): %s\n", mqtt_intopic.c_str(), mqtt_outtopic.c_str());
      } else {
        Serial.print("failed with state ");
        Serial.println(mqttClient.state());
        Serial.println("Sleep for 5 second then try again.");
        delay(5000);
      }
    }
  } else mqtt_config_invaild = true;
}

#if defined(printdebug)
uint32_t debug_lastprint;
#endif


void loop() {
  // Sketches the application here.
  mDNSUpdate(MDNS);
  portal.handleClient();
  handleRTSP();

  //update time
  epochTime = getTime();

  // signal system handle
  if (epochTime) handleLight();

  //check wifi status(non-blocking)
  if (WiFi.status() != WL_DISCONNECTED) {
    //print wifi status once when connected
    if (offline) {
      offline = false;
      Serial.print("WiFi connected\nIP address: ");
      Serial.println(WiFi.localIP());
    }
    //Handle web client
    server.handleClient();
    //Handle mqtt
    if (mqttClient.connected()) {
      mqttClient.loop();
      ticker.detach();
      //keep LED off
      digitalWrite(LED_BUILTIN, HIGH);
    } else if (!mqtt_config_invaild && !mqtt_conn_abort && (mqtt_reconnect_retries < MQTT_MAX_RECONNECT_TRIES)) {
      ticker.attach(1, tick);
      Serial.println("MQTT disconnected, reconnecting!");
      mqttReconnect();
    } else if (mqtt_conn_abort && ((millis() - mqtt_conn_abort_time) > (giveup_delay * 60000))) {
      mqtt_reconnect_retries = 0;
      mqtt_conn_abort = false;
    }
  } else if (!offline) {
    offline = true;
    mqtt_reconnect_retries = 0;
    ticker.attach(0.6, tick);
    Serial.print("WiFi disconnected, reconnecting");
    WiFi.reconnect();
    while (WiFi.status() == WL_DISCONNECTED) {
        Serial.print(".");
        yield();
        delay(300);
    }
    Serial.println("");
  }

  if (updateConfigFS && !updateFS) {
    (writeConfigFS()) ? Serial.println(" Success!") : Serial.println(" Failure!");
  }


#ifdef printdebug
  if (millis() - debug_lastprint > 1000) {
    debug_lastprint = millis();

  }
#endif
}
