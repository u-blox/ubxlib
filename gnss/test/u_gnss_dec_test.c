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

/* Only #includes of u_* and the C standard library are allowed here,
 * no platform stuff and no OS stuff.  Anything required from
 * the platform/OS must be brought in through u_port* to maintain
 * portability.
 */

/** @file
 * @brief Basic tests for the GNSS decode API: they do not require a GNSS
 * module to run, hence these should pass on all platforms.
 * IMPORTANT: see notes in u_cfg_test_platform_specific.h for the
 * naming rules that must be followed when using the U_PORT_TEST_FUNCTION()
 * macro.
 */

# ifdef U_CFG_OVERRIDE
#  include "u_cfg_override.h" // For a customer's configuration override
# endif

#include "limits.h"    // INT_MIN
#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"
#include "string.h"    // memset()
#include "stdio.h"     // snprintf()

#include "u_cfg_sw.h"
#include "u_cfg_os_platform_specific.h"
#include "u_cfg_app_platform_specific.h"
#include "u_cfg_test_platform_specific.h"

#include "u_error_common.h"

#include "u_port.h"
#include "u_port_os.h"
#include "u_port_heap.h"
#include "u_port_debug.h"

#include "u_gnss_module_type.h"
#include "u_gnss_type.h"
#include "u_gnss.h"
#include "u_gnss_dec.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/** The base string to put at the start of all prints from this test.
 */
#define U_TEST_PREFIX_BASE "U_GNSS_DEC_TEST"

/** The string to put at the start of all prints from this test
 * that do not require any iterations on the end.
 */
#define U_TEST_PREFIX U_TEST_PREFIX_BASE ": "

/** Print a whole line, with terminator, prefixed for this test
 * file, no iteration(s) version.
 */
#define U_TEST_PRINT_LINE(format, ...) uPortLog(U_TEST_PREFIX format "\n", ##__VA_ARGS__)

/** The string to put at the start of all prints from this test
 * where an interation is required on the end.
 */
#define U_TEST_PREFIX_X U_TEST_PREFIX_BASE "_%d: "

/** Print a whole line, with terminator and an iteration on the end,
 * prefixed for this test file.
 */
#define U_TEST_PRINT_LINE_X(format, ...) uPortLog(U_TEST_PREFIX_X format "\n", ##__VA_ARGS__)

/** The string to put at the start of all prints from this test
 * where two interations are required on the end.
 */
#define U_TEST_PREFIX_X_Y U_TEST_PREFIX_BASE "_%d_%d: "

/** Print a whole line, with terminator and iterations on the end,
 * prefixed for this test file.
 */
#define U_TEST_PRINT_LINE_X_Y(format, ...) uPortLog(U_TEST_PREFIX_X_Y format "\n", ##__VA_ARGS__)

#ifndef U_GNSS_DEC_TEST_HEX_DUMP_WIDTH
/** Width of a nice hex dump, 16 being good.
 */
# define U_GNSS_DEC_TEST_HEX_DUMP_WIDTH 16
#endif

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* IMPLEMENTATION NOTE: the stuff below is EXTREMELY clumsy.
* This is because MSVC doesn't support the GCC constructor mechanism
* that we use to collect all the ubxlib tests, hence we have to
* compile the tests as C++ code to make that work, and in C++ you
* can't statically initialise union members other than the first,
* hence any type which contains a union (e.g. uGnssMessageId_t
* and uGnssDecUnion_t) that we want to give static test data
* really doesn't work in here; all the individual elements of
* what was a union need to be spread out into their own
* variables/types.
*/

/** An array of binary test data, with length,
 */
typedef struct {
    const char *p;
    size_t length;
} uGnssDecTestDataBinary_t;

/** Message protocol and type, split out to avoid being a union.
 */
typedef struct {
    uGnssProtocol_t type;
    uint16_t idUbxOrRtcm;
    const char *pIdNmea;
} uGnssDecTestDataMessageId_t;

/** An item of test data for a known message type: raw input with
 * protocol/ID and a void pointer to the decoded output
 * (to a(void) (hehe!) it being a union).
 */
typedef struct {
    uGnssDecTestDataBinary_t raw;
    uGnssDecTestDataMessageId_t id;
    void *pDecoded;
} uGnssDecTestDataKnown_t;

/** Array of test data for a known message type.
 */
