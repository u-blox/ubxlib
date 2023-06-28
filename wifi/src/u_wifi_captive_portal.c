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

/** @file
 * @brief Implementation of the WiFi captive portal.
 */

#ifdef U_CFG_OVERRIDE
#include "u_cfg_override.h"  // For a customer's configuration override
#endif

/* Only #includes of u_* and the C standard library are allowed here,
 * no platform stuff and no OS stuff.  Anything required from
 * the platform/OS must be brought in through u_port* to maintain
 * portability.
 */

#include "stddef.h"
#include "stdint.h"
#include "stdbool.h"
#include "string.h"
#include "stdio.h"
#include "limits.h"

#include "u_error_common.h"

#include "u_assert.h"

#include "u_port_os.h"
#include "u_cfg_sw.h"
#include "u_port_debug.h"
#include "u_cfg_os_platform_specific.h"

#include "u_short_range_module_type.h"
#include "u_short_range.h"
#include "u_short_range_cfg.h"

#include "u_network.h"
#include "u_sock_errno.h"
#include "u_sock.h"

#include "u_network_config_wifi.h"
#include "u_wifi_module_type.h"
#include "u_wifi.h"
#include "u_wifi_sock.h"

#include "u_dns_server.h"

#include "u_wifi_captive_portal.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * ------------------------------------------------------------- */

#define LOG_PREFIX "U_WIFI_CAPTIVE_PORTAL: "

/* ----------------------------------------------------------------
 * STATIC VARIABLES
 * ------------------------------------------------------------- */

// Html/javascript code for a web page which acts as a landing page
// for the captive portal function.
static const char gIndexPage[] =
    "<!DOCTYPE html>\r\n"
    "<html>\r\n"
    "<head>\r\n"
    "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1, user-scalable=no\"/>\r\n"
    "<title>WiFi configuration</title>\r\n"
    "<style>\r\n"
    "  body {\r\n"
    "    font-family: Verdana,Arial,Helvetica;\r\n"
    "    font-size: 11pt;\r\n"
    "    line-height: 1.5;\r\n"
    " }\r\n"
    "  button {\r\n"
    "    margin:auto;\r\n"
    "    display:block;\r\n"
    "    cursor: pointer;\r\n"
    "    margin-top:10px;\r\n"
    "  }\r\n"
    "  fieldset {\r\n"
    "    width:0px;\r\n"
    "  }\r\n"
    "  select {\r\n"
    "    margin-bottom:5px;\r\n"
    "  }\r\n"
    "</style>\r\n"
    "<script>\r\n"
    "function update_wifi() {\r\n"
    "  var e = document.getElementById(\"ssid\");\r\n"
    "  var val =\r\n"
    "    {\r\n"
    "      ssid: e.options[e.selectedIndex].text.replace(/\\s+\\(.+\\)$/, \"\"),\r\n"
    "      pw: document.getElementById(\"pw\").value,\r\n"
    "    }\r\n"
    "  var xhttp = new XMLHttpRequest();\r\n"
    "  xhttp.open(\"POST\", \"/set_wifi\", true);\r\n"
    "  xhttp.send(JSON.stringify(val));\r\n"
    "}\r\n"
    "function insert_ssid(data) {\r\n"
    "  var sel = document.getElementById(\"ssid\");\r\n"
    "  sel.innerHTML = '';\r\n"
    "  for (var ind in data['SSIDList']) {\r\n"
    "    var opt = document.createElement(\"option\");\r\n"
    "    opt.text = data['SSIDList'][ind];\r\n"
    "    if (ind == data['ssid'])\r\n"
    "      opt.selected = true;\r\n"
    "    sel.add(opt);\r\n"
    "  }\r\n"
    "}\r\n"
    "function get_ssid_list(data) {\r\n"
    "  var xhttp = new XMLHttpRequest();\r\n"
    "  xhttp.onerror = function() {get_ssid_list(data);}\r\n"
    "  xhttp.onreadystatechange = function() {\r\n"
    "    if (xhttp.readyState == 4) {\r\n"
    "       if (xhttp.status == 200)\r\n"
    "          insert_ssid(JSON.parse(xhttp.responseText));\r\n"
    "    }\r\n"
    "  };\r\n"
    "  xhttp.open(\"GET\", \"/get_ssid_list\", true);\r\n"
    "  xhttp.send();\r\n"
    "}\r\n"
    "function toggle_pwv() {\r\n"
    "  var x = document.getElementById(\"pw\");\r\n"
    "  if (x.type === \"password\") {\r\n"
    "    x.type = \"text\";\r\n"
    "  } else {\r\n"
    "    x.type = \"password\";\r\n"
    "  }\r\n"
    "  x.focus();\r\n"
    "}\r\n"
    "</script>\r\n"
    "</head>\r\n"
    "<body onload=\"get_ssid_list()\">\r\n"
    "<fieldset>\r\n"
    "  <legend>WIFI configuration</legend>\r\n"
    "  SSID: <select id=\"ssid\">\r\n"
    "  <option>Scanning networks...</option>\r\n"
    "  </select>\r\n"
    "  <br>\r\n"
    "  Password: <input type=\"password\" id=\"pw\">\r\n"
    "  <br>\r\n"
    "  <input type=\"checkbox\" onclick=\"toggle_pwv()\">Show password<br>\r\n"
    "  <button onclick=\"update_wifi()\">Set and restart</button>\r\n"
    "</fieldset>\r\n"
    "<br>\r\n"
    "</body>\r\n"
    "</html>\r\n";

