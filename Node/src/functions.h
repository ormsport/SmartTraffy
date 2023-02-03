//#################################
//-------Ticker-------
//#################################
void tick() {
    //toggle state
    bool state = digitalRead(LED_BUILTIN);  // get the current state of GPIO1 pin
    digitalWrite(LED_BUILTIN, !state);     // set pin to the opposite state
}

//#################################
//-----------AutoConnect-----------
//#################################
bool atDetect(IPAddress& softapIP) {
    Serial.println("Captive portal started, SSID: " + String(nodeid) + ", IP: " + softapIP.toString());
    return true;
}

String applyConfig(AutoConnectAux& aux, PageArgument& args) {
    AutoConnectAux& cameraSetup = *portal.aux(_configUrl);

    // Node config
    nickname = cameraSetup["nodeName"].as<AutoConnectInput>().value;
    nodeid = cameraSetup["nodeId"].as<AutoConnectInput>().value;

    // MQTT Config
    _mqtt_host = cameraSetup["mqttHost"].as<AutoConnectInput>().value;
    _mqtt_port = cameraSetup["mqttPort"].as<AutoConnectInput>().value;
    _mqtt_user = cameraSetup["mqttUser"].as<AutoConnectInput>().value;
    _mqtt_pass = cameraSetup["mqttPass"].as<AutoConnectInput>().value;
    mqtt_intopic = cameraSetup["mqttTopic"].as<AutoConnectInput>().value;
    _mqtt_qos = cameraSetup["mqttQos"].as<AutoConnectInput>().value;

    // Camera Config
    // Take over the current settings
    sensor_t *s = esp_camera_sensor_get();
    int res = 0;

    // Framesize
    const String& resolution = cameraSetup["res"].as<AutoConnectSelect>().value();
    if (resolution == "UXGA(1600x1200)")
        res = s->set_framesize(s, FRAMESIZE_UXGA);
    else if (resolution == "SXGA(1280x1024)")
        res = s->set_framesize(s, FRAMESIZE_SXGA);
    else if (resolution == "XGA(1024x768)")
        res = s->set_framesize(s, FRAMESIZE_XGA);
    else if (resolution == "SVGA(800x600)")
        res = s->set_framesize(s, FRAMESIZE_SVGA);
    else if (resolution == "VGA(640x480)")
        res = s->set_framesize(s, FRAMESIZE_VGA);
    else if (resolution == "CIF(400x296)")
        res = s->set_framesize(s, FRAMESIZE_CIF);
    else if (resolution == "QVGA(320x240)")
        res = s->set_framesize(s, FRAMESIZE_QVGA);
    else if (resolution == "HQVGA(240x176)")
        res = s->set_framesize(s, FRAMESIZE_HQVGA);
    else if (resolution == "QQVGA(160x120)")
        res = s->set_framesize(s, FRAMESIZE_QQVGA);

    // Pixel granularity
    res = s->set_quality(s, cameraSetup["qua"].as<AutoConnectRange>().value);

    // Color solid adjustment
    res = s->set_contrast(s, cameraSetup["con"].as<AutoConnectRange>().value);
    res = s->set_brightness(s, cameraSetup["bri"].as<AutoConnectRange>().value);
    res = s->set_saturation(s, cameraSetup["sat"].as<AutoConnectRange>().value);

    // SE
    const String& se = cameraSetup["se"].as<AutoConnectSelect>().value();
    if (se == "No Effect")
        res = s->set_special_effect(s, 0);
    if (se == "Negative")
        res = s->set_special_effect(s, 1);
    if (se == "Grayscale")
        res = s->set_special_effect(s, 2);
    if (se == "Red Tint")
        res = s->set_special_effect(s, 3);
    if (se == "Green Tint")
        res = s->set_special_effect(s, 4);
    if (se == "Blue Tint")
        res = s->set_special_effect(s, 5);
    if (se == "Sepia")
        res = s->set_special_effect(s, 6);

    // White Balance effection
    res = s->set_whitebal(s, cameraSetup["awb"].as<AutoConnectCheckbox>().checked ? 1 : 0);
    res = s->set_awb_gain(s, cameraSetup["wbg"].as<AutoConnectCheckbox>().checked ? 1 : 0);
    const String& wbMode = cameraSetup["wbm"].as<AutoConnectSelect>().value();
    if (wbMode == "Auto")
        res = s->set_wb_mode(s, 0);
    else if (wbMode == "Sunny")
        res = s->set_wb_mode(s, 1);
    else if (wbMode == "Cloudy")
        res = s->set_wb_mode(s, 2);
    else if (wbMode == "Office")
        res = s->set_wb_mode(s, 3);
    else if (wbMode == "Home")
        res = s->set_wb_mode(s, 4);

    // Automatic Exposure Control, Turn off AEC to set the exposure level.
    res = s->set_exposure_ctrl(s, cameraSetup["exp"].as<AutoConnectCheckbox>().checked ? 1 : 0);
    res = s->set_aec2(s, cameraSetup["dsp"].as<AutoConnectCheckbox>().checked ? 1 : 0);
    res = s->set_ae_level(s, cameraSetup["ael"].as<AutoConnectRange>().value);
    res = s->set_aec_value(s, cameraSetup["aec"].as<AutoConnectRange>().value);

    // Automatic Gain Control
    res = s->set_gain_ctrl(s, cameraSetup["agc"].as<AutoConnectCheckbox>().checked ? 1 : 0);
    res = s->set_agc_gain(s, cameraSetup["agv"].as<AutoConnectRange>().value - 1);
    
    // Gain Ceiling Control
    const String& acl = cameraSetup["acl"].as<AutoConnectSelect>().value();
    if (acl == "2X")
        res = s->set_gainceiling(s, GAINCEILING_2X);
    else if (acl == "4X")
        res = s->set_gainceiling(s, GAINCEILING_4X);
    else if (acl == "8X")
        res = s->set_gainceiling(s, GAINCEILING_8X);
    else if (acl == "16X")
        res = s->set_gainceiling(s, GAINCEILING_16X);
    else if (acl == "32X")
        res = s->set_gainceiling(s, GAINCEILING_32X);
    else if (acl == "64X")
        res = s->set_gainceiling(s, GAINCEILING_64X);
    else if (acl == "128X")
        res = s->set_gainceiling(s, GAINCEILING_128X);

    // Gamma (GMA) function is to compensate for the non-linear characteristics of
    // the sensor. Raw gamma compensates the image in the RAW domain.
    res = s->set_raw_gma(s, cameraSetup["gma"].as<AutoConnectCheckbox>().checked ? 1 : 0);

    // Defect pixel cancellation, Black pixel and White pixel
    res = s->set_bpc(s, cameraSetup["bpc"].as<AutoConnectCheckbox>().checked ? 1 : 0);
    res = s->set_wpc(s, cameraSetup["wpc"].as<AutoConnectCheckbox>().checked ? 1 : 0);
    // Lense correction, According to the area where each pixel is located,
    // the module calculates a gain for the pixel, correcting each pixel with its
    // gain calculated to compensate for the light distribution due to lens curvature.
    res = s->set_lenc(s, cameraSetup["lec"].as<AutoConnectCheckbox>().checked ? 1 : 0);

    // Mirror and Flip
    res = s->set_hmirror(s, cameraSetup["hmi"].as<AutoConnectCheckbox>().checked ? 1 : 0);
    res = s->set_vflip(s, cameraSetup["vfl"].as<AutoConnectCheckbox>().checked ? 1 : 0);

    // Reducing image frame UXGA to XGA
    res = s->set_dcw(s, cameraSetup["dcw"].as<AutoConnectCheckbox>().checked ? 1 : 0);

    // Reflecting the setting values on the sensor
    if (res < 0)
        Serial.println("Failed to set camera sensor");

    // save current config
    updateConfigFS = true;

    // Sends a redirect to forward to the root page displaying the streaming from
    // the camera.
    aux.redirect("/config");

    return String();
}