typedef struct {
    const uGnssDecTestDataKnown_t *pTestData; /**< Pointer to the start of an array of test data. */
    size_t size; /**< the number of elements at pTestData. */
    size_t decodedStructureSize; /**< the size of the decoded structure for this message type */
} uGnssDecTestDataKnownSet_t;

/** Struct to hold data for testing the user callback: a
 * string of data that is a message and the corresponding ID.
 */
typedef struct {
    uGnssDecTestDataBinary_t raw;
    uGnssDecTestDataMessageId_t id;
    int32_t callbackDecodeIndicator;
} uGnssDecTestDataCallback_t;

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

/** Decoded test data for UBX-NAV-PVT, to be used by gUbxNavPvt (item 0).
 */
static const uGnssDecUbxNavPvt_t gUbxNavPvtDecoded0 = {
    477230000 /* iTOW */, 2023 /* year */, 8 /* month */, 11 /* day */,
    12 /* hour */, 33 /* min */, 32 /* sec */, 0xf7 /* valid */,
    1003 /* tAcc */, -73790 /* nano */,
    U_GNSS_DEC_UBX_NAV_PVT_FIX_TYPE_3D /* fixType */, 0x01 /* flags */,
    0xea /* flags2 */, 22 /* numSV */, -748276 /* lon */,
    522227387 /* lat */, 128858 /* height */, 83104 /* hMSL */,
    911 /* hAcc */, 1427 /* vAcc */, -8 /* velN */, -5 /* velE */,
    19 /* velD */, 9 /* gSpeed */, 0 /* headMot */, 172 /* sAcc */,
    14253526 /* headAcc */, 118 /* pDOP */, 0 /* flags3 */,
    0 /* headVeh */, 0 /* magDec */, 0 /* magAcc */
};

/** Array of test data for UBX-NAV-PVT.
 */
static const uGnssDecTestDataKnown_t gUbxNavPvt[] = {
    {
        {
            "\xb5\x62\x01\x07\x5c\x00\xb0\xf3\x71\x1c\xe7\x07\x08\x0b\x0c\x21"
            "\x20\xf7\xeb\x03\x00\x00\xc2\xdf\xfe\xff\x03\x01\xea\x16\x0c\x95"
            "\xf4\xff\xbb\x8e\x20\x1f\x5a\xf7\x01\x00\xa0\x44\x01\x00\x8f\x03"
            "\x00\x00\x93\x05\x00\x00\xf8\xff\xff\xff\xfb\xff\xff\xff\x13\x00"
            "\x00\x00\x09\x00\x00\x00\x00\x00\x00\x00\xac\x00\x00\x00\xd6\x7d"
            "\xd9\x00\x76\x00\x00\x00\xee\x13\x4f\x2f\x00\x00\x00\x00\x00\x00"
            "\x00\x00\xbc\x7f", 100
        },
        {
            U_GNSS_PROTOCOL_UBX, 0x0107, NULL
        },
        (void *) &gUbxNavPvtDecoded0
    }
};

/** Decoded test data for UBX-NAV-HPPOSLLH, to be used by gUbxNavHpposllh (item 0).
 */
static const uGnssDecUbxNavHpposllh_t gUbxNavHpposllhDecoded0 = {
    0 /* version */,  0x00 /* flags */, 486173000 /* iTOW */,
    -748127 /* lon */, 522227263 /* lat */, 131404 /* height */,
    85650 /* hMSL */, -42 /* lonHp */, 25 /* latHp */, 3 /* heightHp */,
    2 /* hMSLHp */, 9242 /* hAcc */, 13145 /* vAcc */
};

/** Array of test data for UBX-NAV-HPPOSLLH.
 */
static const uGnssDecTestDataKnown_t gUbxNavHpposllh[] = {
    {
        {
            "\xb5\x62\x01\x14\x24\x00\x00\x00\x00\x00\x48\x69\xfa\x1c\xa1\x95"
            "\xf4\xff\x3f\x8e\x20\x1f\x4c\x01\x02\x00\x92\x4e\x01\x00\xd6\x19"
            "\x03\x02\x1a\x24\x00\x00\x59\x33\x00\x00\x23\xad", 44
        },
        {
            U_GNSS_PROTOCOL_UBX, 0x0114, NULL
        },
        (void *) &gUbxNavHpposllhDecoded0
    }
};

/** Array of arrays of test vectors for all known message types.
 */
