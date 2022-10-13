/*
 * Copyright 2019-2022 u-blox
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

#ifndef _U_CELL_HTTP_H_
#define _U_CELL_HTTP_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

/** \addtogroup _cell
 *  @{
 */

/** @file
 * @brief This header file defines the HTTP client API for cellular
 * modules.  This functions are thread-safe with the exception of
 * uCellHttpClose(), which should not be called while any of the other
 * uCellHttp functions may be running.  However, note that the
 * HTTP request/response behaviour of the underlying cellular module
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

#ifndef U_CELL_HTTP_POST_REQUEST_STRING_MAX_LENGTH_BYTES
/** The maximum amount of data that can be sent in a
 * uCellHttpRequest(); you must use uCellHttpRequestFile() to send
 * more data than this.
 */
# define U_CELL_HTTP_POST_REQUEST_STRING_MAX_LENGTH_BYTES 128
#endif

#ifndef U_CELL_HTTP_CONTENT_TYPE_MAX_LENGTH_BYTES
/** The maximum length of the content-type string.
 */
# define U_CELL_HTTP_CONTENT_TYPE_MAX_LENGTH_BYTES 64
#endif

#ifndef U_CELL_HTTP_TIMEOUT_SECONDS_MIN
/** The minimum HTTP timeout value permitted, in seconds.
 */
# define U_CELL_HTTP_TIMEOUT_SECONDS_MIN 30
#endif

#ifndef U_CELL_HTTP_FILE_NAME_RESPONSE_AUTO_PREFIX
/** The prefix to use for an automatically-allocated response file name.
 */
# define U_CELL_HTTP_FILE_NAME_RESPONSE_AUTO_PREFIX "ubxlibhttp_"
#endif

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/** The types of HTTP request that can be performed.
 */
typedef enum {
    U_CELL_HTTP_REQUEST_HEAD = 0,
    U_CELL_HTTP_REQUEST_GET = 1,
    U_CELL_HTTP_REQUEST_DELETE = 2,
    U_CELL_HTTP_REQUEST_PUT = 3,
    U_CELL_HTTP_REQUEST_POST = 4,
    U_CELL_HTTP_REQUEST_MAX_NUM
} uCellHttpRequest_t;

/** Callback that will be called when a HTTP response has arrived.
 * Such a callback may call uCellFileRead() to get the contents of
 * response files.
 *
 * @param cellHandle              the handle of the cellular instance.
 * @param httpHandle              the handle of the HTTP instance.
 * @param requestType             the request type.
 * @param error                   true if the request or response failed.
 * @param[out] pFileNameResponse  the null-terminated file name where
 *                                the complete HTTP response can be
 *                                found, from which you can obtain
 *                                the HTTP response code or, for an
 *                                #U_CELL_HTTP_REQUEST_GET, the header
 *                                containing the "Content-Type:".
 *                                uCellFileRead() or uCellFileBlockRead()
 *                                can be used to read the response
 *                                from the file but, if the response
 *                                is expected to be large (for example the
 *                                response to an HTTP GET request, so
 *                                if error was false and requestType
 *                                was #U_CELL_HTTP_REQUEST_GET), such a
 *                                read should NOT be done in the
 *                                callback itself, as that would
 *                                block other callbacks from being
 *                                executed.  Do ensure that you make
 *                                a copy of the pFileNameResponse
 *                                string though, rather than trying
 *                                to use the pFileNameResponse pointer
 *                                after the callback function has
 *                                returned.
 * @param[in,out] pCallbackParam  the pCallbackParam pointer that
 *                                was passed to uCellHttpOpen().
 */
typedef void (uCellHttpCallback_t) (uDeviceHandle_t cellHandle,
                                    int32_t httpHandle,
                                    uCellHttpRequest_t requestType,
                                    bool error,
                                    const char *pFileNameResponse,
                                    void *pCallbackParam);

/* ----------------------------------------------------------------
 * FUNCTIONS
 * -------------------------------------------------------------- */

