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

/** @file
 * @brief This file makes a connection from the bottom of the Zephyr IP
 * stack to a PPP interface inside ubxlib.  Such a PPP interface is
 * provided by a cellular module.
 *
 * It is only compiled if CONFIG_NET_PPP and CONFIG_NET_L2_PPP are
 * switched-on in your Zephyr prj.conf file.
 *
 * Implementation note: the Zephyr PPP driver is designed to talk
 * to a UART, one specifically named "zephyr,ppp-uart" in the device
 * tree.  By default it writes to this UART byte-by-byte, rather than
 * buffer-wise, which would be extremely inefficient since we are
 * running a CMUX underneath, as every character would be wrapped in
 * a CMUX frame.  Hence this code expects CONFIG_NET_PPP_ASYNC_UART
 * to be defined, which causes the PPP driver to give us buffer fulls
 * of data to transmit.  And though the interface is called "asynchronous",
 * it really isn't at all since the event callback simply calls back,
 * from Zephyr ppp.c, into this code.  To make it behave aaynchronously,
 * and to provide buffering of the TX segments being sent, an event
 * queue is used on the transmit side, which makes the whole thing
 * somewhat slow I'm afraid.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif
#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"
#include "string.h"    // memset()

#include "u_cfg_os_platform_specific.h" // U_CFG_OS_PRIORITY_MAX
#include "u_cfg_sw.h"
#include "u_error_common.h"

#include "u_sock.h" // uSockStringToAddress()

#include "u_port.h"
#include "u_port_os.h"
#include "u_port_heap.h"
#include "u_port_event_queue.h"
#include "u_port_ppp.h"
#include "u_port_debug.h"

#include <version.h>

#ifdef CONFIG_NET_PPP
# if KERNEL_VERSION_NUMBER >= ZEPHYR_VERSION(3,1,0)
#  include <zephyr/kernel.h>
#  include <zephyr/device.h>
#  include <zephyr/net/net_if.h>
#  include <zephyr/net/net_mgmt.h>
#  include <zephyr/drivers/uart.h>
# else
#  include <kernel.h>
#  include <device.h>
#  include <net/net_if.h>
#  include <net/net_mgmt.h>
#  include <drivers/uart.h>
# endif
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

#ifndef U_PORT_PPP_UART_DEVICE_ID
/** The name that the virtual UART device which represents the
 * PPP interface will appear as; this is the name which must
 * be select as the Zephry PPP UART in the application's device
 * tree for a PPP interface to work with Zephyr.
 *
 * IMPORTANT: MUST BE OF THE FORM pppuartx, where x is a decimal
 * number
 */
# define U_PORT_PPP_UART_DEVICE_ID uartppp
#endif

#ifndef U_PORT_PPP_CONNECT_TIMEOUT_SECONDS
/** How long to wait for PPP to connect.
 */
# define U_PORT_PPP_CONNECT_TIMEOUT_SECONDS 15
#endif

#ifndef U_PORT_PPP_DISCONNECT_TIMEOUT_SECONDS
/** How long to wait for PPP to disconnect.
 */
# define U_PORT_PPP_DISCONNECT_TIMEOUT_SECONDS 10
#endif

#ifndef U_PORT_PPP_TX_LOOP_GUARD
/** How many times around the transmit loop to allow if stuff
 * won't send.
 */
# define U_PORT_PPP_TX_LOOP_GUARD 100
#endif

#ifndef U_PORT_PPP_TX_LOOP_DELAY_MS
/** How long to wait between transmit attempts in milliseconds
 * when the data to transmit won't go all at once.
 */
# define U_PORT_PPP_TX_LOOP_DELAY_MS 10
#endif

#ifndef U_PORT_PPP_TX_TASK_STACK_SIZE_BYTES
/** The stack size for the asynchronous transmit task in bytes.
 */
# define U_PORT_PPP_TX_TASK_STACK_SIZE_BYTES 2048
#endif

#ifndef U_PORT_PPP_TX_TASK_PRIORITY
/** The priority of the transmit task: should be relatively
 * high (e.g. U_CFG_OS_PRIORITY_MAX - 5, which is the same as
 * the AT Client URC task).
 */
# define U_PORT_PPP_TX_TASK_PRIORITY (U_CFG_OS_PRIORITY_MAX - 5)
#endif

#ifndef U_PORT_PPP_TX_BUFFER_COUNT
/** The number of TX buffers to have queued up.  This
 * is intended to work with a CONFIG_NET_TCP_MAX_SEND_WINDOW_SIZE/
 * CONFIG_NET_TCP_MAX_RECV_WINDOW_SIZE of 256 given a
 * #U_PORT_EVENT_QUEUE_MAX_PARAM_LENGTH_BYTES of 128.
 */
# define U_PORT_PPP_TX_BUFFER_COUNT 4
#endif

#ifndef U_PORT_PPP_RX_BUFFER_COUNT
/** The number of RX buffers to have queued up.
 */
# define U_PORT_PPP_RX_BUFFER_COUNT 2
#endif

// THERE ARE ADDITIONAL COMPILE-TIME MACROS AT THE END OF THIS FILE

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