static const uGnssDecTestDataKnownSet_t gTestDataKnownSet[] = {
    {gUbxNavPvt, sizeof(gUbxNavPvt) / sizeof(gUbxNavPvt[0]), sizeof(gUbxNavPvtDecoded0)},
    {gUbxNavHpposllh, sizeof(gUbxNavHpposllh) / sizeof(gUbxNavHpposllh[0]), sizeof(gUbxNavHpposllhDecoded0)}
};

/** Flag to share with the user callback.
 */
static int32_t gCallback;

/** Sample data for testing the use callback: a few NMEA message
 * strings, taken from https://en.wikipedia.org/wiki/NMEA_0183,
 * some sample RTCM messages taken from
 * https://cdn.sparkfun.com/assets/5/3/8/5/7/Example_RTCM_Binary_Output.txt,
 * a few UBX messages and an entirely invented protocol that
 * only the callback will understand.
 */
static const uGnssDecTestDataCallback_t gTestDataCallback[] = {
    // UBX
    {
        {
            "\xb5\x62\x13\x80\x80\x00\x03\x00\x00\xff\x00\x00\x00\x00\x00\x00"
            "\x00\x00\x5c\x40\x10\x05\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
            "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
            "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
            "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
            "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
            "\x00\x00\x12\x80\x07\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
            "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
            "\x00\x00\x00\x00\x00\x00\x5f\x0a", 136
        },
        {
            U_GNSS_PROTOCOL_UBX, 0x1380, NULL
        },
        0
    },
    {
        {
            "\xb5\x62\x06\x8b\x18\x00\x00\x00\x00\x00\x02\x00\xd0\x40\x03\x00"
            "\xd0\x40\x05\x00\xd0\x30\x07\x00\xd0\x20\x06\x00\xd0\x20\xc0\x35", 32
        },
        {
            U_GNSS_PROTOCOL_UBX, 0x068b, NULL
        },
        1
    },
    // NMEA
    {
        {
            "$GPGGA,092750.000,5321.6802,N,00630.3372,W,1,8,1.03,61.7,M,55.2,M,,*76", 69
        },
        {
            U_GNSS_PROTOCOL_NMEA, 0, "GPGGA"
        },
        2
    },
    {
        {
            "$GPGSA,A,3,10,07,05,02,29,04,08,13,,,,,1.72,1.03,1.38*0A", 55
        },
        {
            U_GNSS_PROTOCOL_NMEA, 0, "GPGSA"
        },
        3
    },
    // RTCM
    {
        {
            "\xD3\x00\x99\x43\x50\x00\x28\x63\xF7\x46\x00\x00\x00\x25\x0C\x94"
            "\x80\x00\x00\x00\x20\x00\x00\x00\x7F\xD2\x14\x51\xD0\xD1\x53\x52"
            "\xD4\x54\x00\x00\x00\x00\x00\x41\x9D\x22\x89\x8A\x3C\x9F\xCF\xDD"
            "\x4C\xA3\x05\x43\xD7\x8F\x94\x00\x6D\xFF\xE8\x19\xF0\x49\xFD\x20"
            "\x0A\xB4\x7B\xF5\x80\x9D\x4B\xD2\x93\x4C\x9E\x6F\xF1\xBD\xC5\xE8"
            "\x04\xF0\xC4\xCA\xE9\x90\x62\xDA\x81\xF4\xF3\xBF\x94\xEF\xFF\x67"
            "\x8D\x43\x45\xDC\x7F\xE5\x52\x81\x66\xD3\x03\x99\xFD\x03\x22\xE9"
            "\x81\x53\x44\x24\xA9\xC2\x70\x97\x27\x18\x3E\x26\xA2\x1E\x40\x06"
            "\x21\x48\x66\x17\x85\x81\x50\x5C\x12\x04\xE0\x8D\x9E\xDB\x7F\xE9"
            "\xD1\x4F\x57\xD9\x4F\x4F\x24\x27\xEA\xC0\x63\x00\x52\xBC\xB7", 159
        },
        {
            U_GNSS_PROTOCOL_RTCM, 1077, NULL
        },
        4
    },
    {
        {
            "\xD3\x00\x13\x3E\xD0\x00\x03\x3C\xFF\x55\x48\x17\xB5\x02\xDE\xCA"
            "\xBC\x09\x80\x35\x10\x31\x09\xFA\x3C", 25
        },
        {
            U_GNSS_PROTOCOL_RTCM, 1005, NULL
        },
        5
    },
    // Fantasy
    {
        {
            "\x00bibble", 7
        },
        {
            U_GNSS_PROTOCOL_MAX_NUM, 0x4242, NULL
        },
        6
    }
};

