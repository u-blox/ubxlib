/*
 * Copyright 2019-2023 u-blox
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

#ifndef _U_WIFI_HTTP_H_
#define _U_WIFI_HTTP_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

/** \addtogroup _wifi
 *  @{
 */

/** @file
 * @brief This header file defines the HTTP client API for Wi-Fi
 * modules.  This functions are thread-safe with the exception of
 * uWifiHttpClose(), which should not be called while any of the other
 * uWifiHttp functions may be running.  However, note that the
 * HTTP request/response behaviour of the underlying Wi-FI module
 * is "one-in-one-out", i.e. you must wait for a response to an HTTP
 * request to arrive before sending another HTTP request; if you want
 * this to be handled automagically then you're better off using the
 * common uHttpClient API.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

#ifndef U_WIFI_HTTP_DATA_MAX_LENGTH_BYTES
/** The maximum amount of data that can be sent in a
 * uWifiHttpRequest().
 */
# define U_WIFI_HTTP_DATA_MAX_LENGTH_BYTES 450
#endif

#ifndef U_WIFI_HTTP_CONTENT_TYPE_MAX_LENGTH_BYTES
/** The maximum length of the content-type string.
 */
# define U_WIFI_HTTP_CONTENT_TYPE_MAX_LENGTH_BYTES 50
#endif

#ifndef U_WIFI_HTTP_BLOB_MAX_LENGTH_BYTES
/** The maximum length of the binary blob.
 */
# define U_WIFI_HTTP_BLOB_MAX_LENGTH_BYTES 2000
#endif

#ifndef U_WIFI_HTTP_PATH_MAX_LENGTH_BYTES
/** The maximum length of path that can be sent in a
 * uWifiHttpRequest();
 */
# define U_WIFI_HTTP_PATH_MAX_LENGTH_BYTES 30
#endif

#ifndef U_WIFI_HTTP_TIMEOUT_SECONDS_MIN
/** The minimum HTTP timeout value permitted, in seconds.
 */
# define U_WIFI_HTTP_TIMEOUT_SECONDS_MIN 30
#endif

#ifndef U_HTTP_CLIENT_WIFI_CHUNK_LENGTH
/** The maximum length of data to read or write from/to a file
 * (i.e. in the Wi-Fi case) at any one time; if you have
 * a really reliable UART link with solid handshaking you
 * can probably increase this.
 */
# define U_HTTP_CLIENT_WIFI_CHUNK_LENGTH 312
#endif

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/** The types of HTTP request that can be performed.
 */
typedef enum {
    U_WIFI_HTTP_REQUEST_GET = 0,
    U_WIFI_HTTP_REQUEST_POST = 1,
    U_WIFI_HTTP_REQUEST_PUT = 2,
    U_WIFI_HTTP_REQUEST_PATCH = 3,
    U_WIFI_HTTP_REQUEST_DELETE = 4,
    U_WIFI_HTTP_REQUEST_OPTIONS = 9,
    U_WIFI_HTTP_REQUEST_GET_BINARY = 15,
    U_WIFI_HTTP_REQUEST_MAX_NUM
} uWifiHttpRequest_t;

typedef void (uWifiHttpCallback_t) (uDeviceHandle_t wifiHandle,
                                    int32_t httpHandle,
                                    bool error,
                                    void *pCallbackParam);

/** Private context structures for HTTP, WiFi-flavour.
 * The contents of this structure may be changed without
 * notice at any time; it is only placed here so that the
 * uHttpClient code may use it, please do not refer to it
 * in your application code.
 */
typedef struct {
    int32_t httpHandle;
    int32_t replyOffset;
    bool binary;
    bool atPrintWasOn;
} uHttpClientContextWifi_t;

/* ----------------------------------------------------------------
 * FUNCTIONS
 * -------------------------------------------------------------- */

/** Open a wifi HTTP client instance.
 *
 * @param wifiHandle         the handle of the Wi-Fi instance to
 *                           be used.
 * @param[in] pServerName    the null-terminated name of the HTTP
 *                           server; may be a domain name or an IP
 *                           address and may include a port number,
 *                           for example "u-blox.net:83".
 *                           NOTE: if a domain name is used the module
 *                           may immediately try to perform a DNS
 *                           look-up to establish the IP address of
 *                           the HTTP server and hence you should
 *                           ensure that the module is connected
 *                           beforehand.  Cannot be NULL.
 * @param[in] pUserName      the null-terminated user name, if
 *                           required by the HTTP server (use NULL
 *                           if not).
 * @param[in] pPassword      the null-terminated password, if one is
 *                           required to go with the user name for
 *                           the HTTP server (use NULL if not);
 *                           must be NULL if pUserName is NULL.
 * @param timeoutSeconds     the timeout in seconds when waiting
 *                           for a response from the HTTP server,
 *                           must be at least
 *                           #U_WIFI_HTTP_TIMEOUT_SECONDS_MIN.
 * @param[in] pCallback      a callback to be called when a HTTP response
 *                           has been received (which may indicate
 *                           an error, for example "404 Not Found") or
 *                           an error has occurred.
 * @param[in] pCallbackParam a parameter that will be passed to
 *                           pCallback when it is called; MUST be
 *                           a valid pointer to #uHttpClientContext_t.
 * @return                   the handle of the HTTP instance on success,
 *                           else negative error code.
 */
