/*
 * Copyright (c) 2017, Arm Limited and affiliates.
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *  * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef _U_AT_CLIENT_H_
#define _U_AT_CLIENT_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

/** \addtogroup _AT-client __AT Client
 *  @{
 */

/** @file
 * @brief This header file defines the AT client API, designed to
 * send structured AT commands to an AT server and parse structured
 * responses and unsolicited result codes from the AT server.
 * These functions are thread-safe with the proviso that an AT
 * client should not be accessed before it has been added or after
 * it has been removed.  See also the restrictions for the function
 * uAtClientSetWakeUpHandler().
 *
 * After initialisation/configuration, the general operation for
 * an AT command sequence is as follows:
 *
 * ```
 * uAtClientLock(client);                 <-- lock the stream
 * uAtClientCommandStart(client, "blah"); <-- begin an AT command
 * uAtClientWritexxx(client);             <-- write parameters
 * uAtClientWritexxx(client);
 * ...
 * uAtClientCommandStop(client);         <-- stop the AT command
 * uAtClientResponseStart(client, "blah"); <-- response starts
 * uAtClientReadxxx(client);             <-- read the parameters
 * uAtClientReadxxx(client);
 * ...
 * uAtClientResponseStop(client);        <-- stop the response
 * uAtClientUnlock(client);              <-- unlock the stream
 * ```
 *
 * i.e. the caller needs to understand the correct AT command
 * sequence to write and know what the response to that will be in
 * order to pick out all of the parameters in the response.  This
 * AT client understands all about the required delimiters between
 * parameters, `OK` and `ERROR` responses, timeouts, etc.
 *
 * So for instance, if the AT command were `AT+CGPADDR=0`, to which
 * the response is `+CGPADDR:0,<ip_address>`, the sequence
 * would be:
 *
 * ```
 * uAtClientLock(client);
 * uAtClientCommandStart(client, "AT+CGPADDR=");
 * uAtClientWriteInt(client, 0);
 * uAtClientCommandStop(client);
 * uAtClientResponseStart(client, "+CGPADDR:");
 * uAtClientSkipParameters(client, 1);             <-- skip the zero
 * len = uAtClientReadString(client, buffer,       <-- read <ip_address>
 *                           sizeof(buffer), false);
 * uAtClientResponseStop(client);
 * uAtClientUnlock(client);
 * ```
 *
 * Of course, the return codes should be checked for errors.
 *
 * If the response to an AT command has multiple lines, start each
 * one with a call to uAtClientResponseStart().  For instance,
 * if the response to `AT+SOMETHING` was:
 *
 * ```
 * +SOMETHING: <thing_1>
 * +SOMETHING: <thing_2>
 * OK
 * ```
 *
 * ...it could be read with:
 *
 * ```
 * uAtClientLock(client);
 * uAtClientCommandStart(client, "AT+SOMETHING");
 * uAtClientCommandStop(client);
 * uAtClientResponseStart(client, "+SOMETHING:");
 * x = uAtClientReadInt(client, 1);   <-- read <thing_1>
 * uAtClientResponseStart(client, "+SOMETHING:")
 * y = uAtClientReadInt(client, 1);   <-- read <thing_2>
 * uAtClientResponseStop(client);
 * uAtClientUnlock(client);
 * ```
 *
 * Many AT commands are simpler than this.  For an AT command
 * which has no send parameters, e.g. `AT+COPS?`, to which the response
 * might be `+COPS: <mode>,<format>,<operator_name>`, the sequence
 * would be:
 *
 * ```
 * uAtClientLock(client);
 * uAtClientCommandStart(client, "AT+COPS?");
 * uAtClientCommandStop(client);
 * uAtClientResponseStart(client, "+COPS:");
 * x = uAtClientReadInt(client);                    <-- read <mode>
 * y = uAtClientReadInt(client);                    <-- read <format>
 * z = uAtClientReadString(client, buffer,          <-- read <operator_name>
 *                         sizeof(buffer), false);
 * uAtClientResponseStop(client);
 * uAtClientUnlock(client)
 * ```
 *
 * And many AT commands have simply an `OK` or `ERROR` response,
 * e.g. `AT+CGACT=1,0`, used to activate PDP context 0 on a cellular
 * module, for which the sequence would be:
 *
 * ```
 * uAtClientLock(client);
 * uAtClientCommandStart(client, "AT+CGACT=");
 * uAtClientWriteInt(client, 1);
 * uAtClientWriteInt(client, 0);
 * uAtClientCommandStopReadResponse(client);
 * if (uAtClientUnlock() != 0) {
 *     // Do something 'cos there's been an error
 * }
 * ```
 *
 * Unsolicitied responses from the AT server are handled by
 * registering a URC (unsolicited response code) handler with
 * `uAtClientSetUrcHandler()`.  For instance, if the URC
 * of interest is `+CEREG:` one might register a handler with:
 *
 * `uAtClientSetUrcHandler(client, "+CEREG:", myRegHandler, NULL);`
 *
 * `myRegHandler()` might then be:
 *
 * ```
 * myRegHandler(atClient_t client, void *pUnused)
 * {
 *     int32_t x;
 *
 *     (void) pUnused;
 *     // Read the +CEREG parameter
 *     x = uAtClientReadInt(client);
 * }
 * ```
 *
 * Note, however, that a line of URC can be emitted by
 * the AT server AT ANY TIME (on a line-buffered basis), even
 * in the middle of an AT command sequence, which can
 * complicate matters. For instance, the following sequence can
 * occur:
 *
 * ```
 * AT+CEREG?
 * +CEREG: 1      <-- URC indicating cellular registration success
 * +CEREG: 0,1    <-- Response to AT+CEREG? query indicating
 * OK                 registration success
 * ```
 *
 * Unfortunately the URC and the response to the `AT+CEREG?` command
 * are identical except that one inserts an extra parameter before
 * the "success" indicator.  Simply reading the parameters would
 * not work in this case because their meanings would be
 * misinterpreted.  To work around this the `myRegHandler()` code
 * could be:
 *
 * ```
 * myRegHandler(atClient_t client, void *pUnused)
 * {
 *     int32_t status;
 *     int32_t secondInt;
 *
 *     (void) pUnused;
 *     status = uAtClientReadInt(client);
 *     // Speculatively read second int
 *     secondInt = uAtClientReadInt(client);
 *     if (secondInt >= 0) {
 *         status = secondInt;
 *     }
 * }
 * ```
 *
 * A speculative read is made of a second integer in case the
 * AT command has ended up with the URC and the URC handler has ended
 * up with the response intended for the AT command.  The `AT+CEREG?`
 * AT command sequence would similarly have to check in case it has
 * received a `+CEREG:` response with one parameter instead of two
 * and, if so, do a second read to get the AT response it was after.
 *
 * Also note that an entity that is expecting URCs or has launched
 * asynchronous events using uAtClientCallback() should take care,
 * while shutting down, that such asynchronous events haven't been
 * left in the queue to be processed, potentially after the entity
 * has invalidated the pointers it may have passed to those events.
 * To prevent this happening, uAtClientIgnoreAsync() should be
 * called before the entity begins to shut itself down.
 *
 * Notes:
 *
 * - Spaces in AT responses after the prefix (just one), around
 *   integers (any number) and before terminators (any number) are
 *   ignored but otherwise spaces around strings or byte arrays will
 *   be included in the returned string/array; you will need to clean
 *   these up yourself if the AT server you are talking to adds
 *   spaces there.
 * - While it is possible to skip the remaining parameters in a
 *   response by just calling uAtClientResponseStop() early, there
 *   is potential for this to be confused if any string or byte
 *   parameters that remain contain the expected `\r\n`, `OK` or
 *   `ERROR` stop tags; you should skip or read any string or byte
 *   parameters that remain where this could be the case.
 * - If an error is detected (e.g. stream writes cannot be performed
 *   or reads result in the AT timeout being reached) during the
 *   writing or reading of parameters an error flag is set and any
 *   parameter reads or writes will fail until uAtClientUnlock() is
 *   called (or the error is cleared with uAtClientClearError()).
 *   Therefore it is best to perform all necessary/expected
 *   writes/reads and then check the return code from uAtClientUnlock()
 *   to confirm success.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/** A marker to check for buffer overruns.  Must be a multiple of
 * 4 bytes in size.
 */
