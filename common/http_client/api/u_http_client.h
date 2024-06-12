/*
 * Copyright 2019-2024 u-blox
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

#ifndef _U_HTTP_CLIENT_H_
#define _U_HTTP_CLIENT_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

#include "u_security_tls.h"
#include "u_device.h"

/** \addtogroup HTTP-Client HTTP Client
 *  @{
 */

/** @file
 * @brief This header file defines the u-blox HTTP client API.
 * This API is thread-safe.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

#ifndef U_HTTP_CLIENT_RESPONSE_WAIT_SECONDS
/** The maximum amount of time to wait for a response from an HTTP
 * server in seconds.
 */
# define U_HTTP_CLIENT_RESPONSE_WAIT_SECONDS 30
#endif

#ifndef U_HTTP_CLIENT_CHUNK_LENGTH_BYTES
/** The default chunk-length of a HTTP PUT/POST/GET when a
 * chunked API is used.
 */
# define U_HTTP_CLIENT_CHUNK_LENGTH_BYTES 256
#endif

/** The defaults for an HTTP connection, see #uHttpClientConnection_t.
 * Whenever an instance of #uHttpClientConnection_t is created it
 * should be assigned to this to ensure the correct default
 * settings.
 */
#define U_HTTP_CLIENT_CONNECTION_DEFAULT {NULL, NULL, NULL,                    \
                                          U_HTTP_CLIENT_RESPONSE_WAIT_SECONDS, \
                                          NULL, NULL, false, NULL,             \
                                          U_HTTP_CLIENT_CHUNK_LENGTH_BYTES}

#ifndef U_HTTP_CLIENT_CONTENT_TYPE_LENGTH_BYTES
/** The maximum length of a content-type string, including room
 * for a null-terminator.
 */
# define U_HTTP_CLIENT_CONTENT_TYPE_LENGTH_BYTES (64 + 1)
#endif

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/** Callback that will be called when a HTTP response has arrived if
 * the non-blocking form of an HTTP request is made.
 *
 * When used with a chunked API (uHttpClientPutRequestChunked(),
 * uHttpClientPostRequestChunked() or uHttpClientGetRequestChunked()),
 * the callbacks of those APIs (#uHttpClientDataCallback_t or
 * #uHttpClientResponseBodyCallback_t) will be called BEFORE this
 * callback is called.
 *
 * @param devHandle                       the device handle.
 * @param statusCodeOrError               the status code in the response as an
 *                                        integer (for example "200 OK" is 200),
 *                                        or negative error;  if
 *                                        #U_ERROR_COMMON_UNKNOWN is reported
 *                                        then the module has indicated that
 *                                        the HTTP request has not worked -
 *                                        in this case it may be worth re-trying.
 * @param responseSize                    the amount of data that has been copied
 *                                        to the pResponseBody or pResponseHead
 *                                        parameters to
 *                                        uHttpClientGetRequest() or
 *                                        uHttpClientHeadRequest() or the amount
 *                                        of data that has been offered to the
 *                                        #uHttpClientResponseBodyCallback_t if
 *                                        uHttpClientGetRequestChunked() was the
 *                                        origin of the HTTP request.  For the
 *                                        uHttpClientGetRequest() and
 *                                        uHttpClientGetRequestChunked() cases the
 *                                        content type, if present in the HTTP response
 *                                        header, will also be copied into a
 *                                        null-terminated string and stored at
 *                                        the pContentType storage
 *                                        passed to uHttpClientGetRequest() /
 *                                        uHttpClientGetRequestChunked().
 * @param[in,out] pResponseCallbackParam  the pResponseCallbackParam pointer that
 *                                        was in the pConnection structure
 *                                        passed to pUHttpClientOpen().
 */
typedef void (uHttpClientResponseCallback_t)(uDeviceHandle_t devHandle,
                                             int32_t statusCodeOrError,
                                             size_t responseSize,
                                             void *pResponseCallbackParam);

