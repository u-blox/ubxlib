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

#ifndef _U_CELL_MUX_PRIVATE_H_
#define _U_CELL_MUX_PRIVATE_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

/** @file
 * @brief This header file defines functions that encode and decode
 * 3GPP 27.010 CMUX frames.  These functions are called only inside
 * cellular, they are not intended for external use.  Only basic mode,
 * which is all that is required for u-blox cellular modules, and only
 * the MCU-side of that, is supported.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

#ifndef U_CELL_MUX_PRIVATE_INFORMATION_LENGTH_MAX_BYTES
/** The maximum length of the information field in a CMUX frame;
 * the maximum is 1509, which may be prone to error, or would result
 * in throwing away a lot of data were there an error, the default is
 * 32, which is somewhat constraining when large AT commands or
 * UBX-format GNSS messages may be 500 to 1024 bytes.  That said, it
 * should be small with respect to the buffer size of the thing it is
 * pouring received data into since the multiplexing protocol serialises
 * several things and, if one of them gets "stuck" because it has nowhere
 * to put its data, the ones that follow will be stuck also. So we use 128.
 */
# define U_CELL_MUX_PRIVATE_INFORMATION_LENGTH_MAX_BYTES 128
#endif

#ifndef U_CELL_MUX_PRIVATE_VIRTUAL_SERIAL_BUFFER_LENGTH_BYTES
/** A suggested length for the buffer which a virtual serial port
 * should use for receiving data from the cellular module, e.g.
 * AT commands or NMEA/UBX message.  We give this a buffer length of
 * four times the maximum I-field length to avoid the multiplexer
 * protocol getting stuck due to lack of buffer space to put
 * received frames into, giving multiplexer flow control time to
 * take effect.
 */
# define U_CELL_MUX_PRIVATE_VIRTUAL_SERIAL_BUFFER_LENGTH_BYTES (U_CELL_MUX_PRIVATE_INFORMATION_LENGTH_MAX_BYTES * 4)
#endif

/** The maximum overhead, on top of the information field length, for
 * a CMUX frame, consisting of 1 byte each for the opening and closing
 * flags, 1 byte for the address, 1 byte for control, up to 2 bytes
 * for length and one byte for FCS.
 */
#define U_CELL_MUX_PRIVATE_FRAME_OVERHEAD_MAX_BYTES (2 + 1 + 1 + 2 + 1)

/** The minimum length of a CMUX frame; one less than the maximum
 * overhead, since we may have only a single length byte.
 */
#define U_CELL_MUX_PRIVATE_FRAME_MIN_LENGTH_BYTES (U_CELL_MUX_PRIVATE_FRAME_OVERHEAD_MAX_BYTES - 1)

#ifndef U_CELL_MUX_PRIVATE_ENABLE_DISABLE_DELAY_MS
/** How long to wait after the AT command to start the mux has returned
 * OK before sending SABM, and likewise how long to wait after disabling
 * the mux before beginning an AT command, in milliseconds.
 */
# define U_CELL_MUX_PRIVATE_ENABLE_DISABLE_DELAY_MS 100
#endif

/** The maximum value of the address field.
 * Note: this should really be 0x3F/63 but in the decoding
 * process we need to avoid it being 0xF9 shifted left by 2,
 * which is 62, hence we make it 61.  Somewhat of a moot point
 * since, practically, it never goes above four.
 */
#define U_CELL_MUX_PRIVATE_ADDRESS_MAX 61

/** A wildcard address value, may be used with uCellMuxPrivateDecode().
 */
#define U_CELL_MUX_PRIVATE_ADDRESS_ANY 0xFF

/** The maximum length of the information field.
 */
#define U_CELL_MUX_PRIVATE_INFORMATION_MAX_LENGTH_BYTES 0x7FFF

#ifndef U_CELL_MUX_PRIVATE_BUFFER_LENGTH_BYTES
/** The length of the raw buffer, enough to store at least
 * one maximum-length CMUX frame on each channel.
 */
