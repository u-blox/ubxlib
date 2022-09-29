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

#ifndef _U_SPARTN_CRC_H_
#define _U_SPARTN_CRC_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

/** \addtogroup __spartn __SPARTN
 *  @{
 */

/** @file
 * @brief This header file defines functions that perform CRC 4, 8, 16,
 * 24 and 32 as defined for SPARTN.  They are extracted from the
 * PointPerfect SDK library and re-published as part of ubxlib under
 * an Apache 2 license.
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

/** The possible SPARTN CRC types, values chosen to match
 * those used in the message itself.
 */
typedef enum {
    U_SPARTN_CRC_TYPE_8 = 0,
    U_SPARTN_CRC_TYPE_16 = 1,
    U_SPARTN_CRC_TYPE_24 = 2,
    U_SPARTN_CRC_TYPE_32 = 3,
    U_SPARTN_CRC_TYPE_MAX_NUM,
    // this is out here because it is not message CRC it's the
    // header CRC - useful to have in the list for the
    // test code but not something that has a "selector"
    // field in the SPARTN message
    U_SPARTN_CRC_TYPE_4,
    U_SPARTN_CRC_TYPE_NONE
} uSpartnCrcType_t;

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * FUNCTIONS
 * -------------------------------------------------------------- */

/** Perform a CRC4 calculation on a block of data.
 *
 * @param pData  a pointer to the data to be checked.
 * @param size   the number of bytes pointed to by pData.
 * @return       the CRC.
 */
uint8_t uSpartnCrc4(const char *pData, size_t size);

/** Perform a CRC8 calculation on a block of data.
 *
 * @param pData  a pointer to the data to be checked.
 * @param size   the number of bytes pointed to by pData.
 * @return       the CRC.
 */
uint8_t uSpartnCrc8(const char *pData, size_t size);

/** Perform a CRC16 calculation on a block of data.
 *
 * @param pData  a pointer to the data to be checked.
 * @param size   the number of bytes pointed to by pData.
 * @return       the CRC.
 */
uint16_t uSpartnCrc16(const char *pData, size_t size);

/** Perform a CRC24 Radix 64 calculation on a block of data.
 *
 * @param pData  a pointer to the data to be checked.
 * @param size   the number of bytes pointed to by pData.
 * @return       the CRC.
 */
uint32_t uSpartnCrc24(const char *pData, size_t size);

/** Perform a CRC32 calculation on a block of data.
 *
 * @param pData  a pointer to the data to be checked.
 * @param size   the number of bytes pointed to by pData.
 * @return       the CRC.
 */
uint32_t uSpartnCrc32(const char *pData, size_t size);

#ifdef __cplusplus
}
#endif

/** @}*/

#endif // _U_SPARTN_CRC_H_

// End of file
