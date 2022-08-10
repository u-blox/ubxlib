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

#ifndef _U_PORT_I2C_H_
#define _U_PORT_I2C_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

/** \addtogroup __port
 *  @{
 */

/** @file
 * @brief Porting layer for I2C access functions.  These functions
 * are thread-safe.  Note that these functions are currently only
 * used to talk to u-blox GNSS modules and that reflects the extent
 * to which they are tested; should you decide to use them to talk
 * with other I2C devices then it may be worth expanding the testing
 * also.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

#ifndef U_PORT_I2C_CLOCK_FREQUENCY_HERTZ
/** The default I2C clock frequency in Hertz.
 */
# define U_PORT_I2C_CLOCK_FREQUENCY_HERTZ 100000
#endif

#ifndef U_PORT_I2C_TIMEOUT_MILLISECONDS
/** The default I2C timeout in milliseconds, noting that this value
 * is per-byte, i.e. it is very short.
 */
# define U_PORT_I2C_TIMEOUT_MILLISECONDS 10
#endif

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * FUNCTIONS
 * -------------------------------------------------------------- */

/** Initialise I2C handling.  If I2C has already been initialised
 * this function will return success without doing anything.
 *
 * @return  zero on success else negative error code.
 */
int32_t uPortI2cInit();

/** Shutdown I2C handling; any open I2C instances will be closed.
 */
void uPortI2cDeinit();

/** Open an I2C instance.  If an I2C instance has already
 * been opened on the given I2C HW block this function returns
 * an error.  Note that the pin numbers are those of the MCU:
 * if you are using an MCU inside a u-blox module the IO pin
 * numbering for the module is likely different to that from
 * the MCU: check the data sheet for the module to determine
 * the mapping.
 *
 * IMPORTANT: some platforms, specifically Zephyr (used on NRF53),
 * do not permit I2C pin choices to be made at link-time, only at
 * compile time.  For such platforms the pins passed in here MUST
 * be -1 (otherwise an error will be returned) and you MUST check
 * the README.md for that platform to find out how the pins
 * are chosen.
 *
 * @param i2c            the I2C HW block to use.
 * @param pinSda         the data pin, a positive integer or -1 if
 *                       the pin choice has already been determined
 *                       at compile time.
 * @param pinSdc         the clock pin, a positive integer or -1
 *                       if the pin choice has already been
 *                       determined at compile time.
 * @param controller     set to true for an I2C controller; this is for
 *                       forwards-compatibility only, it must currently
 *                       always be set to true since target/peripheral/
 *                       slave mode is not supported.
 * @return               an I2C handle else negative error code.
 */
int32_t uPortI2cOpen(int32_t i2c, int32_t pinSda, int32_t pinSdc,
                     bool controller);

/** This is like uPortI2cOpen() but it does NOT modify any of the
 * platform HW; use this if you have ALREADY opened/configured the I2C
 * port and you simply want to allow the port API to access it.
 *
 * @param i2c            the I2C HW block to adopt.
 * @param controller     set to true for an I2C controller; this is for
 *                       forwards-compatibility only, it must currently
 *                       always be set to true since target/peripheral/
 *                       slave mode is not supported.
 * @return               an I2C handle else negative error code.
 */
int32_t uPortI2cAdopt(int32_t i2c, bool controller);

/** Close an I2C instance; if the I2C interface was adopted rather
 * than opened this will only free memory etc., it will do nothing
 * to the I2C HW.
 *
 * @param handle the handle of the I2C instance to close.
 */
void uPortI2cClose(int32_t handle);

