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

#ifndef _U_COMMON_SPI_H_
#define _U_COMMON_SPI_H_

/* This file is NOT PERMITTED to bring in any other header files; it
 * should compile in a .c file that only use types from stdint.h and
 * stdbool.h. */

/** \addtogroup common Common
 *  @{
 */

/** @file
 * @brief Types common to SPI at all levels, specifically in the
 * port and device APIs.
 */

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/** The select pin for a given device is assumed to be active
 * low.  If the select pin is actually active high then OR this
 * with the value of the pin passed into this API.
 */
#define U_COMMON_SPI_PIN_SELECT_INVERTED 0x8000

/** AND this with uCommonSpiMode_t to get the CPOL bit, which is 0
 * for normal operation, 1 for inverted operation.
 */
#define U_COMMON_SPI_MODE_CPOL_BIT_MASK 0x02

/** AND this with uCommonSpiMode_t to get the CPHA bit, which is 0
 * for normal operation, 1 for inverted operation.
 */
#define U_COMMON_SPI_MODE_CPHA_BIT_MASK 0x01

#ifndef U_COMMON_SPI_CLOCK_FREQUENCY_HERTZ
/** The default SPI clock frequency in Hertz.
 */
# define U_COMMON_SPI_CLOCK_FREQUENCY_HERTZ 1000000
#endif

#ifndef U_COMMON_SPI_MODE
/** The default SPI mode.
 */
# define U_COMMON_SPI_MODE U_COMMON_SPI_CPOL_0_CPHA_0
#endif

#ifndef U_COMMON_SPI_WORD_SIZE_BYTES
/** The default word size in bytes.
 */
# define U_COMMON_SPI_WORD_SIZE_BYTES 1
#endif

#ifndef U_COMMON_SPI_LSB_FIRST
/** The default bit order.
 */
# define U_COMMON_SPI_LSB_FIRST false
#endif

#ifndef U_COMMON_SPI_START_OFFSET_NANOSECONDS
/** The default time that the chip select line for a given SPI device
 * is asserted before clocking begins in nanoseconds.
 */
# define U_COMMON_SPI_START_OFFSET_NANOSECONDS 0
#endif

#ifndef U_COMMON_SPI_STOP_OFFSET_NANOSECONDS
/** The default time that the chip select line for a given SPI device
 * remains asserted after clocking stops in nanoseconds.
 */
# define U_COMMON_SPI_STOP_OFFSET_NANOSECONDS 0
#endif

#ifndef U_COMMON_SPI_SAMPLE_DELAY_NANOSECONDS
/** The default time from the "read" clock edge until a data bit
 * is sampled in nanoseconds.
 */
# define U_COMMON_SPI_SAMPLE_DELAY_NANOSECONDS 0
#endif

#ifndef U_COMMON_SPI_FILL_WORD
/** The default fill word to be sent when nothing is specified to
 * be sent.
 */
# define U_COMMON_SPI_FILL_WORD 0xFFFFFFFF
#endif

/** The maximum value of indexSelect in the
 * #uCommonSpiControllerDevice_t structure.
 */
#define U_COMMON_SPI_CONTROLLER_MAX_SELECT_INDEX 2

/** The default configuration for an SPI device as seen by
 * a controller, see also
 * #U_COMMON_SPI_CONTROLLER_DEVICE_INDEX_DEFAULTS for systems
 * where pinSelect is replaced by an index (for example you
 * may wish to use that form on Zephyr, though this form
 * will also work).
 */
#define U_COMMON_SPI_CONTROLLER_DEVICE_DEFAULTS(pinSelect) {pinSelect,                                \
                                                            U_COMMON_SPI_CLOCK_FREQUENCY_HERTZ,       \
                                                            U_COMMON_SPI_MODE,                        \
                                                            U_COMMON_SPI_WORD_SIZE_BYTES,             \
                                                            U_COMMON_SPI_LSB_FIRST,                   \
                                                            U_COMMON_SPI_START_OFFSET_NANOSECONDS,    \
                                                            U_COMMON_SPI_STOP_OFFSET_NANOSECONDS,     \
                                                            U_COMMON_SPI_SAMPLE_DELAY_NANOSECONDS,    \
                                                            U_COMMON_SPI_FILL_WORD,                   \
                                                            -1}

/** The default configuration for an SPI device as seen by
 * a controller.  Use this instead of
 * #U_COMMON_SPI_CONTROLLER_DEVICE_DEFAULTS if you want to
 * use indexSelect as an index into a device/platform specific
 * structure which defines an array of chip select pins, rather
 * than specifying the select pin directly; for example, this
 * may be used with Zephyr.
 */
#define U_COMMON_SPI_CONTROLLER_DEVICE_INDEX_DEFAULTS(indexSelect) {-1,                                       \
                                                                    U_COMMON_SPI_CLOCK_FREQUENCY_HERTZ,       \
                                                                    U_COMMON_SPI_MODE,                        \
                                                                    U_COMMON_SPI_WORD_SIZE_BYTES,             \
                                                                    U_COMMON_SPI_LSB_FIRST,                   \
                                                                    U_COMMON_SPI_START_OFFSET_NANOSECONDS,    \
                                                                    U_COMMON_SPI_STOP_OFFSET_NANOSECONDS,     \
                                                                    U_COMMON_SPI_SAMPLE_DELAY_NANOSECONDS,    \
                                                                    U_COMMON_SPI_FILL_WORD,                   \
                                                                    indexSelect}

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/** The possible clock and data-read modes, arranged so that the
 * #U_COMMON_SPI_MODE_CPOL_BIT_MASK and #U_COMMON_SPI_MODE_CPHA_BIT_MASK
 * macros will work.
 */