#if defined(CONFIG_NET_PPP) && defined(U_CFG_PPP_ENABLE)

/** Data provide by the Zephyr PPP driver to be transmitted.
 */
typedef struct {
    /* buf is the biggest it can be so that we shuffle stuff through quickly */
    uint8_t buf[U_PORT_EVENT_QUEUE_MAX_PARAM_LENGTH_BYTES - (sizeof(size_t) + sizeof(int32_t))];
    size_t len;
    int32_t timeoutMs;
} uPortPppTx_t;

/** A receive buffer provided by the Zephyr PPP driver.
 */
typedef struct {
    uint8_t *pBuf;
    size_t len;
    const uint8_t *pRead;
    uint8_t *pWrite;
} uPortPppRx_t;

/** Data assocated with a UART that will be provided
 * to the Zephyr PPP interface.  Normally this would
 * form the "dev" context pointer of a device with
 * the fixed name "ppp_uart" which PPP will look for.
 * However, since there can be only one there is no
 * need to do that in our case, we can simply keep
 * it local.
 */
typedef struct {
    uart_callback_t asyncCallback;
    void *pAsyncCallbackParam;
    bool rxEnabled;
    uPortPppRx_t rxBuffer[U_PORT_PPP_RX_BUFFER_COUNT];
    size_t rxBufferIndexNext;
    size_t rxBufferIndexRead;
    size_t rxBufferIndexWrite;
} uPortPppUartDriver_t;

/** Define a PPP interface.
 */
typedef struct uPortPppInterface_t {
    void *pDevHandle;
    int32_t txQueueHandle;
    uPortPppConnectCallback_t *pConnectCallback;
    uPortPppDisconnectCallback_t *pDisconnectCallback;
    uPortPppTransmitCallback_t *pTransmitCallback;
    bool pppRunning;
    bool ipConnected;
    struct net_if *pNetIf;
    // There are two of these because events from different
    // layers have to be in different structures
    struct net_mgmt_event_callback netIfEventCallbackPpp;
    struct net_mgmt_event_callback netIfEventCallbackIp;
    uPortPppUartDriver_t pppUartDriver;
} uPortPppInterface_t;

#endif // #if defined(CONFIG_NET_PPP) && defined(U_CFG_PPP_ENABLE)

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

#if defined(CONFIG_NET_PPP) && defined(U_CFG_PPP_ENABLE)

/** A place to hook the PPP interface (in Zephyr there can
 * be only one).
 */
static uPortPppInterface_t *gpPppInterface = NULL;

/** Mutex to protect the linked list of PPP entities.
 */
static uPortMutexHandle_t gMutex = NULL;

// THERE ARE ADDITIONAL STATIC VARIABLES AT THE END OF THIS FILE

#endif // #if defined(CONFIG_NET_PPP) && defined(U_CFG_PPP_ENABLE)

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS: UART API FOR ZEPHYR PPP TO TALK TO
 * -------------------------------------------------------------- */

#if defined(CONFIG_NET_PPP) && defined(U_CFG_PPP_ENABLE)

# ifdef CONFIG_UART_ASYNC_API

// Dummy initialisation function for the UART device.
static int uartPppInit(const struct device *dev)
{
    // Don't care about the device data since Zephyr only supports
    // a single PPP connection
    (void) dev;

    return 0; // Return success
}

// Send an event to the asynchronous UART event callback up
// in Zephyr PPP.
static void sendEvent(const uPortPppUartDriver_t *pUartDriver,
                      struct uart_event *pEvent)
{
    if ((pUartDriver != NULL) && (pUartDriver->asyncCallback != NULL)) {
        pUartDriver->asyncCallback(NULL, pEvent, pUartDriver->pAsyncCallbackParam);
    }
}

// Set the callback for asynchronous operation of the "UART" that
// Zephyr PPP is talking to; the callback would be called, for
// instance, when TX is complete.
static int uartCallbackSet(const struct device *pDev,
                           uart_callback_t cb, void *pUserData)
{
    int32_t zephyrErrorCode = -ENODEV;
    uPortPppUartDriver_t *pUartDriver;

    // Don't care about the device data since Zephyr only supports
    // a single PPP connection
    (void) pDev;

    if ((gpPppInterface != NULL) && (gpPppInterface->pppRunning)) {
        pUartDriver = &(gpPppInterface->pppUartDriver);
        pUartDriver->asyncCallback = cb;
        pUartDriver->pAsyncCallbackParam = pUserData;
        zephyrErrorCode = 0;
    }

    return zephyrErrorCode;
}