String postUpload(AutoConnectAux& aux, PageArgument& args) {
    String  content;
    // Explicitly cast to the desired element to correctly extract
    // the element using the operator [].
    AutoConnectFile&  upload = auxUpload["upload_file"].as<AutoConnectFile>();
    AutoConnectText&  aux_filename = aux["filename"].as<AutoConnectText>();
    AutoConnectText&  aux_size = aux["size"].as<AutoConnectText>();
    AutoConnectText&  aux_contentType = aux["content_type"].as<AutoConnectText>();
    // Assignment operator can be used for the element attribute.
    aux_filename.value = upload.value;
    Serial.printf("uploaded file saved as %s\n", aux_filename.value.c_str());
    aux_size.value = String(upload.size);
    aux_contentType.value = upload.mimeType;

    // Include the uploaded content in the object tag to provide feedback
    // in case of success.
    String  uploadFileName = String("/") + aux_filename.value;
    if (SPIFFS.exists(uploadFileName.c_str()))
        auxBrowse["object"].value = String("<object data=\"") + uploadFileName + String("\"></object>");
    else
        auxBrowse["object"].value = "Not saved";
    return String();
}

void _handleFileRead(void) {
    const String& filePath = server.uri();
    if (SPIFFS.exists(filePath.c_str())) {
        File  uploadedFile = SPIFFS.open(filePath, "r");
        String  mime = getContentType(filePath);
        server.streamFile(uploadedFile, mime);
        uploadedFile.close();
    }
}

