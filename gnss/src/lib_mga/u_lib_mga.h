/******************************************************************************
 * Copyright 2013-2023 u-blox AG, Thalwil, Switzerland
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
 *
 ******************************************************************************
 *
 * Project: libMGA
 * Purpose: Library providing functions to help a host application to download
 *          MGA assistance data and pass it on to a u-blox GNSS receiver.
 *
 *****************************************************************************/

// MODIFIED: gate changed to match renamed file
#ifndef __U_LIB_MGA__
#define __U_LIB_MGA__

// MODIFIED: header file renamed
#include "u_lib_mga_common_types.h"
// MODIFED: quotes instead of <> for consistency with ubxlib
#include "time.h"

///////////////////////////////////////////////////////////////////////////////
#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

#define LIBMGA_VERSION      "19.03DEV"   //!< libMGA version

#ifndef MGA_USER_AGENT
#define MGA_USER_AGENT          "libMga" LIBMGA_VERSION
#endif

#define MGA_GNSS_GPS        0x01    //!< MGA_GNSS_TYPE_FLAGS bit to specify GPS assistance data required.
#define MGA_GNSS_GLO        0x02    //!< MGA_GNSS_TYPE_FLAGS bit to specify GLONASS assistance data required.
#define MGA_GNSS_QZSS       0x04    //!< MGA_GNSS_TYPE_FLAGS bit to specify QZSS assistance data required.
#define MGA_GNSS_GALILEO    0x08    //!< MGA_GNSS_TYPE_FLAGS bit to specify Galileo assistance data required.
#define MGA_GNSS_BEIDOU     0x10    //!< MGA_GNSS_TYPE_FLAGS bit to specify BeiDou assistance data required.

    //! Type definition for the flags specifying which GNSS system assistance data is required by the client.
    typedef UBX_U1 MGA_GNSS_TYPE_FLAGS;

#define MGA_DATA_EPH    0x01    //!< MGA_DATA_TYPE_FLAGS bit to specify Ephemeris data required.
#define MGA_DATA_ALM    0x02    //!< MGA_DATA_TYPE_FLAGS bit to specify Almanac data required.
#define MGA_DATA_AUX    0x04    //!< MGA_DATA_TYPE_FLAGS bit to specify Auxiliary (Ionospheric & UTC) data required.
#define MGA_DATA_POS    0x08    //!< MGA_DATA_TYPE_FLAGS bit to specify Position approximation required.

    //! Type definition for the flags which specify the type of assistance data requested by the client.
    typedef UBX_U1 MGA_DATA_TYPE_FLAGS;

