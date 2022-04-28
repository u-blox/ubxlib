/*
 * This file is part of the CmBacktrace Library.
 *
 * Copyright (c) 2016-2019, Armink, <armink.ztl@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * 'Software'), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED 'AS IS', WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * Function: Initialize function and other general function.
 * Created on: 2016-12-15
 */

// The content of this file originates from:
// https://github.com/armink/CmBacktrace/blob/master/cm_backtrace/cm_backtrace.c
// A few functions has been kept and adjusted to the needs of ubxlib

#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"
#include "string.h"

#include "u_port_debug.h"

#include "u_debug_utils_internal.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

#ifndef __thumb__
# error Only ARM thumb mode supported
#endif

#ifdef __ZEPHYR__
# include "u_sections_zephyr.h"
#elif defined(__GNUC__)
# include "u_sections_gcc.h"
#else
# error Only GCC supported out-of-box. Use CODE_START and CODE_END to tell where code section starts and ends.
#endif

// It's quite common that the code section starts at address 0
// but in order to detect null pointer dereferences an MPU
// region is defined starting from address 0. To work around
// this we ignore addresses between 0-1023
#define HANDLE_ADDR_0(addr) (addr == 0 ? 1024 : addr)

#define IS_CODE_SPACE(addr)  ((addr >= HANDLE_ADDR_0(CODE_START)) && (addr <= CODE_END))

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

/* check the disassembly instruction is 'BL' or 'BLX' */
static bool disassembly_ins_is_bl_blx(uint32_t addr)
{
    uint16_t ins1 = *((uint16_t *)addr);
    uint16_t ins2 = *((uint16_t *)(addr + 2));

#define BL_INS_MASK         0xF800
#define BL_INS_HIGH         0xF800
#define BL_INS_LOW          0xF000
#define BLX_INX_MASK        0xFF00
#define BLX_INX             0x4700

    if ((ins2 & BL_INS_MASK) == BL_INS_HIGH && (ins1 & BL_INS_MASK) == BL_INS_LOW) {
        return true;
    } else if ((ins2 & BLX_INX_MASK) == BLX_INX) {
        return true;
    } else {
        return false;
    }
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

bool uDebugUtilsInitStackFrame(uint32_t sp, uint32_t stackTop, uStackFrame_t *pFrame)
{
    (void)stackTop;
    memset(pFrame, 0, sizeof(uStackFrame_t));
    pFrame->sp = sp;
    return true;
}

bool uDebugUtilsGetNextStackFrame(uint32_t stackTop, uStackFrame_t *pFrame)
{
    uint32_t *pSp = (uint32_t *)pFrame->sp;
    for (; ((uint32_t)pSp < stackTop); pSp++) {
        /* the Cortex-M using thumb instruction, so the pc must be an odd number */
        if ((*pSp % 2) == 0) {
            continue;
        }
        /* fix the PC address in thumb mode */
        uint32_t pc = *pSp - 1 - sizeof(void *);
        /* check the the instruction before PC address is 'BL' or 'BLX' */
        if (IS_CODE_SPACE(pc) && disassembly_ins_is_bl_blx(pc)) {
            pFrame->pc = pc;
            pFrame->sp = (uint32_t)++pSp;
            return true;
        }
    }
    return false;
}