// The CRC length for each protocol type
static const size_t gCrcLength[] = {
    2, // U_GNSS_PROTOCOL_UBX
    3, // U_GNSS_PROTOCOL_NMEA
    3, // U_GNSS_PROTOCOL_RTCM
    0, // U_GNSS_PROTOCOL_UNKNOWN (not used)
    0 // U_GNSS_PROTOCOL_MAX_NUM (fantasy protocol)
};

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

// Message decode user callback.
static int32_t callback(uGnssMessageId_t *pId,
                        const char *pBuffer,
                        size_t size,
                        uGnssDecUnion_t **ppBody,
                        void *pCallbackParam)
{
    int32_t returnCode = -1;
    uGnssDecTestDataCallback_t **ppTestData = (uGnssDecTestDataCallback_t **) pCallbackParam;
    uGnssDecTestDataCallback_t *pTestData = NULL;

    gCallback = 0;
    if (ppTestData == NULL) {
        gCallback = 1;
    } else {
        pTestData = *ppTestData;
        if (pTestData != NULL) {
            if (pBuffer != pTestData->raw.p) {
                gCallback = 2;
            }
            // Check size with the CRC removed, 'cos that's what we were sent
            if (size != pTestData->raw.length - gCrcLength[pTestData->id.type]) {
                gCallback = 3;
            }
            if (pId != NULL) {
                switch (pTestData->id.type) {
                    case U_GNSS_PROTOCOL_NMEA:
                        if (pId->type != pTestData->id.type) {
                            gCallback = 4;
                        }
                        if (strcmp(pId->id.pNmea, pTestData->id.pIdNmea) != 0) {
                            gCallback = 5;
                        }
                        break;
                    case U_GNSS_PROTOCOL_RTCM:
                        if (pId->type != pTestData->id.type) {
                            gCallback = 6;
                        }
                        if (pId->id.rtcm != pTestData->id.idUbxOrRtcm) {
                            gCallback = 7;
                        }
                        break;
                    case U_GNSS_PROTOCOL_UBX:
                        if (pId->type != pTestData->id.type) {
                            gCallback = 8;
                        }
                        if (pId->id.ubx != pTestData->id.idUbxOrRtcm) {
                            gCallback = 9;
                        }
                        break;
                    default:
                        // The fantasy protocol, so we set the protocol and ID
                        if (pId->type != U_GNSS_PROTOCOL_UNKNOWN) {
                            gCallback = 10;
                        }
                        pId->type = pTestData->id.type;
                        pId->id.ubx = pTestData->id.idUbxOrRtcm;
                        break;
                }
            } else {
                gCallback = 11;
            }
            if (ppBody != NULL) {
                // Allocate a pBody and set it to the callbackDecodeIndicator
                *ppBody = (uGnssDecUnion_t *) pUPortMalloc(sizeof(int32_t));
                if (*ppBody != NULL) {
                    memcpy (*ppBody, &(pTestData->callbackDecodeIndicator), sizeof(int32_t));
                } else {
                    gCallback = 12;
                }
            } else {
                gCallback = 13;
            }
            // Use callbackDecodeIndicator incremented by 1 as our return value
            returnCode = pTestData->callbackDecodeIndicator + 1;
        } else {
            gCallback = 14;
        }
    }

    return returnCode;
}

// Print out a nice hexdump.
static void hexDump(const char *pPrefix, const char *pBuffer, size_t size)
{
    size_t count = 0;

    // Heading
    if (pPrefix != NULL) {
        uPortLog(pPrefix);
    }
    // Leave enough spaces for a row count plus two spaces after it
    uPortLog("       ");
    for (size_t x = 0; (x < U_GNSS_DEC_TEST_HEX_DUMP_WIDTH) && (size > 0); x++) {
        if (x == U_GNSS_DEC_TEST_HEX_DUMP_WIDTH / 2) {
            uPortLog("  ");
        }
        uPortLog("%02d ", x);
    }
    uPortLog("\n");
    // Hex
    while (size > 0) {
        if (pPrefix != NULL) {
            uPortLog(pPrefix);
        }
        uPortLog("%04d   ", count);
        for (size_t x = 0; (x < U_GNSS_DEC_TEST_HEX_DUMP_WIDTH) && (size > 0); x++) {
            if (x == U_GNSS_DEC_TEST_HEX_DUMP_WIDTH / 2) {
                uPortLog("  ");
            }
            uPortLog("%02x ", (uint8_t) *pBuffer);
            pBuffer++;
            size--;
        }
        count += U_GNSS_DEC_TEST_HEX_DUMP_WIDTH;
        uPortLog("\n");
    }
}