void handleNotFound() {
    String message = "File Not Found\n\n";
    message += "URI: ";
    message += server.uri();
    message += "\nMethod: ";
    message += (server.method() == HTTP_GET) ? "GET" : "POST";
    message += "\nArguments: ";
    message += server.args();
    message += "\n";
    for (uint8_t i = 0; i < server.args(); i++) {
        message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
    }
    server.send(404, "text/plain", message);
}

//#################################
//---------------RTSP--------------
//#################################
void handleRTSP() {
    uint32_t msecPerFrame = 100;
    static uint32_t lastimage = millis();

    // If we have an active client connection, just service that until gone
    // (FIXME - support multiple simultaneous clients)
    if (session) {
        session->handleRequests(0);  // we don't use a timeout here,
        // instead we send only if we have new enough frames
        uint32_t now = millis();
        if ((now > (lastimage + msecPerFrame)) || (now < lastimage)) {  // handle clock rollover
        session->broadcastCurrentFrame(now);
        lastimage = now;

        // check if we are overrunning our max frame rate
        now = millis();
        if (now > lastimage + msecPerFrame)
            printf("warning exceeding max frame rate of %d ms\n", now - lastimage);
        }

        if (session->m_stopped) {
        delete session;
        delete streamer;
        session = NULL;
        streamer = NULL;
        }
    } else {
        client = rtspServer.accept();
        
        if (client) {
        //streamer = new SimStreamer(&client, true);             // our streamer for UDP/TCP based RTP transport
        streamer = new OV2640Streamer(&client, cam);  // our streamer for UDP/TCP based RTP transport
        
        session = new CRtspSession(&client, streamer);  // our threads RTSP session and state
        }
    }
}

//#################################
//--------lightSystemHandle--------
//#################################
void clearLight() { //clear all displayed
    analogWrite(R_PIN, 255);
    analogWrite(Y_PIN, 255);
    analogWrite(G_PIN, 255);
    matrix.fillScreen(0);
    matrix.show();
    rState = yState = false;
}

void updateLight() {    //set update led with current light
    if (light == R) {
        analogWrite(R_PIN, 255-_litBri);
        analogWrite(Y_PIN, 255);
    } else if (light == Y) {
        analogWrite(Y_PIN, 255-_litBri);
        analogWrite(G_PIN, 255);
    } else if (light == G) {
        analogWrite(G_PIN, 255-_litBri);
        analogWrite(R_PIN, 255);
    }
    _light = light; //update current light
}