/** Open a cellular HTTP client instance.
 *
 * @param cellHandle         the handle of the cellular instance to
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
 &                           #U_CELL_HTTP_TIMEOUT_SECONDS_MIN.
 * @param[in] pCallback      a callback to be called when a HTTP response
 *                           has been received (which may indicate
 *                           an error, for example "404 Not Found") or
 *                           an error has occurred.  Cannot be NULL.
 * @param[in] pCallbackParam a parameter that will be passed to
 *                           pCallback when it is called; may be NULL.
 * @return                   the handle of the HTTP instance on success,
 *                           else negative error code.
 */
int32_t uCellHttpOpen(uDeviceHandle_t cellHandle, const char *pServerName,
                      const char *pUserName, const char *pPassword,
                      int32_t timeoutSeconds, uCellHttpCallback_t *pCallback,
                      void *pCallbackParam);

/** Shut-down the given cellular HTTP client instance.  This function
 * should not be called while any of the other HTTP functions may be
 * running.
 *
 * @param cellHandle    the handle of the cellular instance.
 * @param httpHandle    the handle of the HTTP instance (as returned by
 *                      uCellHttpOpen()) to close.
 */
void uCellHttpClose(uDeviceHandle_t cellHandle, int32_t httpHandle);

/** Switch to HTTPS (with TLS-based security); if this is not called
 * HTTP is assumed.
 *
 * @param cellHandle        the handle of the cellular instance to be used.
 * @param httpHandle        the handle of the HTTP instance, as returned by
 *                          uCellHttpOpen().
 * @param securityProfileId the security profile ID containing the TLS
 *                          security parameters.  Specify -1 to let this be
 *                          chosen automatically.
 * @return                  zero on success else negative error code.
 */
int32_t uCellHttpSetSecurityOn(uDeviceHandle_t cellHandle, int32_t httpHandle,
                               int32_t securityProfileId);

/** Switch to HTTP (no TLS-based security).
 *
 * @param cellHandle        the handle of the cellular instance to be used.
 * @param httpHandle        the handle of the HTTP instance, as returned by
 *                          uCellHttpOpen().
 * @return                  zero on success else negative error code.
 */
int32_t uCellHttpSetSecurityOff(uDeviceHandle_t cellHandle, int32_t httpHandle);

/** Determine whether HTTPS (TLS-based security) is on or not.
 *
 * @param cellHandle              the handle of the cellular instance to be used.
 * @param httpHandle              the handle of the HTTP instance, as returned by
 *                                uCellHttpOpen().
 * @param[out] pSecurityProfileId a pointer to a place to put the security profile
 *                                ID that is being used for HTTPS; may be NULL.
 * @return                        true if HTTPS is employed, else false.
 */
bool uCellHttpIsSecured(uDeviceHandle_t cellHandle, int32_t httpHandle,
                        int32_t *pSecurityProfileId);

/** Perform an HTTP request. #U_CELL_HTTP_REQUEST_PUT is not supported by this
 * function; for that, and to avoid the limitations of this function for
 * #U_CELL_HTTP_REQUEST_POST, you must use uCellHttpRequestFile(), which performs
 * HTTP PUT/POST requests using the module's file system.  This function will block
 * while the request is being sent; the response from the server is returned via
 * pCallback as passed to uCellHttpOpen().
 *
 * IMPORTANT: you MUST wait for pCallback to be called before issuing your next
 * HTTP request.
 *
 * This function is thread-safe provided the caller choses a response file name
 * that does not clash with calls made from other threads (or uses the automatic
 * option).
 *
 * If you are going to perform large PUT/POST/GET requests (e.g. more than 1024
 * bytes) then you should ensure that you have flow control on the UART interface
 * to the cellular module or you might experience data loss.
 *
 * @param cellHandle              the handle of the cellular instance to be used.
 * @param httpHandle              the handle of the HTTP instance, as returned by
 *                                uCellHttpOpen().
 * @param requestType             the request type to perform; cannot be
 *                                #U_CELL_HTTP_REQUEST_PUT.
 * @param[in] pPath               the null-terminated path on the HTTP server
 *                                to perform the request on, for example
 *                                "/thing/form.html"; cannot be NULL.
 * @param[in] pFileNameResponse   the null-terminated file name in the cellular
 *                                modules' file system to which the HTTP response
 *                                will be written; this may be NULL and a file name
 *                                will be provided by the cellular module.
 * @param[in] pStrPost            the null-terminated string to send for a
 *                                #U_CELL_HTTP_REQUEST_POST; the data should
 *                                be printable ASCII text (isprint() must be
 *                                true for all characters) and should not contain
 *                                double quotation marks.  Ignored if
 *                                requestType is not #U_CELL_HTTP_REQUEST_POST.
 *                                strlen(pStrPost) cannot be more than
 *                                #U_CELL_HTTP_POST_REQUEST_STRING_MAX_LENGTH_BYTES.
 * @param[in] pContentTypePost    the null-terminated content type, for example
 *                                "application/text"; must be non-NULL for
 *                                #U_CELL_HTTP_REQUEST_POST, ignored otherwise;
 *                                strlen(pContentTypePost) cannot be more than
 *                                #U_CELL_HTTP_CONTENT_TYPE_MAX_LENGTH_BYTES.
 * @return                        zero if the request has been successfully sent,
 *                                else negative error code.
 */
