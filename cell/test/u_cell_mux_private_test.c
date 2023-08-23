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
 * @brief Tests for the internal cellulare mux API.  No cellular module
 * is required to run this set of tests, all testing is back to back.
 * IMPORTANT: see notes in u_cfg_test_platform_specific.h for the
 * naming rules that must be followed when using the U_PORT_TEST_FUNCTION()
 * macro.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"
#include "string.h"    // memcmp()/memset()

#include "u_cfg_sw.h"
#include "u_cfg_os_platform_specific.h"
#include "u_cfg_app_platform_specific.h"
#include "u_cfg_test_platform_specific.h"

#include "u_error_common.h"

#include "u_at_client.h"

#include "u_ringbuffer.h"

#include "u_device_serial.h"

#include "u_port.h"
#include "u_port_os.h"
#include "u_port_heap.h"
#include "u_port_debug.h"

#include "u_cell_module_type.h"
#include "u_cell.h"
#include "u_cell_file.h"
#include "u_cell_net.h"     // Required by u_cell_private.h
#include "u_cell_private.h"

#include "u_cell_mux.h"
#include "u_cell_mux_private.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/** The base string to put at the start of all prints from this test.
 */
#define U_TEST_PREFIX_BASE "U_CELL_MUX_PRIVATE_TEST"

/** The string to put at the start of all prints from this test
 * that do not require an iteration on the end.
 */
#define U_TEST_PREFIX U_TEST_PREFIX_BASE ": "

/** Print a whole line, with terminator, prefixed for this test
 * file, no iteration version.
 */
#define U_TEST_PRINT_LINE(format, ...) uPortLog(U_TEST_PREFIX format "\n", ##__VA_ARGS__)

#ifndef U_CELL_MUX_PRIVATE_TEST_MAX_FRAME_SIZE_BYTES
/** The maximum CMUX frame size to encode, sized to match
 * the largest GNSS-tunnelled UBX-format message we might
 * ever get.
 */
# define U_CELL_MUX_PRIVATE_TEST_MAX_FRAME_SIZE_BYTES (1024 * 2)
#endif

#ifndef U_CELL_MUX_PRIVATE_TEST_MAX_INFORMATION_SIZE_BYTES
/** The maximum length of information field to encode.
 */
# define U_CELL_MUX_PRIVATE_TEST_MAX_INFORMATION_SIZE_BYTES (U_CELL_MUX_PRIVATE_TEST_MAX_FRAME_SIZE_BYTES - U_CELL_MUX_PRIVATE_FRAME_OVERHEAD_MAX_BYTES)
#endif

#ifndef U_CELL_MUX_PRIVATE_TEST_FILL_CHAR
/** Character to use as fill in the mux buffer so that we
 * can check it has been written for the correct length by
 * the decoder.
 */
# define U_CELL_MUX_PRIVATE_TEST_FILL_CHAR 0xFF
#endif

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

/** The command types to test; must have the same number of elements
 * as gCommandResponse.
 */
static uCellMuxPrivateFrameType_t gType[] = {U_CELL_MUX_PRIVATE_FRAME_TYPE_SABM_COMMAND,
                                             U_CELL_MUX_PRIVATE_FRAME_TYPE_UA_RESPONSE,
                                             U_CELL_MUX_PRIVATE_FRAME_TYPE_DM_RESPONSE,
                                             U_CELL_MUX_PRIVATE_FRAME_TYPE_DISC_COMMAND,
                                             U_CELL_MUX_PRIVATE_FRAME_TYPE_UIH,
                                             U_CELL_MUX_PRIVATE_FRAME_TYPE_UI
                                            };

/** What the commmand/response value should be for each of gType.
 */
static bool gCommandResponse[] = {true,  // U_CELL_MUX_PRIVATE_FRAME_TYPE_SABM_COMMAND
                                  false, // U_CELL_MUX_PRIVATE_FRAME_TYPE_UA_RESPONSE,
                                  false, // U_CELL_MUX_PRIVATE_FRAME_TYPE_DM_RESPONSE,
                                  true,  // U_CELL_MUX_PRIVATE_FRAME_TYPE_DISC_COMMAND,
                                  true,  // U_CELL_MUX_PRIVATE_FRAME_TYPE_UIH,
                                  true   // U_CELL_MUX_PRIVATE_FRAME_TYPE_UI
                                 };

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

