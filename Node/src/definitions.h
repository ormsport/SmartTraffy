//set nickname
String nickname = "SmartTraffyNode";
//set node id
String nodeid = "0";
//use chip id for mqtt client id.
#define use_chip_id

//MQTT default config
const char* mqtt_host = "192.168.1.70";
const char* mqtt_port = "1883";
const char* mqtt_user = "mLS8^UuxNXD";
const char* mqtt_pass = "X^56&Fm4&rpb";
String mqtt_intopic = "SmartTraffy/cmd";
const char* mqtt_qos = "0";
#define MQTT_MAX_RECONNECT_TRIES 5
const uint8_t giveup_delay = 1; // min

//HTTP update
const char* update_path = "/update";
const char* update_username = "admin";
const char* update_password = "admin";

//light
#define SWITCH_OFFSET 1
#define BLINK_TIMING 1200
#define LIGHT_BRIGHTNESS 255
#define MATRIX_BRIGHTNESS 255
#define fadeVal 2

//IO
#define R_PIN 13
#define Y_PIN 15
#define G_PIN 14
#define MATRIX_PIN 12

//Cam model
//#define CAMERA_MODEL_WROVER_KIT // Has PSRAM
//#define CAMERA_MODEL_ESP_EYE // Has PSRAM
//#define CAMERA_MODEL_ESP32S3_EYE // Has PSRAM
//#define CAMERA_MODEL_M5STACK_PSRAM // Has PSRAM
//#define CAMERA_MODEL_M5STACK_V2_PSRAM // M5Camera version B Has PSRAM
//#define CAMERA_MODEL_M5STACK_WIDE // Has PSRAM
//#define CAMERA_MODEL_M5STACK_ESP32CAM // No PSRAM
//#define CAMERA_MODEL_M5STACK_UNITCAM // No PSRAM
#define CAMERA_MODEL_AI_THINKER // Has PSRAM
//#define CAMERA_MODEL_TTGO_T_JOURNAL // No PSRAM
// ** Espressif Internal Boards **
//#define CAMERA_MODEL_ESP32_CAM_BOARD
//#define CAMERA_MODEL_ESP32S2_CAM_BOARD
//#define CAMERA_MODEL_ESP32S3_CAM_LCD

#define LED_BUILTIN 33

#define printdebug
//--------------------------------------------------------------------------
//MQTT
String _mqtt_host;
String _mqtt_port;
String _mqtt_user;
String _mqtt_pass;
String mqtt_outtopic;
String _mqtt_qos;
int mqtt_reconnect_retries = 0;
uint32_t mqtt_conn_abort_time;
bool updateConfigFS = false, updateFS = false, cfgLoaded, mqtt_config_invaild, mqtt_conn_abort, offline;

//If using chipid
#ifdef use_chip_id
 char mqtt_clientid[64];
 char chipid[16];
#else
 const char* mqtt_clientid = nickname + "-" + nodeid;
#endif

//AutoConnectAUx entry point
const char*  const _configUrl = "/config";
const char*  const _applyUrl = "/apply";

//WS2812 Matrix status, light variable
uint8_t timing[4] = {20, 5, 10, 15}, seq[4] = {3, 0, 2, 1};
enum status {OFF, R, Y, G} _light, light;
const char* lightStatus[] = {"off", "red", "yellow", "green"};
enum op {AUTO, R_BLK, Y_BLK, MAN, ALL_RED} _mode, mode;
const char* opMode[] = {"auto", "rBlink", "yBlink", "manual", "allRed"};
uint8_t maxSeq = sizeof(timing);
uint8_t color_num, rTiming, yTiming = 3, gTiming, count, q, gId;
uint8_t _litBri, litBri, _matBri, matBri;
uint32_t last_count, lastAdj, lastTick;
uint16_t blkTiming;
bool start = true, next = true, timingUpdate = true, cmdUpdate, rState, yState, gState;

//AutoConnect Custom Page
//This page for an example only, you can prepare the other for your application.
static const char PAGE_LANDING[] PROGMEM = R"(
{
  "title": "SmartTraffyNode",
  "uri": "/",
  "menu": false,
  "element": [
    {
      "name": "css",
      "type": "ACStyle",
      "value": ".noorder{position:absolute;left:0;right:0;margin-left:auto;margin-right:auto;text-align:center;width:600px;}"
    },
    {
      "name": "caption",
      "type": "ACText",
      "value": "<h2>Welcome to SmartTraffyNode Configurations</h2>",
      "style": "color:#2f4f4f;"
    },
    {
      "name": "content",
      "type": "ACText",
      "value": "Press \"Config\" in the menu to config this node.<br>Press \"Upload\" to upload file to SPIFFS.<br>Press \"FSBrowse\" to enter SPIFFS file manager.<br>Press \"Update\" to update Firmware/FileSystem."
    }
  ]
}
)";