#define U_AT_CLIENT_MARKER           "DEADBEEF"

/** Size of #U_AT_CLIENT_MARKER: do NOT use sizeof(#U_AT_CLIENT_MARKER),
 * since we don't want the easy-peasy terminator in the marker string,
 * we only want interesting stuff.
 */
#define U_AT_CLIENT_MARKER_SIZE      8

/** The overhead in the receive buffer structure for buffer
 * management items.
 */
#define U_AT_CLIENT_BUFFER_OVERHEAD_BYTES (U_AT_CLIENT_MARKER_SIZE * 2 + \
                                           (sizeof(size_t) * 5))

/** A suggested AT client buffer length.  The limiting factor is
 * the longest parameter of type string that will ever appear in an
 * AT information response with no prefix, e.g.
 *
 * ```
 * AT+SOMETHING   <-- outgoing AT command
 * this_thing     <-- response string with no prefix (include room
 * OK                 for quotes if present and the \r\n terminator)
 * ```
 *
 * A real example of this is the response to AT+CIMI, which is a
 * string of 15 IMEI digits with no prefix.  Note that
 * #U_AT_CLIENT_BUFFER_OVERHEAD_BYTES of the buffer memory are
 * used for management which must be taken into account.
 *
 * Also note that different underlying network layers might
 * require larger buffers (e.g. if framing or security features
 * have to be accommodated).
 */
#define U_AT_CLIENT_BUFFER_LENGTH_BYTES (U_AT_CLIENT_BUFFER_OVERHEAD_BYTES + 64)

/** The string to put on the end of an AT command.
 */
#define U_AT_CLIENT_COMMAND_DELIMITER       "\r"

/** The length of #U_AT_CLIENT_COMMAND_DELIMITER in bytes.
 */
#define U_AT_CLIENT_COMMAND_DELIMITER_LENGTH_BYTES  1

/** The string which marks the end of the information
 * response line inside an AT command sequence.
 */
#define U_AT_CLIENT_CRLF                    "\r\n"

/** The length of U_AT_CLIENT_CRLF in bytes.
 */
#define U_AT_CLIENT_CRLF_LENGTH_BYTES       2

#ifndef U_AT_CLIENT_DEFAULT_TIMEOUT_MS
/** The default AT command time-out in milliseconds.
 */
# define U_AT_CLIENT_DEFAULT_TIMEOUT_MS 8000
#endif

#ifndef U_AT_CLIENT_DEFAULT_DELIMITER
/** The default delimiter, used between parameters sent
 * as part of an AT command and received as part of an AT
 * information response.
 */
# define U_AT_CLIENT_DEFAULT_DELIMITER ','
#endif

#ifndef U_AT_CLIENT_DEFAULT_DELAY_MS
/** The default minimum delay between the end of the last
 * response and sending a new AT command in milliseconds.
 */
# define U_AT_CLIENT_DEFAULT_DELAY_MS       25
#endif

#ifndef U_AT_CLIENT_URC_TIMEOUT_MS
/** The AT timeout in milliseconds while running in the context
 * of a URC handler. URCs should be handled fast, if you add debug
 * traces within URC processing then you also need to increase
 * this time.
 */
# define U_AT_CLIENT_URC_TIMEOUT_MS   100
#endif

#ifndef U_AT_CLIENT_STREAM_READ_RETRY_DELAY_MS
/** When reading from the input stream it is worth delaying
 * a little if nothing is available so that, when we do get
 * stuff, it is likely to be a substantial string, otherwise
 * we may search pointlessly through partial strings.
 * This also helps ensure that line prefixes are caught,
 * in one go, reducing the chance of us not recognising
 * something because we only have part of the prefix.
 */
# define U_AT_CLIENT_STREAM_READ_RETRY_DELAY_MS 10
#endif

#ifndef U_AT_CLIENT_URC_TASK_STACK_SIZE_BYTES
/** The stack size for the URC task.  This is chosen to
 * work for all platforms, the governing factor being ESP32,
 * which seems to require around twice the stack of NRF52
 * or STM32F4 and more again in the version pre-built for
 * Arduino.
 */
# define U_AT_CLIENT_URC_TASK_STACK_SIZE_BYTES  2304
#endif

#ifndef U_AT_CLIENT_URC_TASK_PRIORITY
/** The priority of the URC task.
 */
# define U_AT_CLIENT_URC_TASK_PRIORITY (U_CFG_OS_PRIORITY_MAX - 5)
#endif

#ifndef U_AT_CLIENT_CALLBACK_TASK_STACK_SIZE_BYTES
/** The stack size for the task in which any callbacks triggered
 * via uAtClientCallback() will run.  This is chosen to
 * work for all platforms, the governing factor being ESP32,
 * which seems to require around twice the stack of NRF52
 * or STM32F4 and more again in the version pre-built for
 * Arduino.
 */
# define U_AT_CLIENT_CALLBACK_TASK_STACK_SIZE_BYTES 2048
#endif

#ifndef U_AT_CLIENT_CALLBACK_TASK_PRIORITY
/** The priority of the task in which any callbacks triggered via
 * uAtClientCallback() will run.  This is set to
 * U_CFG_OS_APP_TASK_PRIORITY because the callback task is often in
 * a chain of event tasks which should be set to the same
 * priority or there will be a "kink" in the chain.
 */