/** Callback to deliver data into a PUT or POST request, used by
 * uHttpClientPutRequestChunked() and uHttpClientPostRequestChunked().
 *
 * This callback will be called repeatedly until it returns 0,
 * indicating the end of the HTTP request data.  Should something go
 * wrong with the transfer this callback will be called with pData
 * set to NULL to indicate that it will not be called again for the
 * given HTTP request.
 *
 * @param devHandle          the device handle.
 * @param[out] pData         a pointer to a place to put the data to be
 *                           sent, NULL will be used to indicate that it
 *                           is no longer possible to send any more data.
 * @param size               the amount of storage at pData; will only be
 *                           zero if pData is NULL, will not be more than
 *                           the maxChunkLengthBytes parameter passed to
 *                           pUHttpClientOpen() in #uHttpClientConnection_t.
 * @param[in,out] pUserParam the user parameter that was passed to
 *                           uHttpClientPutRequestChunked() /
 *                           uHttpClientPostRequestChunked().
 * @return                   the number of bytes that the callback has
 *                           copied into pData, may be up to size bytes;
 *                           if the data happens to be a string that is
 *                           ending the null-terminator should NOT be
 *                           copied or included in the count; the end of a
 *                           string is indicated by this callback returning
 *                           zero the next time it is called.
 */
typedef size_t (uHttpClientDataCallback_t)(uDeviceHandle_t devHandle,
                                           char *pData, size_t size,
                                           void *pUserParam);

/** Callback to receive response data from a GET or a POST request, used
 * by uHttpClientPostRequestChunked() and uHttpClientGetRequestChunked().
 *
 * This callback will be called repeatedly when a response arrives, while
 * the callback returns true; if the callback returns false then it will
 * not be called again for this HTTP response.  When the HTTP response is
 * over the callback will be called once more with pResponseBody set to
 * NULL to indicate that the response has ended.
 *
 * @param devHandle          the device handle.
 * @param[in] pResponseBody  a pointer to the next chunk of HTTP response
 *                           data; the data may be binary data.  This data
 *                           should be copied out before returning, it will
 *                           not be valid once the callback has returned.
 *                           If this parameter is NULL, that indicates the
 *                           end of the response body; should the response
 *                           body be a string, no null terminator will be
 *                           included in pResponseBody.
 * @param size               the amount of data at pResponseBody; zero if
 *                           pResponseBody is NULL, will not be more than
 *                           the maxChunkLengthBytes parameter passed to
 *                           pUHttpClientOpen() in #uHttpClientConnection_t.
 * @param[in,out] pUserParam the user parameter that was passed to
 *                           uHttpClientGetRequestChunked() /
 *                           uHttpClientPostRequestChunked().
 * @return                   true if the callback may be called again for
 *                           this HTTP response; set this to false if, for
 *                           some reason, no more data is wanted, and then
 *                           the callback will not be called again for this
 *                           HTTP response.
 */
typedef bool (uHttpClientResponseBodyCallback_t)(uDeviceHandle_t devHandle,
                                                 const char *pResponseBody,
                                                 size_t size,
                                                 void *pUserParam);

/** HTTP client connection information.  Note that the maximum length
 * of the string fields may differ between modules.
 * NOTE: if this structure is modified be sure to modify
 * #U_HTTP_CLIENT_CONNECTION_DEFAULT to match.
 */