// The list of available network SSIDs
static char gSsidList[1024];
static uDeviceHandle_t gDevHandle;
static bool gKeepGoing = false;
// Selected credentials
static char gSsid[U_WIFI_SSID_SIZE];
static char gPw[100] = {0};

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * ------------------------------------------------------------- */

// Add one SSID to the list
static void scanCallback(uDeviceHandle_t devHandle, uWifiScanResult_t *pResult)
{
    (void)devHandle;
    char info[U_WIFI_SSID_SIZE + 15];
    if (strlen(pResult->ssid)) {
        snprintf(info, sizeof(info), "\"%s (%d%s)\",",
                 pResult->ssid, (int)pResult->rssi,
                 pResult->authSuiteBitmask ? " *" : "");
        if ((sizeof(gSsidList) - strlen(gSsidList)) > (strlen(info) + 3)) {
            strcat(gSsidList, info);
        }
    }
}

// Send corresponding html header
static void sendHeader(int32_t sock, const char *code,
                       const char *type, size_t length)
{
    static char header[255];
    snprintf(header, sizeof(header),
             "HTTP/1.0 %s\r\n"
             "Server: ubxlib\r\n"
             "Content-type: %s\r\n"
             "Content-Length: %d\r\n"
             "Cache-Control: no-store, no-cache, must-revalidate\r\n"
             "\r\n",
             code, type, (int)length);
    uSockWrite(sock, header, strlen(header));
}

// Send the gathered SSID list
static void sendSsidList(int32_t sock)
{
    strcpy(gSsidList, "{\"SSIDList\":[");
    uWifiStationScan(gDevHandle, NULL, scanCallback);
    if (gSsidList[strlen(gSsidList) - 1] == ',') {
        gSsidList[strlen(gSsidList) - 1] = 0;
    }
    strcat(gSsidList, "]}");
    sendHeader(sock, "200 OK", "text/json", strlen(gSsidList));
    uSockWrite(sock, gSsidList, strlen(gSsidList));
}

// Get a json string value, assume quoted name and value,
// and handle escaped quotes as well.
static void getVal(const char *txt,
                   const char *name,
                   char *val,
                   size_t maxLength)
{
    char match[20];
    snprintf(match, sizeof(match), "\"%s", name);
    const char *src = strstr(txt, match);
    size_t dst = 0;
    if (src) {
        src += strlen(match);
        for (int32_t i = 0; i < 2; i++) {
            while (*src && *(src++) != '"') {
            }
        }
        while (*src && dst < maxLength - 1) {
            if ((*src == '"') || (*src == '\\' && *(src + 1) == '"')) {
                break;
            }
            val[dst++] = *(src++);
        }
        val[dst] = 0;
    }
}

// Send the user entered credentials
static void updateWifi(int32_t sock, const char *params)
{
    getVal(params, "ssid", gSsid, sizeof(gSsid));
    getVal(params, "pw", gPw, sizeof(gPw));
    sendHeader(sock, "200 OK", "text/html", 0);
    gKeepGoing = false;
}

// Handle incoming web server requests
static void handleRequest(const char *request, int32_t sock)
{
    char method[15];
    char url[25];
    size_t pos = 0;
    while ((pos < (sizeof(method) - 1)) && (*request != ' ')) {
        method[pos++] = *(request++);
    }
    method[pos] = 0;
    bool ok = *request == ' ';
    if (ok) {
        while (*request == ' ') {
            request++;
        }
        pos = 0;
        while ((pos < (sizeof(url) - 1)) && *request && (*request != ' ')) {
            url[pos++] = *(request++);
        }
        url[pos] = 0;
        uPortLog(LOG_PREFIX "Requested url \"%s\"\n", url);
        if (strcmp(method, "GET") == 0) {
            if (strstr(url, "/get_ssid_list")) {
                sendSsidList(sock);
            } else if (strstr(url, "/favicon.ico")) {
                // Chrome will request this but none available here
                ok = false;
            } else {
                // Any other request else just gets the main page
                const char *p = gIndexPage;
                size_t len = strlen(p);
                sendHeader(sock, "200 OK", "text/html", len);
                uSockWrite(sock, p, len);
            }
        } else if (strcmp(method, "POST") == 0) {
            if (strstr(url, "/set_wifi")) {
                updateWifi(sock, request);
            }
        } else {
            // Unsupported method type
            ok = false;
        }
    }
    if (!ok) {
        sendHeader(sock, "404 Not Found", "text/html", 0);
    }
}

// Callback controlling whether the DNS server should continue or not
static bool dnsKeepGoingCallback(uDeviceHandle_t deviceHandle)
{
    (void)deviceHandle;
    return gKeepGoing;
}

