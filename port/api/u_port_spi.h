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

#ifndef _U_PORT_SPI_H_
#define _U_PORT_SPI_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

#include "u_common_spi.h"

/** \addtogroup __port
 *  @{
 */

/** @file
 * @brief Porting layer for SPI.  These functions are thread-safe.
 * Only a single data line is supported, only controller/master
 * mode is supported and there can only be one device per SPI.
 *
 * Note that these functions are currently only used to talk to
 * u-blox GNSS modules and that reflects the extent to which they
 * are tested; should you decide to use them to talk with other
 * SPI devices then it may be worth expanding the testing also.
 *
 * Note also that the interface is blocking, 'cos that's all we
 * [currently] need.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * FUNCTIONS
 * -------------------------------------------------------------- */

/** Initialise SPI handling.  If SPI has already been initialised
 * this function will return success without doing anything.
 *
 * @return  zero on success else negative error code.
 */
int32_t uPortSpiInit();

/** Shutdown SPI handling; any open SPI instances will be closed.
 */
void uPortSpiDeinit();

/** Open an SPI instance.  If an SPI instance has already
 * been opened on the given SPI HW block this function returns
 * an error.  Note that the pin numbers are those of the MCU:
 * if you are using an MCU inside a u-blox module the IO pin
 * numbering for the module is likely different to that from
 * the MCU: check the data sheet for the module to determine
 * the mapping.
 *
 * IMPORTANT: some platforms, specifically Zephyr (used on NRF53),
 * do not permit SPI pin choices to be made at link-time, only at
 * compile time.  For such platforms the pins passed in here MUST
 * be -1 (otherwise an error will be returned) and you MUST check
 * the README.md for that platform to find out how the pins
 * are chosen.
 *
 * @param spi            the SPI HW block to use.
 * @param pinMosi        the master-out, slave-in data pin, a
 *                       positive integer or -1 if the pin choice
 *                       has already been determined at compile
 *                       time or if only reads will be performed.
 * @param pinMiso        the master-in, slave-out data pin, a positive
 *                       integer or -1 if the pin choice has already
 *                       been determined at compile time or if only
 *                       writes will be performed.
 * @param pinClk         the clock pin, a positive integer or -1
 *                       if the pin choice has already been determined
 *                       at compile time.
 * @param controller     set to true for an SPI controller; this is for
 *                       forwards-compatibility only, it must currently
 *                       always be set to true since device/slave mode
 *                       is not supported.
 * @return               an SPI handle else negative error code.
 */
int32_t uPortSpiOpen(int32_t spi, int32_t pinMosi, int32_t pinMiso,
                     int32_t pinClk, bool controller);

/** Close an SPI instance.
 *
 * @param handle the handle of the SPI instance to close.
 */
void uPortSpiClose(int32_t handle);

/** Set the configuration of the device that this controller will
 * talk to.  If this function is not called
 * #U_COMMON_SPI_CONTROLLER_DEVICE_DEFAULTS /
 * #U_COMMON_SPI_CONTROLLER_DEVICE_INDEX_DEFAULTS will apply (specifically,
 * no chip select will be employed).  Note that, though the presence
 * of a chip select in #uCommonSpiControllerDevice_t might imply that
 * there can be more than one device, it is only the somewhat enlightened
 * ESP-IDF platform that supports this, hence we are not able to support
 * it here; there can be only one per SPI, calling this again will just
 * change the characteristics of the interface towards that single device.
 *
 * @param handle      the handle of the SPI instance.
 * @param[in] pDevice a pointer to the device configuration; it is good
 *                    practice to initialise your device structure using
 *                    #U_COMMON_SPI_CONTROLLER_DEVICE_DEFAULTS or
 *                    #U_COMMON_SPI_CONTROLLER_DEVICE_INDEX_DEFAULTS and
 *                    then only modify the bits that your device
 *                    specifically needs.  Cannot be NULL.
 * @return            zero on success else negative error code.
 */