typedef struct {
    const char *pServerName;                          /**< the null-terminated name
                                                           of the HTTP server.  This may
                                                           be a domain name or an IP
                                                           address and may include a
                                                           port number, for example
                                                           "u-blox.net:83".  Note:
                                                           there should be no prefix
                                                           (i.e. NOT http://u-blox.net:83). */
    const char *pUserName;                            /**< the null-terminated user name
                                                           if required by the HTTP server. */
    const char *pPassword;                            /**< the null-terminated password
                                                           if required by the HTTP server. */
    int32_t timeoutSeconds;                           /**< the timeout when waiting for a
                                                           response to an HTTP request in
                                                           seconds. */
    uHttpClientResponseCallback_t *pResponseCallback; /**< determines whether the HTTP
                                                           request calls are going to be
                                                           blocking or non-blocking for this
                                                           connection.  If NULL (the default)
                                                           the HTTP request functions will
                                                           block until a response is
                                                           returned, a timeout occurs or the
                                                           operation is cancelled using
                                                           pKeepGoingCallback(); if
                                                           pResponseCallback is non-NULL then
                                                           the HTTP request functions will
                                                           return as soon as the HTTP request
                                                           has been sent and pResponseCallback
                                                           will be called when the response
                                                           arrives or a timeout occurs; critically,
                                                           for an uHttpClientPostRequest(), an
                                                           uHttpClientGetRequest() or an
                                                           uHttpClientHeadRequest(), the
                                                           data buffer pointed to by
                                                           pResponseBody/pResponseHead
                                                           MUST REMAIN VALID until the
                                                           response callback function is
                                                           called; the same goes for the data
                                                           buffer pointed-to by the
                                                           pResponseContentType of
                                                           uHttpClientPostRequest() /
                                                           uHttpClientPostRequestChunked() and
                                                           the pContentType parameter of
                                                           uHttpClientGetRequest() /
                                                           uHttpClientGetRequestChunked().
                                                           Note that you can still only have
                                                           one HTTP request in progress at
                                                           a time; this is a limitation of
                                                           the module itself. */
    void *pResponseCallbackParam;                     /**< a parameter that will be passed to
                                                           pResponseCallback when it is called;
                                                           ignored if pResponseCallback is NULL. */
    bool errorOnBusy;                                 /**< if true, the API functions will return
                                                           #U_ERROR_COMMON_BUSY if an HTTP request
                                                           is already in progress, else (and this
                                                           is the default), they will wait for the
                                                           previous request to complete/time-out. */
    bool (*pKeepGoingCallback) (void);                /**< used only for the blocking case:
                                                           a function that will be called
                                                           while the HTTP request is in
                                                           progress.  While pKeepGoingCallback()
                                                           returns true the API will continue
                                                           to wait until success or timeoutSeconds
                                                           is reached.  If pKeepGoingCallback()
                                                           returns false then the API will return.
                                                           Note that the HTTP request may still
                                                           succeed, this does not cancel the
                                                           operation, it simply stops waiting
                                                           for the response.  pKeepGoingCallback()
                                                           can also be used to feed any application
                                                           watchdog timer that might be running.
                                                           May be NULL (the default), in which case
                                                           the HTTP request functions will continue
                                                           to wait until success or timeoutSeconds
                                                           have elapsed. */
    size_t maxChunkLengthBytes;                       /**< the maximum chunk length in bytes, used
                                                           by the chunked APIs. */
} uHttpClientConnection_t;

/** HTTP context data, used internally by this code and
 * exposed here only so that it can be handed around by the
 * caller.  The contents of this structure may be changed
 * without notice and should not be accessed/relied-upon by
 * the caller.
 */
typedef struct {
    uDeviceHandle_t devHandle;
    void *semaphoreHandle; /* no 'p' prefix as this should be treated as a handle,
                              not using actual type to avoid customer having to drag
                              more headers in for what is an internal structure. */
    int32_t eventQueueHandle;
    int32_t lastRequestTimeMs;
    void *pPriv; /* underlying HTTP implementation may use this void pointer
                    to hold the reference to the internal data structures. */
    uSecurityTlsContext_t *pSecurityContext;
    int32_t statusCodeOrError;
    int32_t timeoutSeconds; /* populated from uHttpClientConnection_t. */
    bool errorOnBusy;  /* populated from uHttpClientConnection_t. */
    uHttpClientResponseCallback_t *pResponseCallback; /* populated from uHttpClientConnection_t. */
    void *pResponseCallbackParam;                     /* populated from uHttpClientConnection_t. */
    bool (*pKeepGoingCallback) (void);                /* populated from uHttpClientConnection_t. */
    char *pResponse;       /* set when a HTTP POST, GET or HEAD is being carried out. */
    size_t *pResponseSize; /* set when a HTTP POST, GET or HEAD is being carried out. */
    uHttpClientResponseBodyCallback_t *pResponseBodyCallback; /* set for a chunked POST or GET */
    void *pUserParamResponseBody;  /* set for a chunked POST or GET */
    char *pContentType;    /* set when a HTTP POST or GET is being carried out. */
    size_t chunkLengthBytes; /* holds maxChunkLengthBytes for this context. */
} uHttpClientContext_t;