# define U_AT_CLIENT_CALLBACK_TASK_PRIORITY U_CFG_OS_APP_TASK_PRIORITY
#endif

#ifndef U_AT_CLIENT_MAX_NUM
/** The maximum number of AT handlers that can be active at any
 * one time.
 */
# define U_AT_CLIENT_MAX_NUM 5
#endif

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/** AT client handle.
 */
typedef void *uAtClientHandle_t;

/** The types of underlying stream APIs supported, currently
 * only UART.
 */
//lint -estring(788, uAtClientStream_t::U_AT_CLIENT_STREAM_TYPE_MAX) Suppress not used within defaulted switch
typedef enum {
    U_AT_CLIENT_STREAM_TYPE_UART,
    U_AT_CLIENT_STREAM_TYPE_EDM,
    U_AT_CLIENT_STREAM_TYPE_MAX
} uAtClientStream_t;

/** The types of AT error response.
 */
typedef enum {
    U_AT_CLIENT_DEVICE_ERROR_TYPE_NO_ERROR = 0,
    U_AT_CLIENT_DEVICE_ERROR_TYPE_ERROR,   /**< Just ERROR */
    U_AT_CLIENT_DEVICE_ERROR_TYPE_CMS,     /**< +CMS ERROR */
    U_AT_CLIENT_DEVICE_ERROR_TYPE_CME,     /**< +CME ERROR */
    U_AT_CLIENT_DEVICE_ERROR_TYPE_ABORTED  /**< ABORTED by the user */
} uAtClientDeviceErrorType_t;

/** An AT error response structure with error code and type.
 */
typedef struct {
    uAtClientDeviceErrorType_t type;
    int32_t code;
} uAtClientDeviceError_t;

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS: INITIALISATION AND CONFIGURATION
 * -------------------------------------------------------------- */

/** Initialise the AT client infrastructure.  This must be called
 * before an AT client can be added with uAtClientAdd().
 *
 * @return zero on success else negative error code.
 */
int32_t uAtClientInit();

/** Shut down all AT clients.  No AT client function other than
 * uAtClientInit() should be called afterwards and all data transfer
 * should have been completed before this is called.
 */
void uAtClientDeinit();

/** Add an AT client on the given stream.  uAtClientInit() must
 * be called beforehand.  If an AT client has already been added
 * for the stream this function will return an error.
 *
 * @param streamHandle         the stream handle to use; the stream must
 *                             have already been opened by the caller.
 *                             If the stream is a UART the AT client will call
 *                             uPortUartEventCallbackSet() on it and
 *                             hence the caller must not do so (since
 *                             there can only be one).
 * @param streamType           the type of stream that streamHandle
 *                             is for.
 * @param[out] pReceiveBuffer  a buffer to use for received data from
 *                             the AT server.  The buffer must have
 *                             structure alignment; which usually
 *                             means it must be 4-byte aligned.  May
 *                             be NULL in which case a buffer will be
 *                             allocated by the AT client.
 * @param receiveBufferSize    if pReceiveBuffer is non-NULL this must
 *                             be set to the amount of memory at
 *                             pReceiveBuffer in bytes.  If pReceiveBuffer
 *                             is NULL then this is the size of buffer
 *                             that the AT client will allocate.  What
 *                             size to chose?  See the definition
 *                             of #U_AT_CLIENT_BUFFER_LENGTH_BYTES, noting
 *                             that #U_AT_CLIENT_BUFFER_OVERHEAD_BYTES of
 *                             the buffer memory will be used for management
 *                             overhead.
 * @return                     on success the handle of the AT client, else
 *                             NULL.
 */
uAtClientHandle_t uAtClientAdd(int32_t streamHandle,
                               uAtClientStream_t streamType,
                               void *pReceiveBuffer,
                               size_t receiveBufferSize);

/** Tell the given AT client to throw away asynchronous events; use NULL
 * as the parameter to apply this to all AT clients.  This function
 * is useful when the application that is using the AT client is shutting
 * things down but may have issued callbacks, either for URCs
 * directly or for callbacks via uAtClientCallback().  Such asynchronous
 * callbacks may have been given pointers to context data which will
 * become invalid, yet they may still be sitting in a queue waiting
 * to be processed and so could access out of range memory if they are
 * allowed to run; calling this function before invalidating such pointers,
 * then later calling uAtClientRemove(), avoids any surprises.
 * Note that this function stops any future asynchronous events but
 * an asynchronous event currently running will, of course, continue
 * to execute; hence it is important that any APIs called from an
 * asynchronous event have their own mutex protection against such
 * shut-down situations.
 *
 * @param atHandle  the handle of the AT client the should ignore
 *                  asynchronous events.
 */
void uAtClientIgnoreAsync(uAtClientHandle_t atHandle);

/** Remove the given AT client.  No AT client function must
 * be called on this client once this function is called.
 *
 * @param atHandle  the handle of the AT client to remove.
 */
void uAtClientRemove(uAtClientHandle_t atHandle);

/** Get whether general debug prints are on or off.
 *
 * @param atHandle  the handle of the AT client.
 * @return          true if debug prints are on, else false.
 */
bool uAtClientDebugGet(const uAtClientHandle_t atHandle);

/** Switch general debug prints on or off.
 *
 * @param atHandle  the handle of the AT client.
 * @param onNotOff  set to true to cause debug prints,
 *                  false to switch them off.
 */
void uAtClientDebugSet(uAtClientHandle_t atHandle, bool onNotOff);

/** Get whether printing of AT commands and responses
 * is on or off.
 *
 * @param atHandle  the handle of the AT client.
 * @return          true if printing AT commands and
 *                  responses is on, else false.
 */
bool uAtClientPrintAtGet(const uAtClientHandle_t atHandle);

/** Switch printing of AT commands and responses on or off.
 *
 * @param atHandle  the handle of the AT client.
 * @param onNotOff  set to true to cause AT commands
 *                  and responses to be printed, false to
 *                  switch printing off.
 */
void uAtClientPrintAtSet(uAtClientHandle_t atHandle,
                         bool onNotOff);

/** Get the timeout for completion of an AT command.
 *
 * @param atHandle  the handle of the AT client.
 * @return          the timeout in milliseconds.
 */
int32_t uAtClientTimeoutGet(const uAtClientHandle_t atHandle);

/** Set the timeout for completion of an AT command,
 * from uAtClientLock() to uAtClientUnlock().
 * If this is not called the timeout will be
 * #U_AT_CLIENT_DEFAULT_TIMEOUT_MS.
 *
 * If this is called between uAtClientLock() and
 * uAtClientUnlock() the given timeout will apply only
 * during the lock; the timeout will be automatically
 * returned to its previous value on uAtClientUnlock().
 *
 * @param atHandle   the handle of the AT client.
 * @param timeoutMs  the timeout in milliseconds.
 */
void uAtClientTimeoutSet(uAtClientHandle_t atHandle,
                         int32_t timeoutMs);