// Asynchronous transmit function for the UART that Zephyr PPP is
// talking to.
static int uartTx(const struct device *pDev, const uint8_t *pBuf,
                  size_t len, int32_t timeout)
{
    int32_t zephyrErrorCode = -ENODEV;
    size_t thisLen;
    uPortPppTx_t tx;

    // Don't care about the device data since Zephyr only supports
    // a single PPP connection
    (void) pDev;

    if ((gpPppInterface != NULL) && (gpPppInterface->txQueueHandle >= 0) &&
        (gpPppInterface->pppRunning)) {
        zephyrErrorCode = 0;
        while ((len > 0) && (zephyrErrorCode == 0)) {
            thisLen = len;
            if (thisLen > sizeof(tx.buf)) {
                thisLen = sizeof(tx.buf);
            }
            memcpy(tx.buf, pBuf, thisLen);
            tx.len = thisLen;
            tx.timeoutMs = k_ticks_to_ms_floor32(timeout);
            // Put the transmit into the queue
            if (uPortEventQueueSend(gpPppInterface->txQueueHandle,
                                    &tx, sizeof(tx)) == 0) {
                len -= thisLen;
                pBuf += thisLen;
            } else {
                zephyrErrorCode = -EBUSY;
            }
        }
    }

    return zephyrErrorCode;
}

// Enable asynchronous UART RX into the given [initial] buffer with
// the given timeout.
static int uartRxEnable(const struct device *pDev, uint8_t *pBuf,
                        size_t len, int32_t timeout)
{
    int32_t zephyrErrorCode = -ENODEV;
    uPortPppUartDriver_t *pUartDriver;
    uPortPppRx_t *pRxBuffer;

    // Don't care about the device data since Zephyr only supports
    // a single PPP connection
    (void) pDev;

    // This timeout is intended to be a kind of stutter-reducing affair
    // on data reception, waiting for this long for nothing to happen
    // since the last byte was received before generaing a UART_RX_RDY
    // event.  However, the API here doesn't work like that, there can't
    // be any hanging around in the receive callback, hence it is ignored
    (void) timeout;

    if ((gpPppInterface != NULL) && (gpPppInterface->pppRunning)) {
        pUartDriver = &(gpPppInterface->pppUartDriver);
        zephyrErrorCode = -EBUSY;
        if (!pUartDriver->rxEnabled) {
            pUartDriver->rxBufferIndexNext = 0;
            pRxBuffer = &(pUartDriver->rxBuffer[pUartDriver->rxBufferIndexNext]);
            pUartDriver->rxBufferIndexNext++;
            pRxBuffer->pBuf = pBuf;
            pRxBuffer->len = len;
            pRxBuffer->pRead = pBuf;
            pRxBuffer->pWrite = pBuf;
            pUartDriver->rxBufferIndexRead = 0;
            pUartDriver->rxBufferIndexWrite = 0;
            pUartDriver->rxEnabled = true;
            zephyrErrorCode = 0;
        }
    }

    return zephyrErrorCode;
}

// Set the next buffer for asynchronous UART reception.
static int uartRxBufRsp(const struct device *pDev, uint8_t *pBuf,
                        size_t len)
{
    int32_t zephyrErrorCode = -EACCES;
    uPortPppUartDriver_t *pUartDriver;
    size_t nextIndex;
    uPortPppRx_t *pRxBuffer;

    // Don't care about the device data since Zephyr only supports
    // a single PPP connection
    (void) pDev;

    if ((gpPppInterface != NULL) && (gpPppInterface->pppRunning)) {
        pUartDriver = &(gpPppInterface->pppUartDriver);
        if (pUartDriver->rxEnabled) {
            nextIndex = pUartDriver->rxBufferIndexNext;
            pRxBuffer = &(pUartDriver->rxBuffer[nextIndex]);
            pRxBuffer->pBuf = pBuf;
            pRxBuffer->len = len;
            pRxBuffer->pRead = pBuf;
            pRxBuffer->pWrite = pBuf;
            nextIndex++;
            if (nextIndex > sizeof(pUartDriver->rxBuffer) / sizeof(pUartDriver->rxBuffer[0])) {
                nextIndex = 0;
            }
            pUartDriver->rxBufferIndexNext = nextIndex;
            zephyrErrorCode = 0;
        }
    }

    return zephyrErrorCode;
}

// Disable UART receive; as well as being called in
// the shutdown case, Zephyr ppp.c may call this from
// the asyncCallback() in the middle of our rxCallback()
// if the event being sent is UART_RX_RDY and it has
// no buffer space to read the received data into.
static int uartRxDisable(const struct device *pDev)
{
    int32_t zephyrErrorCode = -EFAULT;
    uPortPppUartDriver_t *pUartDriver;
    uPortPppRx_t *pRxBuffer;
    int32_t readIndex;
    struct uart_event event;

    // Don't care about the device data since Zephyr only supports
    // a single PPP connection
    (void) pDev;

    if (gpPppInterface != NULL) {
        pUartDriver = &(gpPppInterface->pppUartDriver);
        if (pUartDriver->rxEnabled) {
            // The guidance in Zephyr ppp.c is that this code
            // should generate UART_RX_RDY for any pending received
            // data, then UART_RX_BUF_RELEASED for every buffer
            // scheduled, follow by a UART_RX_DISABLED event.
            // However, if this function is being called because
            // ppp.c is out of buffers, generating a UART_RX_RDY
            // may cause it to disable RX ('cos it has nowhere
            // to put the data), which will call this function,
            // etc.  So we don't do that.
            readIndex = pUartDriver->rxBufferIndexRead;
            pRxBuffer = &(pUartDriver->rxBuffer[readIndex]);
            while (pRxBuffer->pBuf != NULL) {
                // We are done with the current buffer, release it
                event.type = UART_RX_BUF_RELEASED;
                event.data.rx_buf.buf = pRxBuffer->pBuf;
                pRxBuffer->pBuf = NULL;
                pRxBuffer->len = 0;
                readIndex++;
                if (readIndex > sizeof(pUartDriver->rxBuffer) / sizeof(pUartDriver->rxBuffer[0])) {
                    readIndex = 0;
                }
                // Put the modified read index back
                pUartDriver->rxBufferIndexRead = readIndex;
                sendEvent(pUartDriver, &event);
                // Move pRxBuffer on
                pRxBuffer = &(pUartDriver->rxBuffer[readIndex]);
            }
            pUartDriver->rxEnabled = false;
            // Acknowledge the disablement
            event.type = UART_RX_DISABLED;
            sendEvent(pUartDriver, &event);
            zephyrErrorCode = 0;
        }
    }

    return zephyrErrorCode;
}