/* ----------------------------------------------------------------
 * FUNCTIONS
 * -------------------------------------------------------------- */

/** Open an HTTP client session.  The module must be powered
 * up for this function to work.  If the pServerNameStr field of
 * pConnection contains a domain name the module may immediately try
 * to perform  a DNS look-up to establish the IP address of the
 * HTTP server and hence you should ensure that the module is
 * connected beforehand.
 *
 * IMPORTANT: if you re-boot the module after calling this function
 * you will lose all settings and must call uHttpClientClose() followed
 * by pUHttpClientOpen() to put them back again.
 *
 * Note that HTTP operation is NOT supported on the LENA-R8 cellular
 * module.
 *
 * When this function is first called it will allocate a mutex which
 * is never subsequently free()ed in order to ensure thread safety.
 *
 * @param devHandle                the device handle to be used,
 *                                 for example obtained using uDeviceOpen().
 * @param[in] pConnection          the connection information, mostly
 *                                 the HTTP server name and port and
 *                                 potentially a callback function which
 *                                 would make the HTTP request functions
 *                                 non-blocking; cannot be NULL.
 *                                 IT IS GOOD PRACTICE to assign this,
 *                                 initially, to
 *                                 #U_HTTP_CLIENT_CONNECTION_DEFAULT and
 *                                 then modify the members you want
 *                                 to be different to the default value.
 * @param[in] pSecurityTlsSettings a pointer to the security settings to
 *                                 be applied if you wish to make an HTTPS
 *                                 connection, NULL for no security.
 * @return                         a pointer to the internal HTTP context
 *                                 structure used by this code or NULL on
 *                                 failure (in which case
 *                                 uHttpClientOpenResetLastError() can
 *                                 be called to obtain an error code).
 */
uHttpClientContext_t *pUHttpClientOpen(uDeviceHandle_t devHandle,
                                       const uHttpClientConnection_t *pConnection,
                                       const uSecurityTlsSettings_t *pSecurityTlsSettings);

/** If pUHttpClientOpen() returned NULL this function can be
 * called to find out why.  That error code is reset to "success"
 * by calling this function.
 *
 * @return the last error code from a call to pUHttpClientOpen().
 */
int32_t uHttpClientOpenResetLastError();

/** Close the given HTTP client session; will wait for any HTTP
 * request that is currently running to end.
 *
 * @param[in] pContext   a pointer to the internal HTTP context
 *                       structure that was originally returned by
 *                       pUHttpClientOpen().
 */
void uHttpClientClose(uHttpClientContext_t *pContext);

/** Make an HTTP PUT request.  If this is a blocking call (i.e.
 * pResponseCallback in the pConnection structure passed to pUHttpClientOpen()
 * was NULL) and a pKeepGoingCallback() was provided in pConnection
 * then it will be called while this function is waiting for a response.
 *
 * Only one HTTP request, of any kind, may be outstanding at a time.
 *
 * Note that HTTP operation is NOT supported on the LENA-R8 cellular
 * module.
 *
 * If you are going to perform large PUT requests (e.g. more than 1024
 * bytes) then you should ensure that you have flow control on the interface
 * to the module or you might experience data loss.  If you do not have
 * flow control connected when using HTTP with a cellular module this code
 * will try to detect that data has been lost and, if so, return the error
 * #U_ERROR_COMMON_TRUNCATED.  You might also take a look at
 * uHttpClientPutRequestChunked() (only supported on cellular devices).
 *
 * @param[in] pContext               a pointer to the internal HTTP context
 *                                   structure that was originally returned by
 *                                   pUHttpClientOpen().
 * @param[in] pPath                  the null-terminated path on the HTTP server
 *                                   to PUT the data to, for example
 *                                   "/thing/upload.html"; cannot be NULL.
 * @param[in] pData                  the data to write; may be binary.
 * @param size                       the amount of data at pData; if pData
 *                                   happens to be text, this should be
 *                                   strlen(pData).
 * @param[in] pContentType           the null-terminated content type, for example
 *                                   "application/text"; must be non-NULL if
 *                                   pData is non-NULL.
 * @return                           in the blocking case the HTTP status code or
 *                                   negative error code; in the non-blocking case
 *                                   zero or negative error code.  If
 *                                   #U_ERROR_COMMON_UNKNOWN is reported then the
 *                                   module has indicated that the HTTP request
 *                                   has not worked; in this case it may be worth
 *                                   re-trying.
 */