/** Set a callback that will be called when there has been
 * one or more consecutive AT command timeouts.  The callback
 * is called internally by the AT client using
 * uAtClientCallback().  The count is reset to zero when
 * an AT command completes without a timeout (either through
 * `OK` or `ERROR`), at which point the callback is also
 * called.  This can be used to detect that the AT server
 * has become unresponsive but note that AT timeouts can be
 * used within a driver (e.g. when polling for receipt
 * of an `OK` to an AT command which takes a long time to
 * return, e.g. AT+COPS=?).
 *
 * @param atHandle       the handle of the AT client.
 * @param[in] pCallback  the callback, which must take as
 *                       a parameter first the AT client handle
 *                       and second a single int32_t * that is
 *                       a pointer to an int32_t giving
 *                       the number of consecutive AT timeouts.
 *                       Use NULL to cancel a previous callback.
 */
void uAtClientTimeoutCallbackSet(uAtClientHandle_t atHandle,
                                 void (*pCallback) (uAtClientHandle_t,
                                                    int32_t *));

/** Get the delimiter that is used between parameters in
 * an outgoing AT command or is expected between parameters in
 * a response from the AT server.
 *
 * @param atHandle  the handle of the AT client.
 * @return          the delimiter character.
 */
char uAtClientDelimiterGet(const uAtClientHandle_t atHandle);

/** Set the delimiter that is used between parameters in
 * an outgoing AT command or is expected between parameters
 * in a response from the AT server.  If this function is not
 * called the delimiter will be #U_AT_CLIENT_DEFAULT_DELIMITER.
 *
 * @param atHandle  the handle of the AT client.
 * @param delimiter the delimiter character, usually ','.
 */
void uAtClientDelimiterSet(uAtClientHandle_t atHandle,
                           char delimiter);

/** Get the delay between ending one AT command and
 * starting the next.
 *
 * @param atHandle  the handle of the AT client.
 * @return          the delay in milliseconds.
 */
int32_t uAtClientDelayGet(const uAtClientHandle_t atHandle);

/** Set the delay between ending one AT command and
 * starting the next.  If this is not called the delay
 * will be #U_AT_CLIENT_DEFAULT_DELAY_MS.
 *
 * @param atHandle  the handle of the AT client.
 * @param delayMs   the delay in milliseconds.
 */
void uAtClientDelaySet(uAtClientHandle_t atHandle,
                       int32_t delayMs);

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS: SEND AN AT COMMAND
 * -------------------------------------------------------------- */

/** Lock the stream in order that the user can send an AT command,
 * something that begins `AT` and is terminated by the AT
 * server returning `OK` or `ERROR`.
 * When this function has returned the AT interface is exclusively
 * locked for the caller, any previous AT errors are cleared
 * and the AT timeout timer starts running.  The caller MUST
 * unlock the AT interface with a call to uAtClientUnlock()
 * when done, e.g. when an AT command sequence has been
 * completed or if a timeout or other error has occurred.
 *
 * @param atHandle  the handle of the AT client.
 */
void uAtClientLock(uAtClientHandle_t atHandle);

/** Unlock the stream.  This MUST be called to release
 * the AT client lock, otherwise the AT client will hang
 * on a subsequent call to uAtClientLock().
 *
 * @param atHandle  the handle of the AT client.
 * @return          the last error that happened on the
 *                  AT interface of this AT client.
 */
int32_t uAtClientUnlock(uAtClientHandle_t atHandle);

/** Start an AT command sequence, e.g. something
 * like "AT+COPS=".  The rest of the AT command is written
 * using uAtClientWriteInt(), uAtClientWriteString(),
 * uAtClientWriteBytes(), etc., which will add delimiters
 * to the sent string as required.
 * The stream must be locked with a call to
 * uAtClientLock() before this function is called.
 *
 * @param atHandle      the handle of the AT client.
 * @param[in] pCommand  the null-terminated command string.
 */
void uAtClientCommandStart(uAtClientHandle_t atHandle,
                           const char *pCommand);

/** Write an integer-type AT command parameter to the AT
 * command sequence, used after uAtClientCommandStart()
 * has been called to start the AT command sequence. The
 * AT client tracks whether this is the first parameter or
 * not and adds delimiters to the outgoing AT command as
 * appropriate.
 *
 * @param atHandle  the handle of the AT client.
 * @param param     the integer to be written.
 */
void uAtClientWriteInt(uAtClientHandle_t atHandle,
                       int32_t param);

/** Write an unsigned 64 bit integer-type AT command
 * parameter to the AT command sequence; used after
 * uAtClientCommandStart() has been called to start the
 * AT command sequence. The AT client tracks whether this
 * is the first parameter or not and adds delimiters to
 * the outgoing AT command as appropriate.
 *
 * @param atHandle  the handle of the AT client.
 * @param param     the unsigned 64-bit integer to be written.
 */
void uAtClientWriteUint64(uAtClientHandle_t atHandle,
                          uint64_t param);

/** Write a string-type AT command parameter to the AT
 * command sequence; used after uAtClientCommandStart()
 * has been called to start the AT command sequence.
 * Quotes are added around the string if useQuotes is
 * true. The AT client tracks whether this is the first
 * parameter or not and adds delimiters to the outgoing
 * AT command as appropriate.  You can skip a parameter
 * (e.g. AT+BLAH=thing,,next_thing) by pointing pParam at
 * an empty string (i.e. "") and setting useQuotes to
 * false.
 *
 * @param atHandle    the handle of the AT client.
 * @param[in] pParam  the null-terminated string to be
 *                    written as the AT command parameter.
 * @param useQuotes   if true then quotes will be added
 *                    around the string when it is written
 *                    to the stream.
 */
void uAtClientWriteString(uAtClientHandle_t atHandle,
                          const char *pParam,
                          bool useQuotes);

/** Write bytes to the AT interface.
 * This is useful in situations where binary data has to
 * be sent.
 *
 * @param atHandle     the handle of the AT client.
 * @param[in] pData    the bytes to be written as the
 *                     AT command parameter.
 * @param lengthBytes  the number of bytes in pData.
 * @param standalone   set this to true if this is a
 *                     simple injection of bytes into
 *                     into the AT server, e.g. as part
 *                     of an AT sockets packet write
 *                     operation, where delimiters are
 *                     irrelevant. Set this to false if
 *                     the bytes are being written as
 *                     part of an AT command along with
 *                     other parameters and hence a
 *                     delimiter should be inserted as
 *                     necessary by the AT client.
 * @return             the number of bytes written.
 */
size_t uAtClientWriteBytes(uAtClientHandle_t atHandle,
                           const char *pData,
                           size_t lengthBytes,
                           bool standalone);