# endif // #ifdef CONFIG_UART_ASYNC_API

#endif // #if defined(CONFIG_NET_PPP) && defined(U_CFG_PPP_ENABLE)

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS: TRANSMIT, RECEIVE AND NETWORK EVENTS
 * -------------------------------------------------------------- */

#if defined(CONFIG_NET_PPP) && defined(U_CFG_PPP_ENABLE)

// Send UART_TX_DONE or UART_TX_ABORTED.
static bool txEventSend(uPortPppUartDriver_t *pUartDriver,
                        const uint8_t *pBuf, int32_t len,
                        int32_t sent)
{
    struct uart_event event;

    event.data.tx.buf = pBuf;
    event.data.tx.len = sent;
    if ((len == 0) || (sent > 0)) {
        // If we had nothing to send or we sent at
        // least something then TX was done
        event.type = UART_TX_DONE;
    } else {
        event.type = UART_TX_ABORTED;
    }
    sendEvent(pUartDriver, &event);

    return true;
}

// Task to perform asynchronous data transmission, sits on
// the end of the transmit queue.
static void txTask(void *pParameters, size_t paramLength)
{
    uPortPppTx_t *pTransmit = (uPortPppTx_t *) pParameters;
    int32_t timeoutMs = pTransmit->timeoutMs;
    int32_t len = pTransmit->len;
    const uint8_t *pBuf = pTransmit->buf;
    uPortPppUartDriver_t *pUartDriver = NULL;
    int32_t startTimeMs = k_uptime_get();
    int32_t x = 0;
    size_t sent = 0;

    (void) paramLength;

    if ((gpPppInterface != NULL) && (gpPppInterface->pTransmitCallback != NULL) &&
        (gpPppInterface->pppRunning)) {
        pUartDriver = &(gpPppInterface->pppUartDriver);
        // Send off the data
        while ((len > 0) && (x >= 0) && (k_uptime_get() - startTimeMs < timeoutMs)) {
            x = gpPppInterface->pTransmitCallback(gpPppInterface->pDevHandle, pBuf + sent,
                                                  len - sent);
            if (x > 0) {
                len -= x;
                sent += x;
            } else {
                k_msleep(U_PORT_PPP_TX_LOOP_DELAY_MS);
            }
        }
    }

    // Let the asynchronous API callback know what happened
    txEventSend(pUartDriver, pBuf, pTransmit->len, sent);
}

