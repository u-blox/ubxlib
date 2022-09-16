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

#ifndef _U_CFG_APP_PLATFORM_SPECIFIC_H_
#define _U_CFG_APP_PLATFORM_SPECIFIC_H_

/** @file
 * @brief This header file contains dummy application
 * configuration information for the static size build.
 */

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

#define U_CFG_APP_CELL_UART                 0
#define U_CFG_APP_SHORT_RANGE_UART          0
#define U_CFG_APP_GNSS_UART                 0
#define U_CFG_APP_GNSS_I2C                  0
#define U_CFG_APP_SHORT_RANGE_ROLE          0
// Note: pins set to 0 rather than -1 in order to not accidentally
// disable any code that is conditional on them existing
#define U_CFG_APP_PIN_CELL_ENABLE_POWER     0
#define U_CFG_APP_PIN_CELL_PWR_ON           0
#define U_CFG_APP_PIN_CELL_RESET            0
#define U_CFG_APP_PIN_CELL_VINT             0
#define U_CFG_APP_PIN_CELL_DTR              0
#define U_CFG_APP_PIN_CELL_TXD              0
#define U_CFG_APP_PIN_CELL_RXD              0
#define U_CFG_APP_PIN_CELL_CTS              0
#define U_CFG_APP_PIN_CELL_RTS              0
#define U_CFG_APP_PIN_CELL_CTS_GET U_CFG_APP_PIN_CELL_CTS
#define U_CFG_APP_PIN_CELL_RTS_GET U_CFG_APP_PIN_CELL_RTS
#define U_CFG_APP_PIN_SHORT_RANGE_TXD       0
#define U_CFG_APP_PIN_SHORT_RANGE_RXD       0
#define U_CFG_APP_PIN_SHORT_RANGE_CTS       0
#define U_CFG_APP_PIN_SHORT_RANGE_RTS       0
#define U_CFG_APP_PIN_GNSS_EN               0
#define U_CFG_APP_PIN_GNSS_TXD              0
#define U_CFG_APP_PIN_GNSS_RXD              0
#define U_CFG_APP_PIN_GNSS_CTS              0
#define U_CFG_APP_PIN_GNSS_RTS              0
#define U_CFG_APP_PIN_GNSS_SDA              0
#define U_CFG_APP_PIN_GNSS_SCL              0

#endif // _U_CFG_APP_PLATFORM_SPECIFIC_H_

// End of file