/** Write a part of a string argument to AT command sequence.
 * Used after uAtClientCommandStart() has been called to
 * start the AT command sequence.
 * This function can be used to break up writing of a long
 * string argument into smaller fragments. On the first call
 * set isFirst to true and to append text to the same string
 * argument call the function again but with isFirst set to
 * false.
 *
 * Example:
 *
 * String argument to write: "tcp://www.google.com:80"
 * Can be written like this:
 * ```
 * uAtClientWritePartialString(atHandle, true, "tcp://");
 * uAtClientWritePartialString(atHandle, false, "www.google.com");
 * uAtClientWritePartialString(atHandle, false, ":");
 * uAtClientWritePartialString(atHandle, false, "80");
 * ```
 *
 * @param atHandle    the handle of the AT client.
 * @param isFirst     set to true when starting a new string argument
 *                    (only when isFirst is set to true a delimiter
 *                    will be added if needed).
 * @param[in] pParam  the partial string to be written.
 */
void uAtClientWritePartialString(uAtClientHandle_t atHandle,
                                 bool isFirst,
                                 const char *pParam);

/** Stop the outgoing AT command by writing the
 * command terminator.  Should be called after
 * uAtClientCommandStart() and any uAtClientWritexxx()
 * functions have been called in order to terminate the
 * outgoing part of the AT command sequence.  Usually
 * followed by uAtClientResponseStart() to read the
 * response from the AT server.
 *
 * @param atHandle  the handle of the AT client.
 */
void uAtClientCommandStop(uAtClientHandle_t atHandle);

/** As uAtClientCommandStop() but ALSO terminates the
 * entire AT command sequence by looking for the `OK` or
 * `ERROR` response from the AT server.  Use this with
 * AT commands where the AT server sends nothing else of
 * interest back, e.g. "AT+CEREG=1" will simply get back
 * `OK` or `ERROR`, nothing more.
 *
 * @param atHandle  the handle of the AT client.
 */
void uAtClientCommandStopReadResponse(uAtClientHandle_t atHandle);

/** Start waiting for the response to an AT command that
 * is more than a simple `OK` or `ERROR` (which would be
 * handled by calling uAtClientCommandStopReadResponse()).
 * This is normally called after uAtClientCommandStop().
 * and may be called multiple times if the response to the
 * AT command has multiple lines.
 * The AT response parameters, which will be separated by
 * delimiters, must then be read using uAtClientReadInt(),
 * uAtClientReadString(), uAtClientReadBytes(), etc.
 * Stop tags (e.g. `\r\n` or `OK` or `ERROR`) are obeyed
 * by the AT client as appropriate.
 *
 * @param atHandle     the handle of the AT client.
 * @param[in] pPrefix  the string which is expected to begin
 *                     the AT response, e.g. "+CGATT:".  May be
 *                     NULL if there is no prefix, e.g. in the
 *                     case of an AT command such as "AT+CGSN"
 *                     which just returns the IMEI of a cellular
 *                     module, e.g. "357862090123456".
 * @return             0 if pPrefix is matched otherwise negative value.
 */
int32_t uAtClientResponseStart(uAtClientHandle_t atHandle,
                               const char *pPrefix);

/** Read an integer parameter from the received AT response.
 * Both negative and positive integers are supported however
 * a negative return value is also used to indicate an error.
 * If a negative integer is possible then a check for errors
 * should be performed, e.g. by calling uAtClientErrorGet(),
 * before the return value is considered valid.
 *
 * @param atHandle  the handle of the AT client.
 * @return          the positive integer or negative error
 *                  code.
 */
int32_t uAtClientReadInt(uAtClientHandle_t atHandle);

/** Read a 64-bit unsigned integer parameter from the
 * received AT response.
 *
 * @param atHandle      the handle of the AT client.
 * @param[out] pUint64  a place to put the uint64_t parameter.
 * @return              zero on success or negative error code.
 */
int32_t uAtClientReadUint64(uAtClientHandle_t atHandle,
                            uint64_t *pUint64);

/** Read characters from the received AT response stream.
 * The received string will be null-terminated. Any quotation
 * marks found are skipped.  The delimiter (e.g. ',') is obeyed,
 * as is the stop tag (e.g. `\r\n` or `OK` or `ERROR` depending
 * on the context) unless ignoreStopTag is true.  If you don't
 * want the delimiter to be obeyed then read its current value
 * with uAtClientDelimiterGet(), change it to 0 and then
 * restore it afterwards.
 *
 * @param atHandle       the handle of the AT client.
 * @param[out] pString   a buffer in which to place the
 *                       received string. This may be NULL
 *                       in which case the received
 *                       characters are thrown away.
 * @param lengthBytes    the maximum number of chars to write
 *                       including the null terminator. If
 *                       pString is NULL this should be
 *                       set to the maximum number of bytes
 *                       to be read and thrown away. If
 *                       the string is longer than lengthBytes
 *                       any remaining characters up to the
 *                       next delimiter or stop tag are
 *                       thrown away.
 * @param ignoreStopTag  if true then continue reading even
 *                       if the stop tag is found; set this
 *                       to true to read a multi-line response
 *                       that doesn't fit into the
 *                       uAtClientResponseStart() pattern in
 *                       one go.
 *                       Since the AT client can detect no
 *                       stop tag, use this only if there
 *                       is a delimiter (which will stop the read
 *                       if it is not inside quotes) or if the
 *                       length of the string is known so that
 *                       you can curtail the read by setting an
 *                       appropriate buffer length (which must
 *                       allow room for a null terminator).
 * @return               the length of the string stored in
 *                       pString (as in the value that strlen()
 *                       would return) or negative error code
 *                       if a read timeout occurs before the
 *                       delimiter or the stop tag is found.
 *                       If pString is NULL the value that
 *                       would have been written to pString
 *                       is returned.
 */
int32_t uAtClientReadString(uAtClientHandle_t atHandle,
                            char *pString, size_t lengthBytes,
                            bool ignoreStopTag);

/** Read the given number of bytes from the received
 * AT response stream.  The stop tag (e.g. `\r\n` or `OK`
 * or `ERROR`) is obeyed but any delimiter within
 * lengthBytes will be ignored; you need to know how many
 * bytes you are going to read.  If you don't want the
 * stop tag to be obeyed either, call
 * uAtClientIgnoreStopTag() first.
 *
 * @param atHandle      the handle of the AT client.
 * @param[out] pBuffer  a buffer in which to place the
 *                      bytes read.  May be set to NULL
 *                      in which case the received bytes
 *                      are thrown away.
 * @param lengthBytes   the maximum number of bytes to read.
 *                      If pBuffer is NULL this should be
 *                      set to the number of bytes to be
 *                      read and thrown away.
 * @param standalone    set this to true if the bytes form
 *                      a standalone sequence, e.g. when reading
 *                      a specific number of bytes of an IP
 *                      socket AT command.  If this is false
 *                      then a delimiter (or the stop tag
 *                      for a response line) will be expected
 *                      following the sequence of bytes. If
 *                      you have previously set
 *                      uAtClientIgnoreStopTag() then standalone
 *                      should be set to true, otherwise this
 *                      function will search for a non-existent
 *                      stop tag.
 * @return              the number of bytes read or negative
 *                      error code.  If pBuffer is NULL the
 *                      number of bytes that would have been
 *                      written to pBuffer is returned.
 */