// Callback for received data.
static void rxCallback(void *pDevHandle, const char *pData,
                       size_t dataSize, void *pCallbackParam)
{
    uPortPppUartDriver_t *pUartDriver;
    uPortPppRx_t *pRxBuffer;
    int32_t writeIndex;
    int32_t readIndex;
    size_t thisDataSize;
    size_t x;
    const uint8_t *pRead;
    uint8_t *pWrite;
    struct uart_event event;

    (void) pDevHandle;
    (void) pCallbackParam;

    if (gpPppInterface != NULL) {
        pUartDriver = &(gpPppInterface->pppUartDriver);
        if (pUartDriver->rxEnabled) {
            writeIndex = pUartDriver->rxBufferIndexWrite;
            readIndex = pUartDriver->rxBufferIndexRead;
            pRxBuffer = &(pUartDriver->rxBuffer[writeIndex]);
            do {
                // Write as much as we can to the current buffer
                if (pRxBuffer->pBuf != NULL) {
                    // Sample the read and write pointers
                    pRead = pRxBuffer->pRead;
                    pWrite = pRxBuffer->pWrite;

                    thisDataSize = dataSize;
                    x = pRxBuffer->len - (pRead - pWrite);
                    if (thisDataSize > x) {
                        thisDataSize = x;
                    }
                    memcpy(pWrite, pData, thisDataSize);
                    dataSize -= thisDataSize;
                    pWrite += thisDataSize;
                    // Tell the application that there is data to read
                    event.type = UART_RX_RDY;
                    event.data.rx.buf = pRxBuffer->pBuf;
                    event.data.rx.len = pWrite - pRead;
                    event.data.rx.offset = pRead - pRxBuffer->pBuf;
                    sendEvent(pUartDriver, &event);

                    // The event above will have caused the application
                    // to read the received data; either it has read all
                    // of it or it will have called uartRxDisable()
                    if (pUartDriver->rxEnabled) {
                        // If we get here the two pointers must be
                        // equal so we can switch buffers or not based
                        // on just one of them.  Of course this means that
                        // the two buffers are, actually, complete pointless,
                        // we could work with one, but since ppp.c allocates
                        // two it seems churlish not to use both.
                        pRead = pWrite;
                        // Check if we've done with the buffer
                        if (pRead >= pRxBuffer->pBuf + pRxBuffer->len) {
                            event.type = UART_RX_BUF_RELEASED;
                            event.data.rx_buf.buf = pRxBuffer->pBuf;
                            pRxBuffer->pBuf = NULL;
                            pRxBuffer->len = 0;
                            readIndex++;
                            if (readIndex > sizeof(pUartDriver->rxBuffer) / sizeof(pUartDriver->rxBuffer[0])) {
                                readIndex = 0;
                            }
                            // Put the modified read index back
                            pUartDriver->rxBufferIndexRead = readIndex;
                            // Release the buffer
                            sendEvent(pUartDriver, &event);
                            writeIndex++;
                            if (writeIndex > sizeof(pUartDriver->rxBuffer) / sizeof(pUartDriver->rxBuffer[0])) {
                                writeIndex = 0;
                            }
                            // Put the modified write index back
                            pUartDriver->rxBufferIndexWrite = writeIndex;
                            // Move pRxBuffer on
                            pRxBuffer = &(pUartDriver->rxBuffer[writeIndex]);
                            if (pRxBuffer->pBuf == NULL) {
                                // Don't have the next buffer yet: ask for one
                                event.type = UART_RX_BUF_REQUEST;
                                sendEvent(pUartDriver, &event);
                            }
                        }
                    }
                }
                // Loop around if receive is still enabled and we have not written
                // everything and there is another, empty, free buffer
            } while (pUartDriver->rxEnabled && (dataSize > 0) &&
                     (pRxBuffer->pBuf != NULL) && (pRxBuffer->pWrite == pRxBuffer->pBuf));
        }
    }
}

// Callback for network interface PPP and IP events.
static void netIfEventCallback(struct net_mgmt_event_callback *pCb,
                               uint32_t event,
                               struct net_if *pNetIf)
{
    (void) pCb;
    (void) pNetIf;

    if (gpPppInterface != NULL) {
        switch (event) {
            case NET_EVENT_IF_DOWN:
                gpPppInterface->ipConnected = false;
                break;
            case NET_EVENT_IPV4_ADDR_ADD:
                gpPppInterface->ipConnected = true;
                break;
            default:
                break;
        }
    }
}

#endif // #if defined(CONFIG_NET_PPP) && defined(U_CFG_PPP_ENABLE)

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS: MISC
 * -------------------------------------------------------------- */

#if defined(CONFIG_NET_PPP) && defined(U_CFG_PPP_ENABLE)

// Detach the Zephyr PPP interface.
static void pppDetach(uPortPppInterface_t *pPppInterface)
{
    int32_t startTimeMs;

    if ((pPppInterface != NULL) && (pPppInterface->pNetIf != NULL)) {
        // START: HACK HACK HACK HACK HACK HACK HACK
        //
        // See here: https://github.com/zephyrproject-rtos/zephyr/issues/67627
        // There is a bug in Zephyr 3.4.99 which means that Zephyr
        // PPP does not terminate the link with the peer, the peer
        // is left entirely up, which does no good at all as it
        // then won't connect the next time you try.
        //
        // As a workaround, we call net_if_carrier_off() ('cos
        // when the issue is fixed we don't want to be falling over
        // each other) and let the disconnect callback conduct
        // the PPP shut-down process on its behalf
        net_if_carrier_off(pPppInterface->pNetIf);
        // The Zephyr-side PPP connection will time out by itself.
        //
        // END: HACK HACK HACK HACK HACK HACK HACK

        // Disconnect PPP; this will eventually bring the interface down
        net_if_down(pPppInterface->pNetIf);

        // Wait for netIfEventCallback to be called-back
        // with the event NET_EVENT_IF_DOWN; it
        // will set gpPppInterface->ipConnected
        startTimeMs = uPortGetTickTimeMs();
        while ((gpPppInterface->ipConnected) &&
               (uPortGetTickTimeMs() - startTimeMs < U_PORT_PPP_DISCONNECT_TIMEOUT_SECONDS * 1000)) {
            uPortTaskBlock(250);
        }
        pPppInterface->ipConnected = false;
        net_mgmt_del_event_callback(&gpPppInterface->netIfEventCallbackPpp);
        net_mgmt_del_event_callback(&gpPppInterface->netIfEventCallbackIp);
        pPppInterface->pNetIf = NULL;
        if (pPppInterface->pDisconnectCallback != NULL) {
            // Disconnect the channel: pppRunning will be true (see hack above)
            pPppInterface->pDisconnectCallback(pPppInterface->pDevHandle,
                                               pPppInterface->pppRunning);
        }
        pPppInterface->pppRunning = false;
        uPortLog("U_PORT_PPP: disconnected.\n");

#ifndef U_CFG_PPP_ZEPHYR_TERMINATE_WAIT_DISABLE
        // START: HACK HACK HACK HACK HACK HACK HACK
        //
        // For the reason detailed above, we need to wait here for
        // Zephyr PPP to actually exit (to time out) or otherwise
        // it won't come up correctly again.  If this is not necessary
        // in your particular application you may disable it by
        // defining U_CFG_PPP_ZEPHYR_TERMINATE_WAIT_DISABLE for your build
        uPortLog("U_PORT_PPP: waiting 20 seconds for Zephyr PPP to terminate;"
                 " compile with U_CFG_PPP_ZEPHYR_TERMINATE_WAIT_DISABLE to disable this.\n");
        uPortTaskBlock(20000);
        //
        // END: HACK HACK HACK HACK HACK HACK HACK
#endif
    }
}

