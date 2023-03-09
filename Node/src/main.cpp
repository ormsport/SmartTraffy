// เรียกใช้ไลบารี่ต่างๆ
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

// กำหนดชื่อคลาสใช้เรียกแทนชื่อไลบารี่
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

// รวมการประกาศตัวแปรและการตกำหนด pin ของกล้อง
#include "FSBrowse.h"
#include "definitions.h"
#include "camera_pins.h"

// กำหนดหน้าเว็บเพจต่างๆของไลบารี่ AutoConnect
AutoConnectConfig Config;
AutoConnectAux auxLanding;
AutoConnectAux auxUpload;
AutoConnectAux auxBrowse;
AutoConnectAux auxUpdate(update_path, "Update");

// กำหนดค่าของ 8x8 WS2812b Matrix LED
Adafruit_NeoMatrix matrix = Adafruit_NeoMatrix(8, 8, 1, 1, MATRIX_PIN,
  NEO_MATRIX_BOTTOM + NEO_MATRIX_RIGHT,
  NEO_GRB + NEO_KHZ800);

// กำหนดค่าสีของ 8x8 WS2812b Matrix LED ไว้ในอาเรย์
const uint16_t colors[] = { matrix.Color(255,0,0), matrix.Color(219,123,43), matrix.Color(0,255,0) };

// รวมการใช้งานฟังก์ชั้นต่างๆ
#include "functions.h"