# define U_CELL_MUX_PRIVATE_BUFFER_LENGTH_BYTES ((U_CELL_MUX_PRIVATE_INFORMATION_LENGTH_MAX_BYTES +  \
                                                  U_CELL_MUX_PRIVATE_FRAME_OVERHEAD_MAX_BYTES)       \
                                                  * U_CELL_MUX_MAX_CHANNELS)
#endif

#ifndef U_CELL_MUX_PRIVATE_CONTROL_CHANNEL_INFORMATION_LENGTH_BYTES
/** Enough room to store the maximum expected control channel information-field.
 * Only MCS contents are supported and each MCS thing contains a command byte,
 * a length byte, a channel ID byte, a signals bitmap bytes and an optional
 * break signal byte, so 5 bytes.  Assumption is that a maximum of two might
 * be present for any given channel in any given information field.
 */
# define U_CELL_MUX_PRIVATE_CONTROL_CHANNEL_INFORMATION_LENGTH_BYTES (5 * U_CELL_MUX_MAX_CHANNELS * 2)
#endif

#ifndef U_CELL_MUX_PRIVATE_CONTROL_CHANNEL_BUFFER_LENGTH_BYTES
/** As #U_CELL_MUX_PRIVATE_BUFFER_LENGTH_BYTES but for the control channel.
 * Only MCS contents are supported and each MCS thing contains a command byte,
 * a length byte, a channel ID byte, a signals bitmap byte and an optional
 * break signal byte, so 5 bytes.  Assumption is that a maximum of two might
 * be present for any given channel in any given information field.
 */
# define U_CELL_MUX_PRIVATE_CONTROL_CHANNEL_BUFFER_LENGTH_BYTES (((U_CELL_MUX_PRIVATE_CONTROL_CHANNEL_INFORMATION_LENGTH_BYTES / \
                                                                   U_CELL_MUX_MAX_CHANNELS) *                                    \
                                                                  U_CELL_MUX_PRIVATE_FRAME_OVERHEAD_MAX_BYTES) +                 \
                                                                 U_CELL_MUX_PRIVATE_CONTROL_CHANNEL_INFORMATION_LENGTH_BYTES)
#endif

#ifndef U_CELL_MUX_PRIVATE_HOLDING_BUFFER_LENGTH_BYTES
/** Holding buffer, used to get things out of the underyling stream
 * and into a ring buffer; some sensible fraction of the maximum
 * information field length is probably about right, which will also
 * be big enough to hold the control channel MCS frames (5 information
 * bytes each plus frame overhead, a maximum of two of which might be
 * present per channel in a given MCS frame, so (5 + 7) * 2 * 3) and
 * must be at least as big as #U_CELL_MUX_PRIVATE_CONTROL_CHANNEL_BUFFER_LENGTH_BYTES.
 */
# define U_CELL_MUX_PRIVATE_HOLDING_BUFFER_LENGTH_BYTES_X ((U_CELL_MUX_PRIVATE_INFORMATION_LENGTH_MAX_BYTES + \
                                                            U_CELL_MUX_PRIVATE_FRAME_OVERHEAD_MAX_BYTES) / 2)
# if U_CELL_MUX_PRIVATE_HOLDING_BUFFER_LENGTH_BYTES_X < U_CELL_MUX_PRIVATE_CONTROL_CHANNEL_BUFFER_LENGTH_BYTES
#  define U_CELL_MUX_PRIVATE_HOLDING_BUFFER_LENGTH_BYTES U_CELL_MUX_PRIVATE_CONTROL_CHANNEL_BUFFER_LENGTH_BYTES
# else
#  define U_CELL_MUX_PRIVATE_HOLDING_BUFFER_LENGTH_BYTES U_CELL_MUX_PRIVATE_HOLDING_BUFFER_LENGTH_BYTES_X
# endif
#endif

#ifndef U_CELL_MUX_PRIVATE_SCRATCH_BUFFER_LENGTH_BYTES
/** A scratch buffer, used by the message decoders.  This is created
 * as part of the multiplexer context to be used like a stack variable
 * by any multiplexer function, avoiding putting a largish buffer on
 * the stack.  It must be at least as big as the maximum information
 * field length and the maximum control channel buffer length.
 */