// Free all the memory of a PPP interface, and the interface itself.
static void freeInterface()
{
    if (gpPppInterface != NULL) {
        if (gpPppInterface->txQueueHandle >= 0) {
            uPortEventQueueClose(gpPppInterface->txQueueHandle);
        }
        uPortFree(gpPppInterface);
        gpPppInterface = NULL;
    }
}

#endif // #if defined(CONFIG_NET_PPP) && defined(U_CFG_PPP_ENABLE)

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS THAT ARE PRIVATE TO THIS PORT LAYER
 * -------------------------------------------------------------- */

#if defined(CONFIG_NET_PPP) && defined(U_CFG_PPP_ENABLE)

// Initialise the PPP stuff.
int32_t uPortPppPrivateInit()
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;

    if (gMutex == NULL) {
        errorCode = uPortMutexCreate(&gMutex);
    }

    return errorCode;
}

// Deinitialise the PPP stuff.
void uPortPppPrivateDeinit()
{
    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        if (gpPppInterface != NULL) {
            // Make sure we don't accidentally try to call the
            // down callback since the device handle will have
            // been destroyed by now
            gpPppInterface->pDisconnectCallback = NULL;
            pppDetach(gpPppInterface);
            freeInterface();
        }

        U_PORT_MUTEX_UNLOCK(gMutex);
        uPortMutexDelete(gMutex);
        gMutex = NULL;
    }
}

#else

// Initialise the PPP stuff.
int32_t uPortPppPrivateInit()
{
    return (int32_t) U_ERROR_COMMON_SUCCESS;
}

// Deinitialise the PPP stuff.
void uPortPppPrivateDeinit()
{
}

#endif // #if defined(CONFIG_NET_PPP) && defined(U_CFG_PPP_ENABLE)

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

#if defined(CONFIG_NET_PPP) && defined(U_CFG_PPP_ENABLE)

// Attach a PPP interface to the bottom of the Zephyr IP stack.
int32_t uPortPppAttach(void *pDevHandle,
                       uPortPppConnectCallback_t *pConnectCallback,
                       uPortPppDisconnectCallback_t *pDisconnectCallback,
                       uPortPppTransmitCallback_t *pTransmitCallback)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;

    (void) pDevHandle;

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
        if (gpPppInterface == NULL) {
            errorCode = (int32_t) U_ERROR_COMMON_NO_MEMORY;
            gpPppInterface = (uPortPppInterface_t *) pUPortMalloc(sizeof(*gpPppInterface));
            if (gpPppInterface != NULL) {
                memset(gpPppInterface, 0, sizeof(*gpPppInterface));
                gpPppInterface->txQueueHandle = -1;
                errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
                if (pTransmitCallback != NULL) {
                    // We use an event queue to perform the asynchronous transmit
                    errorCode = uPortEventQueueOpen(txTask, "pppTxTask",
                                                    sizeof(uPortPppTx_t),
                                                    U_PORT_PPP_TX_TASK_STACK_SIZE_BYTES,
                                                    U_PORT_PPP_TX_TASK_PRIORITY,
                                                    U_PORT_PPP_TX_BUFFER_COUNT);
                    gpPppInterface->txQueueHandle = errorCode;
                }
                if (errorCode >= 0) {
                    gpPppInterface->pDevHandle = pDevHandle;
                    gpPppInterface->pConnectCallback = pConnectCallback;
                    gpPppInterface->pDisconnectCallback = pDisconnectCallback;
                    gpPppInterface->pTransmitCallback = pTransmitCallback;
                    errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
                } else {
                    // Clean up on error
                    freeInterface();
                }
            }
        }

        if (errorCode < 0) {
            uPortLog("U_PORT_PPP: *** WARNING *** unable to attach PPP (%d).\n", errorCode);
        }

        U_PORT_MUTEX_UNLOCK(gMutex);
    }

    return errorCode;
}