void setLightRed() {    //force set light to red w/o countdown
    light = R;
    matrix.fillScreen(0);
    matrix.setTextColor(colors[0]);
    matrix.setCursor(1, 6);
    matrix.print("--");
    matrix.show();
    updateLight();
}

void setLightGreen() {  //force set light to green w/o countdown
    light = G;
    matrix.fillScreen(0);
    matrix.setTextColor(colors[2]);
    matrix.setCursor(1, 6);
    matrix.print("--");
    matrix.show();
    updateLight();
}

void setLightOff() {
    clearLight();
    light = OFF;
}

void rTick() {
  bool state = rState;
  if (!state) { //on
    matrix.fillScreen(0);
    matrix.setTextColor(colors[0]);
    matrix.setCursor(1, 6);
    matrix.print("--");
    matrix.show();
    analogWrite(R_PIN, 255-_litBri);
    rState = !rState;
  } else {  //off
    matrix.fillScreen(0);
    matrix.show();
    analogWrite(R_PIN, 255);
    rState = !rState;
  }
}
void yTick() {
 bool state = yState;
  if (!state) { //on
    matrix.fillScreen(0);
    matrix.setTextColor(colors[1]);
    matrix.setCursor(1, 6);
    matrix.print("--");
    matrix.show();
    analogWrite(Y_PIN, 255-_litBri);
    yState = !yState;
  } else {  //off
    matrix.fillScreen(0);
    matrix.show();
    analogWrite(Y_PIN, 255);
    yState = !yState;
  }
}
void gTick() {
  bool state = gState;
  if (!state) { //on
    matrix.fillScreen(0);
    matrix.setTextColor(colors[2]);
    matrix.setCursor(1, 6);
    matrix.print("--");
    matrix.show();
    analogWrite(G_PIN, 255-_litBri);
    gState = !gState;
  } else {  //off
    matrix.fillScreen(0);
    analogWrite(G_PIN, 255);
    gState = !gState;
  }
}

void calLightTiming() { //light timing calculation
    uint8_t id = nodeid.toInt();
    q = 0;
    while (id != seq[q]) {  //find queue number with current id
        q++;
    }

    gTiming = timing[id];

    if (start) {    //calculate only queue before queue number
        if (q > 0) rTiming = (SWITCH_OFFSET * q) + (yTiming * q);
        else rTiming = 3;
        for (uint8_t i=0; i < q; i++) {
            rTiming += timing[seq[i]];
        }
    } else {    //calculate all queue
        rTiming = (SWITCH_OFFSET * (maxSeq - 1)) + (yTiming * (maxSeq - 1));
        for (int8_t i = 0; i < maxSeq; i++) {
            rTiming += timing[i];
        }
        rTiming -= timing[id];
        if (rTiming > 99) rTiming = 99; //only 2 digits cause 8*8 matrix display limitation
        timingUpdate = false;
    }
    #if defined(printdebug)
    Serial.printf("queue: %d, gTiming: %d, rTiming: %d\n", q, gTiming, rTiming);
    #endif
}