# if U_CELL_MUX_PRIVATE_INFORMATION_LENGTH_MAX_BYTES > U_CELL_MUX_PRIVATE_CONTROL_CHANNEL_BUFFER_LENGTH_BYTES
#  define U_CELL_MUX_PRIVATE_SCRATCH_BUFFER_LENGTH_BYTES U_CELL_MUX_PRIVATE_INFORMATION_LENGTH_MAX_BYTES
# else
#  define U_CELL_MUX_PRIVATE_SCRATCH_BUFFER_LENGTH_BYTES U_CELL_MUX_PRIVATE_CONTROL_CHANNEL_BUFFER_LENGTH_BYTES
# endif
#endif

#ifndef U_CELL_MUX_PRIVATE_RX_FLOW_ON_THRESHOLD_PERCENT
/** The threshold, as a percentage of the receiveBufferSize passed to
 * the pDeviceSerial open() function, above which we tell the far end
 * that it can now send us data again.
 */
# define U_CELL_MUX_PRIVATE_RX_FLOW_ON_THRESHOLD_PERCENT 60
#endif

#ifndef U_CELL_MUX_PRIVATE_RX_FLOW_OFF_THRESHOLD_PERCENT
/** The threshold, as a percentage of the receiveBufferSize passed to
 * the pDeviceSerial open() function, below which we tell the far end
 * that it should stop sending us data.
 */
# define U_CELL_MUX_PRIVATE_RX_FLOW_OFF_THRESHOLD_PERCENT 40
#endif

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/** The types of CMUX frame, values chosen so that they can be
 * written directly to a control word in an encoded frame (with
 * poll/final bit not set).
 */
typedef enum {
    U_CELL_MUX_PRIVATE_FRAME_TYPE_NONE = 0,
    U_CELL_MUX_PRIVATE_FRAME_TYPE_SABM_COMMAND = 0x2F,
    U_CELL_MUX_PRIVATE_FRAME_TYPE_UA_RESPONSE = 0x63,
    U_CELL_MUX_PRIVATE_FRAME_TYPE_DM_RESPONSE = 0x0F,
    U_CELL_MUX_PRIVATE_FRAME_TYPE_DISC_COMMAND = 0x43,
    U_CELL_MUX_PRIVATE_FRAME_TYPE_UIH = 0xEF,
    U_CELL_MUX_PRIVATE_FRAME_TYPE_UI = 0x03
} uCellMuxPrivateFrameType_t;

/** The possible MUX channel IDs.
 */
typedef enum {
    U_CELL_MUX_PRIVATE_CHANNEL_ID_CONTROL = 0, /**< fixed by 3GPP 27.010. */
    U_CELL_MUX_PRIVATE_CHANNEL_ID_AT = 1,      /**< AT channel, common to all u-blox cellular modules. */
    U_CELL_MUX_PRIVATE_CHANNEL_ID_PPP = 2,     /**< PPP data channel, common to all u-blox cellular modules. */
    U_CELL_MUX_PRIVATE_CHANNEL_ID_MAX = U_CELL_MUX_PRIVATE_ADDRESS_MAX
} uCellMuxPrivateChannelId_t;

/** Possible states of a CMUX channel.
 */
typedef enum {
    U_CELL_MUX_PRIVATE_CHANNEL_STATE_NULL = 0,
    U_CELL_MUX_PRIVATE_CHANNEL_STATE_OPEN,
    U_CELL_MUX_PRIVATE_CHANNEL_STATE_OPEN_DISCONNECTED, /**< if the remote-end disconnects. */
    U_CELL_MUX_PRIVATE_CHANNEL_STATE_MAX_NUM
} uCellMuxPrivateChannelState_t;

/** The input/output structure for parsing some input data in search of a CMUX frame.
 */