void setup() {
  Serial.begin(115200); // เริ่มการทำงานของ serial โดยใช้ buadrate 115200 บิต/วินาที
  Serial.println("\nBooting...");   // แสดงข้อความ "Booting..."

  // ***************************************************************************
  // Setup: Camera
  // ***************************************************************************
  // กำหนดการตั้งค่า pin ของกล้อง
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

  Serial.print("Camera init...");   // แสดงข้อความ "Camera init..."
  if (!cam.init(config)) {  // เงื่อนไขถ้าเริ่มการทำงานของกล้องด้วยค่าที่ตั้งไว้สำเร็จ
    Serial.println("done"); // แสดงข้อความ "done"
  } else {  // ถ้าเริ่มการทำงานของกล้องด้วยค่าที่ตั้งไว้ไม่สำเร็จ
    Serial.println("failed");   // แสดงข้อความ "failed"
    while (true) { yield(); } // หยุดการทำงานต่อด้วยลูป
  }

  // ***************************************************************************
  // Setup: GPIO
  // ***************************************************************************
  pinMode(LED_BUILTIN, OUTPUT); // กำหนดขา LED บนตัวบอร์ดเป็น output
  pinMode(R_PIN, OUTPUT);   // กำหนดขาไฟแดงเป็น output
  pinMode(Y_PIN, OUTPUT);   // กำหนดไฟเหลืองเป็น output
  pinMode(G_PIN, OUTPUT);   // กำหนดไปเขียวเป็น output
  pinMode(MATRIX_PIN, OUTPUT);  // กำหนดขา 8x8 WS2812b Matrix LED เป็น output

  // ***************************************************************************
  // Setup: 8x8 WS2812b Matrix LED
  // ***************************************************************************
  matrix.begin();   // เริ่มต้นการทำงานของไลบารี่ใช้การควบคุม 8x8 WS2812b Matrix LED
  matrix.setTextWrap(false);    // ปิดพื้นหลังในการแสดงข้อความ
  matrix.setFont(&TomThumb); // ตั้งค่าให้ใช้ฟ้อนต์ TomThumb
  matrix.setBrightness(20); // ตั้งระดับความสว่างไว้ที่ 20
  matrix.setTextColor(colors[0]);   // ตั้งค่าสีเป็นสีที่ 0 จากในอาเรย์

  clearLight(); // เรียกใช้ฟังก์ชั่น clearLight

  // ***************************************************************************
  // Setup: SPIFFS
  // ***************************************************************************
  if (!SPIFFS.begin()) {    //เริ่มการทำงานของไลบารี่ SPIFFS โดยถ้าเริ่มการทำงานไม่สำเร็จ
    Serial.println("SPIFFS Mount failed, Forrmatting...");  // ให้แสดงข้อความ "SPIFFS Mount failed, Forrmatting..."
    SPIFFS.format();    // ทำการฟอร์แมต SPI Flash
    ESP.restart();  // สั่งให้ ESP32 รีบูท
  } else {  // ถ้าเริ่มการทำงานสำเร็จ ให้แสดงรายชื่อไฟล์ที่มีทั้งหมด
    File root = SPIFFS.open("/");   // กำหนดให้โฟลเดอร์หลัก ("/" หมายถึงโฟลเดอร์นอกสุด)
    File file = root.openNextFile();    // กำหนดให้เปิดไฟล์ในโฟลเดอร์หลัก โดยถ้าเปิดไฟล์สำเร็จจะเก็บค่าหมายเลขของไฟล์นั้นในตัวแปร file โดยเริ่มจากหมายเลขไฟล์มากที่สุด
    while (file) {  // ถ้าตัวแปร file ยังไม่เป็น 0 ให้ทำงานในลูป
      String fileName = file.name();    // เก็บชื่อไฟล์ที่เปิดในตัวแปร fileName
      size_t fileSize = file.size();    // เก็บขนาดไฟล์ที่เปิดไว้ในตัวแปร fileSize
      Serial.printf("FS File: %s, size: %s\n", fileName.c_str(), formatBytes(fileSize).c_str());    // แสดงข้อความ "FS File: ชื่อไฟล์, size: ขนาดไฟล์" แล้วขึ้นปรรทัดใหม่
      file = root.openNextFile();   // เก็บค่าหมายเลขไฟล์ถัดไปไว้ในตัวแปร file
    }
    Serial.printf("\n");    // ขึ้นบรรทัดใหม่ใน serial monitor
  }

  // ***************************************************************************
  // Setup: Load config file
  // ***************************************************************************
    Serial.print("Load config from FS..."); // แสดงข้อความ ""
    if (readConfigFS()) {   // เรียกใช้ฟังก์ชั่น readConfigFS โดยถ้าสำเร็จจะส่งค่ากลับเป็น true
        Serial.println("done"); // แสดงข้อความ "done"
    } else {    // กรณีสำเร็จ (ส่งค่ากลับเป็น false) จะทำการดึงค่าตั้งต้น
        Serial.println(", using default config");   // แสดงข้อความ "using default config"
        _mqtt_host = String(mqtt_host); // นำค่าจากตั้วแปร mqtt_host มาเก็บในตัวแปร _mqtt_host ในรูปแบบ string
        _mqtt_port = String(mqtt_port); // นำค่าจากตั้วแปร mqtt_port มาเก็บในตัวแปร _mqtt_port ในรูปแบบ string
        _mqtt_user = String(mqtt_user); // นำค่าจากตั้วแปร mqtt_user มาเก็บในตัวแปร _mqtt_user ในรูปแบบ string
        _mqtt_pass = String(mqtt_pass); // นำค่าจากตั้วแปร mqtt_pass มาเก็บในตัวแปร _mqtt_pass ในรูปแบบ string
        mqtt_intopic += "/in";  // นำค่าเดิมในตัวแปร mqtt_intopic มาต่อท้ายด้วยข้อความ "/in"
        _mqtt_qos = mqtt_qos;   // นำค่าจากตั้วแปร mqtt_qos มาเก็บในตัวแปร _mqtt_qos
        nickname += "-" + nodeid;   // นำค่าเดิมในตัวแปร nickname มาต่อท้ายด้วยข้อความ "-" และต่อด้วยข้อความในตัวแปร nodeid
        litBri = LIGHT_BRIGHTNESS; // กำหนดค่าในตัวแปร litBri
        matBri = MATRIX_BRIGHTNESS; // กำหนดค่าในตัวแปร matBri
        blkTiming = BLINK_TIMING;   // กำหนดค่าในตัวแปร blkTiming
        mode = R_BLK;   // กำหนดค่าในตัวแปร mode เป็นค่าในตัวแปร enumerate R_BLK
        updateConfigFS = true;  // กำหนดค่าในตัวแปร updateConfigFS เป็น true
    }

  // ***************************************************************************
  // Setup: Set node ID
  // ***************************************************************************
  uint64_t chipid64 = ESP.getEfuseMac();    // เก็นค่า MAC address ของ ESP32 ในตัวแปร chipid64
  snprintf(chipid, 16, "%04X%08X", (uint16_t)(chipid64 >> 32), (uint32_t)chipid64); // แปลข้อมูลในตัวแปร chipid64 โดยการเลื่อนบิตไปทางขวา 32 บืตและเก็บในตัวแปร chipid
#if defined(use_chip_id)    // ถ้ามีการเปิดใช้งาน use_chip_id
  snprintf(mqtt_clientid, 64, "%s_%s", nickname.c_str(), chipid);   // นำข้อความในตัวแปร nickname ต่อด้วยข้อมูลในตัวแปร chipid จากนั้นเก็บข้อมูลไว้ในตัวแปร mqtt_clientid
#endif
  Serial.printf("Node name: %s\n", nickname.c_str());   // แสดงข้อความ "Node name: ข้อมูลในตัวแปร nickname"



  // ***************************************************************************
  // Setup: HTTP Update
  // ***************************************************************************
  httpUpdater.setup(&server, update_path, update_username, update_password);    // ตั้งค่าไลบารี่ในการอัพเดตเฟิร์มแวร์ผ่านเว็บ

  // ***************************************************************************
  // Setup: WebServer/FSBrowseHandle
  // ***************************************************************************
  //reboot
  server.on("/reboot", []() {   // ตั้งค่าการสั่งงานรีบูทโดยเรียก url จากเว็บ
    Serial.printf("/reboot\n"); // แสดงข้อความ "/reboot" และขึ้นบรรทัดใหม่
    server.sendHeader("Access-Control-Allow-Origin", "*");  // ส่ง header กลับไปที่ browse ที่เรียกใช้งาน
    server.send(200, "text/plain", "rebooting..." );    // ส่งข้อความแสดงคำว่า "rebooting..." กลับไปที่ browse ที่เรียกใช้งาน
    ESP.restart();  // สั่งให้ ESP32 รีบูท
  });
  //access edit page
  //load editor
  server.on("/edit", HTTP_GET, []() {   // เมื่อมีการเรียก url /edit ด้วย HTTP GET
    if (!handleFileRead("/edit.htm")) server.send(404, "text/plain", "FileNotFound. Upload edit.htm to FS"); // ถ้าหาไฟล์ "/edit.htm" ไม่เจอให้ส่ง html error code 404 โดยแสดงข้อความ "FileNotFound. Upload edit.htm to FS"
  });
  //list directory
  server.on("/list", HTTP_GET, handleFileList); // เมื่อมีการเรียก url /list ด้วย HTTP GET ให้เรียกใช้ฟังก์ชั้น handleFileList
  //create file
  server.on("/edit", HTTP_PUT, handleFileCreate);   // เมื่อมีการเรียก url /edit ด้วย HTTP PUT ให้เรียกใช้ฟังก์ชั้น handleFileCreate
  //delete file
  server.on("/edit", HTTP_DELETE, handleFileDelete);    // เมื่อมีการเรียก url /edit ด้วย HTTP DELETE ให้เรียกใช้ฟังก์ชั้น handleFileDelete
  //first callback is called after the request has ended with all parsed arguments
  //second callback handles file uploads at that location
  server.on("/edit", HTTP_POST, []() {  // เมื่อมีการเรียก url /edit ด้วย HTTP PSOT ให้เรียกใช้ฟังก์ชั้น handleFileDelete
      server.send(200, "text/plain", "");   // ส่ง html code 200 ให้เรียกใช้ฟังก์ชั้น handleFileUpload
    }, handleFileUpload);
  //not found
  server.onNotFound([]() {  // เมื่อมีการเรียกไฟล์ที่ไม่มีในระบบด้วย url
    if (!handleFileRead(server.uri()))  // เรียกใช้ฟังก์ชั้น handleFileRead โดยส่งค่า url ที่ไฟล์และส่งค่ากลับเป็น true หรือ false
      handleNotFound(); // เรียกใช้ฟังก์ชั้น handleNotFound
  });

  server.on("/esp_status", HTTP_GET, []() { // เมื่อมีการเรียก url /esp_status ด้วย HTTP GET ให้ส่งค่าสถานะต่างๆกลับในรูปแบบ json
    DynamicJsonDocument jsonBuffer(JSON_OBJECT_SIZE(15) + 150); // กำหนดขนาดบัฟเฟอร์ของข้อมูล json
    JsonObject json = jsonBuffer.to<JsonObject>();  // สร้างคลาสของข้อมูล โดยกำหนดขนาดจากบัฟเฟอร์
    json["heap"] = ESP.getFreeHeap();   // กำหนดตีย์ heap ในคลาสข้อมูลที่สร้างไว้ โดยเก็บข้อมูลที่ส่งกลับจากการเรียกใช้ฟังก์ชั่น ESP.getFreeHeap
    json["sketch_size"] = ESP.getSketchSize();  // กำหนดตีย์ sketch_size ในคลาสข้อมูลที่สร้างไว้ โดยเก็บข้อมูลที่ส่งกลับจากการเรียกใช้ฟังก์ชั่น ESP.getSketchSize
    json["free_sketch_space"] = ESP.getFreeSketchSpace();   // กำหนดตีย์ free_sketch_space ในคลาสข้อมูลที่สร้างไว้ โดยเก็บข้อมูลที่ส่งกลับจากการเรียกใช้ฟังก์ชั่น ESP.getFreeSketchSpace
    json["flash_chip_size"] = ESP.getFlashChipSize();   // กำหนดตีย์ flash_chip_size ในคลาสข้อมูลที่สร้างไว้ โดยเก็บข้อมูลที่ส่งกลับจากการเรียกใช้ฟังก์ชั่น ESP.getFlashChipSize
    json["flash_chip_speed"] = ESP.getFlashChipSpeed(); // กำหนดตีย์ flash_chip_speed ในคลาสข้อมูลที่สร้างไว้ โดยเก็บข้อมูลที่ส่งกลับจากการเรียกใช้ฟังก์ชั่น ESP.getFlashChipSpeed
    json["sdk_version"] = ESP.getSdkVersion();  // กำหนดตีย์ sdk_version ในคลาสข้อมูลที่สร้างไว้ โดยเก็บข้อมูลที่ส่งกลับจากการเรียกใช้ฟังก์ชั่น ESP.getSdkVersion
    json["chip_rev"] = ESP.getChipRevision();   // กำหนดตีย์ chip_rev ในคลาสข้อมูลที่สร้างไว้ โดยเก็บข้อมูลที่ส่งกลับจากการเรียกใช้ฟังก์ชั่น ESP.getChipRevision
    json["cpu_freq"] = ESP.getCpuFreqMHz(); // กำหนดตีย์ cpu_freq ในคลาสข้อมูลที่สร้างไว้ โดยเก็บข้อมูลที่ส่งกลับจากการเรียกใช้ฟังก์ชั่น ESP.getCpuFreqMHz
    json["mqtt"] = mqttClient.connected();  // กำหนดตีย์ mqtt ในคลาสข้อมูลที่สร้างไว้ โดยเก็บข้อมูลที่ส่งกลับจากการเรียกใช้ฟังก์ชั่น mqttClient.connected

    String json_str;    // ประกาศตัวแปรชื่อ json_str ชนิด string
    serializeJson(json, json_str);  // เรียกฟังก์ชั่นในการส่งออกข้อมูลในคลาส json มาเก็บในตัวแปร json_str
    server.sendHeader("Access-Control-Allow-Origin", "*");  // ส่ง header กลับไปยัง browse ที่เรียก url
    server.send(200, "application/json", json_str); // ส่งหลับข้อมูลในตัวแปร json_str ไปยัง browse โดยกำหนด HTTP code 200
  });

  // ***************************************************************************
  // Setup: AutoConnect
  // ***************************************************************************
  //config softAP ssid
  Config.apid = mqtt_clientid;  // กำหนดข้อมูลในคลาส Config.apid โดยใช้ข้อมูลจากตัวแปร mqtt_clientid
  Config.psk = "";  // กำหนดข้อมูลในคลาส Config.psk เป็นค่าว่าง
  Config.autoReconnect = true;  // กำหนดข้อมูลในคลาส Config.autoReconnect เป็น true
  Config.hostName = mqtt_clientid;  // กำหนดข้อมูลในคลาส Config.hostName โดยใช้ข้อมูลจากตัวแปร mqtt_clientid
  portal.config(Config);    // นำการตั้งค่าไปใช้งาน
  portal.onDetect(atDetect);    // เมื่อเริ่มการทำงานของระบบ captive portal แล้วให้เรียกใช้ฟังก์ชั่น atDetect
  //append FSBrowse path
  portal.append("/edit", "FSBrowse");   // สร้างเมนูจัดการไฟล์ โดยกำหนดการเรียก url ให้เป็น /edit
  // Load a custom web page 
  portal.load(NODE_CONFIG_PAGE);    // โหลดหน้าเว็บการตั้งค่า node จากข้อมูลที่อยู่ในตัวแปร NODE_CONFIG_PAGE
  if (portal.load(NODE_CONFIG_EXEC)) portal.on(_applyUrl, applyConfig); // ถ้าโหลดหน้าเว็บการตั้งค่า node จากตัวแปร NODE_CONFIG_EXEC สำเร็จ ให้ตั้งว่าเมื่อมีการเรียก url ในตัวแปร _applyUrl ให้เรียกใช้ฟงัก์ชั่น applyConfig
  auxLanding.load(PAGE_LANDING); // สั่งให้คลาส auxLanding โหลดหน้าเว็บ Landing จากข้อมุลในตัวแปร PAGE_LANDING
  auxUpload.load(PAGE_UPLOAD);  // สั่งให้คลาส auxUpload โหลดหน้าเว็บการอัพโหลดไฟล์จากข้อมุลในตัวแปร PAGE_UPLOAD
  auxBrowse.load(PAGE_BROWSE);  // สั่งให้คลาส auxBrowse โหลดหน้าเว็บแสดงผลการอัพโหลดไฟล์จากข้อมุลในตัวแปร PAGE_BROWSE
  auxBrowse.on(postUpload); // เมื่อคลาส auxBrowse มีการรับข้อมูลให้เรียกใช้ฟังก์ชั่น postUpload
  portal.onNotFound(_handleFileRead);   // เมื่อเรียก url ที่ไม่ได้ตั้งไว้ให้ลองหาไฟล์ตาม url ที่เรียก โดยใช้งานฟังก์ชั่น _handleFileRead
  portal.join({ auxLanding, auxUpload, auxBrowse, auxUpdate }); // รวมหน้าเว็บที่โหลดไว้ข้างต้นเข้ากับหน้าเว็บหลัก
  WiFi.setSleep(WIFI_PS_NONE);  // ปิดการโหมดประหยัดพลังงานของ wifi
  if (portal.begin()) { // เมื่อหน้าเว็บเริ่มการทำงานสำเร็จ
    if (MDNS.begin(nickname.c_str())) { // เมื่อ mDNS เริ่มการทำงานสำเร็จ
        MDNS.addService("http", "tcp", 80); // เพิ่มบริการ mDNS ชื่อ http ด้วย network protocol tcp พอร์ต 80
        MDNS.addService("rtsp", "udp", 554);    // เพิ่มบริการ mDNS ชื่อ rtsp ด้วย network protocol udp พอร์ต 554
        rtspServer.begin(); // เรื่มการทำงานของ RTSP server
        offline = false;    // กำหนดค่าในตัวแปร offline เป็น false
        Serial.printf("Server ready!\nTo config node open http://%s.local/ in your browser.\nRTSP stream url: rtsp://%s:554/mjpeg/1\n", nickname.c_str(), WiFi.localIP().toString());   // แสดงข้อความ "Server ready! ขึ้นบรรทัดใหม่ To config node open http://เลขไอพีที่ได้รับ.local/ in your browser.ขึ้นบรรทัดใหม่RTSP stream url: rtsp://เลขไอพีที่ได้รับ:554/mjpeg/1ขึ้นบรรทัดใหม่"
    } else Serial.println("Error setting up MDNS responder");   // โดยถ้า mDNS เริ่มการทำงานไม่สำเร็จ ให้แสดงข้อความ "Error setting up MDNS responder"
  } else {  // โดยถ้าหน้าเว็บเริ่มการทำงานไม่สำเร็จ
    Serial.println("Connection failed.");   // แสดงข้อความ "Connection failed."
    while (true) { yield(); }   // หยุดการทำงานต่อโดยใช้ลูป
  }

  // ***************************************************************************
  // Setup: NTP
  // ***************************************************************************
  configTime(0, 0, ntpServer);  //ตั้งค่า NTP โดยกำหนด timezone, daylight time และ NTP server

  // ***************************************************************************
  // Setup: MQTT(PubSupClient)
  // ***************************************************************************
  if ((_mqtt_host != "") && (_mqtt_port.toInt() > 0) && (_mqtt_port.toInt() < 65535)) { // ตรวจสอบการตั้งค่า server และ port ของ mqtt
    mqtt_config_invaild = false;    // กำหนดค่าในตัวแปร mqtt_config_invaild เป็น false
    mqtt_outtopic = "SmartTraffy/" + nickname + "/out"; // กำหนดค่าในตัวแปร mqtt_outtopic เป็นข้อความ "SmartTraffy/" ตามด้วยข้อความในตัวแปร nickname และต่อด้วย "/out"

    Serial.printf("MQTT active: %s:%d\n", _mqtt_host, _mqtt_port.toInt());  // แสดงข้อความ "MQTT active: ข้อควาในตัวแร _mqtt_host:ข้อควาในตัวแปร _mqtt_port" ปละขึ้นบรรทัดใหม่

    mqttClient.setServer(_mqtt_host.c_str(), _mqtt_port.toInt());   // ตั้งค่า server ให้กับไลบารี่ mqtt
    mqttClient.setCallback(mqtt_callback);  // กำหนดฟังก์ชั่นเรียกกลับในกรณีมีข้อมูล mqtt เข้ามา

    while (!offline && !mqttClient.connected()) {   //เงื่อนไขเมื่อเชื่อต่อ wifi อยู่ แต่ไม่ได้เชื่อมต่อไปยัง mqtt broker
      Serial.print("Connecting to MQTT Server..."); // แสดงข้อความ "Connecting to MQTT Server..."

      if (mqttClient.connect(mqtt_clientid, _mqtt_user.c_str(), _mqtt_pass.c_str())) {  // เชื่อต่อไปยัง mqtt broker เมื่อเชื่อมต่อได้ จะส่งค่ากลับ้เป็น true
        Serial.println("Connected");    // แสดงข้อความ "Connected"
        mqttClient.subscribe(mqtt_intopic.c_str(), _mqtt_qos.toInt());  //ทำการรับข้อมูลจากหัวข้อในตัวแปร mqtt_intopic ตาม quslity of service ที่กำหนดไว้
        Serial.printf("Command topic(in): %s\nStatus topic(out): %s\n", mqtt_intopic.c_str(), mqtt_outtopic.c_str());   // แสดงข้อความ ""
      } else {  //กรณีไม่สามารถเชื่อมต่อได้
        Serial.print("failed with state "); // แสดงข้อความ "failed with state"
        Serial.println(mqttClient.state()); // แสดงสาเหตุที่เชื่อมต่อไม่ได้
        Serial.println("Sleep for 5 second then try again.");   // แสดงข้อความ "Sleep for 5 second then try again."
        delay(5000);    //หน่วงเวลา 5 วินาที
      }
    }
  } else mqtt_config_invaild = true;    // กรณีการตั้งค่า mqtt ไม่ถูกต้อง กำหนดค่าในตัวแปร mqtt_config_invaild เป็น true
}

