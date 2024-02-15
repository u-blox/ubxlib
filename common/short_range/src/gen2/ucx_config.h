/** @file
 * @brief Configuration file for the uCX AT client
 */
#ifndef U_CX_CONFIG_H
#define U_CX_CONFIG_H

#include "u_cfg_sw.h"
#include "u_port.h"
#include "u_port_os.h"
#include "u_port_debug.h"
#include "u_assert.h"
#include "u_sock.h"

#define U_CX_AT_PORT_ASSERT(COND) U_ASSERT(COND)

/* Porting layer for printf().*/
#define U_CX_PORT_PRINTF uPortLog

/* Porting layer for mutexes.*/
#define U_CX_MUTEX_HANDLE uPortMutexHandle_t
#define U_CX_MUTEX_CREATE(mutex) uPortMutexCreate(&mutex)
#define U_CX_MUTEX_DELETE(mutex) uPortMutexDelete(mutex)
#define U_CX_MUTEX_LOCK(mutex) uPortMutexLock(mutex)
#define U_CX_MUTEX_TRY_LOCK(mutex, timeoutMs) uPortMutexTryLock(mutex, timeoutMs)
#define U_CX_MUTEX_UNLOCK(mutex) uPortMutexUnlock(mutex)

/* Porting layer for getting time in ms.*/
#define U_CX_PORT_GET_TIME_MS() uPortGetTickTimeMs()

/* Configuration for enabling use of AT commands in URC callbacks */
#ifndef U_CX_USE_URC_QUEUE
# define U_CX_USE_URC_QUEUE 1
#endif

/* Configuration for enabling logging off AT protocol.*/
#ifndef U_CX_LOG_AT
# define U_CX_LOG_AT 1
#endif

/* Configuration for enabling ANSI color for logs.*/
#ifndef U_CX_LOG_USE_ANSI_COLOR
# define U_CX_LOG_USE_ANSI_COLOR 1
#endif

/* Configuration for enabling additional debug printouts.*/
#ifndef U_CX_LOG_DEBUG
# define U_CX_LOG_DEBUG 0
#endif

#endif // U_CX_CONFIG_H