int32_t uHttpClientPutRequest(uHttpClientContext_t *pContext,
                              const char *pPath,
                              const char *pData, size_t size,
                              const char *pContentType);

/** As uHttpClientPutRequest() but with a callback for the data, permitting it
 * to be sent in chunks.  Only supported on cellular devices.
 *
 * @param[in] pContext               a pointer to the internal HTTP context
 *                                   structure that was originally returned by
 *                                   pUHttpClientOpen().
 * @param[in] pPath                  the null-terminated path on the HTTP server
 *                                   to PUT the data to, for example
 *                                   "/thing/upload.html"; cannot be NULL.
 * @param[in] pDataCallback          the callback that delivers data into the
 *                                   PUT request.
 * @param[in,out] pUserParam         a parameter that will be passed to
 *                                   pDataCallback when it is called; may be NULL.
 * @param[in] pContentType           the null-terminated content type, for example
 *                                   "application/text"; must be non-NULL if
 *                                   pData is non-NULL.
 * @return                           in the blocking case the HTTP status code or
 *                                   negative error code; in the non-blocking case
 *                                   zero or negative error code.  If
 *                                   #U_ERROR_COMMON_UNKNOWN is reported then the
 *                                   module has indicated that the HTTP request
 *                                   has not worked; in this case it may be worth
 *                                   re-trying.
 */
int32_t uHttpClientPutRequestChunked(uHttpClientContext_t *pContext,
                                     const char *pPath,
                                     uHttpClientDataCallback_t *pDataCallback,
                                     void *pUserParam,
                                     const char *pContentType);

/** Make an HTTP POST request.  If this is a blocking call (i.e.
 * pResponseCallback in the pConnection structure passed to pUHttpClientOpen()
 * was NULL) and a pKeepGoingCallback() was provided in pConnection
 * then it will be called while this function is waiting for a response.
 *
 * Only one HTTP request, of any kind, may be outstanding at a time.
 *
 * IMPORTANT: see warning below about the validity of the pResponseBody and
 * pResponseContentType pointers.
 *
 * Note that HTTP operation is NOT supported on the LENA-R8 cellular
 * module.
 *
 * If you are going to perform large POST requests (e.g. more than 1024
 * bytes) then you should ensure that you have flow control on the interface
 * to the module or you might experience data loss.  If you do not have
 * flow control connected when using HTTP with a cellular module this code
 * will try to detect that data has been lost and, if so, return the error
 * #U_ERROR_COMMON_TRUNCATED.
 *
 * If you have large amounts of data to POST, or you expect to get a large
 * response body back from a POST request, you may prefer to use
 * uHttpClientPostRequestChunked() (only supported on cellular devices).
 *
 * @param[in] pContext               a pointer to the internal HTTP context
 *                                   structure that was originally returned by
 *                                   pUHttpClientOpen().
 * @param[in] pPath                  the null-terminated path on the HTTP server
 *                                   to POST the data to, for example
 *                                   "/thing/form.html"; cannot be NULL.
 * @param[in] pData                  the data to write; may be binary.
 * @param size                       the amount of data at pData; if pData
 *                                   happens to be text, this should be
 *                                   strlen(pData).
 * @param[in] pContentType           null-terminated content type, for example
 *                                   "application/text"; must be non-NULL if
 *                                   pData is non-NULL.
 * @param[out] pResponseBody         a pointer to a place to put the HTTP response
 *                                   body; in the non-blocking case this storage MUST
 *                                   REMAIN VALID until pResponseCallback is called,
 *                                   which will happen on a timeout as well as in
 *                                   the success case; best not put the storage on
 *                                   the stack, just in case.  The same goes for
 *                                   pResponseContentType below.  This may point
 *                                   to the same buffer as pData if you don't mind
 *                                   that data being over-written.
 * @param[in,out] pResponseSize      on entry the amount of storage at pResponseBody;
 *                                   on return, in the blocking case, the amount of
 *                                   data copied to pResponseBody (in the non-blocking
 *                                   case the callback function is instead given the
 *                                   amount of data returned).
 * @param[out] pResponseContentType  a place to put the content type of the response,
 *                                   for example "application/text". In the non-blocking
 *                                   case this storage MUST REMAIN VALID until
 *                                   pResponseCallback is called, which will happen on
 *                                   a timeout as well as in the success case.  Will
 *                                   always be null-terminated.  AT LEAST
 *                                   #U_HTTP_CLIENT_CONTENT_TYPE_LENGTH_BYTES of storage
 *                                   must be provided.
 * @return                           in the blocking case the HTTP status code or
 *                                   negative error code; in the non-blocking case
 *                                   zero or negative error code.  If
 *                                   #U_ERROR_COMMON_UNKNOWN is reported then the
 *                                   module has indicated that the HTTP request
 *                                   has not worked; in this case it may be worth
 *                                   re-trying.
 */
