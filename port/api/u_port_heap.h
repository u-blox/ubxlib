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
 *
 * In addition to heap memory allocation, it is also possible to
 * switch on heap tracking by defining U_CFG_HEAP_MONITOR.  This
 * will add guards either end of a memory block and check them
 * when it is free'd (U_ASSERT() will be called with false if
 * a guard is corrupted), and will also log each allocation so that
 * they can be printed with uPortHeapDump().  Note that monitoring
 * will require at least 28 additional bytes of heap storage per
 * heap allocation.
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

#ifndef U_CFG_HEAP_MONITOR
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
#else
/** For heap monitoring, pUPortMalloc() becomes a macro so that we
 * get to trap the file/line and add our structure in
 * pUPortMallocMonitor() before, internally, calling pUPortMalloc().
 */
# define pUPortMalloc(sizeBytes) pUPortMallocMonitor(sizeBytes, __FILE__, __LINE__)

/** Allocate memory, adding monitoring information along the
 * way: this should NOT be called directly, it is called through
 * the pUPortMalloc() macro when U_CFG_HEAP_MONITOR is defined.
 *
 * @param sizeBytes the amount of memory required in bytes.
 * @param[in] pFile the name of the file that the calling
 *                  function is in.
 * @param line      the line in pFile that is calling this
 *                  function.
 * @return          a pointer to at least sizeBytes of memory,
 *                  aligned for the worst-case structure-type
 *                  alignment, else NULL.
 */
void *pUPortMallocMonitor(size_t sizeBytes, const char *pFile,
                          int32_t line);
#endif

/** Free memory that was allocated by pUPortMalloc(); does whatever
 * free() does on your platform.
 *
 * If U_CFG_HEAP_MONITOR is defined then the guards applied either
 * end of the allocation at creation by the pUPortMalloc() macro will
 * be checked and U_ASSERT() will be called with false if a guard is
 * corrupted.
 *
 * @param[in] pMemory a pointer to a block of heap memory that was
 *                    returned by pUPortMalloc(); may be NULL.
 */
void uPortFree(void *pMemory);

/** Print out the contents of the heap; only useful if
 * U_CFG_HEAP_MONITOR is defined.
 *
 * @param[in] pPrefix  print this before each line; may be NULL.
 * @return             the number of entries printed.
 */
int32_t uPortHeapDump(const char *pPrefix);

/** Initialise heap monitoring: you do NOT need to call this, it
 * is called internally by the porting layer if U_CFG_HEAP_MONITOR
 * is defined.
 *
 * @param[in] pMutexCreate normally this will be NULL; it is only
 *                         provided for platforms where the
 *                         implementation of uPortMutexCreate()
 *                         itself calls pUPortMalloc(), which won't
 *                         work here as uPortHeapMonitorInit() needs
 *                         to create a mutex before heap allocations
 *                         can be done.  Where this is the case, a
 *                         special version of uPortMutexCreate()
 *                         can be passed in by the platform to be
 *                         called by uPortHeapMonitorInit() instead
 *                         of the usual one.
 * @param[in] pMutexLock   similar to pMutexCreate, a pointer to a
 *                         special mutex lock function, else
 *                         (the normal case) use NULL.
 * @param[in] pMutexUnlock similar to pMutexLock, a pointer to a
 *                         special mutex unlock function, else
 *                         (the normal case) use NULL.
 * @return                 zero on success else negative error code.
 */
int32_t uPortHeapMonitorInit(int32_t (*pMutexCreate) (uPortMutexHandle_t *),
                             int32_t (*pMutexLock) (const uPortMutexHandle_t),
                             int32_t (*pMutexUnlock) (const uPortMutexHandle_t));

#ifdef __cplusplus
}
#endif

/** @}*/

#endif // _U_PORT_HEAP_H_

// End of file