int32_t uWifiHttpOpen(uDeviceHandle_t wifiHandle, const char *pServerName,
                      const char *pUserName, const char *pPassword,
                      int32_t timeoutSeconds, uWifiHttpCallback_t *pCallback,
                      void *pCallbackParam);

/** Shut-down the given Wi-FI HTTP client instance.  This function
 * should not be called while any of the other HTTP functions may be
 * running.
 *
 * @param wifiHandle    the handle of the Wi-Fi instance.
 * @param httpHandle    the handle of the HTTP instance (as returned by
 *                      uWifiHttpOpen()) to close.
 */
void uWifiHttpClose(uDeviceHandle_t wifiHandle, int32_t httpHandle);

/** Perform an HTTP request. Primary for GET and DELETE.
 * This function will block while the request is being sent.
 *
 * IMPORTANT: you MUST wait for the function to return before issuing your next
 * HTTP request.
 *
 * If you are going to perform large PUT/POST/GET requests (e.g. more than 1024
 * bytes) then you should ensure that you have flow control on the UART interface
 * to the Wi-Fi module or you might experience data loss.
 *
 * @param wifiHandle              the handle of the Wi-Fi instance to be used.
 * @param httpHandle              the handle of the HTTP instance, as returned by
 *                                uWifiHttpOpen().
 * @param requestType             the request type to perform.
 * @param[in] pPath               the null-terminated path on the HTTP server
 *                                to perform the request on, for example
 *                                "/thing/form.html"; cannot be NULL.
 * @param[in] pContent            the byte-array to send. Cannot be more than
 *                                #U_WIFI_HTTP_DATA_MAX_LENGTH_BYTES.
 * @param[in] pContentType        the null-terminated content type, for example
 *                                "application/text"; cannot be more than
 *                                #U_WIFI_HTTP_CONTENT_TYPE_MAX_LENGTH_BYTES.
 * @return                        zero if the request has been successfully sent,
 *                                else negative error code.
 */
int32_t uWifiHttpRequest(uDeviceHandle_t wifiHandle, int32_t httpHandle,
                         uWifiHttpRequest_t requestType, const char *pPath,
                         const char *pContent, const char *pContentType);

/** Perform an extended HTTP request. Primary PUT/POST/PATCH/OPTIONS/GET_BINARY
 * This function will block while the request is being sent.
 *
 * IMPORTANT: you MUST wait for the function to return before issuing your next
 * HTTP request.
 *
 * If you are going to perform large PUT/POST/GET requests (e.g. more than 1024
 * bytes) then you should ensure that you have flow control on the UART interface
 * to the Wi-Fi module or you might experience data loss.
 *
 * @param wifiHandle              the handle of the Wi-Fi instance to be used.
 * @param httpHandle              the handle of the HTTP instance, as returned by
 *                                uWifiHttpOpen().
 * @param requestType             the request type to perform.
 * @param[in] pPath               the null-terminated path on the HTTP server
 *                                to perform the request on, for example
 *                                "/thing/form.html"; cannot be NULL.
 * @param[in] pData               the binary blob to send.
 * @param[in] contentLength       length of the blob, in bytes; cannot be more
 *                                than #U_WIFI_HTTP_DATA_MAX_LENGTH_BYTES.
 * @param[in] pContentType        the null-terminated content type, for example
 *                                "application/text"; cannot be more than
 *                                #U_WIFI_HTTP_CONTENT_TYPE_MAX_LENGTH_BYTES.
 * @return                        zero if the request has been successfully sent,
 *                                else negative error code.
 */
int32_t uWifiHttpRequestEx(uDeviceHandle_t wifiHandle, int32_t httpHandle,
                           uWifiHttpRequest_t requestType, const char *pPath,
                           const char *pData, size_t contentLength, const char *pContentType);

/** Get the last HTTP error code.
 *
 * @param wifiHandle     the handle of the Wi-Fi instance to be used.
 * @param httpHandle     the handle of the HTTP instance, as returned by
 *                       uWifiHttpOpen().
 * @return               an error code, the meaning of which is utterly
 *                       module specific.
 */
int32_t uWifiHttpGetLastErrorCode(uDeviceHandle_t wifiHandle,
                                  int32_t httpHandle);

#ifdef __cplusplus
}
#endif

/** @}*/

#endif // _U_WIFI_HTTP_H_

// End of file