// Return "true" or "false" for a Boolean type.
const char *pBool(bool isTrue)
{
    return isTrue ? "true" : "false";
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

/** Test the mux encode/decode functions back-to-back.
 *
 * IMPORTANT: see notes in u_cfg_test_platform_specific.h for the
 * naming rules that must be followed when using the
 * U_PORT_TEST_FUNCTION() macro.
 */
U_PORT_TEST_FUNCTION("[cellMuxPrivate]", "cellMuxPrivateBackToBack")
{
    int32_t heapUsed;
    uint8_t address;
    bool pollFinal = false;
    char *pInformation;
    int32_t expectedFrameLength;
    uCellMuxPrivateParserContext_t parserContext = {0};
    int32_t z;

    heapUsed = uPortGetHeapFree();
    U_PORT_TEST_ASSERT(uPortInit() == 0);

    // Grab some memory for the encoded CMUX frame and the information field we want to encode
    parserContext.bufferSize = U_CELL_MUX_PRIVATE_TEST_MAX_FRAME_SIZE_BYTES;
    parserContext.pBuffer = (char *) pUPortMalloc(parserContext.bufferSize);
    U_PORT_TEST_ASSERT(parserContext.pBuffer != NULL);
    pInformation = (char *) pUPortMalloc(U_CELL_MUX_PRIVATE_TEST_MAX_FRAME_SIZE_BYTES);
    U_PORT_TEST_ASSERT(pInformation != NULL);

    // Encode a variety of lengths, addresses and types
    for (size_t informationLength = 0;
         informationLength < U_CELL_MUX_PRIVATE_TEST_MAX_INFORMATION_SIZE_BYTES;
         informationLength += 10) {
        // Fill the information field with a known pattern
        for (size_t y = 0; y < informationLength; y++) {
            *(pInformation + y) = (char) y;
        }
        for (size_t x = 0; x < sizeof(gType) / sizeof(gType[0]); x++) {
            // Fill the frame with a known character
            memset(parserContext.pBuffer, U_CELL_MUX_PRIVATE_TEST_FILL_CHAR,
                   U_CELL_MUX_PRIVATE_TEST_MAX_FRAME_SIZE_BYTES);
            // The expected length is the information length plus the
            // maximum overhead size, one less if the information
            // field fits into a single byte
            expectedFrameLength = informationLength + U_CELL_MUX_PRIVATE_FRAME_OVERHEAD_MAX_BYTES;
            if (informationLength <= 0x7F) {
                expectedFrameLength--;
            }
            // Set up for encoding
            address = 0;
            pollFinal = !pollFinal;
            // Encode into the buffer
            parserContext.bufferSize = uCellMuxPrivateEncode(address,
                                                             gType[x], pollFinal,
                                                             pInformation, informationLength,
                                                             parserContext.pBuffer);
            if ((int32_t)parserContext.bufferSize != expectedFrameLength) {
                U_TEST_PRINT_LINE("encoded frame length %d when %d was expected.",
                                  parserContext.bufferSize, expectedFrameLength);
                U_PORT_TEST_ASSERT(false);
            }

            // Set up for decoding
            parserContext.address = U_CELL_MUX_PRIVATE_ADDRESS_ANY;
            parserContext.commandResponse = !gCommandResponse[x];
            parserContext.type = U_CELL_MUX_PRIVATE_FRAME_TYPE_NONE;
            parserContext.pollFinal = !pollFinal;
            // Decode the information field back into the same buffer
            parserContext.pInformation = parserContext.pBuffer;
            parserContext.informationLengthBytes = U_CELL_MUX_PRIVATE_TEST_MAX_FRAME_SIZE_BYTES;
            parserContext.bufferIndex = 0;

            // Decode from the buffer, information field back into the buffer
            z = (int32_t) U_ERROR_COMMON_NOT_FOUND;
            while ((parserContext.bufferIndex < parserContext.bufferSize) &&
                   (z < 0) && (z != (int32_t) U_ERROR_COMMON_TIMEOUT)) {
                z = uCellMuxPrivateParseCmux(NULL, &parserContext);
            }

            if (parserContext.informationLengthBytes != informationLength) {
                U_TEST_PRINT_LINE("decoded information field length %d when %d was expected.",
                                  parserContext.informationLengthBytes, informationLength);
                U_PORT_TEST_ASSERT(false);
            }
            if (memcmp(parserContext.pBuffer, pInformation, informationLength) != 0) {
                U_TEST_PRINT_LINE("decoded information field not as expected.");
                U_PORT_TEST_ASSERT(false);
            }
            if (parserContext.address != address) {
                U_TEST_PRINT_LINE("decoded address 0x%02x when 0x%02x was expected.",
                                  parserContext.address, address);
                U_PORT_TEST_ASSERT(false);
            }
            if (parserContext.type != gType[x]) {
                U_TEST_PRINT_LINE("decoded type 0x%02x when 0x%02x was expected.",
                                  parserContext.type, gType[x]);
                U_PORT_TEST_ASSERT(false);
            }
            if (parserContext.commandResponse != gCommandResponse[x]) {
                U_TEST_PRINT_LINE("decoded command/response %s when %s was expected.",
                                  pBool(parserContext.commandResponse), pBool(gCommandResponse[x]));
                U_PORT_TEST_ASSERT(false);
            }
            if (parserContext.pollFinal != pollFinal) {
                U_TEST_PRINT_LINE("decoded poll/final %s when %s was expected.",
                                  pBool(parserContext.pollFinal), pBool(pollFinal));
                U_PORT_TEST_ASSERT(false);
            }
            if (parserContext.bufferIndex != parserContext.bufferSize) {
                U_TEST_PRINT_LINE("bufferIndex %d when %d was expected.",
                                  parserContext.bufferIndex, parserContext.bufferSize);
                U_PORT_TEST_ASSERT(false);
            }

            // Invert poll/final for next time
            pollFinal = !pollFinal;
        }

        // Switch addresses
        if (address == 0) {
            address = U_CELL_MUX_PRIVATE_ADDRESS_MAX;
        } else {
            address = 0;
        }
        // Some platforms run a task watchdog which might be starved with such
        // a large processing loop: give it a bone
        uPortTaskBlock(U_CFG_OS_YIELD_MS);
    }

    // Free memory
    uPortFree(parserContext.pBuffer);
    uPortFree(pInformation);

    uPortDeinit();

# ifndef __XTENSA__
    // Check for memory leaks
    // TODO: this if'ed out for ESP32 (xtensa compiler) at
    // the moment as there is an issue with ESP32 hanging
    // on to memory in the UART drivers that can't easily be
    // accounted for.
    heapUsed -= uPortGetHeapFree();
    U_TEST_PRINT_LINE("we have leaked %d byte(s).", heapUsed);
    // heapUsed < 0 for the Zephyr case where the heap can look
    // like it increases (negative leak)
    U_PORT_TEST_ASSERT(heapUsed <= 0);
# else
    (void) heapUsed;
# endif
}

// End of file