typedef struct {
    uint8_t address; /**< set this to the wanted address field, which could be
                          #U_CELL_MUX_PRIVATE_ADDRESS_ANY; the decoding
                          process will set it to the decoded address of a CMUX
                          frame if one is found. */
    bool commandResponse; /**< this will be set to the decoded state of the
                               command/response bit of a CMUX frame if one is
                               found. */
    uCellMuxPrivateFrameType_t type; /**< this will be set to the decoded frame type
                                          of a CMUX frame if one is found. */
    bool pollFinal; /**< this will be set to the decode state of the poll/final bit
                         of a CMUX frame if one is found. */
    char *pInformation;  /**< storage for a decoded information field from a CMUX
                              frame if one is found; may be NULL. */
    size_t informationLengthBytes; /**< set this to the amount of storage at pInformation;
                                        the decoding process will set it to the number of
                                        information-field bytes in the decoded CMUX frame.
                                        This may be more than the size of pInformation,
                                        though the buffer size of pInformation will always
                                        be respected. */
    char *pBuffer;       /**< a buffer to be decoded; may be NULL if the source of
                              information to be decoded is actually a ring-buffer (which
                              works differently, see uCellMuxPrivateParseCmux()). */
    size_t bufferSize;   /**< set this to the amount of data at pBuffer. */
    size_t bufferIndex;  /**< set this to the current position in pBuffer; the decoding
                              process will set it to the next byte to be decoded from
                              pBuffer, which may be bufferSize if an error is being
                              returned. */
} uCellMuxPrivateParserContext_t;

/** The context data for CMUX mode.
 */
typedef struct {
    uCellPrivateInstance_t *pInstance; /**< need to know the instance data for UART power saving. */
    uAtClientHandle_t savedAtHandle; /**< the AT client handle we were using in normal mode. */
    int32_t underlyingStreamHandle; /**< the handle of the stream [UART] that the MUX is running on. */
    uint8_t channelGnss; /**< the CMUX channel to use for GNSS. */
    uDeviceSerial_t *pDeviceSerial[U_CELL_MUX_MAX_CHANNELS]; /**< the channels. */
    uRingBuffer_t ringBuffer; /**< the ring buffer where we put the stream from the cellular module,
                                   generic version. */
    char linearBuffer[U_CELL_MUX_PRIVATE_BUFFER_LENGTH_BYTES + 1];  /**< the linear buffer that will be
                                                                         used by ringBuffer, +1 since we
                                                                         lose one byte in the ring buffer. */
    char holdingBuffer[U_CELL_MUX_PRIVATE_HOLDING_BUFFER_LENGTH_BYTES];   /**< a temporary buffer, used to get
                                                                               stuff into ringBuffer and
                                                                               in which we hold partially
                                                                               decoded control channel stuff. */
    size_t holdingBufferIndex;                                    /**< where we are in holdingBuffer.*/
    char scratch[U_CELL_MUX_PRIVATE_SCRATCH_BUFFER_LENGTH_BYTES]; /** a scratch buffer that may be used like
                                                                      a stack variable. */
    int32_t readHandle;
    int32_t eventQueueHandle; /** an event queue to carry callbacks from the channels. */
} uCellMuxPrivateContext_t;

/** Structure to hold the user event callback for a CMUX channel.
 */
typedef struct {
    void (*pFunction)(struct uDeviceSerial_t *, uint32_t, void *);
    void *pParam;
    uint32_t filter;
} uCellMuxPrivateEventCallback_t;

/** Structure to hold stuff to do with data transfer for a channel.
 */
typedef struct {
    char *pRxBufferStart;     /**< this buffer stores the UIH information fields received. */
    bool rxBufferIsMalloced;
    size_t rxBufferSizeBytes;
    char *pRxBufferWrite;
    const char *pRxBufferRead;
    uCellMuxPrivateFrameType_t wantedResponseFrameType; /**< this will be reset to
                                                             #U_CELL_MUX_PRIVATE_FRAME_TYPE_NONE
                                                             if a response of this type is received. */
    bool discardOnOverflow;
    bool txIsFlowControlledOff; /**< remote-end doesn't want us to send to it. */
    bool rxIsFlowControlledOff; /**< we don't want the remote-end to send stuff to us. */
} uCellMuxPrivateTraffic_t;

/** The context data for a single CMUX channel.
 */
typedef struct {
    uCellMuxPrivateContext_t *pContext;
    uCellMuxPrivateChannelState_t state;
    uint8_t channel;
    bool markedForDeletion;
    uPortMutexHandle_t mutex;
    uPortMutexHandle_t mutexUserDataWrite;
    uPortMutexHandle_t mutexUserDataRead;
    uCellMuxPrivateTraffic_t traffic;
    uCellMuxPrivateEventCallback_t eventCallback;
    int32_t discTimeoutMs;
} uCellMuxPrivateChannelContext_t;