//Node config
static const char NODE_CONFIG_PAGE[] PROGMEM = R"*(
{
  "title": "Config",
  "uri": "/config",
  "menu": true,
  "element": [
    {
      "name": "css",
      "type": "ACStyle",
      "value": ".noorder label{display:inline-block;min-width:150px;padding:5px;} .noorder select{width:160px} .magnify{width:20px}"
    },
    {
      "name": "nodeHead",
      "type": "ACText",
      "value": "<h1>Node Config</h1>",
      "style": "text-align:center;color:#2f4f4f;"
    },
    {
      "name": "nodeName",
      "type": "ACInput",
      "label": "Nickname",
      "placeholder": "SmartTraffyNode",
      "pattern": "^.{1,30}"
    },
    {
      "name": "nodeId",
      "type": "ACInput",
      "label": "Node ID",
      "apply": "number",
      "placeholder": "0",
      "pattern": "[0-9]{1,2}"
    },
    {
      "name": "mqttHead",
      "type": "ACText",
      "value": "<h1>MQTT Config</h1>",
      "style": "text-align:center;color:#2f4f4f;"
    },
    {
      "name": "mqttHost",
      "type": "ACInput",
      "label": "Host",
      "placeholder": "0.0.0.0",
      "pattern": "(([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5])\\.){3}([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5])"
    },
    {
      "name": "mqttPort",
      "type": "ACInput",
      "label": "Port",
      "apply": "number",
      "placeholder": "1883",
      "pattern": "[1-9]|[1-5]?[0-9]{2,4}|6[1-4][0-9]{3}|65[1-4][0-9]{2}|655[1-2][0-9]|6553[1-5]"
    },
    {
      "name": "mqttUser",
      "type": "ACInput",
      "label": "User",
      "placeholder": "username",
      "pattern": "^.{1,20}"
    },
    {
      "name": "mqttPass",
      "type": "ACInput",
      "label": "Password",
      "apply": "password",
      "placeholder": "password",
      "pattern": "^.{1,20}"
    },
    {
      "name": "mqttTopic",
      "type": "ACInput",
      "label": "Listen Topic",
      "placeholder": "SmartTraffy/cmd/in",
      "maxlength": "^.{1,64}"
    },
    {
      "name": "mqttQos",
      "type": "ACInput",
      "label": "QOS(0-2)",
      "apply": "number",
      "placeholder": "0",
      "pattern": "[0-2]{1,1}"
    },
    {
      "name": "camHead",
      "type": "ACText",
      "value": "<h1>Camera Config</h1>",
      "style": "text-align:center;color:#2f4f4f;"
    },
    {
      "name": "res",
      "type": "ACSelect",
      "label": "Resolution",
      "option": [
        "UXGA(1600x1200)",
        "SXGA(1280x1024)",
        "XGA(1024x768)",
        "SVGA(800x600)",
        "VGA(640x480)",
        "CIF(400x296)",
        "QVGA(320x240)",
        "HQVGA(240x176)",
        "QQVGA(160x120)"
      ],
      "selected": 4
    },
    {
      "name": "qua",
      "type": "ACRange",
      "label": "Quality",
      "value": 10,
      "min": 10,
      "max": 63,
      "magnify": "infront"
    },
    {
      "name": "con",
      "type": "ACRange",
      "label": "Contrast",
      "value": 0,
      "min": -2,
      "max": 2,
      "magnify": "infront"
    },
    {
      "name": "bri",
      "type": "ACRange",
      "label": "Brightness",
      "value": 0,
      "min": -2,
      "max": 2,
      "magnify": "infront"
    },
    {
      "name": "sat",
      "type": "ACRange",
      "label": "Saturation",
      "value": 0,
      "min": -2,
      "max": 2,
      "magnify": "infront"
    },
    {
      "name": "se",
      "type": "ACSelect",
      "label": "Special Effect",
      "option": [
        "No Effect",
        "Negative",
        "Grayscale",
        "Red Tint",
        "Green Tint",
        "Blue Tint",
        "Sepia"
      ],
      "selected": 1
    },
    {
      "name": "awb",
      "type": "ACCheckbox",
      "label": "AWB",
      "labelposition": "infront",
      "checked": true
    },
    {
      "name": "wbg",
      "type": "ACCheckbox",
      "label": "AWB Gain",
      "labelposition": "infront",
      "checked": true
    },
    {
      "name": "wbm",
      "type": "ACSelect",
      "label": "WB Mode",
      "option": [
        "Auto",
        "Sunny",
        "Cloudy",
        "Office",
        "Home"
      ],
      "selected": 1
    },
    {
      "name": "aec",
      "type": "ACCheckbox",
      "label": "AEC SENSOR",
      "labelposition": "infront",
      "checked": true
    },
    {
      "name": "dsp",
      "type": "ACCheckbox",
      "label": "AEC DSP",
      "labelposition": "infront",
      "checked": true
    },
    {
      "name": "ael",
      "type": "ACRange",
      "label": "AE Level",
      "value": 0,
      "min": -2,
      "max": 2,
      "magnify": "infront"
    },
    {
      "name": "exp",
      "type": "ACRange",
      "label": "Exposure",
      "value": 204,
      "min": 0,
      "max": 1200,
      "magnify": "infront",
      "style": "margin-left:20px;width:110px"
    },
    {
      "name": "agc",
      "type": "ACCheckbox",
      "label": "AGC",
      "labelposition": "infront",
      "checked": true
    },
    {
      "name": "agv",
      "type": "ACRange",
      "label": "AGC Gain (Nx)",
      "value": 5,
      "min": 1,
      "max": 31,
      "magnify": "infront"
    },
    {
      "name": "acl",
      "type": "ACSelect",
      "label": "Gain Ceiling",
      "option": [
        "2X",
        "4X",
        "8X",
        "16X",
        "32X",
        "64X",
        "128X"
      ],
      "selected": 1
    },
    {
      "name": "bpc",
      "type": "ACCheckbox",
      "label": "DPC Black",
      "labelposition": "infront",
      "checked": true
    },
    {
      "name": "wpc",
      "type": "ACCheckbox",
      "label": "DPC White",
      "labelposition": "infront",
      "checked": true
    },
    {
      "name": "gma",
      "type": "ACCheckbox",
      "label": "GMA enable",
      "labelposition": "infront",
      "checked": true
    },
    {
      "name": "lec",
      "type": "ACCheckbox",
      "label": "Lense Correction",
      "labelposition": "infront",
      "checked": true
    },
    {
      "name": "hmi",
      "type": "ACCheckbox",
      "label": "H-Mirror",
      "labelposition": "infront"
    },
    {
      "name": "vfl",
      "type": "ACCheckbox",
      "label": "V-Flip",
      "labelposition": "infront"
    },
    {
      "name": "dcw",
      "type": "ACCheckbox",
      "label": "DCW (Downsize EN)",
      "labelposition": "infront"
    },
    {
      "name": "apply",
      "type": "ACSubmit",
      "value": "Apply",
      "uri": "/apply"
    }
  ]
}
)*";