// Tests of helper functions, called by the test "gnssDecKnown".
static void testHelperFunctions(const uGnssMessageId_t *pId,
                                const uGnssDecUnion_t *pBody,
                                const uGnssDecTestDataBinary_t *pRaw)
{
    // *INDENT-OFF* (otherwise AStyle makes a mess of this)
    switch (pId->type) {
        case U_GNSS_PROTOCOL_NMEA:
            break;
        case U_GNSS_PROTOCOL_RTCM:
            break;
        case U_GNSS_PROTOCOL_UBX:
            switch (pId->id.ubx) {
                case U_GNSS_UBX_MESSAGE(U_GNSS_DEC_UBX_NAV_PVT_MESSAGE_CLASS,
                                        U_GNSS_DEC_UBX_NAV_PVT_MESSAGE_ID): {
                        // Check the time calculation using the first item
                        // in the gUbxNavPvt array
                        if (pRaw == &(gUbxNavPvt[0].raw)) {
                            U_PORT_TEST_ASSERT(uGnssDecUbxNavPvtGetTimeUtc(&(pBody->ubxNavPvt)) == ((1691757212LL * 1000000000) - 73790));
                        }
                    }
                    break;
                case U_GNSS_UBX_MESSAGE(U_GNSS_DEC_UBX_NAV_HPPOSLLH_MESSAGE_CLASS,
                                        U_GNSS_DEC_UBX_NAV_HPPOSLLH_MESSAGE_ID): {
                        // Check the high precision position calculation using
                        // the first item in the gUbxNavHpposllh array
                        uGnssDecUbxNavHpposllhPos_t pos;
                        memset(&pos, 0xFF, sizeof(pos));
                        if (pRaw == &(gUbxNavHpposllh[0].raw)) {
                            uGnssDecUbxNavHpposllhGetPos(&(pBody->ubxNavHpposllh), &pos);
                            U_PORT_TEST_ASSERT(pos.longitudeX1e9 == -74812742);
                            U_PORT_TEST_ASSERT(pos.latitudeX1e9 == 52222726325);
                            U_PORT_TEST_ASSERT(pos.heightMillimetresX1e1 == 1314043);
                            U_PORT_TEST_ASSERT(pos.heightMeanSeaLevelMillimetresX1e1 == 856502);
                        }
                    }
                    break;
                default:
                    break;
            }
            break;
        default:
            break;
    }
    // *INDENT-ON*
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

/** Test the user-callback stuff.
 */
U_PORT_TEST_FUNCTION("[gnssDec]", "gnssDecCallback")
{
    int32_t heapUsed;
    uGnssDec_t *pDec;
    const uGnssDecTestDataCallback_t *pTestData = NULL;

    // Obtain the initial heap size
    heapUsed = uPortGetHeapFree();

    // Do this three times: first run without a callback
    // set, second run with a callback, and then one more
    // time with the callback removed again
    for (size_t x = 0; x < 3; x++) {
        if (x == 1) {
            U_TEST_PRINT_LINE_X("test GNSS decode with callback set", x);
            uGnssDecSetCallback(callback, (void *) &pTestData);
        } else if (x == 2) {
            U_TEST_PRINT_LINE_X("test GNSS decode with callback removed", x);
            uGnssDecSetCallback(NULL, NULL);
        } else {
            U_TEST_PRINT_LINE_X("test GNSS decode with callback not set", x);
        }
        for (size_t y = 0; y < sizeof(gTestDataCallback) / sizeof(gTestDataCallback[0]); y++) {
            gCallback = INT_MIN;
            pTestData = &(gTestDataCallback[y]);
            switch (pTestData->id.type) {
                case U_GNSS_PROTOCOL_NMEA:
                    U_TEST_PRINT_LINE_X_Y("test GNSS decode with protocol %d, ID %s.",
                                          x, y, pTestData->id.type, pTestData->id.pIdNmea);
                    break;
                case U_GNSS_PROTOCOL_RTCM:
                    U_TEST_PRINT_LINE_X_Y("test GNSS decode with protocol %d, ID %d.",
                                          x, y, pTestData->id.type, pTestData->id.idUbxOrRtcm);
                    break;
                case U_GNSS_PROTOCOL_UBX:
                    U_TEST_PRINT_LINE_X_Y("test GNSS decode with protocol %d, ID 0x%04x.",
                                          x, y, pTestData->id.type, pTestData->id.idUbxOrRtcm);
                    break;
                default:
                    U_TEST_PRINT_LINE_X_Y("test GNSS decode with fantasy protocol (%d), ID %d.",
                                          x, y, pTestData->id.type, pTestData->id.idUbxOrRtcm);
                    break;
            }
            // We test with the checksum stuff removed from the length as the
            // decoders shouldn't care about that
            pDec = pUGnssDecAlloc(pTestData->raw.p, pTestData->raw.length - gCrcLength[pTestData->id.type]);
            U_PORT_TEST_ASSERT(pDec != NULL);
            switch (pTestData->id.type) {
                case U_GNSS_PROTOCOL_NMEA:
                    U_PORT_TEST_ASSERT(pDec->id.type == pTestData->id.type);
                    U_PORT_TEST_ASSERT(strcmp(pDec->id.id.pNmea, pTestData->id.pIdNmea) == 0);
                    break;
                case U_GNSS_PROTOCOL_RTCM:
                    U_PORT_TEST_ASSERT(pDec->id.type == pTestData->id.type);
                    U_PORT_TEST_ASSERT(pDec->id.id.rtcm == pTestData->id.idUbxOrRtcm);
                    break;
                case U_GNSS_PROTOCOL_UBX:
                    U_PORT_TEST_ASSERT(pDec->id.type == pTestData->id.type);
                    U_PORT_TEST_ASSERT(pDec->id.id.ubx == pTestData->id.idUbxOrRtcm);
                    break;
                default:
                    // This will be our fantasy protocol, which is only
                    // decoded if the callback is in circuit
                    if (x == 1) {
                        U_PORT_TEST_ASSERT(pDec->id.type == pTestData->id.type);
                        U_PORT_TEST_ASSERT(pDec->id.id.ubx == pTestData->id.idUbxOrRtcm);
                    } else {
                        U_PORT_TEST_ASSERT(pDec->id.type == U_GNSS_PROTOCOL_UNKNOWN);
                    }
                    break;
            }

            if (x == 1) {
                // Callback should have been called
                U_PORT_TEST_ASSERT(gCallback == 0);
                // If the callback is in place then it will decode and
                // pass back the callbackDecodeIndicator of this entry
                // in pBody and also send it back, with one added, as the
                // errorCode
                U_PORT_TEST_ASSERT(pDec->pBody != NULL);
                U_TEST_PRINT_LINE_X_Y("callback returned error code %d, decode indicator %d.",
                                      x, y, pDec->errorCode, *(int32_t *) pDec->pBody);
                U_PORT_TEST_ASSERT(pDec->errorCode == pTestData->callbackDecodeIndicator + 1);
                U_PORT_TEST_ASSERT(*(int32_t *) pDec->pBody == pTestData->callbackDecodeIndicator);
            } else {
                // Callback should not have been called
                U_PORT_TEST_ASSERT(gCallback == INT_MIN);
                if (pTestData->id.type == U_GNSS_PROTOCOL_MAX_NUM) {
                    // Fantasy protocol is only known by the callback
                    U_PORT_TEST_ASSERT(pDec->errorCode == (int32_t) U_ERROR_COMMON_UNKNOWN);
                } else {
                    // All the other protocol types are known but not supported unless the callback is in town
                    U_PORT_TEST_ASSERT(pDec->errorCode == (int32_t) U_ERROR_COMMON_NOT_SUPPORTED);
                }
                U_PORT_TEST_ASSERT(pDec->pBody == NULL);
            }

            // Free the structure once more
            uGnssDecFree(pDec);
        }
    }

    // Check for memory leaks
    heapUsed -= uPortGetHeapFree();
    U_TEST_PRINT_LINE("we have leaked %d byte(s).", heapUsed);
    // heapUsed < 0 for the Zephyr case where the heap can look
    // like it increases (negative leak)
    U_PORT_TEST_ASSERT(heapUsed <= 0);
}

/** Test of decoding the known functions.
 */
U_PORT_TEST_FUNCTION("[gnssDec]", "gnssDecKnown")
{
    int32_t heapUsed;
    uGnssDec_t *pDec;
    const uGnssDecTestDataKnown_t *pTestData = NULL;
    size_t decodedStructureSize;
    char prefix[64]; // Just for printing

    // Obtain the initial heap size
    heapUsed = uPortGetHeapFree();

    // For each message type
    for (size_t x = 0; x < sizeof(gTestDataKnownSet) / sizeof(gTestDataKnownSet[0]); x++) {
        decodedStructureSize = gTestDataKnownSet[x].decodedStructureSize;
        // For each item of test data for that message type
        for (size_t y = 0; y < gTestDataKnownSet[x].size; y++) {
            pTestData = gTestDataKnownSet[x].pTestData + y;
            switch (pTestData->id.type) {
                case U_GNSS_PROTOCOL_NMEA:
                    U_TEST_PRINT_LINE_X_Y("test GNSS decode with protocol %d, ID %s.",
                                          x, y, pTestData->id.type, pTestData->id.pIdNmea);
                    break;
                case U_GNSS_PROTOCOL_RTCM:
                    U_TEST_PRINT_LINE_X_Y("test GNSS decode with protocol %d, ID %d.",
                                          x, y, pTestData->id.type, pTestData->id.idUbxOrRtcm);
                    break;
                case U_GNSS_PROTOCOL_UBX:
                    U_TEST_PRINT_LINE_X_Y("test GNSS decode with protocol %d, ID 0x%04x.",
                                          x, y, pTestData->id.type, pTestData->id.idUbxOrRtcm);
                    break;
                default:
                    break;
            }
            // We test with the checksum stuff removed from the length as the
            // decoders shouldn't care about that
            pDec = pUGnssDecAlloc(pTestData->raw.p, pTestData->raw.length - gCrcLength[pTestData->id.type]);
            U_PORT_TEST_ASSERT(pDec != NULL);
            switch (pTestData->id.type) {
                case U_GNSS_PROTOCOL_NMEA:
                    U_PORT_TEST_ASSERT(pDec->id.type == pTestData->id.type);
                    U_PORT_TEST_ASSERT(strcmp(pDec->id.id.pNmea, pTestData->id.pIdNmea) == 0);
                    break;
                case U_GNSS_PROTOCOL_RTCM:
                    U_PORT_TEST_ASSERT(pDec->id.type == pTestData->id.type);
                    U_PORT_TEST_ASSERT(pDec->id.id.rtcm == pTestData->id.idUbxOrRtcm);
                    break;
                case U_GNSS_PROTOCOL_UBX:
                    U_PORT_TEST_ASSERT(pDec->id.type == pTestData->id.type);
                    U_PORT_TEST_ASSERT(pDec->id.id.ubx == pTestData->id.idUbxOrRtcm);
                    break;
                default:
                    break;
            }
            U_TEST_PRINT_LINE_X_Y("pUGnssDecAlloc() returned error code %d.",  x, y, pDec->errorCode);
            U_PORT_TEST_ASSERT(pDec->errorCode == 0);
            U_PORT_TEST_ASSERT(pDec->pBody != NULL);
            if (memcmp(pDec->pBody, pTestData->pDecoded, decodedStructureSize) != 0) {
                snprintf(prefix, sizeof(prefix), U_TEST_PREFIX_X_Y, (int) x, (int) y);
                U_TEST_PRINT_LINE_X_Y("decoded:", x, y);
                hexDump(prefix, (const char *) pDec->pBody, decodedStructureSize);
                U_TEST_PRINT_LINE_X_Y("expected:", x, y);
                hexDump(prefix, (const char *) pTestData->pDecoded, decodedStructureSize);
                U_PORT_TEST_ASSERT(false);
            } else {
                // Callouts to spot-tests for any helper functions
                testHelperFunctions(&(pDec->id), pDec->pBody, &(pTestData->raw));
            }
            // Free the structure once more
            uGnssDecFree(pDec);
        }
    }

    // Check for memory leaks
    heapUsed -= uPortGetHeapFree();
    U_TEST_PRINT_LINE("we have leaked %d byte(s).", heapUsed);
    // heapUsed < 0 for the Zephyr case where the heap can look
    // like it increases (negative leak)
    U_PORT_TEST_ASSERT(heapUsed <= 0);
}

// End of file