/* ----------------------------------------------------------------
 * FUNCTIONS: PRIVATE TO CELLULAR (SEE U_CELL_MUX.C)
 * -------------------------------------------------------------- */

/** Enable multiplexer mode.  Puts the cellular module's AT
 * interface into multiplexer (3GPP 27.010 CMUX) mode.  This
 * is useful when you want to access a GNSS module that is
 * connected via, or embedded inside, a cellular module as if it
 * were connected directly to this MCU via a serial interface (see
 * uCellMuxAddChannel()).  Note that this function _internally_
 * opens and uses a CMUX channel for the AT interface, you do not
 * have to do that.  The AT handle that was originally passed to
 * uCellAdd() will remain locked, the handle of the new one that is
 * created for use internally can be obtained by calling
 * uCellAtClientHandleGet(); uCellAtClientHandleGet() will always
 * return the AT handle currently in use.
 *
 * Whether multiplexer mode is supported or not depends on the cellular
 * module and the interface in use: for instance a USB interface to
 * a module does not support multiplexer mode.
 *
 * The module must be powered on for this to work.  Returns success
 * without doing anything if multiplexer mode is already enabled.
 * Multiplexer mode does not survive a power-cycle, either deliberate
 * (with uCellPwrOff(), uCellPwrReboot(), etc.) or accidental, and
 * cannot be used with 3GPP power saving (since it will also be
 * reset during module deep sleep).
 *
 * Note: if you have passed the AT handle to a GNSS instance (e.g.
 * via uGnssAdd()) it will stop working when multiplexer mode is
 * enabled (because the AT handle will have been changed), hence you
 * should enable multiplexer mode _before_ calling uGnssAdd()
 * (and, likewise, remove any GNSS instance before disabling
 * multiplexer mode).  However, if you have enabled multiplexer
 * mode it is much better to call uCellMuxAddChannel() with
 * #U_CELL_MUX_CHANNEL_ID_GNSS and then you can pass the
 * #uDeviceSerial_t handle that returns to uGnssAdd() (with the
 * transport type #U_GNSS_TRANSPORT_VIRTUAL_SERIAL) and you will
 * have streamed position.
 *
 * Note: gUCellPrivateMutex should be locked before this is called.
 *
 * @param[in] pInstance  a pointer to the cellular instance.
 * @return               zero on success or negative error code
 *                       on failure.
 */
int32_t uCellMuxPrivateEnable(uCellPrivateInstance_t *pInstance);

/** Determine if the multiplexer is currently enabled.
 *
 * Note: gUCellPrivateMutex should be locked before this is called.
 *
 * @param[in] pInstance  a pointer to the cellular instance.
 * @return               true if the multiplexer is enabled,
 *                       else false.
 */
bool uCellMuxPrivateIsEnabled(uCellPrivateInstance_t *pInstance);