#if defined(printdebug) // ถ้าเปิดใช้งาน printdebug
uint32_t debug_lastprint;   // กำหนดตัวแปรชื่อ debug_lastprint
#endif


void loop() {
  // Sketches the application here.
  mDNSUpdate(MDNS); // กำทำงานของไลบารี่ mDNS
  portal.handleClient();    // การทำงานของหน้าเว็บ captive portal
  handleRTSP(); // การทำงานของ RTPS server

  //update time
  epochTime = getTime();    // อัพเดตเวลาจาก RTC

  // signal system handle
  if (epochTime) handleLight(); // ถ้าค่าในตัวแปร epochTime มากกว่า 0 ให้เรียกฟังก์ชั่น handleLight

  //check wifi status(non-blocking)
  if (WiFi.status() != WL_DISCONNECTED) {   // ถ้า wifi เชื่อมต่ออยู่
    //print wifi status once when connected
    if (offline) {  // ถ้าตัวแปร offline มีสถานะเป็น true
      offline = false;  // กำหนดแปร offline ให้มีสถานะเป็น false
      Serial.print("WiFi connected\nIP address: "); // แสดงข้อความ "WiFi connected\nIP address: "
      Serial.println(WiFi.localIP());   // แสดงเลขไอพีที่ได้รับ
    }
    //Handle web client
    server.handleClient();  // การทำงานของหน้าเว็บการตั้งค่า node
    //Handle mqtt
    if (mqttClient.connected()) {   // ถ้า mqtt เชื่อมต่ออยู่
      mqttClient.loop();    // การทำงานของ mqtt
      ticker.detach();  // ปิดไฟกระพริบ
      //keep LED off
      digitalWrite(LED_BUILTIN, HIGH);  // ปิดไฟ LED
    } else if (!mqtt_config_invaild && !mqtt_conn_abort && (mqtt_reconnect_retries < MQTT_MAX_RECONNECT_TRIES)) {   // ถัาพยายามเชื่อมต่อเกินจำนวนครั้งที่กำหนด
      ticker.attach(1, tick); // เปิดไฟกระพริบทุก 1 วินาที
      Serial.println("MQTT disconnected, reconnecting!");   // แสดงข้อความ "MQTT disconnected, reconnecting!"
      mqttReconnect();  // เรียกใช้ฟังก์ชั่น mqttReconnect
    } else if (mqtt_conn_abort && ((millis() - mqtt_conn_abort_time) > (giveup_delay * 60000))) {   // ถ้าหมดเวลาในการยกเลิกพยายามเชื่อมต่อ
      mqtt_reconnect_retries = 0;   // กำหนดค่าในตัวแปร mqtt_reconnect_retries เป็น 0
      mqtt_conn_abort = false;  // กำหนดค่าในตัวแปร mqtt_conn_abort เป็น false
    }
  } else if (!offline) {    // ถ้าค่าในตัวแปร offline เป็น false
    offline = true; // กำหนดค่าในตัวแปร offline เป็น true
    mqtt_reconnect_retries = 0; // กำหนดค่าในตัวแปร mqtt_reconnect_retries เป็น 0
    ticker.attach(0.6, tick);   // เปิดไฟกระพริบทุก 0.6 วินาที
    Serial.print("WiFi disconnected, reconnecting");    // แสดงข้อความ "WiFi disconnected, reconnecting"
    WiFi.reconnect();   // สั่งให้ลองเชื่อมต่อ wifi ใหม่
    while (WiFi.status() == WL_DISCONNECTED) { // ใช้ลูปถ้า wifi ยังไม่เชื่อมต่อ
        Serial.print(".");  // แสดงข้อความ "."
        yield();    // ให้การทำงานหลักของ ESP32 ทำงานต่อ
        delay(300); // หน่วงเวลา 0.3 วินาที
    }
    Serial.println(""); // ขึ้นบรรทัดใหม่
  }

  if (updateConfigFS && !updateFS) {    // ถ้าต้องการบันทึกการตั้งค่าและไม่มีการเข้าถึง SPI flash อยู่
    (writeConfigFS()) ? Serial.println(" Success!") : Serial.println(" Failure!");  // แสดงผลการทำงานของฟังก์ชั่น writeConfigFS ด้วยข้อความ "Success!" หรือ " Failure!"
  }


#ifdef printdebug   // ถ้าเปิดใช้งาน printdebug
  if (millis() - debug_lastprint > 1000) {  // หน่วงเวลา 1 วินาที ด้วย millis
    debug_lastprint = millis(); // อัพเดตเวลาล่าสุดในตัวแปร debug_lastprint

  }
#endif
}