int32_t uHttpClientPostRequest(uHttpClientContext_t *pContext,
                               const char *pPath,
                               const char *pData, size_t size,
                               const char *pContentType,
                               char *pResponseBody, size_t *pResponseSize,
                               char *pResponseContentType);

/** As uHttpClientPostRequest() but with callbacks for the uplink data and
 * downlink response, permitting them to be sent and received in chunks.
 *
 * IMPORTANT: see warning below about the validity of the
 * pResponseContentType pointer.
 *
 * @param[in] pContext                    a pointer to the internal HTTP
 *                                        context structure that was originally
 *                                        returned by
 *                                        pUHttpClientOpen().
 * @param[in] pPath                       the null-terminated path on the HTTP
 *                                        server to POST the data to, for example
 *                                        "/thing/form.html"; cannot be NULL.
 * @param[in] pDataCallback               the callback that delivers data into
 *                                        the POST request.
 * @param[in,out] pUserParamData          a parameter that will be passed to
 *                                        pDataCallback when it is called; may be
 *                                        NULL.
 * @param[in] pContentType                null-terminated content type, for example
 *                                        "application/text"; must be non-NULL if
 *                                        pData is non-NULL.
 * @param[out] pResponseBodyCallback      the callback that receives the body of the
 *                                        POST response.
 * @param[in,out] pUserParamResponseBody  a parameter that will be passed to
 *                                        pResponseBodyCallback when it is called;
 *                                        may be NULL.
 * @param[out] pResponseContentType       a place to put the content type of the
 *                                        response, for example "application/text".
 *                                        In the non-blocking case this storage
 *                                        MUST REMAIN VALID until pResponseCallback
 *                                        is called, which will happen on a timeout
 *                                        as well as in the success case.  Will
 *                                        always be null-terminated.  AT LEAST
 *                                        #U_HTTP_CLIENT_CONTENT_TYPE_LENGTH_BYTES
 *                                        of storage must be provided.
 * @return                                in the blocking case the HTTP status code
 *                                        or negative error code; in the non-blocking
 *                                        case zero or negative error code.  If
 *                                        #U_ERROR_COMMON_UNKNOWN is reported then
 *                                        the module has indicated that the HTTP
 *                                        request has not worked; in this case it may
 *                                        be worth re-trying.
 */
int32_t uHttpClientPostRequestChunked(uHttpClientContext_t *pContext,
                                      const char *pPath,
                                      uHttpClientDataCallback_t *pDataCallback,
                                      void *pUserParamData,
                                      const char *pContentType,
                                      uHttpClientResponseBodyCallback_t *pResponseBodyCallback,
                                      void *pUserParamResponseBody,
                                      char *pResponseContentType);