void handleLight() {
    if (start) {  //run somethings at start once
        clearLight();
        light = R;
        mode = AUTO;
    }

    if (_mode != mode) {    //if mode change clear displayed light
        clearLight();
    }

    //light mode condition
    if (mode == AUTO) { //if mode is auto
        if (_mode != mode) {
            light = R;
            next = true;
            start = timingUpdate = true;
        }
    } else if (mode == R_BLK) { //if mode is red blink
        if (_mode != mode) {
            light = R;
        }
        if ((millis() - lastTick) >= blkTiming) {
            lastTick = millis();
            rTick();
        }
    } else if (mode == Y_BLK) { //if mode is yellow blink
        if (_mode != mode) {
            light = Y;
        }
        if ((millis() - lastTick) >= blkTiming) {
            lastTick = millis();
            yTick();
        }
    } else if (mode == MAN) {   //if mode is manual
        if (cmdUpdate) {
            if (gId == nodeid.toInt()) {
                setLightGreen();
            } else if (_light == G) {
                next = true;
            }
            cmdUpdate = false;
        }
    } else if (mode == ALL_RED) {   //if mode is all red
        if (_mode != mode) {
            if (light == G) {
                next = true;
            } else {
                setLightRed();
            }
        }
    }

    //countdown end
    if (next) {
        if(timingUpdate || start) calLightTiming();  //if data updated do timing calculation
        if (!start && (mode == AUTO || mode == MAN || mode == ALL_RED )) { //rotate to next light
            if (_light == R) {
                light = G;
            } else if (_light == Y) {
                light = R;
            } else if (_light == G) {
                light = Y;
            }
        } 

        if (mode == AUTO || mode == MAN || mode == ALL_RED) {
            if (light == R) {
                count = rTiming;
                color_num = 0;
            } else if (light == Y) {
                count = yTiming;
                color_num = 1;
            } else if (light == G) {
                count = gTiming;
                color_num = 2;
            }
        }
        next = false;
    }

    //counter, matrix countdown
    if (!next && ((millis() - last_count) >= 1000)) {
        last_count = millis();
        
        if ((mode == AUTO) || ((mode == MAN || mode == ALL_RED) && light == Y)) {
            updateLight();
            matrix.fillScreen(0);
            matrix.setTextColor(colors[color_num]);
            if(count > 9) matrix.setCursor(1, 6);
            else matrix.setCursor(4, 6);
            matrix.print(count);
            matrix.show();
            count--;
        }
        if (count == 0)
            next = true;

        if ((mode == ALL_RED || mode == MAN) && light == R) {
            setLightRed();
        }
        #if defined(printdebug)
        Serial.printf("litBri: %d, matBri: %d, count: %d, next: %d, light: %s, mode: %s, rTiming: %d, yTiming: %d, gTiming: %d\n", litBri, matBri, count+1, next, lightStatus[light], opMode[mode], rTiming, yTiming, gTiming);
        #endif
    }
  
    //brightness handler
    if (((litBri != _litBri) || (matBri != _matBri)) && ((millis() - lastAdj) >= fadeVal)) {
        lastAdj = millis();
        //light brightness (invert value - common anode)
        if (_litBri < litBri) { //increase brightness
            _litBri++;
        } else if (_litBri > litBri) {  //decrease brightness
            _litBri--;
        }
        if (rState || yState || mode == AUTO || mode == MAN || mode == ALL_RED) {
            if (light == R) analogWrite(R_PIN, 255-_litBri);
            else if (light == Y) analogWrite(Y_PIN, 255-_litBri);
            else if (light == G) analogWrite(G_PIN, 255-_litBri);
        }

        //matrix brightness
        if (_matBri < matBri) { //increase brightness
            _matBri++;
        } else if (_matBri > matBri) {  //decrease brightness
            _matBri--;
        }
        matrix.setBrightness(_matBri);
        matrix.show();
    }
    
    start = false;
    _mode = mode;   //update current mode
}

