/*
 * Copyright 2019-2022 u-blox Ltd
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

#ifndef _U_PORT_APP_PLATFORM_SPECIFIC_H_
#define _U_PORT_APP_PLATFORM_SPECIFIC_H_

/** @file
 * @brief Most of the parameters in this header file are required only to permit
 * the ubxlib example/test code to compile, and hence are present and set to -1,.
 */

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS FOR A CELLULAR MODULE ON SARAR5UCPU: MISC
 * -------------------------------------------------------------- */

#ifndef U_CFG_APP_CELL_UART
/** The UART HW block to use inside the chip to talk to a
 * cellular module.
 */
# define U_CFG_APP_CELL_UART              0
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS FOR A CELLULAR MODULE ON SARAR5UCPU: PINS
 * -------------------------------------------------------------- */

#ifndef U_CFG_APP_PIN_CELL_ENABLE_POWER
/** Required for compilation of ubxlib tests/examples but not
 * relevant to SARAR5UCPU and hence set to -1.
 */
# define U_CFG_APP_PIN_CELL_ENABLE_POWER -1
#endif

#ifndef U_CFG_APP_PIN_CELL_PWR_ON
/** Required for compilation of ubxlib tests/examples but not
 * relevant to SARAR5UCPU and hence set to -1.
 */
# define U_CFG_APP_PIN_CELL_PWR_ON       -1
#endif

#ifndef U_CFG_APP_PIN_CELL_VINT
/** Required for compilation of ubxlib tests/examples but not
 * relevant to SARAR5UCPU and hence set to -1.
 */
# define U_CFG_APP_PIN_CELL_VINT         -1
#endif

#ifndef U_CFG_APP_PIN_CELL_DTR
/** Required for compilation of ubxlib tests/examples but not
 * relevant to SARAR5UCPU and hence set to -1.
 */
# define U_CFG_APP_PIN_CELL_DTR         -1
#endif

#ifndef U_CFG_APP_PIN_CELL_TXD
/** Required for compilation of ubxlib tests/examples but not
 * relevant to SARAR5UCPU and hence set to -1.
 */
# define U_CFG_APP_PIN_CELL_TXD          -1
#endif

#ifndef U_CFG_APP_PIN_CELL_RXD
/** Required for compilation of ubxlib tests/examples but not
 * relevant to SARAR5UCPU and hence set to -1.
 */
# define U_CFG_APP_PIN_CELL_RXD          -1
#endif

#ifndef U_CFG_APP_PIN_CELL_CTS
/** Required for compilation of ubxlib tests/examples but not
 * relevant to SARAR5UCPU and hence set to -1.
 */
# define U_CFG_APP_PIN_CELL_CTS          -1
#endif

#ifndef U_CFG_APP_PIN_CELL_RTS
/** Required for compilation of ubxlib tests/examples but not
 * relevant to SARAR5UCPU and hence set to -1.
 */
# define U_CFG_APP_PIN_CELL_RTS          -1
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS FOR A BLE/WIFI MODULE ON SARAR5UCPU: MISC
 * -------------------------------------------------------------- */

/** Required for compilation of ubxlib tests/examples but not
 * relevant to SARAR5UCPU and hence set to -1.
 */
#ifndef U_CFG_APP_SHORT_RANGE_UART
# define U_CFG_APP_SHORT_RANGE_UART        -1
#endif

/** Required for compilation of ubxlib tests/examples but not
 * relevant to SARAR5UCPU and hence set to -1.
 */
#ifndef U_CFG_APP_SHORT_RANGE_ROLE
# define U_CFG_APP_SHORT_RANGE_ROLE        -1
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS FOR A BLE/WIFI MODULE ON SARAR5UCPU: PINS
 * -------------------------------------------------------------- */

/** Required for compilation of ubxlib tests/examples but not
 * relevant to SARAR5UCPU and hence set to -1.
 */
#ifndef U_CFG_APP_PIN_SHORT_RANGE_TXD
# define U_CFG_APP_PIN_SHORT_RANGE_TXD      -1
#endif

/** Required for compilation of ubxlib tests/examples but not
 * relevant to SARAR5UCPU and hence set to -1.
 */
#ifndef U_CFG_APP_PIN_SHORT_RANGE_RXD
# define U_CFG_APP_PIN_SHORT_RANGE_RXD      -1
#endif