typedef enum {
    U_COMMON_SPI_CPOL_0_CPHA_0 = 0x00, /**< "in" pin should read data when clock is high,
                                            "out" pin should hold data when clock is high. */
    U_COMMON_SPI_CPOL_0_CPHA_1 = 0x01, /**< "in" pin should read data when clock is high,
                                            "out"  pin should hold data when clock is low. */
    U_COMMON_SPI_CPOL_1_CPHA_0 = 0x02, /**< "in" pin should read data when clock is low,
                                            "out" pin should hold data when clock is high. */
    U_COMMON_SPI_CPOL_1_CPHA_1 = 0x03  /**< "in" pin should read data when clock is low,
                                            "out" pin should hold data when clock is low. */
} uCommonSpiMode_t;

/** The configuration information for an SPI device that a controller needs to know.
 *
 * Note: though there are many options here, and the implementations are written to
 * support them, where permitted, what we TEST is operation with a u-blox GNSS
 * receiver, so: pinSelect non-inverted, 1 MHz clock, CPOL/CPHA 0, 1 byte word length,
 * no offsets/delays and 0xFF fill.
 *
 * Note: if this is ever updated don't forget to update
 * #U_COMMON_SPI_CONTROLLER_DEVICE_DEFAULTS and
 * #U_COMMON_SPI_CONTROLLER_DEVICE_INDEX_DEFAULTS to match.
 */
typedef struct {
    int32_t pinSelect;              /**< the pin that should be toggled to select
                                         the device; assumed to be active low
                                         unless #U_COMMON_SPI_PIN_SELECT_INVERTED is
                                         ORed with this value, in which case the
                                         pin is assumed to be active high.
                                         Use -1 here and in indexSelect if there is
                                         no select pin.  On platforms where pin choices
                                         are made at compile time (e.g. Zephyr or Linux)
                                         you may prefer to set this to -1 and instead use
                                         indexSelect to choose which of the chip select
                                         pins predefined for the SPI controller is to be
                                         used; for the Zephyr case you _can_ still just
                                         set the pin here, whether or not it is listed as
                                         a chip select pin for your SPI controller, while
                                         for Linux you _must_ use indexSelect.  Note that
                                         platforms may restrict the choice of select pin,
                                         depending on the SPI HW block in use (for instance
                                         STM32F4 does, see the data sheet for your STM32F4
                                         device for more details). */
    int32_t frequencyHertz;         /**< the clock frequency in Hertz.  Note that the
                                         frequency you end up with is the nearest the
                                         MCU can achieve, bearing in mind multiples of
                                         bus clocks etc., that is LESS THAN OR EQUAL
                                         to this; it may end up being half this if
                                         you're unlucky - please read back the value
                                         that is achieved and experiment. */
    uCommonSpiMode_t mode;          /**< the clock/data-read mode. */
    size_t wordSizeBytes;           /**< the word size in bytes; the number of bytes
                                         to be sent or received MUST BE an integer
                                         multiple of this size.  Values bigger than 1
                                         are not supported on all platforms; use
                                         uPortSpiControllerGetDevice() with the
                                         SPI transport handle to determine what
                                         setting has taken effect. */
    bool lsbFirst;                  /**< set this to true if LSB is transmitted first,
                                         false if MSB is transmitted first. */
    int32_t startOffsetNanoseconds; /**< the time that pinSelect must be asserted
                                         before the start of clocking in nanoseconds;
                                         not supported on all platforms, use
                                         uPortSpiControllerGetDevice() with the SPI
                                         transport handle to determine what setting
                                         has taken effect. */
    int32_t stopOffsetNanoseconds;  /**< the time that pinSelect must remain asserted
                                         after the end of clocking in nanoseconds;
                                         not supported on all platforms, use
                                         uPortSpiControllerGetDevice() with the SPI
                                         transport handle to determine what setting
                                         has taken effect. */
    int32_t sampleDelayNanoseconds; /**< the time from the "read" clock edge until
                                         the incoming data bit is sampled in nanoseconds;
                                         not supported on all platforms, use
                                         uPortSpiControllerGetDevice() with the SPI
                                         transport handle to determine what setting has
                                         taken effect. */
    uint32_t fillWord;              /**< the fill word to be sent while reading data;
                                         not supported on all platforms (where 0xFF
                                         will be used), use uPortSpiControllerGetDevice()
                                         with the SPI transport handle to determine what
                                         setting has taken effect. */
    int32_t indexSelect;            /**< the index of the chip select pin from the set
                                         of chip select pins defined for the SPI
                                         controller to use for this device.  Only takes
                                         effect if pinSelect is -1.  Use this on platforms
                                         where the chip select pins are predefined at
                                         compile time for the SPI controller (e.g.
                                         Zephyr or Linux) and you wish to chose which entry
                                         from the array is used with this device (e.g. 0 for
                                         the first, maybe only, entry).  Use -1 here
                                         (and in pinSelect) to not use a select pin.
                                         Indexes up to
                                         #U_COMMON_SPI_CONTROLLER_MAX_SELECT_INDEX are
                                         supported.  Note that, where this structure is
                                         returned by a "getter", indexSelect may not
                                         be populated, pinSelect may be populated
                                         instead. */
} uCommonSpiControllerDevice_t;

/** @}*/

#endif // _U_COMMON_SPI_H_

// End of file