/** Close an I2C instance and attempt to recover the I2C bus; useful
 * if a slave has stopped working in a bad way, pulling SDA low.
 * WHATEVER THE RETURN VALUE of this function, you must ALWAYS call
 * uPortI2cOpen() once more to continue using I2C; even if bus
 * recovery is not supported on a given platform (e.g. ESP-IDF
 * performs bus recovery when it encounters an error, there is no
 * explicit function to do so), provided you have given a valid
 * handle the I2C instance WILL have been closed.  Note that this
 * function will not recover all situations and it is not always
 * possible for this function to determine that it has succeeded;
 * it is best for you to do that by addressing a peripheral that
 * you know works.  Ultimately the only reliable I2C bus recovery
 * method is out-of-band, i.e. wire the reset pins of your I2C
 * devices together and hang them off a GPIO pin of this MCU that
 * you can reset them all with.
 * Note that if the I2C interface was adopted rather than
 * opened this will return #U_ERROR_COMMON_NOT_SUPPORTED.
 *
 * @param handle the handle of the I2C instance.
 * @return       zero on success else negative error code.
 */
int32_t uPortI2cCloseRecoverBus(int32_t handle);

/** Set the I2C clock frequency.  If this is not called
 * #U_PORT_I2C_CLOCK_FREQUENCY_HERTZ will be used.  Note that
 * the I2C specification generally permits only certain
 * frequencies (e.g. 100 kHz, 400 kHz, 1 MHz, 3.4 MHz and
 * 5 MHz) and which frequencies will work depends on the
 * host chipset and the peripheral on the I2C bus being
 * addressed.  On some platforms (e.g. ESP-IDF) setting the
 * clock requires the I2C instance to be taken down and brought
 * back up again, hence if this function returns an error the
 * I2C instance should be closed and re-opened to ensure
 * that all is good.
 * Note that if the I2C interface was adopted rather than
 * opened this will return #U_ERROR_COMMON_NOT_SUPPORTED.
 *
 * @param handle      the handle of the I2C instance.
 * @param clockHertz  the clock frequency in Hertz.
 * @return            zero on success else negative error code.
 */
int32_t uPortI2cSetClock(int32_t handle, int32_t clockHertz);

/** Get the I2C clock frequency.
 * Note that if the I2C interface was adopted rather than
 * opened this will return #U_ERROR_COMMON_NOT_SUPPORTED.
 *
 * @param handle     the handle of the I2C instance.
 * @return           the clock frequency in Hertz, else negative
 *                   error code.
 */
int32_t uPortI2cGetClock(int32_t handle);

/** Set the timeout for an I2C instance; this timeout is PER BYTE,
 * i.e. it is very short.  Not all platforms support setting the
 * I2C timeout through an API (e.g. Zephyr doesn't).  Where setting
 * of a timeout in this way is supported, and this function is not
 * called, #U_PORT_I2C_TIMEOUT_MILLISECONDS will be used.  It is
 * best to call this once after opening the I2C instance since
 * setting the timeout may reset the I2C HW.
 * Note that on some platforms, if the I2C interface was adopted
 * rather than opened, this will return #U_ERROR_COMMON_NOT_SUPPORTED.
 *
 * @param handle     the handle of the I2C instance.
 * @param timeoutMs  the timeout in milliseconds.
 * @return           zero on success else negative error code.
 */
int32_t uPortI2cSetTimeout(int32_t handle, int32_t timeoutMs);

/** Get the timeout for an I2C instance.  Not all platforms support
 * getting the I2C timeout through an API (e.g. Zephyr doesn't).
 * Note that on some platforms, if the I2C interface was adopted
 * rather than opened, this will return #U_ERROR_COMMON_NOT_SUPPORTED.
 *
 * @param handle     the handle of the I2C instance.
 * @return           the timeout in milliseconds, else negative
 *                   error code.
 */
int32_t uPortI2cGetTimeout(int32_t handle);