int32_t uAtClientReadBytes(uAtClientHandle_t atHandle,
                           char *pBuffer, size_t lengthBytes,
                           bool standalone);

/** Marks the end of an AT response, should be called
 * after uAtClientResponseStart() when all of the
 * wanted parameters have been read.  The remainder of
 * the incoming stream is read until the expected stop tag
 * for a response (`\r\n` or `OK` or `ERROR`, depending
 * on context) has been received, ensuring that no
 * characters are left in the buffer to confuse a
 * subsequent AT command sequence.
 *
 * @param atHandle  the handle of the AT client.
 */
void uAtClientResponseStop(uAtClientHandle_t atHandle);

/** A stop tag is something like `\r\n` or `OK` or
 * `ERROR`, detected by the AT client when handling
 * structured AT commands. This behavour can cause
 * problems when the AT server is emitting binary
 * information, e.g. an IP packet, which may contain
 * the characters of the stop tag.  In such situations
 * this function should be called.  After the user
 * has read whatever they require from the response
 * and called uAtClientResponseStop() the AT client
 * will throw away ALL the received characters remaining
 * in the buffer so that nothing is left to confuse a
 * subsequent AT sequence. Normal stop tag behaviour
 * resumes when the next AT command is sent.
 *
 * @param atHandle  the handle of the AT client.
 */
void uAtClientIgnoreStopTag(uAtClientHandle_t atHandle);

/** Put the stop tag back again: use this after
 * uAtClientIgnoreStopTag() has been called if you want
 * to make sure that the AT client pauses to wait for
 * the stop tag (e.g. `OK` or `ERROR`) before the end
 * of your current AT command sequence.
 *
 * @param atHandle  the handle of the AT client.
 */
void uAtClientRestoreStopTag(uAtClientHandle_t atHandle);

/** Consume the given number of parameters from the
 * received AT response stream.
 *
 * @param atHandle  the handle of the AT client.
 * @param count     the number of parameters to be skipped.
 */
void uAtClientSkipParameters(uAtClientHandle_t atHandle,
                             size_t count);

/** Consume the given number of bytes from the
 * received AT response stream.  May be required when
 * dealing with AT responses that have an unconventional
 * structure.  Neither delimiters nor stop tags are
 * obeyed; you need to know how many bytes to skip.
 *
 * @param atHandle     the handle of the AT client.
 * @param lengthBytes  the number of bytes to be consumed.
 */
void uAtClientSkipBytes(uAtClientHandle_t atHandle,
                        size_t lengthBytes);

/** Special case: wait for a single character to
 * arrive.  This can be used without starting
 * a command or response, it doesn't care.
 * The character is consumed.  Delimiters and stop
 * tags are not obeyed but any URC found is acted
 * upon.
 *
 * @param atHandle   the handle of the AT client.
 * @param character  the character that is expected;
 *                   should not be 0x0d or 0x0a as these
 *                   characters always precede a URC
 *                   and will be processed as part of URC
 *                   reception.
 * @return           zero if the character arrived,
 *                   else negative error code.
 */
int32_t uAtClientWaitCharacter(uAtClientHandle_t atHandle,
                               char character);

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS: HANDLE UNSOLICITED RESPONSES
 * -------------------------------------------------------------- */

/** Set the handler for an unsolicited response code from
 * the AT server, e.g. something like "+CEREG: 1" emitted
 * asynchronously by the AT server at any time.
 * If the URC is found when parsing AT responses the
 * handler is called.  If a handler is already set for the
 * given prefix then the new setting is ignored.
 * In a URC handler you only need to be concerned with reading
 * the parameters of interest, don't worry about locking/
 * unlocking or about any trailing parameters unless they
 * are of string or byte type and might contain the `\r\n`
 * line terminator: if this is the case those parameters
 * should be specifically read or skipped to avoid them
 * getting in the way of subsequent reads.
 * IMPORTANT: don't do anything heavy in a handler, e.g. don't
 * printf() or, at most, print a few characters; URC handlers
 * have to run quickly as they are interleaved with everything
 * else handling incoming data and any delay may result in
 * buffer overflows.  If you need to do anything heavy then
 * have your handler call uAtClientCallback().
 * IMPORTANT: make sure that this function is only called
 * when you know that atHandle is not going to be pulled out
 * from underneath it.
 *
 * @param atHandle           the handle of the AT client.
 * @param[in] pPrefix        the prefix for the URC. A prefix might
 *                           for example be "+CEREG:".
 * @param[in] pHandler       the function to be called if the prefix
 *                           is found at the start of an AT string
 *                           from the AT server.
 * @param[in] pHandlerParam  void * parameter to be passed to the
 *                           function call as the second parameter,
 *                           may be NULL.
 * @return                   zero on success else negative error code.
 */
int32_t uAtClientSetUrcHandler(uAtClientHandle_t atHandle,
                               const char *pPrefix,
                               void (*pHandler) (uAtClientHandle_t,
                                                 void *),
                               void *pHandlerParam);

/** Remove an unsolicited response code handler.
 * IMPORTANT: make sure that this function is only called
 * when you know that atHandle is not going to be pulled out
 * from underneath it.
 *
 * @param atHandle     the handle of the AT client.
 * @param[in] pPrefix  the prefix for the URC, which would have been
 *                     set in a call to uAtClientSetUrcHandler().
 */
void uAtClientRemoveUrcHandler(uAtClientHandle_t atHandle,
                               const char *pPrefix);

/** Get the stack high watermark for the URC task, the
 * minimum amount of free stack space.  If this gets close
 * to zero you need to do less in your URCs or you need to
 * increase U_AT_CLIENT_URC_TASK_STACK_SIZE_BYTES.
 *
 * @param atHandle  the handle of the AT client.
 * @return          the minimum amount of free stack during
 *                  the lifetime of the URC task in bytes,
 *                  else negative error code.
 */
int32_t uAtClientUrcHandlerStackMinFree(uAtClientHandle_t atHandle);

/** Make an asynchronous callback that is run in its own task
 * context with a stack size of
 * #U_AT_CLIENT_CALLBACK_TASK_STACK_SIZE_BYTES running at
 * priority #U_AT_CLIENT_CALLBACK_TASK_PRIORITY.  Use this
 * function if you need to do any heavy work from a URC
 * handler to avoid blocking the stream interface.  Callbacks
 * are queued and so are guaranteed to be run in the order
 * they are called.  A single callback queue is shared between
 * all AT client instances; you can determine which instance
 * has made the call by checking #uAtClientHandle_t, the first
 * parameter passed to the callback.
 *
 * @param atHandle            the handle of the AT client.
 * @param[in] pCallback       the callback function.
 * @param[in] pCallbackParam  a parameter to pass to the callback,
 *                            as the second parameter, may be NULL.
 * @return                    zero on success else negative error code.
 */