//#################################
//-----------json process----------
//#################################
bool processJson(char* message) {
    const size_t bufferSize = JSON_OBJECT_SIZE(4) + JSON_OBJECT_SIZE(4) + JSON_OBJECT_SIZE(4) + 5;
    DynamicJsonDocument jsonBuffer(bufferSize);
    DeserializationError error = deserializeJson(jsonBuffer, message);
    if (error) {
        Serial.print("parseObject() failed: ");
        Serial.println(error.c_str());
        return false;
    }
    //Serial.println("JSON ParseObject() done!");
    JsonObject root = jsonBuffer.as<JsonObject>();

    if (root.containsKey("mode")) {
        mode = root["mode"];
    }
    if (root.containsKey("id")) {
        gId = root["id"];
        cmdUpdate = true;
    }
    if (root.containsKey("blkTiming")) {
        blkTiming = root["blkTiming"];
        updateConfigFS = true;
    }
    if (root.containsKey("litBri")) {
        litBri = root["litBri"];
        updateConfigFS = true;
    }
    if (root.containsKey("matBri")) {
        matBri = root["matBri"];
        updateConfigFS = true;
    }
    if (root.containsKey("sequence")) {
        JsonArray _seq = root["sequence"];
        for (uint8_t i=0; i<maxSeq; i++) {
            seq[i] = _seq[i];
        }
        //memcpy(seq, &_seq, maxSeq);
        updateConfigFS = true;
        timingUpdate = true;
    }
    if (root.containsKey("timing")) {
        JsonArray _timing = root["timing"];
        for (uint8_t i=0; i<maxSeq; i++) {
            if (_timing[i] > 99) timing[i] = 99;
            else timing[i] = _timing[i];
        }
        //memcpy(timing, &_timing, maxSeq);
        updateConfigFS = true;
        timingUpdate = true;
    }
    
    jsonBuffer.clear();
    //sendState();
    return true;
}

//#################################
//----------MQTT callback----------
//#################################
void checkpayload(uint8_t * payload, bool mqtt = false, uint8_t num = 0) {
  if (payload[0] == '#') {
    mqttClient.publish(mqtt_outtopic.c_str(), String(String("OK ") + String((char *)payload)).c_str());
  }
}

void mqtt_callback(char* topic, byte* payload_in, unsigned int length) {
  uint8_t * payload = (uint8_t *)malloc(length + 1);
  memcpy(payload, payload_in, length);
  payload[length] = NULL;
  Serial.printf("MQTT: Message arrived [%s]  from topic: [%s]\n", payload, topic);
  if (strcmp(topic, mqtt_intopic.c_str()) == 0) {
    //mqtt_cmd = true;
    if (!processJson((char*)payload)) {
      return;
    }
  } else if (strcmp(topic, (char *)mqtt_intopic.c_str()) == 0) {
    checkpayload(payload, true);
    free(payload);
  }
}

void mqttReconnect() {
    // Loop until we're reconnected
    if (mqtt_reconnect_retries < MQTT_MAX_RECONNECT_TRIES) {
        mqtt_reconnect_retries++;
        Serial.printf("Attempting MQTT connection %d / %d ...\n", mqtt_reconnect_retries, MQTT_MAX_RECONNECT_TRIES);
        // Attempt to connect
        if (mqttClient.connect(mqtt_clientid, mqtt_user, mqtt_pass)) {
        Serial.println("MQTT connected!");
        // Once connected, publish an announcement...
        char* message = new char[25 + strlen(nickname.c_str()) + 1];
        strcpy(message, "Device's ready: ");
        strcat(message, nickname.c_str());
        mqttClient.publish(mqtt_outtopic.c_str(), message);
        // ... and resubscribe
        mqttClient.subscribe(mqtt_intopic.c_str(), _mqtt_qos.toInt());
        Serial.printf("MQTT topic in: %s\n", mqtt_intopic);
        Serial.printf("MQTT topic out: %s\n", mqtt_outtopic);
        mqtt_reconnect_retries = 0;
        } else {
        if (mqtt_reconnect_retries != MQTT_MAX_RECONNECT_TRIES) {
            Serial.print("failed, rc=");
            Serial.print(mqttClient.state());
            Serial.println(" try again in 5 seconds");
            // Wait 5 seconds before retrying
            delay(5000);
        } else {
            Serial.print("failed, rc=");
            Serial.println(mqttClient.state());
            Serial.printf("MQTT connection aborted, giving up after %d tries, sleep for %d minute(s) then try again.\n", mqtt_reconnect_retries, giveup_delay);
            mqtt_conn_abort = true;
            mqtt_conn_abort_time = millis();
            ticker.attach(3, tick);
        }
        }
    }
}