#define MGA_FLAGS_USE_POSITION      0x01    //!< MGA_FLAGS_USE bit to specify position fields are valid.
#define MGA_FLAGS_USE_LATENCY       0x02    //!< MGA_FLAGS_USE bit to specify latency field is valid.
#define MGA_FLAGS_USE_TIMEACC       0x04    //!< MGA_FLAGS_USE bit to specify time accuracy field is valid.
#define MGA_FLAGS_USE_LEGACY_AIDING 0x08    //!< MGA_FLAGS_USE bit to specify legacy aiding data requested.


    //! Type definition for the flags that specify which optional data fields are valid in the MgaOnlineServerConfig structure.
    typedef UBX_U1 MGA_FLAGS_USE;

    //! libMga API result codes.
    /*! Calls to libMga functions always return one of these listed codes.
    */
    typedef enum
    {
        MGA_API_OK = 0,             //!< Call to API was successful.
        MGA_API_CANNOT_CONNECT,     //!< Could not connect to requested server.
        MGA_API_CANNOT_GET_DATA,    //!< Could not retrieve requested data from server.
        MGA_API_CANNOT_INITIALIZE,  //!< Could not initialize the libMga.
        MGA_API_ALREADY_RUNNING,    //!< MGA session is already running.
        MGA_API_ALREADY_IDLE,       //!< MGA session is already idle.
        MGA_API_IGNORED_MSG,        //!< No processing was performed on the supplied message.
        MGA_API_BAD_DATA,           //!< Parts of the data supplied to the MGA library are not properly formed UBX messages.
        MGA_API_OUT_OF_MEMORY,      //!< A memory allocation failed.
        MGA_API_NO_MGA_INI_TIME,    //!< The first UBX message in the MGA Online data block is not MGA_INI_TIME (it should be).
        MGA_API_NO_DATA_TO_SEND,    //!< The data block does not contain any MGA messages.
        MGA_API_INIT_SSL_FAIL,      //!< mbedTLS library initialization failed.
        MGA_API_INIT_SSL_CERT_FAIL, //!< SSL certificate initialization failed.
        MGA_API_CONNECT_SSL_FAIL,   //!< Connect to server failed.
        MGA_API_CONFIG_SSL_FAIL,    //!< SSL configuration failed.
        MGA_API_SETUP_SSL_FAIL,     //!< SSL setup failed.
        MGA_API_SET_HOST_SSL_FAIL,  //!< SSL setting hostname failed.
        MGA_API_HANDSHAKE_SSL_FAIL, //!< SSL handshake failed.
        MGA_API_WRITE_SSL_FAIL,     //!< SSL write of request message failed.
        MGA_API_READ_SSL_FAIL,      //!< SSL read of response message failed.
    } MGA_API_RESULT;

    //! Progress event types that can be generated from the libMga.
    /*! The libMga can send progress events to the client application by calling the 'evtProgress' callback function.
        The following lists the type of possible progress events.
        */
    typedef enum
    {
        MGA_PROGRESS_EVT_START,                 //!< MGA message transfer session has started.
        MGA_PROGRESS_EVT_FINISH,                //!< MGA message transfer session has completed successfully.
        MGA_PROGRESS_EVT_MSG_SENT,              //!< A MGA message has been sent to the receiver.
        MGA_PROGRESS_EVT_MSG_TRANSFER_FAILED,   //!< A MGA message transfer has failed.
        MGA_PROGRESS_EVT_MSG_TRANSFER_COMPLETE, //!< A MGA message has be successfully transferred.

        MGA_PROGRESS_EVT_TERMINATED,            //!< The current message transfer session has been terminated.

        MGA_PROGRESS_EVT_SERVER_CONNECTING,     //!< Connecting to server.
        MGA_PROGRESS_EVT_UNKNOWN_SERVER,        //!< Could not find server.
        MGA_PROGRESS_EVT_SERVER_CONNECT,        //!< Connected to a server.
        MGA_PROGRESS_EVT_SERVER_CANNOT_CONNECT, //!< Cannot connect to server.
        MGA_PROGRESS_EVT_REQUEST_HEADER,        //!< A request has been made to an AssistNow service for MGA data.
        MGA_PROGRESS_EVT_RETRIEVE_DATA,         //!< AssistNow MGA data is being received.
        MGA_PROGRESS_EVT_SERVICE_ERROR,         //!< There was an error in the communication with the MGA service. The event info object will supply more detailed information.

        MGA_PROGRESS_EVT_LEGACY_AIDING_STARTUP,             //!< Legacy aiding flash data transfer session has started.
        MGA_PROGRESS_EVT_LEGACY_AIDING_STARTUP_FAILED,      //!< Legacy aiding flash data transfer session start up failed.
        MGA_PROGRESS_EVT_LEGACY_AIDING_STARTUP_COMPLETED,   //!< Legacy aiding flash data transfer session startup has completed.
        MGA_PROGRESS_EVT_LEGACY_AIDING_FLASH_BLOCK_SENT,    //!< A legacy aiding flash data transfer message has been sent to the receiver.
        MGA_PROGRESS_EVT_LEGACY_AIDING_FLASH_BLOCK_FAILED,  //!< A legacy aiding flash data transfer message has not been acknowledged.
        MGA_PROGRESS_EVT_LEGACY_AIDING_FLASH_BLOCK_COMPLETE,//!< A legacy aiding flash data transfer message has been successfully transferred.
        MGA_PROGRESS_EVT_LEGACY_AIDING_FINALIZE_START,      //!< Legacy aiding flash data transfer session completion sequence has started.
        MGA_PROGRESS_EVT_LEGACY_AIDING_FINALIZE_FAILED,     //!< Legacy aiding flash data transfer session completion sequence has failed.
        MGA_PROGRESS_EVT_LEGACY_AIDING_FINALIZE_COMPLETED,  //!< Legacy aiding flash data transfer session completion sequence has finished.

        MGA_PROGRESS_EVT_LEGACY_AIDING_SERVER_STARTED,              //!< Legacy aiding server support has been started.
        MGA_PROGRESS_EVT_LEGACY_AIDING_SERVER_STOPPED,              //!< Legacy aiding server support has been stopped.
        MGA_PROGRESS_EVT_LEGACY_AIDING_SERVER_REQUEST_RECEIVED,     //!< A legacy aiding request (incoming UBX-AID-ALPSRV message) has been received from the receiver.
        MGA_PROGRESS_EVT_LEGACY_AIDING_SERVER_REQUEST_COMPLETED,    //!< A legacy aiding request has been handled and a response (outgoing UBX-AID-ALPSRV message) has been sent to the receiver.
        MGA_PROGRESS_EVT_LEGACY_AIDING_SERVER_UPDATE_RECEIVED,      //!< A legacy aiding data update message has been received from the received from the receiver.
        MGA_PROGRESS_EVT_LEGACY_AIDING_SERVER_UPDATE_COMPLETED,     //!< Finished processing the previously reported legacy aiding data update message.
        MGA_PROGRESS_EVT_LEGACY_AIDING_REQUEST_FAILED_NO_MEMORY,    //!< Could not send the reponse to a data request due to lack of memory.
        MGA_PROGRESS_EVT_LEGACY_AIDING_REQUEST_FAILED_ID_MISMATCH   //!< Could not send a response to a data request due to a mismatch if File Ids.
    } MGA_PROGRESS_EVENT_TYPE;

    //! Defines the possible states the libMga maintains for each assistance message being processed.
    /*! Each message messages given to the libMga for processing, is assigned a state, which changes
        as the message progresses through the system. These possible state are list here.
        */
    typedef enum
    {
        MGA_MSG_WAITING_TO_SEND,                //!< Initial state of a message. Waiting to be transmitted to receiver.
        MGA_MSG_WAITING_FOR_ACK,                //!< Message has been sent to receiver. libMga now waiting for some kind
        //!< of acknowledgment from the receiver.
        MGA_MSG_WAITING_FOR_RESEND,             //!< Receiver has failed to receive the message. libMga is waiting for
        //!< a suitable opportunity to resend the message.
        MGA_MSG_RECEIVED,                       //!< Message has successfully been received by the receiver.
        MGA_MSG_FAILED,                         //!< Receiver has failed to receive the message. libMga will not try to resend.
        MGA_MSG_WAITING_FOR_ACK_SECOND_CHANCE   //!< Waiting for ACK after receiver has been 'nudged'. Flash update only.
    } MGA_MSG_STATE;

    //! Defines the possible flow control schemes for transferring MGA data between host and receiver.
    typedef enum
    {
        MGA_FLOW_SIMPLE,        //!< For each message transferred to the receiver, libMga waits for an ACK before sending the next. Reliable but slow.
        MGA_FLOW_NONE,          //!< No flow control. libMga sends MGA data to receiver as fast as possible. Fast but not necessarily reliable.
        MGA_FLOW_SMART          //!< Initially a burst of messages is sent, that will fit into the receiver's 1000 byte RX buffer. Then for every ACK received, the next message is sent.
    } MGA_FLOW_CONTROL_TYPE;

    //! Definitions of the possible states libMga is in.
    /*! Depending on what the libMga is doing, it could be in one of the following possible states.
    */
    typedef enum
    {
        MGA_IDLE,                       //!< Library is not processing any messages, or expecting any more to process.
        MGA_ACTIVE_PROCESSING_DATA,     //!< Library is busy. There are still messages to process.
        MGA_ACTIVE_WAITING_FOR_DATA     //!< Library has processed successfully all messages given to it, but is expecting more.
    } MGA_LIB_STATE;

    //! Definitions of possible reasons for receiving a NAK response from the receiver.
    typedef enum
    {
        // Failed reason code from receiver
        MGA_FAILED_REASON_CODE_NOT_SET,     //!< The is currently no NAK reason code.
        MGA_FAILED_NO_TIME,                 //!< The receiver doesn't know the time so can't use the data (to resolve this an UBX-MGA-INI-TIME message should be supplied first).
        MGA_FAILED_VERSION_NOT_SUPPORTED,   //!< The message version is not supported by the receiver.
        MGA_FAILED_SIZE_VERSION_MISMATCH,   //!< The message size does not match the message version.
        MGA_FAILED_COULD_NOT_STORE,         //!< The message data could not be stored to the receiver's database.
        MGA_FAILED_RECEIVER_NOT_READY,      //!< The receiver is not ready to use the message data.
        MGA_FAILED_MESSAGE_UNKNOWN,         //!< The message type is unknown.

        // Failed reason code generated by libMga
        MGA_FAILED_REASON_TOO_MANY_RETRIES = 1000,  //!< Too many retries have occurred for this message.
        MGA_FAILED_REASON_LEGACY_NO_ACK = 1001      //!< No acknowledgement message received from receiver for the last legacy aiding message sent.
    } MGA_FAILED_REASON;


    //! These enumerations reflect the types of adjustments that can be made to the MGA-INI-TIME message.
    typedef enum
    {
        MGA_TIME_ADJUST_ABSOLUTE,   //!< The supplied time should be used to replace the time in the MGA-INI-TIME message.
        MGA_TIME_ADJUST_RELATIVE    //!< The supplied time should be used to adjust the MGA-INI-TIME message. Only the hours, minutes and seconds fields are valid.
    } MGA_TIME_ADJUST_TYPE;

    //! Function definition for the progress event callback handler.
    typedef void(*EvtProgress)(MGA_PROGRESS_EVENT_TYPE evtType,    //!< Progress event type.
                               const void* pContext,               //!< Pointer to context information supplied to the MGA library in 'mgaInitialize'.
                               const void* pEvtInfo,               //!< Pointer to event information object. See application note for more detailed description.
                               UBX_I4 evtInfoSize                  //!< Size of event information object.
                               );

    //! Function definition for the write device callback handler.
    typedef void(*EvtWriteDevice)(const void* pContext,            //!< Pointer to the context information supplied to the libMga in 'mgaInitialize'.
                                  const UBX_U1* pData,             //!< Pointer to UBX message to write.
                                  UBX_I4 iSize                     //!< Number of bytes to write.
                                  );

    //! Event handler jump table.
    /*! This structure defines the jump table of pointers to callback functions for implementing handlers
        to events generated from libMga. This jump table has to be supplied by the application to libMga.
        */
    typedef struct
    {
        EvtProgress     evtProgress;        //!< Pointer to the 'progress' event handler function.
        EvtWriteDevice  evtWriteDevice;     //!< Pointer to the 'write device data' event handler function.
    } MgaEventInterface;

    //! Flow control configuration structure.
    /*! This structure supplies 'flow' configuration information to libMga.
        Flow information determines how libMga will manage the flow of MGA messages between the host and the receiver.
        */
    typedef struct
    {
        UBX_I4                  msgTimeOut;     //!< Time, in ms, libMga will wait for a message acknowledgment before marking the message as needing to be re-sent.
        UBX_I4                  msgRetryCount;  //!< The number of retries that a message can have before being declared a failure.
        MGA_FLOW_CONTROL_TYPE   mgaFlowControl; //!< The type of flow control to use for transferring MGA data to the receiver.
        // MODIFIED: add mgaCfgVal to indicate that the module requires the new UBX-CFG-VAL interface (M10 and beyond)
        bool                    mgaCfgVal; //!< If true then configurion of flow control is carried out using UBX-CFG-VAL instead of UBX-CFG-NAVX5
    } MgaFlowConfiguration;

    // MODIFIED: the members of this structure have been re-ordered to put the smallest at the end,
    // most likely to lead to efficient structure packing
    //! Message information structure.
    /*! For each UBX message that needs to be transferred to the receiver, a message info structure instance is maintained by the libMga.
        These are used to manage the transfer of UBX messages to the receiver.
        When the progress events MGA_PROGRESS_EVT_MSG_SENT, MGA_PROGRESS_EVT_MSG_TRANSFER_COMPLETE or MGA_PROGRESS_EVT_MSG_TRANSFER_FAILED
        are generated, the 'pEvtinfo' parameter is a pointer to the message information structure of the respective UBX message.
        */
    typedef struct
    {
        time_t              timeOut;            //!< The time in the future when the UBX message is considered to have been lost and not made it to the receiver.
        const UBX_U1*       pMsg;               //!< Pointer to the start of the UBX message.
        struct                                  //!< Fields related to MGA message transfers
        {
            UBX_U1              msgId;              //!< UBX-MGA Message Id.
            UBX_U1              mgaPayloadStart[4]; //!< First four bytes of the UBX message payload.
        } mgaMsg;
        UBX_U2              msgSize;            //!< The length in bytes of the UBX message.
        UBX_U2              sequenceNumber;     //!< Sequence number (order) of the UBX message. Starts from zero.
        MGA_MSG_STATE       state;              //!< Current state of the UBX message.
        MGA_FAILED_REASON   mgaFailedReason;    //!< If this UBX message fails to be accepted by the receiver, this is the reason code.
        UBX_U1              retryCount;         //!< The number of times the UBX message has been re-sent to the receiver.
    } MgaMsgInfo;

    //! Definitions of the possible errors from the AssistNow services.
    /*! Definitions of the possible errors that can occur when retrieving MGA data from the AssistNow services.
    */
    typedef enum
    {
        MGA_SERVICE_ERROR_NOT_HTTP_HEADER,  //!< No HTTP header was received from service.
        MGA_SERVICE_ERROR_NO_RESPONSE_CODE, //!< The received HTTP header does not contain a response code.
        MGA_SERVICE_ERROR_BAD_STATUS,       //!< The received HTTP header's response code is not 200 (i.e. not ok).
        MGA_SERVICE_ERROR_NO_LENGTH,        //!< The received HTTP header does not contain a contents length field.
        MGA_SERVICE_ERROR_ZERO_LENGTH,      //!< The received HTTP header content length field is zero. i.e no AssistNow data.
        MGA_SERVICE_ERROR_LENGTH_TOO_BIG,   //!< The received HTTP header content length is too big.
        MGA_SERVICE_ERROR_NO_CONTENT_TYPE,  //!< The received HTTP header does not contain a content type field. So we don't know if the data is going to be MGA data.
        MGA_SERVICE_ERROR_NOT_UBX_CONTENT,  //!< The received HTTP header's content type field is not UBX.
        MGA_SERVICE_ERROR_PARTIAL_CONTENT,   //!< The amount of AssistNow data received is less than specified in the header's content length field.
        MGA_SERVICE_ERROR_INIT_SSL,         //!< Error when initializing SSL.
        MGA_SERVICE_ERROR_INIT_CERT_SSL,    //!< Error when initializing certificates.
        MGA_SERVICE_ERROR_CONNECT_SSL,      //!< SSL connection failed.
        MGA_SERVICE_ERROR_CONFIGURE_SSL,    //!< SSL configuration failed.
        MGA_SERVICE_ERROR_SETUP_SSL,        //!< SSL setup failed.
        MGA_SERVICE_ERROR_HOSTNAME_SSL,     //!< SSL setting hostname failed.
        MGA_SERVICE_ERROR_HANDSHAKE_SSL,    //!< SSL handshake failed.
        MGA_SERVICE_ERROR_VERIFY_SSL,       //!< SSL server certificate verification failed.
        MGA_SERVICE_ERROR_WRITE_SSL,        //!< SSL write request error.
        MGA_SERVICE_ERROR_READ_SSL,         //!< SSL read response error.
    } MgaServiceErrors;

    //! Event information structure associated with the MGA_PROGRESS_EVT_SERVICE_ERROR progress event type.
    /*! Whenever a MGA_PROGRESS_EVT_SERVICE_ERROR event is generated, a pointer to this structure is passed as the
        'pEvtInfo' parameter.
        */
    typedef struct
    {
        MgaServiceErrors errorType;     //!< AssistNow service error type.
        UBX_U4 httpRc;                  //!< The HTTP response code (if available).
        char errorMessage[100];         //!< The HTTP error message if the the HTTP response code is not 200.
    } EvtInfoServiceError;

    //! The reasons why a transfer session has been terminated.
    typedef enum
    {
        TERMINATE_HOST_CANCEL,              //!< Host software cause the termination by calling the mgaSessionStop function.
        TERMINATE_RECEIVER_NAK,             //!< The receiver sent a NAK to the host. This only applies to transferring MGA data to flash.
        TERMINATE_RECEIVER_NOT_RESPONDING,  //!< The termination was caused by the receiver not responding to messages.
        TERMINATE_PROTOCOL_ERROR            //!< Termination caused by libMga receiving an unexpected ACK.
    } EVT_TERMINATION_REASON;

    typedef struct
    {
        const char* strPrimaryServer;       //!< Pointer to string containing the FQDN of the primary server.
        const char* strSecondaryServer;     //!< Pointer to string containing the FQDN of the secondary server.
        const char* strServerToken;         //!< Pointer to the string containing the service access token.
        MGA_GNSS_TYPE_FLAGS gnssTypeFlags;  //!< Requested GNSS data - GPS, GLONASS, QZSS etc.
        MGA_DATA_TYPE_FLAGS dataTypeFlags;  //!< Requested assistance data types - Ephemeris, Almanac, Iono etc.
        MGA_FLAGS_USE useFlags;             //!< Flags specifying what optional data is to be used.
        // The fields starting intX1e7 below are MODIFIED from being doubles to being integers multiplied by 10,000,000.
        int intX1e7Latitude;                //!< Latitude in 10,000,000ths of a degree to be used for filtering returned assistance data.
        int intX1e7Longitude;               //!< Longitude in  10,000,000ths of a degree to be used for filtering returned assistance data.
        // The fields starting intX1e3 below are MODIFIED from being doubles to being integers multiplied by 1000.
        int intX1e3Altitude;                //!< Altitude in millimetres to be used for filtering returned assistance data.
        int intX1e3Accuracy;                //!< Accuracy in millimetres to be used for filtering returned assistance data.
        int intX1e3Latency;                 //!< Time in milliseconds to be added to any requested time assistance data.
        int intX1e3TimeAccuracy;            //!< Time in milliseconds, provided to service, to mark the accuracy of time assistance data.
        bool bFilterOnPos;                  //!< If true, use the position information (lat/lon/alt/acc) to filter returned assistance data.
        bool bValidateServerCert;           //!< If true the server certificate gets validated when SSL is used.
        const char* strFilterOnSv;          //!< Reserved. Set to NULL.
        void* pInternal;                    //!< Reserved. Set to NULL.
        // MODIFIED: the fields below are added versus the original libMga.
        int encodedMessageLength;           //!< Written by mgaBuildOnlineRequestParams().
    } MgaOnlineServerConfig;

    //! Offline service configuration information structure.
    /*! When obtaining MGA data from the Offline service, this structure is used to supply information on where the service
        is located and what data is being requested.
        */
    typedef struct
    {
        const char* strPrimaryServer;       //!< Pointer to string containing the FQDN of the primary server.
        const char* strSecondaryServer;     //!< Pointer to string containing the FQDN of the secondary server.
        const char* strServerToken;         //!< Pointer to the string containing the service access token.
        MGA_GNSS_TYPE_FLAGS gnssTypeFlags;  //!< Requested GNSS data - GPS, GLONASS, QZSS etc.
        MGA_GNSS_TYPE_FLAGS almFlags;       //!< Requested almanac data from GNSS - GPS, GLONASS, QZSS, etc.
        int period;                         //!< The number of weeks into the future the MGA data should be valid for. Min 1, max 5.
        int resolution;                     //!< The resolution of the MGA data: 1=every day, 2=every other day, 3=every third day.
        void* pInternal;                    //!< Reserved for u-blox internal use. Set to NULL.
        MGA_FLAGS_USE useFlags;             //!< Flags specifying what additional features to use.
        int numofdays;                      //!< Number of requested days of Almanac Plus data needed.
        bool bValidateServerCert;           //!< If true the server certificate gets validated when SSL is used.
        // MODIFIED: the fields below are added versus the original libMga.
        int encodedMessageLength;           //!< Written by mgaBuildOfflineRequestParams().
    } MgaOfflineServerConfig;


    //! Time adjustment structure used by libMga.
    /*! The transfer of MGA data to the receiver typically includes the transfer of a UBX-MGA-INI-TIME message.
        If the application needs to specify or adjust the time contained in this message, then the time adjustment
        structure is used to pass this time, either as an absolute time or a time modification, to the appropriate libMga API function.
        */
    typedef struct
    {
        MGA_TIME_ADJUST_TYPE mgaAdjustType; //!< Type of time adjustment. Relative or absolute.
        UBX_U2 mgaYear;                     //!< Year i.e. 2013
        UBX_U1 mgaMonth;                    //!< Month, starting at 1.
        UBX_U1 mgaDay;                      //!< Day, starting at 1.
        UBX_U1 mgaHour;                     //!< Hour, from 0 to 23.
        UBX_U1 mgaMinute;                   //!< Minute, from 0 to 59.
        UBX_U1 mgaSecond;                   //!< Seconds, from 0 to 59.
        UBX_U2 mgaAccuracyS;                //!< Accuracy of time - Seconds part
        UBX_U2 mgaAccuracyMs;               //!< Accuracy of time - Milli-seconds part
    } MgaTimeAdjust;

    //! Position adjustment structure used by libMga.
    /*! The transfer of MGA data to the receiver typically includes the transfer of a UBX-MGA-INI-POS message.
    If the application needs to specify or adjust the Position contained in this message, then the time adjustment
    structure is used to pass this position to the appropriate libMga API function.
    */
    typedef struct
    {
        // MODIFIED: UBX_R8 -> UBX_I4
        UBX_I4 mgaLatX1e7;                  //!< Latitude, 10 millionths of a degree
        UBX_I4 mgaLonX1e7;                  //!< Longitude, 10 millionths of a degree
        UBX_I4 mgaAlt;                      //!< Altitude, in cm.
        UBX_U4 mgaAcc;                      //!< Accuracy of position, in cm
    } MgaPosAdjust;

    //! Structure definition of a legacy aiding data request packet
    /*! This is the structure defining the header for a legacy aiding data request. It resides at the beginning of
        the payload of a UBX-AID-ALPSRV message, which is sent from the receiver.
        */
    typedef struct
    {
        UBX_U1 idSize;      //!< identifier size [bytes]
        UBX_U1 type;        //!< data type
        UBX_U2 ofs;         //!< requested data offset [16bit words]
        UBX_U2 size;        //!< requested data size [16bit words]
        UBX_U2 fileId;      //!< submitting ALP file id - Fill in by host when responding
        UBX_U2 dataSize;    //!< submitted data size [bytes] - Fill in by host when responding
        UBX_U1 alpsvix;     //!< ALP SV index
        UBX_U1 src;         //!< requested data source
        UBX_U4 tow : 20;    //!< reference TOW
        UBX_U4 wno : 12;    //!< reference week number
    } LegacyAidingRequestHeader;

    //! Structure definition of a legacy aiding data update packet
    /*! This is the structure defining the header for a legacy aiding data update packet. It resides at the beginning of
        the payload of a UBX-AID-ALPSRV message, which is sent from the receiver.
        The data follows on immediately after this header.
        */
    typedef struct
    {
        UBX_U1 idSize;      //!< identifier size [bytes]
        UBX_U1 type;        //!< data type
        UBX_U2 ofs;         //!< data offset [16bit words]
        UBX_U2 size;        //!< data size [16bit words]
        UBX_U2 fileId;      //!< submitting ALP file id
        //!< data follows immediately after the header
    } LegacyAidingUpdateDataHeader;

    //! Structure defining the legacy aiding data's header.
    /*! The legacy aiding data that is downloaded from the MGA service has a header at it's beginning.
        This structure defines the format of that header.
        */
    typedef struct
    {
        UBX_U4 magic;       //!< magic word
        UBX_U2 offset[32];  //!< offsets from the beginning of this struct to the Almanacs and APF [16bit words]
        UBX_U2 size;        //!< size of full file, incl. header, in units of 4 bytes
        UBX_U2 completed;   //!< in the ALP file, this value is set to 0xffff. Once the ALP file has been fully downloaded, it is set to 65535 - the size parameter, to indicate that the file has been fully transferred.
        UBX_U2 reserved2;   //!< reserved for checksums etc
        UBX_U2 padding;     //!< Padding for field alignment
        UBX_U4 tow;         //!< time of week of prediction start [s]
        UBX_U2 wno;         //!< week number of prediction start
        UBX_U2 duration;    //!< duration of ALP predicton [600s]
    } LegacyAidingDataHeader;


    ///////////////////////////////////////////////////////////////////////////////
    // libMGA Interface

    // MODIFIED: can return MGA_API_OUT_OF_MEMORY
    //! Initialize libMga.
    /*! First thing that should be done. Setups up any internal library resources.
        \return     MGA_API_OK if message processing successful.\n
        Fails with:\n
        MGA_API_OUT_OF_MEMORY   - Not enough memory to allocate mutex.\n
        */
    MGA_API_RESULT mgaInit(void);


    //! De-initialize libMga.
    /*! Last thing that should be done. Releases any internal library resources.

        \return Always returns MGA_API_OK
        */
    MGA_API_RESULT mgaDeinit(void);


    //! Get library version information.
    /*!

        \return     Pointer to a text string containing the library version.
        */
    const UBX_CH* mgaGetVersion(void);


    //! Configure the libMga.
    /*! Sets the flow control, event handler functions and event handler context data.
        \param pFlowConfig      Pointer to a MgaFlowConfiguration structure containing information about how to transfer messages to the receiver.
        \param pEvtInterface    Pointer to event handler callback function jump table.
        \param pCallbackContext Pointer to context information to pass in future to event handler callback functions.

        \return MGA_API_OK if configuration is successful.\n
        MGA_API_ALREADY_RUNNING if configuration failed because library is currently active.
        */
    MGA_API_RESULT mgaConfigure(const MgaFlowConfiguration* pFlowConfig,
                                const MgaEventInterface* pEvtInterface,
                                const void* pCallbackContext);


    //! Start a data transfer session.
    /*!
        \return     MGA_API_OK if session started successfully.\n
        Fails with:\n
        MGA_API_OUT_OF_MEMORY if no more memory to allocate internal buffers.\n
        MGA_API_ALREADY_RUNNING if a session is already running and not idle.
        */
    MGA_API_RESULT mgaSessionStart(void);


    //! Transfer Online MGA message data to the receiver.
    /*!
        \param pMgaData         Pointer to message data.
        \param iSize            Size in bytes.
        \param pMgaTimeAdjust   Pointer to time adjustment structure, to be used to adjust the MGA-TIME-INI message in the message stream. If NULL, then no adjustment will take place.

        \return     MGA_API_OK if session started successfully.\n
        Fails with:\n
        MGA_API_OUT_OF_MEMORY if no more memory to allocate internal buffers.\n
        MGA_API_ALREADY_IDLE if a session is not active and is idle.\n
        */
    MGA_API_RESULT mgaSessionSendOnlineData(const UBX_U1* pMgaData,
                                            UBX_I4 iSize,
                                            const MgaTimeAdjust* pMgaTimeAdjust);


    //! Transfer MGA Offline message data to the receiver.
    /*!
        \param pMgaData         Pointer to message data block.
        \param iSize            Size in bytes.
        \param pTime            Pointer to time adjustment structure, to be used to set the MGA-TIME-INI message which is sent to the receiver before the supplied MGA data.
        \param pPos             Pointer to position adjustment structure, to be used to set the MGA-POS-INI message which is sent to the receiver before the supplied MGA data.

        \return     MGA_API_OK if message transfer started successfully.\n
        Fails with:\n
        MGA_API_NO_MGA_INI_TIME - No UBX-MGA-INI-TIME message in message data stream.\n
        MGA_API_NO_DATA_TO_SEND - No data to send. \n
        MGA_API_OUT_OF_MEMORY   - Not enough memory to start message transfer.\n
        MGA_API_ALREADY_IDLE    - If a session is not active and is idle.\n
        MGA_API_BAD_DATA        - If the supplied data is not MGA data.
        */
    MGA_API_RESULT mgaSessionSendOfflineData(const UBX_U1* pMgaData,
                                             UBX_I4 iSize,
                                             const MgaTimeAdjust* pTime,
                                             const MgaPosAdjust* pPos);


    //! Transfer MGA Offline messages to the receiver's flash.
    /*!
        \param pMgaData     Pointer to message data block.
        \param iSize        Size in bytes of message data.

        \return     MGA_API_OK if message processing successful.\n
        Fails with:\n
        MGA_API_OUT_OF_MEMORY   - Not enough memory to start message transfer.\n
        MGA_API_ALREADY_IDLE    - If a session is not active and is idle.\n
        MGA_API_BAD_DATA        - If the supplied data is not MGA data.


        */
    MGA_API_RESULT mgaSessionSendOfflineToFlash(const UBX_U1* pMgaData, UBX_I4 iSize);


    //! Stops a data transfer session.
    /*!
        \return     MGA_API_OK if session started successfully.\n
        Fails with:\n
        MGA_API_ALREADY_IDLE if no session is running and library is already idle.
        */
    MGA_API_RESULT mgaSessionStop(void);


    // MODIFIED: pServerConfig is no longer const so that it can carry back the encoded length.
    //! Retrieve data from the MGA Online service.
    /*! Connects to the MGA servers and requests and retrieves online assistance data according to the supplied
        server configuration information. A buffer is allocated to hold the retrieved data, which is
        returned to the client application.
        It is the client applications responsibility to release this buffer.

        \param pServerConfig    Pointer to server information structure.
        \param ppData           Pointer to return allocated buffer pointer to.
        \param piSize           Pointer to return size of buffer allocated.

        \return     MGA_API_OK if data retrieved successfully.\n
        Fails with:\n
        MGA_API_CANNOT_CONNECT if cannot connect to any servers.\n
        MGA_API_CANNOT_GET_DATA if all data cannot be downloaded.
        */
    MGA_API_RESULT mgaGetOnlineData(MgaOnlineServerConfig* pServerConfig,
                                    UBX_U1** ppData,
                                    UBX_I4* piSize);


    // MODIFIED: pServerConfig is no longer const so that it can carry back the encoded length.
    //! Retrieve data from the MGA Offline service.
    /*! Connects to the MGA servers and requests and retrieves offline assistance data according to the supplied
        server configuration information. A buffer is allocated to hold the retrieved data, which is
        returned to the client application.
        It is the client applications responsibility to release this buffer.

        \param pServerConfig    Pointer to server information structure.
        \param ppData           Pointer to return allocated buffer pointer to.
        \param piSize           Pointer to return size of buffer allocated.

        \return     MGA_API_OK if data retrieved successfully.\n
        Fails with:\n
        MGA_API_CANNOT_CONNECT if cannot connect to any servers.\n
        MGA_API_CANNOT_GET_DATA if all data cannot be downloaded.
        */
    MGA_API_RESULT mgaGetOfflineData(MgaOfflineServerConfig* pServerConfig,
                                     UBX_U1** ppData,
                                     UBX_I4* piSize);


    //! Process a message that has come from the receiver.
    /*! The host application should pass any messages received from the receiver to this function.
        The libMga will then inspect the message and if it is of interest, act upon it.
        Messages of interest are those concerned with ACK/NAK of MGA messages that have been transferred to the receiver.
        This function expects the pointer to the message data to pointer to the start of a message and be one message in length.

        \param pMgaData     Pointer to message data.
        \param iSize        Size in bytes of message data.

        \return     MGA_API_OK if message was processed.\n
        MGA_API_IGNORED_MSG if message was ignored.
        */
    MGA_API_RESULT mgaProcessReceiverMessage(const UBX_U1* pMgaData,
                                             UBX_I4 iSize);


    //! Poll the libMga for any overdue message ACKs.
    /*! If any timeouts have been reached, then it will be resent.
        If the maximum number of resends has been reached, then libMga flags the message as failed and moves on the the
        next message to send.

        \return     Always returns MGA_API_OK.
        */
    MGA_API_RESULT mgaCheckForTimeOuts(void);


    // MODIFIED: pServerConfig is no longer const so that it can carry back the encoded length.
    //! Builds the query string which will be sent to the service to request Online data.
    /*! This is really to aid debugging to see what is actually being sent to the service.
        The query string is constructed based on the contents of the supplied server information structure.

        \param pServerConfig    Pointer to server information structure.
        \param pBuffer          Pointer to the buffer to receive the constructed query string.
        \param iSize            Size in bytes of the buffer.

        \return     Always returns MGA_API_OK.
        */
    MGA_API_RESULT mgaBuildOnlineRequestParams(MgaOnlineServerConfig* pServerConfig,
                                               UBX_CH* pBuffer,
                                               UBX_I4 iSize);

    // MODIFIED: pServerConfig is no longer const so that it can carry back the encoded length.
    //! Builds the query string which will be sent to the service to request Offline data.
    /*! This is really to aid debugging to see what is actually being sent to the service.
        The query string is constructed based on the contents of the supplied server information structure.

        \param pServerConfig    Pointer to server information structure.
        \param pBuffer          Pointer to the buffer to receive the constructed query string.
        \param iSize            Size in bytes of the buffer.

        \return     Always returns MGA_API_OK.
        */
    MGA_API_RESULT mgaBuildOfflineRequestParams(MgaOfflineServerConfig* pServerConfig,
                                                UBX_CH* pBuffer,
                                                UBX_I4 iSize);


    //! Erases the MGA Offline data in the receiver's flash.
    /*!
        \return     MGA_API_OK if flash erased.\n
        MGA_API_ALREADY_IDLE if no session has been started.
        */
    MGA_API_RESULT mgaEraseOfflineFlash(void);

    //! Extracts ALM messages from a superset of MGA Offline data.
    /*! Used to support the 'Flash Based' MGA Offline scenario where the host application holds all the MGA Offline data
    and sent only ALM data to the receiver.
    This API function will 'malloc' a buffer to rerun the extracted MGA messages. It is the responsibility of the
    application to 'free' this buffer when it is finished with it.

    \param pOfflineData     Pointer to a buffer contained all MGA messages.
    \param offlineDataSize  Size in bytes of the buffer.
    \param ppAlmData     Pointer to a pointer to return the allocated buffer containing the extracted MGA messages.
    \param pAlmDataSize  Pointer to return the size of the allocated buffer.

    \return     MGA_API_OK if extraction took place.\n
    MGA_API_NO_DATA_TO_SEND if no data could be extracted.
    */
    MGA_API_RESULT mgaGetAlmOfflineData(UBX_U1* pOfflineData, UBX_I4 offlineDataSize, UBX_U1** ppAlmData, UBX_I4* pAlmDataSize);

    //! Extracts Offline MGA messages for a given day from a superset of MGA Offline data.
    /*! Used to support the 'Host Based' MGA Offline scenario where the host application holds all the MGA Offline data
        and a single days worth is extracted and sent to the receiver.
        This API function will 'malloc' a buffer to rerun the extracted MGA messages. It is the responsibility of the
        application to 'free' this buffer when it is finished with it.

        \param pTime            Pointer to a time structure containing the date of the MGA messages to extract. Only year, month & day fields are used.
        \param pOfflineData     Pointer to a buffer contained all MGA messages.
        \param offlineDataSize  Size in bytes of the buffer.
        \param ppTodaysData     Pointer to a pointer to return the allocated buffer containing the extracted MGA messages.
        \param pTodaysDataSize  Pointer to return the size of the allocated buffer.

        \return     MGA_API_OK if extraction took place.\n
        MGA_API_NO_DATA_TO_SEND if no data could be extracted.
        */
    MGA_API_RESULT mgaGetTodaysOfflineData(const struct tm* pTime, UBX_U1* pOfflineData, UBX_I4 offlineDataSize, UBX_U1** ppTodaysData, UBX_I4* pTodaysDataSize);

    //! Transfer legacy aiding data to the receiver's flash.
    /*!
        \param pAidingData  Pointer to legacy aiding data.
        \param iSize        Size in bytes of aiding data.

        \return     MGA_API_OK if message processing successful.\n
        Fails with:\n
        MGA_API_OUT_OF_MEMORY   - Not enough memory to start message transfer.\n
        MGA_API_ALREADY_IDLE    - If a session is not active and is idle.\n
        MGA_API_NO_DATA_TO_SEND - If no data is supplied.\n

        */
    MGA_API_RESULT mgaSessionSendLegacyOfflineToFlash(const UBX_U1* pAidingData, UBX_U4 iSize);

    //! Start legacy aiding server support
    /*! Used to support 'Host Based' legacy offline aiding. When active, mgaProcessReceiverMessage will look for UBX-AID-ALPSRV messages
        from the receiver, and will send out the appropriate UBX-AID-ALPSRV response.
        Generates a MGA_PROGRESS_EVT_LEGACY_AIDING_SERVER_STARTED event when startup has been completed.
        mgaProcessReceiverMessage will generate MGA_PROGRESS_EVT_LEGACY_AIDING_SERVER_REQUEST_RECEIVED and MGA_PROGRESS_EVT_LEGACY_AIDING_SERVER_REQUEST_COMPLETED
        events on receiving and sending UBX-AID-ALPSRV messages.

        \param pAidingData  Pointer to the legacy aiding data block.
        \param iSize        Size of the legacy aiding data block.

        \return     MGA_API_OK
        */
    MGA_API_RESULT mgaStartLegacyAiding(UBX_U1* pAidingData, UBX_I4 iSize);

    //! Stop legacy aiding server
    /*! Used to stop support for Host Based' legacy offline aiding.
        Generates a MGA_PROGRESS_EVT_LEGACY_AIDING_SERVER_STOPPED event on completion of shutting down the server.

        \return     MGA_API_OK
        */
    MGA_API_RESULT mgaStopLegacyAiding(void);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif //__U_LIB_MGA__