/** Send and/or receive over the I2C interface as a controller.
 * Note that the NRF52 and NRF53 chips require all buffers to
 * be in RAM.
 * Note that the uPortI2cSetTimeout() (or the equivalent set
 * by a platform at compile-time) applies for the whole of this
 * transaction, i.e. the peripheral must begin responding within
 * that time; if you wish to allow the peripheral longer to respond
 * you should take control of the time allowed yourself by calling
 * uPortI2cControllerSend() and then, after the appropriate time,
 * this function with only the receive buffer set.
 *
 * @param handle         the handle of the I2C instance.
 * @param address        the I2C address to send to; only the lower
 *                       7 bits are used unless the platform supports
 *                       10-bit addressing.  Note that the NRF5 SDK,
 *                       and hence Zephyr on NRF52/53 (which uses the NRF5
 *                       SDK under the hood) does not support 10-bit
 *                       addressing and, in any case, we've not yet found
 *                       a device that supports 10-bit addressing to test
 *                       against.
 * @param pSend          a pointer to the data to send, use NULL
 *                       if only receive is required.  This function
 *                       will do nothing, and return success, if both
 *                       pSend and pReceive are NULL; if you want to do
 *                       a "scan" for valid addresses, use
 *                       uPortI2cControllerSend() with a NULL pSend,
 *                       though note that not all platforms support this.
 * @param bytesToSend    the number of bytes to send, must be zero if pSend
 *                       is NULL.
 * @param pReceive       a pointer to a buffer in which to store received
 *                       data; use NULL if only send is required.
 * @param bytesToReceive the size of buffer pointed to by pReceive, must
 *                       be zero if pReceive is NULL.
 * @return               if pReceive is not NULL the number of bytes
 *                       received or negative error code; if pReceive is
 *                       NULL then zero on success else negative error code.
 *                       Note that the underlying platform drivers often
 *                       do not report the number of bytes received and
 *                       hence the return value may just be either an
 *                       error code or bytesToReceive copied back to you.
 */
int32_t uPortI2cControllerSendReceive(int32_t handle, uint16_t address,
                                      const char *pSend, size_t bytesToSend,
                                      char *pReceive, size_t bytesToReceive);

/** Perform just a send over the I2C interface as a controller, with the
 * option of omitting the stop marker on the end.
 * Note that the NRF52 and NRF53 chips require the buffer to be in RAM.
 *
 * @param handle         the handle of the I2C instance.
 * @param address        the I2C address to send to; only the lower
 *                       7 bits are used unless the platform supports
 *                       10-bit addressing.  Note that the NRF5 SDK,
 *                       and hence Zephyr on NRF52/53 (which uses the NRF5
 *                       SDK under the hood) does not support 10-bit
 *                       addressing and, in any case, we've not yet found
 *                       a device that supports 10-bit addressing to test
 *                       against.
 * @param pSend          a pointer to the data to send; setting this to
 *                       NULL will return success only if a device with
 *                       the given address is present on the I2C bus;
 *                       however note that the NRFX drivers used on nRF52
 *                       and nRF53 by NRF-SDK and Zephyr don't support
 *                       sending only the address, data must follow.
 * @param bytesToSend    the number of bytes to send; must be zero if
 *                       pSend is NULL.
 * @param noStop         if true then no stop is sent at the end of the
 *                       transmission; this is useful for devices such
 *                       as EEPROMs or, in certain situations, u-blox GNSS
 *                       modules, which allow writing of a memory address
 *                       byte or bytes, followed by no stop bit; the data
 *                       from that memory address may then be received
 *                       e.g. by calling uPortI2cControllerSendReceive()
 *                       with a receive buffer only.  This is sometimes
 *                       called using a "repeated start bit", because
 *                       there is no stop bit between the start bit
 *                       sent by this function and that sent by
 *                       uPortI2cControllerSendReceive().
 * @return               zero on success else negative error code.
 */
int32_t uPortI2cControllerSend(int32_t handle, uint16_t address,
                               const char *pSend, size_t bytesToSend,
                               bool noStop);

#ifdef __cplusplus
}
#endif

/** @}*/

#endif // _U_PORT_I2C_H_

// End of file