int32_t uPortSpiControllerSetDevice(int32_t handle,
                                    const uCommonSpiControllerDevice_t *pDevice);

/** Get the configuration of the device that the given SPI instance is
 * talking to.
 *
 * @param handle       the handle of the SPI instance.
 * @param[out] pDevice a place to put the device configuration; cannot be NULL.
 * @return             zero on success else negative error code.
 */
int32_t uPortSpiControllerGetDevice(int32_t handle,
                                    uCommonSpiControllerDevice_t *pDevice);

/** Exchange a single word with the device, blocking.  Use this API if
 * your device requires a word length greater than one and using a word
 * length greater than one is not supported by the platform, e.g. your
 * device requires a 4-byte word, with chip select released either side
 * of it, when the underlying platform only supports single byte words
 * (with chip select released either side of a whole block).  There is
 * no need to use this function if the device you are talking to uses a
 * word length of 1 byte.
 *
 * This function also sorts out any endianness issues for you: if the
 * endianness of your processor does not match the endianness of SPI word
 * transmission, e.g. you have the default MSB first but your processor is
 * little-endian (as many are), first set the word length in
 * #uCommonSpiControllerDevice_t passed to uPortSpiControllerGetDevice()
 * to 1 and this function will perform any required byte-reversal. If you
 * don't know whether there is an endianness mismatch it is always safe
 * to set the word length to 1 when you are going to call this function;
 * the function will do no byte-reversal if endianness conversion is not
 * required.  If you use this function and the word length used by
 * uPortSpiControllerGetDevice() is _not_ 1 you will need to handle any
 * potential endianness issues yourself.
 *
 * @param handle                the handle of the SPI instance.
 * @param value                 the word to send.
 * @param bytesToSendAndReceive the length of the word, i.e. the number of
 *                              bytes from value to send and the number of
 *                              bytes to receive.
 * @return                      the word received, of length
 *                              bytesToSendAndReceive.
 */
uint64_t uPortSpiControllerSendReceiveWord(int32_t handle, uint64_t value,
                                           size_t bytesToSendAndReceive);

/** Exchange a block of data with an SPI device.  Note that the NRF52,
 * NRF53 and ESP32 chips require all buffers to be in RAM; for the ESP32
 * case it is more efficient if buffers are 32-bit aligned (an internal
 * copy is avoided).
 *
 * Note that, since SPI is a symmetrical interface, i.e. for every bit
 * sent a bit must be received, if bytesToReceive is less than bytesToSend
 * the difference will, by definition, be thrown away.  For instance, if
 * you send 10 bytes but only ask to receive 5, the last 5 bytes, the ones
 * that arrived after the receive buffer ran out, will be thrown away.
 * If you wish to ensure that no received data is lost you should always
 * provide a receive buffer that is the same length as your send buffer.
 *
 * @param handle         the handle of the SPI instance.
 * @param[in] pSend      a pointer to the block of data to send; may be NULL.
 * @param bytesToSend    the amount of data at pSend in BYTES (not words);
 *                       this must be an integer multiple of the confgured
 *                       word size for the device.
 * @param[out] pReceive  a pointer to a place to put the received data; may be
 *                       NULL.
 * @param bytesToReceive the amount of storage at pReceive in BYTES (not words);
 *                       this must be an integer multiple of the confgured
 *                       word size for the device.
 * @return               if pReceive is not NULL the number of bytes received
 *                       (which may contain fill bytes of course) or negative
 *                       error code; if pReceive is NULL then zero on success
 *                       else negative error code.
 */
int32_t uPortSpiControllerSendReceiveBlock(int32_t handle, const char *pSend,
                                           size_t bytesToSend, char *pReceive,
                                           size_t bytesToReceive);

#ifdef __cplusplus
}
#endif

/** @}*/

#endif // _U_PORT_SPI_H_

// End of file