//#################################
//----------config handle----------
//#################################
bool writeConfigFS() {
    updateFS = true;
    //save the strip state to FS JSON
    Serial.print("Saving cfg: ");
    const size_t capacity = JSON_OBJECT_SIZE(5) + JSON_OBJECT_SIZE(2) + JSON_OBJECT_SIZE(6) + JSON_OBJECT_SIZE(22) + JSON_OBJECT_SIZE(6) + JSON_OBJECT_SIZE(4) + JSON_OBJECT_SIZE(4) + 300;
    DynamicJsonDocument jsonBuffer(capacity);
    JsonObject json = jsonBuffer.to<JsonObject>();
    JsonObject node = json.createNestedObject("node");
    JsonObject cam = json.createNestedObject("cam");
    JsonObject mqtt = json.createNestedObject("mqtt");
    JsonObject light = json.createNestedObject("light");

    //node config
    node["name"] = nickname;
    node["id"] = nodeid;

    //mqtt config
    mqtt["host"] = _mqtt_host;
    mqtt["port"] = _mqtt_port;
    mqtt["user"] = _mqtt_user;
    mqtt["pass"] = _mqtt_pass;
    mqtt["cmd_topic"] = mqtt_intopic;
    mqtt["qos"] = _mqtt_qos;
    
    //cam config
    sensor_t *camCfg = esp_camera_sensor_get();
    cam["res"] = camCfg->status.framesize; //frame size
    cam["qua"] = camCfg->status.quality; //quality
    cam["con"] = camCfg->status.contrast; //contrast
    cam["bri"] = camCfg->status.brightness; //brightness
    cam["sat"] = camCfg->status.saturation; //saturation
    cam["se"] = camCfg->status.special_effect; //effect
    cam["awb"] = camCfg->status.awb; //whitebalance
    cam["awg"] = camCfg->status.awb_gain; //whitebalance gain
    cam["wbm"] = camCfg->status.wb_mode; //whitebalance mode
    cam["exp"] = camCfg->status.aec; //exposure control
    cam["dsp"] = camCfg->status.aec2; //dsp
    cam["ael"] = camCfg->status.ae_level; //ae level
    cam["aec"] = camCfg->status.aec_value; //aec
    cam["agc"] = camCfg->status.agc; //gain control
    cam["agv"] = camCfg->status.agc_gain; //agc gain
    cam["acl"] = camCfg->status.gainceiling; //gain ceiling
    cam["gma"] = camCfg->status.raw_gma; //gamma
    cam["bpc"] = camCfg->status.bpc; //black pixel
    cam["wpc"] = camCfg->status.wpc; //white pixel
    cam["lec"] = camCfg->status.lenc; //lense correction
    cam["hmi"] = camCfg->status.hmirror; //h-mirror
    cam["vfl"] = camCfg->status.vflip; //v-flip
    cam["dcw"] = camCfg->status.dcw; //reducing image frame UXGA to XGA

    //last light config
    light["mode"] = mode;
    light["blkTiming"] = blkTiming;
    light["litBri"] = litBri;
    light["matBri"] = matBri;
    JsonArray sequence = light.createNestedArray("sequence");
    for (uint8_t i; i<maxSeq; i++) {
        sequence.add(seq[i]);
    }
    JsonArray time = light.createNestedArray("time");
    for (uint8_t i; i<maxSeq; i++) {
        time.add(timing[i]);
    }

    //SPIFFS.remove("/config.json") ? Serial.println("removed file") : Serial.println("failed removing file");
    File configFile = SPIFFS.open("/config.json", "w");
    if (!configFile) {
        Serial.println("Failed!");
        updateFS = false;
        updateConfigFS = false;
        return false;
    }
    serializeJson(json, Serial);
    serializeJson(json, configFile);
    configFile.close();
    updateFS = false;
    updateConfigFS = false;
    return true;
    //end save
}