/** Make an HTTP GET request.  If this is a blocking call (i.e.
 * pResponseCallback in the pConnection structure passed to pUHttpClientOpen()
 * was NULL) and a pKeepGoingCallback() was provided in pConnection then
 * it will be called while this function is waiting for a response.
 *
 * Only one HTTP request, of any kind, may be outstanding at a time.
 *
 * IMPORTANT: see warning below about the validity of the pResponseBody and
 * pContentType pointers.
 *
 * Note that HTTP operation is NOT supported on the LENA-R8 cellular
 * module.
 *
 * Multi-part content is not handled here: should you wish to handle such
 * content you will need to do the re-assembly yourself.
 *
 * If you are going to perform large GET requests (e.g. more than 1024
 * bytes) then you should ensure that you have flow control on the interface
 * to the module or you might experience data loss.  You might also take
 * a look at uHttpClientGetRequestChunked() (only supported on cellular devices).
 *
 * @param[in] pContext               a pointer to the internal HTTP context
 *                                   structure that was originally returned by
 *                                   pUHttpClientOpen().
 * @param[in] pPath                  the null-terminated path on the HTTP server
 *                                   to GET the data from, for example
 *                                   "/thing/download/1.html"; cannot be NULL.
 * @param[out] pResponseBody         a pointer to a place to put the HTTP response
 *                                   body; in the non-blocking case this storage MUST
 *                                   REMAIN VALID until pResponseCallback is called,
 *                                   which will happen on a timeout as well as in
 *                                   the success case; best not put the storage on
 *                                   the stack, just in case.  The same goes for
 *                                   pContentType below.
 * @param[in,out] pSize              on entry the amount of storage at pResponseBody;
 *                                   on return, in the blocking case, the amount of
 *                                   data copied to pResponseBody (in the non-blocking
 *                                   case the callback function is instead given the
 *                                   amount of data returned).
 * @param[out] pContentType          a place to put the content type of the response,
 *                                   for example "application/text". In the non-blocking
 *                                   case this storage MUST REMAIN VALID until
 *                                   pResponseCallback is called, which will happen on
 *                                   a timeout as well as in the success case.  Will
 *                                   always be null-terminated.  AT LEAST
 *                                   #U_HTTP_CLIENT_CONTENT_TYPE_LENGTH_BYTES of storage
 *                                   must be provided.
 * @return                           in the blocking case the HTTP status code or
 *                                   negative error code; in the non-blocking case
 *                                   zero or negative error code.  If
 *                                   #U_ERROR_COMMON_UNKNOWN is reported then the
 *                                   module has indicated that the HTTP request
 *                                   has not worked; in this case it may be worth
 *                                   re-trying.
 */
int32_t uHttpClientGetRequest(uHttpClientContext_t *pContext,
                              const char *pPath,
                              char *pResponseBody, size_t *pSize,
                              char *pContentType);

/** As uHttpClientGetRequest() but with a callback that permits the response
 * to be received in chunks.  Only supported on cellular devices.
 *
 * IMPORTANT: see warning below about the validity of the pContentType pointer.
 *
 * @param[in] pContext               a pointer to the internal HTTP context
 *                                   structure that was originally returned by
 *                                   pUHttpClientOpen().
 * @param[in] pPath                  the null-terminated path on the HTTP server
 *                                   to GET the data from, for example
 *                                   "/thing/download/1.html"; cannot be NULL.
 * @param[out] pResponseBodyCallback the callback that receives the body of the
 *                                   response to the GET request.
 * @param[in,out] pUserParam         a parameter that will be passed to
 *                                   pResponseBodyCallback when it is called; may
 *                                   be NULL.
 * @param[out] pContentType          a place to put the content type of the response,
 *                                   for example "application/text". In the non-blocking
 *                                   case this storage MUST REMAIN VALID until
 *                                   pResponseCallback is called, which will happen on
 *                                   a timeout as well as in the success case.  Will
 *                                   always be null-terminated.  AT LEAST
 *                                   #U_HTTP_CLIENT_CONTENT_TYPE_LENGTH_BYTES of storage
 *                                   must be provided.
 * @return                           in the blocking case the HTTP status code or
 *                                   negative error code; in the non-blocking case
 *                                   zero or negative error code.  If
 *                                   #U_ERROR_COMMON_UNKNOWN is reported then the
 *                                   module has indicated that the HTTP request
 *                                   has not worked; in this case it may be worth
 *                                   re-trying.
 */
