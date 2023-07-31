/*
 * Copyright 2023 u-blox
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
 * Ubxlib WiFi captive portal test program peer.
 * This program will constantly try to connect to a captive portal
 * with a specified name. Once connected it will provide the credentials
 * for the portal to connect to a specified WiFi access point.
 *
 * In order to build this program the environment makeEspArduino is
 * needed. Available from here: https://github.com/plerup/makeEspArduino
 *
 */

#include <Arduino.h>
#include <FS.h>
#include <HTTPClient.h>
#include <LittleFS.h>
#include <WiFi.h>
#include <buildinfo.h>

#define PROG_NAME "Ubxlib captive portal test"
#define ROOT_URL "http://ubxlib-thingy.com"  // Could be anything

#define LOG(form, ...) Serial.printf(form "\n", ##__VA_ARGS__)
#define LOG_ERR(form, ...) Serial.printf("* " form "\n", ##__VA_ARGS__)
#define SHOW_WAIT LOG("\nWaiting for captive portal: %s ...", gPortalName.c_str())
#define STARTS_WITH(s, m) (strcasestr(s, m) == s)

// Global settings are stored in a file system. LittleFS is used for this.
#define _FS LittleFS
#define CRED_FILE_NAME "/cred"
#define PORT_NAME_FILE_NAME "/port_name"

const char HELP[] PROGMEM = R"===(
Available commands:
  cred:<SSID> <Password>
    Set the credentials to be sent to the captive portal
  port_name:<PortalName>
    Set SSID of the captive portal to connect to
  factory
    Remove previous set credentials and portal SSID
  restart
    Restart this module
)===";

// Global settings (credentials and target portal name)
static char gSSID[33];
static char gPW[65];
static String gPortalName;

String file_to_string(String file_name) {
  String res = "";
  File f = _FS.open(file_name.c_str(), "r");
  if (f) {
    res.reserve(f.size());
    while (f.available()) {
      res += (char)f.read();
    }
    f.close();
  }
  return res;
}

//------------------------------------------------------

bool string_to_file(String file_name, const char *str) {
  File f = _FS.open(file_name.c_str(), "w");
  bool ok = f;
  if (ok) {
    size_t len = strlen(str);
    ok = f.write((uint8_t *)str, len) == len;
    f.close();
  }
  return ok;
}

//------------------------------------------------------

bool set_cred(const char *cred_str) {
  // Parse and set credentials
  bool ok = false;
  char *pPW = strrchr(cred_str, ' ');
  if (pPW) {
    pPW++;
    size_t len = strchr(cred_str, ' ') - cred_str;
    if (len > 0 && len < sizeof(gSSID) - 1) {
      strncpy(gSSID, cred_str, len);
      gSSID[len] = 0;
      strlcpy(gPW, pPW, sizeof(gPW));
      ok = true;
    }
  }
  return ok;
}

//------------------------------------------------------

void handle_input() {
  // The input uart can be used for sending commands which
  // change settings and trigger operations.
  static char input[100];
  static size_t pos = 0;
  while (Serial.available()) {
    char ch = Serial.read();
    Serial.write(ch);
    if (ch == '\n') {
      // Complete command received
      bool ok = false;
      bool restart = false;
      input[pos] = 0;
      if (STARTS_WITH(input, "cred:")) {
        char *pCred = input + 5;
        if (set_cred(pCred)) {
          if (string_to_file(CRED_FILE_NAME, pCred)) {
            LOG("Credentials updated, SSID = \"%s\"", gSSID);
            SHOW_WAIT;
          } else {
            LOG_ERR("Failed to save credentials");
          }
        } else {
          LOG_ERR("Invalid credentials");
        }
      } else if (STARTS_WITH(input, "port_name:") && strlen(input) > 10) {
        gPortalName = input + 10;
        string_to_file(PORT_NAME_FILE_NAME, gPortalName.c_str());
        WiFi.begin(gPortalName.c_str(), NULL);
        SHOW_WAIT;
      } else if (STARTS_WITH(input, "factory")) {
        _FS.remove(PORT_NAME_FILE_NAME);
        _FS.remove(CRED_FILE_NAME);
        restart = true;
      } else if (STARTS_WITH(input, "restart")) {
        restart = true;
      } else if (STARTS_WITH(input, "help") || input[0] == 0) {
        LOG("%s", HELP);
      } else {
        LOG_ERR("Invalid input");
      }
      pos = 0;
      if (restart) {
        LOG("Restarting...");
        ESP.restart();
      }
    } else if (ch != '\r' && pos < (sizeof(input) - 1)) {
      input[pos++] = ch;
    }
  }
}

//------------------------------------------------------

void init_fs() {
  if (!_FS.begin()) {
    LOG_ERR("Failed to initiate file system. Formating...");
    _FS.format();
    ESP.restart();
  }
}

//------------------------------------------------------

void setup() {
  Serial.begin(115200);
  LOG("\n== %s, built: %s %s (%s)", PROG_NAME,
      _BuildInfo.date, _BuildInfo.time, _BuildInfo.env_version);
  init_fs();

  gPortalName = file_to_string(PORT_NAME_FILE_NAME);
  if (gPortalName.length() == 0) {
    gPortalName = "UBXLIB_TEST_PORTAL";
  }

  if (set_cred(file_to_string(CRED_FILE_NAME).c_str())) {
    LOG("Credentials are defined, SSID = %s", gSSID);
    SHOW_WAIT;
  } else {
    LOG_ERR("No credentials have been defined");
  }
  WiFi.begin(gPortalName.c_str(), NULL);
}

//------------------------------------------------------

void loop() {
  handle_input();
  if (gSSID[0] != 0 && WiFi.status() == WL_CONNECTED) {
    // Wait a while in order to get the module dns etc ready
    LOG("Connected, stabilizing...");
    delay(5000);
    HTTPClient http;
    LOG("Checking connection...");
    http.setConnectTimeout(15000);
    http.begin(ROOT_URL "/");
    int httpCode = http.GET();
    if (httpCode == HTTP_CODE_OK) {
      delay(500);
      // Make sure the target WiFi network is actually available
      LOG("Checking for SSID: %s ...", gSSID);
      String match = gSSID;
      match += " (";
      http.setURL(ROOT_URL "/get_ssid_list");
      httpCode = http.GET();
      if (httpCode == HTTP_CODE_OK & http.getString().indexOf(match) > 0) {
        LOG("Sending credentials...");
        http.setURL(ROOT_URL "/set_wifi");
        String cred = "{\"ssid\": \"";
        cred += gSSID;
        cred += "\" \"pw\":\"";
        cred += gPW;
        cred += "\"}";
        httpCode = http.POST(cred);
        LOG("Result: %d", httpCode);
      } else {
        LOG_ERR("SSID not found: %s", http.getString().c_str());
      }
    } else {
      LOG_ERR("Invalid response from captive portal: %d", httpCode);
    }
    LOG("Disconnecting...");
    WiFi.disconnect();
    delay(5000);
    WiFi.reconnect();
    SHOW_WAIT;
  } else {
    delay(100);
  }
}
