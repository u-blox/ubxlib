/*
 * Copyright 2019-2024 u-blox
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

#ifndef _U_PORT_NAMED_PIPE_H_
#define _U_PORT_NAMED_PIPE_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

/** \addtogroup __port
 *  @{
 */

/** @file
 * @brief Porting layer for named pipes for inter-application
 * communication.
 *
 * Note: this API is currently only used while testing BLE bonding,
 * it is not used in the core ubxlib code.  It is currently only
 * implemented on Windows and native Linux platforms.  If you are
 * creating your own port and do not wish to test BLE bonding you
 * do not need to implement it.  Where it is not implemented a weak
 * implementation in u_port_named_pipe_default.c will take over and
 * return #U_ERROR_COMMON_NOT_SUPPORTED.
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

/** Named pipe handle.
 */
typedef void *uPortNamePipeHandle_t;

/* ----------------------------------------------------------------
 * FUNCTIONS
 * -------------------------------------------------------------- */

/** Create a named pipe in server or client mode.
 *
 * When in server mode the pipe is created on a system wide basis and
 * the caller program instance will be the owner of this.
 * If client mode this function just establishes what reference
 * to use in subsequent calls using the returned handle.
 * Both server and client can perform read and write on the pipe.
 * All subsequent calls are blocking. This means that startup order
 * for server and client doesn't matter.
 *
 * @param[out] pPipeHandle a handle to the created named pipe..
 * @param[in]  pName       a null-terminated string naming the pipe,
 *                         may NOT be NULL.
 * @param      server      True for server, false for client.
 * @return                 zero on success else negative error code.
 */
int32_t uPortNamedPipeCreate(uPortNamePipeHandle_t *pPipeHandle, const char *pName, bool server);

/** Write a string to a named pipe. The complete string including the null-terminator
 * will be written.
 *
 * @param      pipeHandle a handle received earlier from uPortNamedPipeCreate().
 * @param[in]  pStr       a null-terminated string to be written.
 * @return                zero on success else negative error code.
 */
int32_t uPortNamedPipeWriteStr(uPortNamePipeHandle_t pipeHandle, const char *pStr);

/** Read a string from a named pipe.
 *
 * @param       pipeHandle a handle received earlier from uPortNamedPipeCreate().
 * @param[out]  pStr       a string where the received data can be written. The
 *                         string will always be null-terminated on return.
 * @param       maxLength  the maximum number length of the string.
 * @return                 the length of the read string (excluding terminator) on
 *                         success else negative error code.
 */
int32_t uPortNamedPipeReadStr(uPortNamePipeHandle_t pipeHandle, char *pStr, size_t maxLength);

/** Delete a named pipe and the structures referenced by the specified handle.
 * The pipe will not actually be deleted from the system until all references are gone.
 *
 * @param[out] pipeHandle a handle received earlier from uPortNamedPipeCreate().
 * @return                 zero on success else negative error code.
 */
int32_t uPortNamedPipeDelete(uPortNamePipeHandle_t pipeHandle);

#ifdef __cplusplus
}
#endif

/** @}*/

#endif // _U_PORT_NAMED_PIPE_H_

// End of file