// Connect a PPP interface.
int32_t uPortPppConnect(void *pDevHandle,
                        uSockIpAddress_t *pIpAddress,
                        uSockIpAddress_t *pDnsIpAddressPrimary,
                        uSockIpAddress_t *pDnsIpAddressSecondary,
                        const char *pUsername,
                        const char *pPassword,
                        uPortPppAuthenticationMode_t authenticationMode)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    struct net_if *pNetIf;
    int32_t startTimeMs;

    // Note: Zephyr does not (as of version 3.5 at least) support
    // entering a user name and password, and probably doesn't
    // support CHAP authentication at all.  However, it is often the
    // case that networks, despite indicating that a user name and
    // password are required, don't care either, hence we do not
    // reject a user name and password entered here, we just let
    // PPP try

    (void) pUsername;
    (void) pPassword;
    (void) authenticationMode;

    // PPP negotiation will set these
    (void) pIpAddress;
    (void) pDnsIpAddressPrimary;
    (void) pDnsIpAddressSecondary;

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        errorCode = (int32_t) U_ERROR_COMMON_NOT_FOUND;
        if (gpPppInterface != NULL) {
            errorCode = (int32_t) U_ERROR_COMMON_NO_MEMORY;
            // Get hold of the PPP network interface
            pNetIf = net_if_get_first_by_type(&NET_L2_GET_NAME(PPP));
            if (pNetIf != NULL) {
                errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
                if (gpPppInterface->pConnectCallback != NULL) {
                    errorCode = gpPppInterface->pConnectCallback(pDevHandle, rxCallback,
                                                                 NULL, NULL,
                                                                 U_PORT_PPP_RECEIVE_BUFFER_BYTES,
                                                                 NULL);
                }
                if (errorCode == 0) {
                    gpPppInterface->pppRunning = true;
                    // Use a nice specific error message here, most likely to point
                    // people at a PPP kinda problem
                    errorCode = (int32_t) U_ERROR_COMMON_PROTOCOL_ERROR;
                    // Zephyr event callbacks for different layers are required to be
                    // in different structs as they may overlap otherwise
                    net_mgmt_init_event_callback(&gpPppInterface->netIfEventCallbackPpp,
                                                 netIfEventCallback, NET_EVENT_IF_DOWN);
                    net_mgmt_add_event_callback(&gpPppInterface->netIfEventCallbackPpp);
                    net_mgmt_init_event_callback(&gpPppInterface->netIfEventCallbackIp,
                                                 netIfEventCallback, NET_EVENT_IPV4_ADDR_ADD);
                    net_mgmt_add_event_callback(&gpPppInterface->netIfEventCallbackIp);
                    net_if_carrier_on(pNetIf);
                    if (net_if_up(pNetIf) == 0) {
                        // Wait for netIfEventCallback to be called back
                        // with the event NET_EVENT_IPV4_ADDR_ADD; it
                        // will set gpPppInterface->ipConnected
                        startTimeMs = uPortGetTickTimeMs();
                        while ((!gpPppInterface->ipConnected) &&
                               (uPortGetTickTimeMs() - startTimeMs < U_PORT_PPP_CONNECT_TIMEOUT_SECONDS * 1000)) {
                            uPortTaskBlock(250);
                        }
                        if (gpPppInterface->ipConnected) {
                            gpPppInterface->pNetIf = pNetIf;
                            errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
                            uPortLog("U_PORT_PPP: connected.\n");
                        }
                    }
                }
                if ((errorCode != 0) && (gpPppInterface->pppRunning) &&
                    (gpPppInterface->pDisconnectCallback != NULL)) {
                    // Clean up on error
                    gpPppInterface->pDisconnectCallback(gpPppInterface->pDevHandle, false);
                    gpPppInterface->pppRunning = false;
                }
            }
        }

        U_PORT_MUTEX_UNLOCK(gMutex);
    }

    return errorCode;
}

// Reconnect a PPP interface.
int32_t uPortPppReconnect(void *pDevHandle,
                          uSockIpAddress_t *pIpAddress)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        errorCode = (int32_t) U_ERROR_COMMON_NOT_FOUND;
        if ((gpPppInterface != NULL) && (gpPppInterface->ipConnected)) {
            (void) pIpAddress;
            errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
            if (gpPppInterface->pConnectCallback != NULL) {
                errorCode = gpPppInterface->pConnectCallback(pDevHandle, rxCallback,
                                                             NULL, NULL,
                                                             U_PORT_PPP_RECEIVE_BUFFER_BYTES,
                                                             NULL);
            }
        }

        U_PORT_MUTEX_UNLOCK(gMutex);
    }

    return errorCode;
}

// Disconnect a PPP interface.
int32_t uPortPppDisconnect(void *pDevHandle)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;

    (void) pDevHandle;

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        errorCode = (int32_t) U_ERROR_COMMON_NOT_FOUND;
        if (gpPppInterface != NULL) {
            // No different from detach, it's going dowwwwwwn...
            pppDetach(gpPppInterface);
            errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
        }

        U_PORT_MUTEX_UNLOCK(gMutex);
    }

    return errorCode;
}

// Detach a PPP interface from the bottom of the Zephyr IP stack.
int32_t uPortPppDetach(void *pDevHandle)
{
    (void) pDevHandle;

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        if (gpPppInterface != NULL) {
            pppDetach(gpPppInterface);
            freeInterface();
        }

        U_PORT_MUTEX_UNLOCK(gMutex);
    }

    return (int32_t) U_ERROR_COMMON_SUCCESS;
}