int32_t uAtClientCallback(uAtClientHandle_t atHandle,
                          void (*pCallback) (uAtClientHandle_t, void *),
                          void *pCallbackParam);

/** Get the stack high watermark for the task at the end of the
 * AT callback event queue, the minimum amount of free stack
 * space.  If this gets close to zero you either need to do less
 * in your callbacks or you need to increase
 * #U_AT_CLIENT_CALLBACK_TASK_STACK_SIZE_BYTES.
 *
 * @return  the minimum amount of free stack during the lifetime
 *          of the task that is at the end of the AT callback
 *          queue in bytes, else negative error code.
 */
int32_t uAtClientCallbackStackMinFree();

/** It should NOT normally be necessary to use this, URCs should
 * be handled with the uAtClientSetUrcHandler() function since they
 * arrive asynchronously.  However, there are cases (e.g. in
 * code handling wake-up from deep sleep when asynchronous URCs
 * are held back held back for processing after wake-up has been
 * completed) where it is necessary to wait for a URC "in-line".
 * For those cases the same URC handler callback as would be
 * written for the uAtClientSetUrcHandler() case can be called
 * directly using this function, just like a
 * uAtClientResponseStart()/Stop() but for the URC case. Make
 * sure that this is used within the SAME AT client locks that
 * caused the URC to be generated, otherwise the asynchronous
 * URC handler may jump in, process the URC string and throw it
 * away (as it will not be recognised as a URC by it).
 * In other words, an exchange might look something like this:
 *
 * ```
 * Lock the AT client:
 * uAtClientLock(client);
 * ...do normal stuff:
 * uAtClientCommandStart(client, "AT+THINGY=");
 * ...write things...
 * uAtClientCommandStop(client);
 * uAtClientResponseStart(client, "+THINGY:");
 * ...read things...
 * uAtClientResponseStop(client);
 * ...now do the "URC direct" bit within the same AT client lock:
 * uAtClientUrcDirect(client, "+URCTHINGY:", myUrcFunction,
 *                    (void *) pMyUrcFunctionParameter);
 * ...only now unlock the AT client:
 * uAtClientUnlock(client);
 * ```
 *
 * @param atHandle           the handle of the AT client.
 * @param[in] pPrefix        the prefix for the URC; cannot by NULL.
 * @param[in] pHandler       the function to be called if the prefix
 *                           is found; cannot be NULL.
 * @param[in] pHandlerParam  void * parameter to be passed to the
 *                           function call as the second parameter;
 *                           may be NULL.
 * @return                   zero if pPrefix is matched otherwise
 *                           negative value..
 */
int32_t uAtClientUrcDirect(uAtClientHandle_t atHandle,
                           const char *pPrefix,
                           void (*pHandler) (uAtClientHandle_t,
                                             void *),
                           void *pHandlerParam);

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS: MISC
 * -------------------------------------------------------------- */

/** Empty the underlying receive buffer.
 *
 * @param atHandle  the handle of the AT client.
 */
void uAtClientFlush(uAtClientHandle_t atHandle);

/** Clear the error status. Note that the error is
 * cleared anyway when uAtClientLock() is called.
 *
 * @param atHandle  the handle of the AT client.
 */
void uAtClientClearError(uAtClientHandle_t atHandle);

/** Return the last error that occured in the AT client,
 * e.g. as a result of an AT timeout.
 *
 * @param atHandle  the handle of the AT client.
 * @return          the last error.
 */
int32_t uAtClientErrorGet(uAtClientHandle_t atHandle);

/** Return the last error code received from the AT server,
 * an error code from "+CME ERROR" or "+CMS ERROR".
 *
 * @param atHandle           the handle of the AT client.
 * @param[out] pDeviceError  a place to put the device error,
 *                           cannot be NULL.
 */
void uAtClientDeviceErrorGet(uAtClientHandle_t atHandle,
                             uAtClientDeviceError_t *pDeviceError);

/** Get the handle and type of the underlying stream.
 *
 * @param atHandle          the handle of the AT client.
 * @param[out] pStreamType  a pointer to a place to put the
 *                          stream type; cannot be NULL.
 * @return                  the stream handle.
 */
int32_t uAtClientStreamGet(uAtClientHandle_t atHandle,
                           uAtClientStream_t *pStreamType);

/** Add a function that will intercept the transmitted
 * data before it is presented to the stream and may return
 * a modified buffer or hold onto the data until a whole
 * command has been assembled etc., allowing it to add
 * framing, perform encryption, etc. of the data.
 *
 * The intercept function may do the following:
 *
 * - advance the data-pointer it is passed: this indicates
 *   to the AT client that the intercept function has
 *   "consumed" that amount of data,
 * - modify the length it is passed: the AT client uses
 *   this length to determine how much data it must send
 *   to the stream when the intercept function returns,
 * - as its return value, provide a data pointer that is
 *   entirely different to that it was passed, let's call
 *   this "pDataReturned",
 * - modify the contents of the buffer it is passed without
 *   limitation (up to the length it is given).
 *
 * On return from the intercept function, if length is
 * greater than zero, the AT client will send that many
 * bytes from pDataReturned onwards to the stream.
 *
 * Then, if the data-pointer indicates that the intercept
 * function did not consume all of the data it was given,
 * it will be called again (with updated values for the
 * data-pointer and length) and the process above repeated
 * until it does indicate that all of the data has been
 * consumed.
 *
 * To indicate that the transmitted data from the
 * AT client is at an end (e.g. an AT command has been
 * completely written) pCallback will be called again
 * with a NULL data pointer so that the intercept
 * function can return any data it may still be holding.
 *
 * This function should only be called when the AT client has
 * been locked (with uAtClientLock()) to ensure thread safety.
 *
 * @param atHandle       the handle of the AT client.
 * @param[in] pCallback  the callback function that forms
 *                       the intercept where the first
 *                       parameter is the AT client handle,
 *                       the second parameter a pointer to
 *                       the pointer to the data to be written
 *                       (may be NULL), the third parameter a
 *                       pointer to the length (will never be
 *                       NULL, will point to zero if the
 *                       pointer to the data pointer is NULL)
 *                       and the fourth parameter the pContext
 *                       pointer that was passed to this function.
 *                       Use NULL to cancel a previous intercept.
 * @param[in] pContext   a context pointer which will be passed
 *                       to pCallback as its fourth parameter.
 *                       May be NULL.
 */
void uAtClientStreamInterceptTx(uAtClientHandle_t atHandle,
                                const char *(*pCallback) (uAtClientHandle_t,
                                                          const char **,
                                                          size_t *,
                                                          void *),
                                void *pContext);

