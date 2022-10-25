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

#ifndef _U_PORT_HEAP_H_
#define _U_PORT_HEAP_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

/** \addtogroup __port __Port
 *  @{
 */

/** @file
 * @brief Heap memory allocation API.  These functions are thread-safe.
 * A default implementation of these functions is provided in
 * u_port_heap.c; you should override them as you wish in your
 * port code, or you may just leave them as they are (in which case
 * malloc() and free() for your platform will be called by the
 * default implementation).
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

/** Allocate memory: does whatever malloc() does on your platform,
 * which should be to return a pointer to a block of heap memory
 * of at least the requested size, aligned for the worst-case
 * structure type alignment, or NULL if unsufficient contiguous
 * memory is available.
 *
 * @param sizeBytes the amount of memory required in bytes.
 * @return          a pointer to at least sizeBytes of memory,
 *                  aligned for the worst-case structure-type
 *                  alignment, else NULL.
 */
void *pUPortMalloc(size_t sizeBytes);

/** Free memory that was allocated by pUPortMalloc(); does whatever
 * free() does on your platform.
 *
 * @param[in] pMemory a pointer to a block of heap memory that was
 *                    returned by pUPortMalloc(); may be NULL.
 */
void uPortFree(void *pMemory);

#ifdef __cplusplus
}
#endif

/** @}*/

#endif // _U_PORT_HEAP_H_

// End of file