#endif // #if defined(CONFIG_NET_PPP) && defined(U_CFG_PPP_ENABLE)

/* ----------------------------------------------------------------
 * MORE VARIABLES: THOSE RELATED TO THE LINK INTO ZEPHYR PPP
 * These are conventionally placed at the end of a Zephyr driver file.
 * -------------------------------------------------------------- */

#if defined(CONFIG_NET_PPP) && defined(U_CFG_PPP_ENABLE)

// Zephyr UART driver structure, used because Zephyr PPP wants to
// talk to a UART.  We only populate the calls that PPP needs, the
// rest we let the compiler initialise to NULL.
static const struct uart_driver_api gUart = {
# ifdef CONFIG_UART_ASYNC_API
    .callback_set = uartCallbackSet,
    .tx = uartTx,
    // Note tx_abort() not populated since the Zephyr PPP driver
    // never calls it and, in any case, there is no easy way
    // to abort an asynchronous transmit that is in the queue
    .rx_enable = uartRxEnable,
    .rx_buf_rsp = uartRxBufRsp,
    .rx_disable = uartRxDisable
# endif
    // *INDENT-OFF* (otherwise AStyle makes a mess of this)
    // Note: poll_in() not populated since the Zephyr PPP driver
    // never calls it when running in asynchronous mode,
    // poll_out() not populated since we use the asynchronous
    // transmit mode in order to get a buffer-full of data to
    // send, rather than single bytes at a time
    // Note: none of the interrupt driven functions are populated
    // since they are not used when the asynchronous API is employed
    // *INDENT-ON*
};

// This needs to be defined for the DEVICE_DT_INST_DEFINE()
// macro to work.  The name we give to the driver in the binding
// file over in the "dts" directory, following Zephyr convention,
// is "u-blox,uart-ppp", but the device tree macros needs any
// punctuation to be replaced with an underscore, hence the name
// becomes "u_blox_uart_ppp"
# define DT_DRV_COMPAT u_blox_uart_ppp

// Define a UART device with the given device name: this device
// must be instantiated and mapped to the device "zephyr,ppp-uart"
// (that Zephyr ppp.c is looking for) in the application's .dts or
// .overlay file with something like:
//
// / {
//    chosen {
//        zephyr,ppp-uart = &uart99;
//    };
//  uart99: uart-ppp@8000 {
//        compatible = "u-blox,uart-ppp";
//        reg = <0x8000 0x100>;
//        status = "okay";
//    };
//};
//
// Note that the "@8000 and the "reg" line are all irrelevant but
// are required for Zephyr to understand what we want.  The only
// thing that really matters is that uartX is an instance of the
// driver "u-blox,uart-ppp" (which is defined as a UART over
// in the binding file "u-blox,uart-ppp.yaml") and that uartX is
// chosen as the zephyr,ppp-uart.

# define U_PORT_PPP_UART_DEFINE(i)                                                   \
DEVICE_DT_INST_DEFINE(i,                                                            \
                      uartPppInit, /* Initialisation callback function */           \
                      NULL, /* pm_device */                                         \
                      NULL, /* Context: this would be pDev->data if we needed it */ \
                      NULL, /* Constant configuration data */                       \
                      APPLICATION, /* Device initialisation level */                \
                      CONFIG_SERIAL_INIT_PRIORITY, /* Initialisation priority */    \
                      &gUart); // API jump-table

DT_INST_FOREACH_STATUS_OKAY(U_PORT_PPP_UART_DEFINE)

#else // #if defined(CONFIG_NET_PPP) && defined(U_CFG_PPP_ENABLE)
# if defined(CONFIG_NET_PPP) && !defined(U_CFG_PPP_DUMMY_DISABLE)
// This code added so that we can keep the PPP UART entry in the generic
// .overlay files we provide at all times: if it gets in your way then
// you may define U_CFG_PPP_DUMMY_DISABLE to get rid of it

// Stub initialisation function for the UART device.
static int dummy(const struct device *dev)
{
    (void) dev;
    return 0;
}

// Stub u-blox,uart-ppp driver entry.
# define DT_DRV_COMPAT u_blox_uart_ppp

# define U_PORT_PPP_UART_DUMMY(i)                                                   \
DEVICE_DT_INST_DEFINE(i,                                                            \
                      dummy, /* Initialisation callback function */                 \
                      NULL, /* pm_device */                                         \
                      NULL, /* Context: this would be pDev->data if we needed it */ \
                      NULL, /* Constant configuration data */                       \
                      APPLICATION, /* Device initialisation level */                \
                      CONFIG_SERIAL_INIT_PRIORITY, /* Initialisation priority */    \
                      NULL); // API jump-table

DT_INST_FOREACH_STATUS_OKAY(U_PORT_PPP_UART_DUMMY)
# endif // # if defined(CONFIG_NET_PPP) && !defined(U_CFG_PPP_DUMMY_EXCLUDE)
#endif // #if defined(CONFIG_NET_PPP) && defined(U_CFG_PPP_ENABLE)

// End of file