/** Add a function that will intercept the received
 * data from the stream before it is processed by the AT
 * client.  The operations the receive intercept function
 * can perform are *different* to those the transmit
 * intercept function can perform in that the receive
 * intercept function is operating "in place" on the buffer:
 * it can only move the data pointer forward or reduce the
 * length pointer, i.e. take away from the buffer, it
 * can't go beyond the boundaries it is given or return
 * a pointer to a completely different memory space.
 *
 * The intercept function may do the following:
 *
 * - advance the data-pointer it is passed; this indicates
 *   to the AT client how much of the received data the
 *   intercept function has consumed,
 * - reduce the length passed to it; this is used by the
 *   AT client to know how much received data there
 *   is ready for it,
 * - as its return value, return a pointer that is further
 *   on in the received data buffer, up to the length it
 *   was passed,
 * - modify the contents of the buffer it is passed
 *   without limitation (up to the length it was given).
 *
 * If the intercept function is unable to consume the
 * buffer it is passed, e.g. if it is a partial frame
 * and the intercept function needs to leave it there
 * and try again when more data has arrived, it should
 * return NULL and set the value at the length pointer
 * to zero.
 *
 * The intercept function will be called repeatedly
 * *until* it returns NULL.  If it has not been able
 * to decode any data more will be requested from the
 * input stream, and repeat until the normal AT timeout
 * occurs.
 *
 * Otherwise, on return from the intercept function, the
 * return value should point to valid data, length should
 * point to the length of that data and the data-pointer
 * that was passed in should be advanced to indicate how
 * much of the buffer was consumed.  The AT client will
 * then use that valid data and remember where in the
 * received data stream the intercept function had got to.
 * The intercept function may cause data in the receive
 * buffer to be discarded by setting its return value to
 * the data-pointer, setting the value at the length pointer
 * to zero and advancing the data-pointer by the amount it
 * wants to be discarded.
 *
 * This function should only be called when the AT client
 * has been locked (with uAtClientLock()) to ensure thread
 * safety.
 *
 * This function will delete any data in the receive buffers
 * before hooking-in the intercept function.
 *
 * @param atHandle       the handle of the AT client.
 * @param[in] pCallback  the callback function that forms
 *                       the intercept where the first
 *                       parameter is the AT client handle,
 *                       the second parameter a pointer to
 *                       the pointer to the received data
 *                       (may be NULL), the third parameter
 *                       a pointer to the length (will never
 *                       be NULL, will point to zero if
 *                       the received data pointer is NULL)
 *                       and the fourth parameter the
 *                       pContext pointer that was passed to
 *                       this function.  Use NULL to cancel
 *                       a previous intercept.
 * @param[in] pContext   a context pointer which will be passed
 *                       to pCallback as its fourth parameter.
 *                       May be NULL.
 */
void uAtClientStreamInterceptRx(uAtClientHandle_t atHandle,
                                char *(*pCallback) (uAtClientHandle_t,
                                                    char **,
                                                    size_t *,
                                                    void *),
                                void *pContext);

/** Set a wake-up handler function.  This is useful where the
 * other end of the link is a module which may go to sleep
 * to save power and require some specific handling to recover
 * from that state before normal AT operations can continue.
 * The handler function is called before anything is transmitted
 * if the time since the end of the last AT transmit is greater
 * than inactivityTimeoutMs.  The handler function may carry out
 * whatever pin-toggling or AT-command-based power-up processes that
 * are required and return an integer; if the return value is zero
 * then the power-up is assumed to have succeeded and operations
 * will continue, else an error is assumed to have occurred.
 *
 * Note: if you have your own port, for this function to work
 * the port API functions uPortTaskGetHandle(),
 * uPortEnterCritical() and uPortExitCritical() must be
 * implemented.
 *
 * IMPORTANT: the wake-up handler may send and receive AT
 * commands and may expect URCs to be processed but should not
 * remove/deinitialise the AT interface or call this function,
 * and probably shouldn't do anything with global effect (e.g.
 * change the AT timeout UNLESS it is between the
 * uAtClientLock()/uAtClientUnlock() functions) as such changes
 * _will_ take effect.
 * Also, this function should only be called when there is no
 * "send" (i.e. from the MCU to the module) AT activity on-going.
 *
 * @param atHandle             the handle of the AT client.
 * @param[in] pHandler         the function to be called if
 *                             inactivityTimeoutMs milliseconds
 *                             have passed since the last AT
 *                             communication; use NULL to
 *                             remove a previous wake-up handler.
 * @param[in] pHandlerParam    void * parameter to be passed to
 *                             the function call as the second
 *                             parameter, may be NULL.
 * @param inactivityTimeoutMs  the time since the last AT
 *                             communication, in milliseconds,
 *                             that will cause pHandler to be
 *                             invoked.
 * @return                     zero on success else negative error
 *                             code.
 */
int32_t uAtClientSetWakeUpHandler(uAtClientHandle_t atHandle,
                                  int32_t (*pHandler) (uAtClientHandle_t,
                                                       void *),
                                  void *pHandlerParam,
                                  int32_t inactivityTimeoutMs);

/** Return true if a wake-up handler is set.
 *
 * @param atHandle  the handle of the AT client.
 * @return          true if a wake-up handler is set for the AT client,
 *                  else false.
 */
bool uAtClientWakeUpHandlerIsSet(const uAtClientHandle_t atHandle);

/** Set an "activity" pin.  This is useful where the module at the
 * other end of the link requires a pin to be raised or lowered while
 * this MCU is actively communicating over the AT interface (i.e.
 * sending stuff or waiting for a response to stuff).  The pin must
 * have already been configured as an output and set to the correct
 * initial level by the caller.
 *
 * @param atHandle      the handle of the AT client.
 * @param pin           the pin number of this MCU to use; use -1
 *                      to remove a previously-set activity pin.
 * @param readyMs       how long after the pin is set to wait until
 *                      AT commands can continue; value in milliseconds.
 * @param hysteresisMs  the minimum time between the pin being toggled;
 *                      value in milliseconds.
 * @param highIsOn      if true then the pin is set high while an AT
 *                      command is executed, else it is set low
 *                      while an AT command is executed.
 * @return              zero on success else negative error code.
 */
int32_t uAtClientSetActivityPin(uAtClientHandle_t atHandle,
                                int32_t pin, int32_t readyMs,
                                int32_t hysteresisMs, bool highIsOn);

/** Get the activity pin, if one is set.
 *
 * @param atHandle  the handle of the AT client.
 * @return          the activity pin if one is set, else negative
 *                  error code.
 */
int32_t uAtClientGetActivityPin(const uAtClientHandle_t atHandle);

#ifdef __cplusplus
}
#endif

/** @}*/

#endif // _U_AT_CLIENT_H_

// End of file