int32_t uCellHttpRequest(uDeviceHandle_t cellHandle, int32_t httpHandle,
                         uCellHttpRequest_t requestType,
                         const char *pPath, const char *pFileNameResponse,
                         const char *pStrPost, const char *pContentTypePost);

/** Perform an HTTP request using a file from the cellular module's file
 * system as the source for #U_CELL_HTTP_REQUEST_PUT and #U_CELL_HTTP_REQUEST_POST.
 * This function will block while the request is being sent; the response from
 * the server is returned via pCallback as passed to uCellHttpOpen().
 *
 * IMPORTANT: you MUST wait for pCallback to be called before issuing your next
 * HTTP request.
 *
 * This function is thread-safe provided the caller choses file names
 * that do not clash with calls made from other threads (or uses the
 * automatic option).
 *
 * If you are going to perform large PUT/POST/GET requests (e.g. more than 1024
 * bytes) then you should ensure that you have flow control on the UART interface
 * to the cellular module or you might experience data loss.
 *
 * @param cellHandle              the handle of the cellular instance to be used.
 * @param httpHandle              the handle of the HTTP instance, as returned by
 *                                uCellHttpOpen().
 * @param requestType             the request type to perform.
 * @param[in] pPath               the null-terminated path on the HTTP server
 *                                to put or get from, for example "/thing/wotsit.html";
 *                                cannot be NULL.
 * @param[in] pFileNameResponse   the null-terminated file name in the cellular modules'
 *                                file system to which the HTTP response will be
 *                                written; this may be NULL and a file name will be
 *                                provided by the cellular module.
 * @param[in] pFileNamePutPost    the null-terminated file name in the cellular
 *                                module's file system to use as a source for the
 *                                data to be sent for #U_CELL_HTTP_REQUEST_PUT
 *                                or #U_CELL_HTTP_REQUEST_POST; you must have populated
 *                                this file with the data you wish to PUT/POST using
 *                                uCellFileDelete() followed by uCellFileWrite();
 *                                ignored for other HTTP request types.
 * @param[in] pContentTypePutPost the null-terminated content type, for example
 *                                "application/json"; must be non-NULL for
 *                                #U_CELL_HTTP_REQUEST_PUT and
 *                                #U_CELL_HTTP_REQUEST_POST, ignored otherwise;
 *                                cannot be more than
 *                                #U_CELL_HTTP_CONTENT_TYPE_MAX_LENGTH_BYTES long.
 * @return                        zero if the request has been successfully sent,
 *                                else negative error code.
 */
int32_t uCellHttpRequestFile(uDeviceHandle_t cellHandle, int32_t httpHandle,
                             uCellHttpRequest_t requestType,
                             const char *pPath, const char *pFileNameResponse,
                             const char *pFileNamePutPost,
                             const char *pContentTypePutPost);

/** Get the last HTTP error code.
 *
 * @param cellHandle     the handle of the cellular instance to be used.
 * @param httpHandle     the handle of the HTTP instance, as returned by
 *                       uCellHttpOpen().
 * @return               an error code, the meaning of which is utterly
 *                       module specific.
 */
int32_t uCellHttpGetLastErrorCode(uDeviceHandle_t cellHandle,
                                  int32_t httpHandle);

#ifdef __cplusplus
}
#endif

/** @}*/

#endif // _U_CELL_HTTP_H_

// End of file