static const char NODE_CONFIG_EXEC[] PROGMEM = R"*(
{
  "title": "Camera",
  "uri": "/apply",
  "response": false,
  "menu": false
}
)*";

// Upload request custom Web page
static const char PAGE_UPLOAD[] PROGMEM = R"(
{
  "uri": "/upload",
  "title": "Upload",
  "menu": true,
  "element": [
    {
      "name": "caption",
      "type": "ACText",
      "value": "<h2>File upload</h2>"
    },
    {
      "name": "upload_file",
      "type": "ACFile",
      "label": "Select file: ",
      "store": "fs"
    },
    {
      "name": "upload",
      "type": "ACSubmit",
      "value": "UPLOAD",
      "uri": "/_upload"
    }
  ]
}
)";

// Upload result display
static const char PAGE_BROWSE[] PROGMEM = R"(
{
  "uri": "/_upload",
  "title": "Upload",
  "menu": false,
  "element": [
    {
      "name": "caption",
      "type": "ACText",
      "value": "<h2>Uploading ended</h2>"
    },
    {
      "name": "filename",
      "type": "ACText",
      "posterior": "br"
    },
    {
      "name": "size",
      "type": "ACText",
      "format": "%s bytes uploaded",
      "posterior": "br"
    },
    {
      "name": "content_type",
      "type": "ACText",
      "format": "Content: %s",
      "posterior": "br"
    },
    {
      "name": "object",
      "type": "ACElement"
    }
  ]
}
)";