/** Add a multiplexer channel; may be called after uCellMuxEnable()
 * has returned success in order to, for instance, create a virtual
 * serial port to a GNSS chip inside a SARA-R422M8S or SARA-R510M8S
 * module.  The virtual serial port handle returned in *ppDeviceSerial
 * can be used in #uDeviceCfg_t to open the GNSS device using the
 * uDevice API, or it can be passed to uGnssAdd() (with the transport
 * type #U_GNSS_TRANSPORT_VIRTUAL_SERIAL) if you prefer to use the
 * uGnss API the hard way.
 *
 * If the channel is already open, this function returns success
 * without doing anything.  An error is returned if uCellMuxEnable()
 * has not been called.
 *
 * Note: there is a known issue with SARA-R5 modules where, if a GNSS
 * multiplexer channel is opened, closed, and then re-opened the GNSS
 * chip will be unresponsive.  For that case, please open the GNSS
 * multiplexer channel once at start of day.
 *
 * UART POWER SAVING: when UART power saving is enabled in the module
 * any constraints arising will also apply to a multiplexer channel;
 * specifically, if a DTR pin is not used to wake-up the module, i.e.
 * the module supports and is using the "wake up on TX activity" mode
 * of UART power saving then, though the AT interface will continue
 * to work correctly (as it knows to expect loss of the first few
 * characters of an AT string), the other multiplexer channels have
 * the same restriction and have no such automated protection. Hence
 * if you (a) expect to use a multiplexer channel to communicate with
 * a GNSS chip in a cellular module and (b) are not able to use a DTR
 * pin to wake the module up from power-saving, then you should call
 * uCellPwrDisableUartSleep() to disable UART sleep while you run the
 * multiplexer channel (and uCellPwrEnableUartSleep() to re-enable it
 * afterwards).
 *
 * NOTES ON DEVICE SERIAL OPERATION: the operation of *pDeviceSerial
 * is constrained in certain ways, since what you have is not a real
 * serial port, it is a virtual serial port which has hijacked some
 * of the functionality of the physical serial port that was
 * previously running, see notes below, but particularly flow control,
 * or not taking data out of one or more multiplexed serial ports fast
 * enough, can have an adverse effect on other multiplexed serial ports.
 * This is difficult to avoid since they are on the same transport.  Hence
 * it is important to service your multiplexed serial ports often or,
 * alternatively, you may call serialDiscardOnFlowControl() with true
 * on any serial port where you are happy for any overruns to be
 * discarded (e.g. the GNSS one), so that it cannot possibly interfere
 * with others (e.g. the AT command one).
 *
 * The stack size and priority of any event serial callbacks are not
 * respected: what you end up with is #U_CELL_MUX_CALLBACK_TASK_PRIORITY
 * and #U_CELL_MUX_CALLBACK_TASK_STACK_SIZE_BYTES since a common
 * event queue is used for all serial devices.
 *
 * Note: gUCellPrivateMutex should be locked before this is called.
 *
 * @param[in] pInstance       a pointer to the cellular instance.
 * @param channel             the channel number to open; channel
 *                            numbers are module-specific, however
 *                            the value #U_CELL_MUX_CHANNEL_ID_GNSS
 *                            can be used, in all cases, to open a
 *                            channel to an embedded GNSS chip.
 *                            Note that channel zero is reserved
 *                            for management operations and channel
 *                            one is the existing AT interface;
 *                            neither value can be used here.
 * @param[out] ppDeviceSerial a pointer to a place to put the
 *                            handle of the virtual serial port
 *                            that is the multiplexer channel.
 * @return                    zero on success or negative error
 *                            code on failure.
 */
int32_t uCellMuxPrivateAddChannel(uCellPrivateInstance_t *pInstance,
                                  int32_t channel,
                                  uDeviceSerial_t **ppDeviceSerial);

/** Disable CMUX on the given cellular instance.  This does NOT free
 * memory to ensure thread safety; only uCellMuxPrivateRemoveContext()
 * frees memory.  Note that this may cause the atHandle in pInstance to
 * change, so if you have a local copy of it you will need to refresh
 * it once this function returns.
 *
 * Note: gUCellPrivateMutex should be locked before this is called.
 *
 * @param[in] pInstance a pointer to the cellular instance.
 */
int32_t uCellMuxPrivateDisable(uCellPrivateInstance_t *pInstance);

/** Get the serial device for the given channel.
 *
 * @param[in] pContext the mux context.
 * @param channel      the channel number.
 * @return             the serial device.
 */
uDeviceSerial_t *pUCellMuxPrivateGetDeviceSerial(uCellMuxPrivateContext_t *pContext,
                                                 uint8_t channel);

/** Close a CMUX channel.  This does NOT free memory to ensure
 * thread safety; only uCellMuxPrivateRemoveContext() frees memory.
 *
 * Note: gUCellPrivateMutex should be locked before this is called.
 *
 * @param[in] pContext the mux context.
 * @param channel      the channel number to close.
 */
void uCellMuxPrivateCloseChannel(uCellMuxPrivateContext_t *pContext, uint8_t channel);

/* ----------------------------------------------------------------
 * FUNCTIONS: 3GPP 27.010 CMUX ENCODE/DECODE (SEE U_CELL_MUX_PRIVATE.C)
 * -------------------------------------------------------------- */