/** Required for compilation of ubxlib tests/examples but not
 * relevant to SARAR5UCPU and hence set to -1.
 */
#ifndef U_CFG_APP_PIN_SHORT_RANGE_CTS
# define U_CFG_APP_PIN_SHORT_RANGE_CTS      -1
#endif

/** Required for compilation of ubxlib tests/examples but not
 * relevant to SARAR5UCPU and hence set to -1.
 */
#ifndef U_CFG_APP_PIN_SHORT_RANGE_RTS
# define U_CFG_APP_PIN_SHORT_RANGE_RTS      -1
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS FOR A GNSS MODULE ON SARAR5UCPU: MISC
 * -------------------------------------------------------------- */

#ifndef U_CFG_APP_GNSS_UART
/** Required for compilation of ubxlib tests/examples but not
 * relevant to SARAR5UCPU and hence set to -1.
 */
# define U_CFG_APP_GNSS_UART                  -1
#endif

#ifndef U_CFG_APP_GNSS_I2C
/** Required for compilation of ubxlib tests/examples but not
 * relevant to SARAR5UCPU and hence set to -1.
 */
# define U_CFG_APP_GNSS_I2C                   -1
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS FOR A GNSS MODULE ON SARAR5UCPU: PINS
 * -------------------------------------------------------------- */

#ifndef U_CFG_APP_PIN_GNSS_ENABLE_POWER
/** Required for compilation of ubxlib tests/examples but not
 * relevant to SARAR5UCPU and hence set to -1.
 */
# define U_CFG_APP_PIN_GNSS_ENABLE_POWER     -1
#endif

#ifndef U_CFG_APP_PIN_CELL_RESET
/** Required for compilation of ubxlib tests/examples but not
 * relevant to SARAR5UCPU and hence set to -1.
 */
# define U_CFG_APP_PIN_CELL_RESET             -1
#endif

#ifndef U_CFG_APP_PIN_GNSS_TXD
/** Required for compilation of ubxlib tests/examples but not
 * relevant to SARAR5UCPU and hence set to -1.
 */
# define U_CFG_APP_PIN_GNSS_TXD              -1
#endif

#ifndef U_CFG_APP_PIN_GNSS_RXD
/** Required for compilation of ubxlib tests/examples but not
 * relevant to SARAR5UCPU and hence set to -1.
 */
# define U_CFG_APP_PIN_GNSS_RXD              -1
#endif

#ifndef U_CFG_APP_PIN_GNSS_CTS
/** Required for compilation of ubxlib tests/examples but not
 * relevant to SARAR5UCPU and hence set to -1.
 */
# define U_CFG_APP_PIN_GNSS_CTS              -1
#endif

#ifndef U_CFG_APP_PIN_GNSS_RTS
/** Required for compilation of ubxlib tests/examples but not
 * relevant to SARAR5UCPU and hence set to -1.
 */
# define U_CFG_APP_PIN_GNSS_RTS              -1
#endif

#ifndef U_CFG_APP_PIN_GNSS_SDA
/** Required for compilation of ubxlib tests/examples but not
 * relevant to SARAR5UCPU and hence set to -1.
 */
# define U_CFG_APP_PIN_GNSS_SDA               -1
#endif

#ifndef U_CFG_APP_PIN_GNSS_SCL
/** Required for compilation of ubxlib tests/examples but not
 * relevant to SARAR5UCPU and hence set to -1.
 */
# define U_CFG_APP_PIN_GNSS_SCL               -1
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS FOR A GNSS MODULE ON SARAR5UCPU: CELLULAR MODULE PINS
 * -------------------------------------------------------------- */

#ifndef U_CFG_APP_CELL_PIN_GNSS_POWER
/** Required for compilation of ubxlib tests/examples but not
 * relevant to SARAR5UCPU and hence set to -1.
 */
# define U_CFG_APP_CELL_PIN_GNSS_POWER  -1
#endif

#ifndef U_CFG_APP_CELL_PIN_GNSS_DATA_READY
/** Required for compilation of ubxlib tests/examples but not
 * relevant to SARAR5UCPU and hence set to -1.
 */
# define U_CFG_APP_CELL_PIN_GNSS_DATA_READY  -1
#endif

#endif // _U_PORT_APP_PLATFORM_SPECIFIC_H_

// End of file