bool readConfigFS() {
    //read settings from FS JSON
    updateFS = true;
    //if (resetsettings) { SPIFFS.begin(); SPIFFS.remove("/config.json"); SPIFFS.format(); delay(1000);}
    if (SPIFFS.exists("/config.json")) {
        //file exists, reading and loading
        Serial.print("Read cfg: ");
        File configFile = SPIFFS.open("/config.json", "r");
        if (configFile) {
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);
        configFile.readBytes(buf.get(), size);
        DynamicJsonDocument jsonBuffer(JSON_OBJECT_SIZE(5) + JSON_OBJECT_SIZE(2) + JSON_OBJECT_SIZE(6) + JSON_OBJECT_SIZE(22) + JSON_OBJECT_SIZE(6) + JSON_OBJECT_SIZE(4) + JSON_OBJECT_SIZE(4) + 300);
        DeserializationError error = deserializeJson(jsonBuffer, buf.get());
        if (!error) {
            JsonObject json = jsonBuffer.as<JsonObject>();
            serializeJson(json, Serial);
            //node config
            nickname = json["node"]["name"].as<String>();
            nodeid = json["node"]["id"].as<String>();

            //mqtt config
            _mqtt_host = json["mqtt"]["host"].as<String>();
            _mqtt_port = json["mqtt"]["port"].as<String>();
            _mqtt_user = json["mqtt"]["user"].as<String>();
            _mqtt_pass = json["mqtt"]["pass"].as<String>();
            mqtt_intopic = json["mqtt"]["cmd_topic"].as<String>();
            _mqtt_qos = json["mqtt"]["qos"].as<String>();

            //cam config
            sensor_t *s = esp_camera_sensor_get();
            int8_t res = 0;
            res = s->set_framesize(s, json["cam"]["res"]);
            res = s->set_quality(s, json["cam"]["qua"]);
            res = s->set_contrast(s, json["cam"]["con"]);
            res = s->set_brightness(s, json["cam"]["bri"]);
            res = s->set_saturation(s, json["cam"]["sat"]);
            res = s->set_special_effect(s, json["cam"]["se"]);
            res = s->set_whitebal(s, json["cam"]["awb"]);
            res = s->set_awb_gain(s, json["cam"]["wbg"]);
            res = s->set_wb_mode(s, json["cam"]["wbm"]);
            res = s->set_exposure_ctrl(s, json["cam"]["exp"]);
            res = s->set_aec2(s, json["cam"]["dsp"]);
            res = s->set_ae_level(s, json["cam"]["ael"]);
            res = s->set_aec_value(s, json["cam"]["aec"]);
            res = s->set_gain_ctrl(s, json["cam"]["agc"]);
            res = s->set_agc_gain(s, json["cam"]["agv"]);
            res = s->set_gainceiling(s, json["cam"]["acl"]);
            res = s->set_raw_gma(s, json["cam"]["gma"]);
            res = s->set_bpc(s, json["cam"]["bpc"]);
            res = s->set_wpc(s, json["cam"]["wpc"]);
            res = s->set_lenc(s, json["cam"]["lec"]);
            res = s->set_hmirror(s, json["cam"]["hmi"]);
            res = s->set_vflip(s, json["cam"]["vfl"]);
            res = s->set_dcw(s, json["cam"]["dcw"]);
            if (res < 0)
                Serial.println("Failed to set camera sensor");
            
            //last light config
            mode = json["light"]["mode"];
            blkTiming = json["light"]["blkTiming"];
            litBri = json["light"]["litBri"];
            matBri = json["light"]["matBri"];
            JsonArray _seq = json["light"]["sequence"];
            for (uint8_t i=0; i<maxSeq; i++) {
                seq[i] = _seq[i];
            }
            JsonArray _timing = json["light"]["time"];
            for (uint8_t i=0; i<maxSeq; i++) {
                timing[i] = _timing[i];
            }

            updateFS = false;
            return true;
        } else {
            Serial.print("Failed to parse JSON!");
        }
        } else {
        Serial.print("Failed to open \"/config.json\"");
        }
    } else {
        Serial.print("Couldn't find \"/config.json\"");
    }
    //end read
    updateFS = false;
    return false;
}