/** Encode a 3GPP 27.010 mux frame.
 *
 * @param address                 the address of the data link.
 * @param type                    the frame type to encode.
 * @param pollFinal               the state of the poll/final bit to encode.
 * @param[in] pInformation        the contents for the information
 *                                field; must be non-NULL if
 *                                informationLengthBytes is not zero.
 * @param informationLengthBytes  the number of bytes at pInformation;
 *                                may be zero, max
 *                                #U_CELL_MUX_PRIVATE_INFORMATION_MAX_LENGTH_BYTES.
 * @param[out] pBuffer            a pointer to a place to put the encoded
 *                                CMUX frame; must be at least
 *                                informationLengthBytes +
 *                                #U_CELL_MUX_PRIVATE_FRAME_OVERHEAD_MAX_BYTES.
 * @return                        on success the number of bytes written
 *                                to pBuffer, else negative error code.
 */
int32_t uCellMuxPrivateEncode(uint8_t address, uCellMuxPrivateFrameType_t type,
                              bool pollFinal, const char *pInformation,
                              size_t informationLengthBytes, char *pBuffer);

/** Parse [a ring-buffer] for a CMUX frame.  The function signature is such that
 * this can be used as a ring-buffer parser; pUserParam MUST be a pointer
 * to a structure of type #uCellMuxPrivateParserContext_t.  However, if
 * parseHandle is NULL, data will be taken out of the linear buffer in
 * #uCellMuxPrivateParserContext_t instead, allowing the buffer to be parsed
 * (e.g. for control frames) instead of the ring-buffer, see
 * uCellMuxPrivateParseCmux().  On entry the type field in
 * #uCellMuxPrivateParserContext_t should be set to
 * #U_CELL_MUX_PRIVATE_FRAME_TYPE_NONE, on return it will be set to the decoded
 * CMUX frame type if one was found.  The address field of
 * #uCellMuxPrivateParserContext_t may be set to a specific address if only
 * that address is of interest, else it should be set to
 * #U_CELL_MUX_PRIVATE_ADDRESS_ANY and this will be replaced by the address of
 * the decoded CMUX frame when one is found.
 *
 * @param parseHandle    the parse handle of the ring buffer to read from,
 *                       NULL to use the pBuffer field of the second parameter
 *                       instead.
 * @param[in] pUserParam the user parameter, a void * in order to match
 *                       the function signature of a generic ring-buffer parser
 *                       so that this function can be used with
 *                       uRingBufferParseHandle() however it _must_ be a pointer
 *                       to a structure of type #uCellMuxPrivateParserContext_t.
 * @return               a positive integer on success, else
 *                       #U_ERROR_COMMON_NOT_FOUND if nothing was found or
 *                       #U_ERROR_COMMON_TIMEOUT if a sniff of a thing was found
 *                       but not enough to decode.
 */
int32_t uCellMuxPrivateParseCmux(uParseHandle_t parseHandle, void *pUserParam);

/* ----------------------------------------------------------------
 * FUNCTIONS: MISC (SEE U_CELL_MUX_PRIVATE.C)
 * -------------------------------------------------------------- */

/** Copy the settings of one AT client into another AT client.
 *
 * @param atHandleSource       the source AT client.
 * @param atHandleDestination  the destination AT client.
 * @return                     zero on success else negative error code.
 */
int32_t uCellMuxPrivateCopyAtClient(uAtClientHandle_t atHandleSource,
                                    uAtClientHandle_t atHandleDestination);

/** Remove the CMUX context for the given cellular instance.  If CMUX
 * is active it will be disabled first.  Note that this may cause the
 * atHandle in pInstance to change, so if you have a local copy of it
 * you will need to refresh it once this function returns.
 *
 * Note: gUCellPrivateMutex should be locked before this is called.
 *
 * @param[in] pInstance a pointer to the cellular instance.
 */
void uCellMuxPrivateRemoveContext(uCellPrivateInstance_t *pInstance);

#ifdef __cplusplus
}
#endif

#endif // _U_CELL_MUX_PRIVATE_H_

// End of file
