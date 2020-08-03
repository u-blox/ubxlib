/*
 * Copyright 2020 u-blox Ltd
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef _U_PORT_UART_H_
#define _U_PORT_UART_H_

/* No #includes allowed here */

/** @file
 * @brief Porting layer for UART access functions.  These functions
 * are thread-safe.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

#ifndef U_PORT_UART_RX_BUFFER_SIZE
/** The size of ring buffer to use for receive. For instance,
 * 1024 bytes would be sufficient to accommodate the maximum length
 * of a single AT response from a cellular module.
 */
# define U_PORT_UART_RX_BUFFER_SIZE 1024
#endif

#ifndef U_PORT_UART_EVENT_QUEUE_SIZE
/** The UART event queue size, in units of
 * sizeof(uPortUartEventData_t).
 */
# define U_PORT_UART_EVENT_QUEUE_SIZE 20
#endif

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * FUNCTIONS
 * -------------------------------------------------------------- */

/** Initialise a UART.  If the UART has already been initialised
 * this function returns success without doing anything.
 *
 * @param pinTx           the transmit (output) pin, a positive
 *                        integer.
 * @param pinRx           the receive (input) pin, a positive
 *                        integer.
 * @param pinCts          the CTS (input) flow control pin, asserted
 *                        by the modem when it is ready to receive
 *                        data; use -1 for none.
 * @param pinRts          the RTS (output) flow control pin, asserted
 *                        when we are ready to receive data from the
 *                        modem; use -1 for none.
 * @param baudRate        the baud rate to use.
 * @param uart            the UART number to use.
 * @param pUartQueue      a place to put the UART event queue.
 * @return                zero on success else negative error code.
 */
int32_t uPortUartInit(int32_t pinTx, int32_t pinRx,
                      int32_t pinCts, int32_t pinRts,
                      int32_t baudRate,
                      int32_t uart,
                      uPortQueueHandle_t *pUartQueue);

/** Shutdown a UART.  Note that this should NOT be called if
 * a UART read or write could be in progress.
 *
 * @param uart the UART number to shut down.
 * @return     zero on success else negative error code.
 */
int32_t uPortUartDeinit(int32_t uart);

/** Send a data event to the UART event queue.  This is not
 * normally required, it is all done within this UART driver,
 * however there are occasions when a receive event is handled
 * but the data is then only partially read.  This function
 * can be used to generate a new event so that the remaining
 * data can be processed naturally by the receive thread.
 *
 * @param queueHandle      the handle for the UART event queue.
 * @param sizeBytesOrError the number of bytes of received data
 *                         to be signalled or negative to signal
 *                         an error.
 * @return                 zero on success else negative error
 *                         code.
 */
int32_t uPortUartEventSend(const uPortQueueHandle_t queueHandle,
                           int32_t sizeBytesOrError);

/** Receive a UART event, blocking until one turns up.
 *
 * @param queueHandle the handle for the UART event queue.
 * @return            if the event was a receive event then
 *                    the length of the data received by the`
 *                    UART, else a negative number.
 */
int32_t uPortUartEventReceive(const uPortQueueHandle_t queueHandle);

/** Receive a UART event with a timeout.
 *
 * @param queueHandle the handle for the UART event queue.
 * @param waitMs      the time to wait in milliseconds.
 * @return            if the event was a receive event then
 *                    the length of the data received by the`
 *                    UART, else a negative number.
 */
int32_t uPortUartEventTryReceive(const uPortQueueHandle_t queueHandle,
                                 int32_t waitMs);

/** Get the number of bytes waiting in the receive buffer.
 *
 * @param uart the UART number to use.
 * @return     the number of bytes in the receive buffer
 *             or negative error code.
 */
int32_t uPortUartGetReceiveSize(int32_t uart);

/** Read from the given UART interface, non-blocking:
 * up to sizeBytes of data already in the UART buffer will
 * be returned.
 *
 * @param uart      the UART number to use.
 * @param pBuffer   a pointer to a buffer in which to store
 *                  received bytes.
 * @param sizeBytes the size of buffer pointed to by pBuffer.
 * @return          the number of bytes received else negative
 *                  error code.
 */
int32_t uPortUartRead(int32_t uart, char *pBuffer,
                      size_t sizeBytes);

/** Write to the given UART interface.  Will block until
 * all of the data has been written or an error has occurred.
 *
 * @param uart      the UART number to use.
 * @param pBuffer   a pointer to a buffer of data to send.
 * @param sizeBytes the number of bytes in pBuffer.
 * @return          the number of bytes sent or negative
 *                  error code.
 */
int32_t uPortUartWrite(int32_t uart,
                       const char *pBuffer,
                       size_t sizeBytes);

/** Determine if RTS flow control, i.e. a signal from
 * the module to this software that the module is ready to
 * receive data, is enabled.
 *
 * @param uart  the UART number.
 * @return      true if RTS flow control is enabled
 *              on this UART, else false.
 */
bool uPortIsRtsFlowControlEnabled(int32_t uart);

/** Determine if CTS flow control, i.e. a signal from
 * this software to the module that this sofware is ready
 * to accept data, is enabled.
 *
 * @param uart  the UART number.
 * @return      true if CTS flow control is enabled
 *              on this UART, else false.
 */
bool uPortIsCtsFlowControlEnabled(int32_t uart);

#ifdef __cplusplus
}
#endif

#endif // _U_PORT_UART_H_

// End of file