int32_t uHttpClientGetRequestChunked(uHttpClientContext_t *pContext,
                                     const char *pPath,
                                     uHttpClientResponseBodyCallback_t *pResponseBodyCallback,
                                     void *pUserParam,
                                     char *pContentType);

/** Make a request for an HTTP header.  If this is a blocking call (i.e.
 * pResponseCallback in the pConnection structure passed to pUHttpClientOpen()
 * was NULL) and a pKeepGoingCallback() was provided in pConnection then
 * it will be called while this function is waiting for a response.
 *
 * Only one HTTP request, of any kind, may be outstanding at a time.
 *
 * IMPORTANT: see warning below about the validity of the pResponseHead pointer.
 *
 * Note that HTTP operation is NOT supported on the LENA-R8 cellular
 * module.
 *
 * @param[in] pContext               a pointer to the internal HTTP context
 *                                   structure that was originally returned by
 *                                   pUHttpClientOpen().
 * @param[in] pPath                  the null-terminated path on the HTTP server
 *                                   to get the data from, for example
 *                                   "/thing/download/1.html"; cannot be NULL.
 * @param[out] pResponseHead         a pointer to a place to put the HTTP response
 *                                   header data; in the non-blocking case this
 *                                   storage MUST REMAIN VALID until pResponseCallback
 *                                   is called, which will happen on a timeout as
 *                                   well as in the success case; best not put the
 *                                   storage on  the stack, just in case.
 * @param[in,out] pSize              on entry the amount of storage at pResponseHead;
 *                                   on return, in the blocking case, the amount of
 *                                   data copied to pResponseHead (in the non-blocking
 *                                   case the callback function is instead given the
 *                                   amount of data returned).
 * @return                           in the blocking case the HTTP status code or
 *                                   negative error code; in the non-blocking case
 *                                   zero or negative error code.  If
 *                                   #U_ERROR_COMMON_UNKNOWN is reported then the
 *                                   module has indicated that the HTTP request
 *                                   has not worked; in this case it may be worth
 *                                   re-trying.
 */
int32_t uHttpClientHeadRequest(uHttpClientContext_t *pContext,
                               const char *pPath,
                               char *pResponseHead, size_t *pSize);

/** Make an HTTP DELETE request.  If this is a blocking call (i.e.
 * pResponseCallback in the pConnection structure passed to pUHttpClientOpen()
 * was NULL) and a pKeepGoingCallback() was provided in pConnection
 * then it will be called while this function is waiting for a response.
 *
 * Only one HTTP request, of any kind, may be outstanding at a time.
 *
 * @param[in] pContext               a pointer to the internal HTTP context
 *                                   structure that was originally returned by
 *                                   pUHttpClientOpen().
 * @param[in] pPath                  the null-terminated path on the HTTP server
 *                                   to DELETE, for example "/thing/rubbish.html";
 *                                   cannot be NULL.
 * @return                           in the blocking case the HTTP status code or
 *                                   negative error code; in the non-blocking case
 *                                   zero or negative error code.  If
 *                                   #U_ERROR_COMMON_UNKNOWN is reported then the
 *                                   module has indicated that the HTTP request
 *                                   has not worked; in this case it may be worth
 *                                   re-trying.
 */
int32_t uHttpClientDeleteRequest(uHttpClientContext_t *pContext,
                                 const char *pPath);

#ifdef __cplusplus
}
#endif

/** @}*/

#endif // _U_HTTP_CLIENT_H_

// End of file