// The DNS server
static void dnsServerTask(void *pParameters)
{
    uDnsServer(gDevHandle, (const char *)pParameters, dnsKeepGoingCallback);
    uPortTaskDelete(NULL);
}

/* ----------------------------------------------------------------
 * FUNCTIONS
 * ------------------------------------------------------------- */

// Special for now, non exposed global for accept timeouts, see u_wifi_sock.c
extern int32_t gUWifiSocketAcceptTimeoutS;

// Captive portal main function
int32_t uWifiCaptivePortal(uDeviceHandle_t deviceHandle,
                           const char *pSsid,
                           const char *pPassword,
                           uWifiCaptivePortalKeepGoingCallback_t cb)
{
    // Wifi access point configuration
    uNetworkCfgWifi_t networkCfg = {
        .type = U_NETWORK_TYPE_WIFI,
        .mode = U_WIFI_MODE_AP,
        .apAuthentication =
        pPassword == NULL ? U_WIFI_AUTH_OPEN : U_WIFI_AUTH_WPA_PSK,
        .pApSssid = pSsid,
        .pApPassPhrase = pPassword,
        .pApIpAddress = "8.8.8.8"  // Required for Android
    };
    int32_t errorCode = 0;
    gDevHandle = deviceHandle;
    gKeepGoing = true;
    gSsid[0] = 0;
    gPw[0] = 0;
    if (pSsid != NULL) {
        // Make sure that possible auto connected station mode is disconnected
        uWifiStationDisconnect(gDevHandle);
        // Start the access point
        errorCode = uNetworkInterfaceUp(gDevHandle, U_NETWORK_TYPE_WIFI, &networkCfg);
    }
    if (errorCode == 0) {
        // Start a dns server which redirects all requests to this portal
        uPortTaskHandle_t dnsServer;
        uPortTaskCreate(dnsServerTask,
                        "dns",
                        U_WIFI_CAPTIVE_PORTAL_DNS_TASK_STACK_SIZE_BYTES,
                        (void *)networkCfg.pApIpAddress,
                        U_WIFI_CAPTIVE_PORTAL_DNS_TASK_PRIORITY,
                        &dnsServer);

        // Start the web server
        int32_t sock = uSockCreate(gDevHandle,
                                   U_SOCK_TYPE_STREAM,
                                   U_SOCK_PROTOCOL_TCP);
        if (sock >= 0) {
            uSockAddress_t remoteAddr;
            char addrStr[15];
            remoteAddr.ipAddress.address.ipv4 = 0;
            remoteAddr.port = 80;
            uSockBind(sock, &remoteAddr);
            uSockListen(sock, 1);
            gUWifiSocketAcceptTimeoutS = 2;
            uPortLog(LOG_PREFIX "\"%s\" started\n", pSsid ? pSsid : "Servers only");
            while (gKeepGoing) {
                static char request[1024];
                // Wait for connection
                int32_t clientSock = uSockAccept(sock, &remoteAddr);
                if (clientSock >= 0) {
                    uSockIpAddressToString(&(remoteAddr.ipAddress), addrStr, sizeof(addrStr));
                    uPortLog(LOG_PREFIX "Connected to: %s\n", addrStr);
                    int32_t cnt = uSockRead(clientSock, request, sizeof(request) - 1);
                    if (cnt > 0) {
                        request[cnt] = 0;
                        handleRequest(request, clientSock);
                    } else {
                        uPortLog(LOG_PREFIX "ERROR No request\n");
                    }
                    uSockClose(clientSock);
                } else if (clientSock != U_ERROR_COMMON_TIMEOUT) {
                    uPortLog(LOG_PREFIX "ERROR Accept failed: %d\n", clientSock);
                    gKeepGoing = false;
                } else if (cb) {
                    gKeepGoing = cb(gDevHandle);
                }
            }
            uSockClose(sock);
            // Close down the access point and try to connect and save the entered credentials
            uPortTaskBlock(1000);
            if (pSsid != NULL || strlen(gSsid) > 0) {
                uNetworkInterfaceDown(gDevHandle, U_NETWORK_TYPE_WIFI);
            }
            if (strlen(gSsid)) {
                uPortTaskBlock(1000);
                networkCfg.authentication = strlen(gPw) == 0 ? U_WIFI_AUTH_OPEN : U_WIFI_AUTH_WPA_PSK;
                networkCfg.pSsid = gSsid;
                networkCfg.pPassPhrase = gPw;
                networkCfg.mode = U_WIFI_MODE_STA;
                errorCode = uNetworkInterfaceUp(gDevHandle, U_NETWORK_TYPE_WIFI, &networkCfg);
                if (errorCode == 0) {
                    errorCode = uWifiStationStoreConfig(gDevHandle, false);
                }
            } else {
                errorCode = U_ERROR_COMMON_NOT_INITIALISED;
            }
        } else {
            uPortLog(LOG_PREFIX "ERROR Failed to create server socket: %d\n", sock);
            uNetworkInterfaceDown(gDevHandle, U_NETWORK_TYPE_WIFI);
            errorCode = sock;
        }
    } else {
        uPortLog(LOG_PREFIX "ERROR to start the access point: %d\n", errorCode);
    }
    return errorCode;
}