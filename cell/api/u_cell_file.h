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

#ifndef _U_CELL_FILE_H_
#define _U_CELL_FILE_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

#include "u_device.h"

/** \addtogroup _cell
 *  @{
 */

/** @file
 * @brief This header file defines the u-blox API for file system.
 * These functions are thread-safe unless otherwise specified
 * in the function description.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/** Maximum allowed file name length for a file on file system;
 * this does NOT include room for a null terminator, so any storage
 * buffer should be this length plus one.
 */
#define U_CELL_FILE_NAME_MAX_LENGTH 248

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * FUNCTIONS
 * -------------------------------------------------------------- */

/** The file system is able to read/write/delete/list files from
 * pre-defined "tagged" areas of the file system, a little like
 * directories but the tags are ONLY pre-defined by the module.
 * To use a tagged area, call this function with the tag name (refer
 * to the file system section of the AT manual for your module to
 * find out what the permitted tags are).  If this function is not
 * called the default "USER" area of the file system applies.
 * Note that uCellFileBlockRead() does NOT support use of tags, i.e.
 * only files from the default "USER" area of the file system can
 * be read in blocks.
 *
 * @param cellHandle the handle of the cellular instance.
 * @param[in] pTag   the null-terminated string that is the name of the
 *                   tag to use - this tag name will apply until the
 *                   cellular API is deinitialised; use NULL to return
 *                   to default operation (where no specific tag is
 *                   used and hence the default "USER" area of the file
 *                   system will be addressed).
 * @return           zero on success or negative error code on failure.
 */
int32_t uCellFileSetTag(uDeviceHandle_t cellHandle, const char *pTag);

/** Get the file system tag that is currently in use, see
 * uCellFileSetTag() for more information.  If NULL is returned then
 * no specific tag is being applied and hence the default "USER" area
 * of the file system is being addressed.
 *
 * @param cellHandle the handle of the cellular instance.
 * @return           the null-terminated tag name currently in use or
 *                   NULL if no specific tag is in use.
 */
const char *pUCellFileGetTag(uDeviceHandle_t cellHandle);

/** Open a file in write mode on the file system and write a stream of
 * bytes to it. If the file already exists, the data will be appended to
 * the file already stored in the file system. In order to avoid
 * character loss it is recommended that flow control lines are
 * connected on the interface to the module.
 *
 * @param cellHandle     the handle of the cellular instance.
 * @param[in] pFileName  a pointer to file name to be stored on file
 *                       system. File name cannot contain these
 *                       characters: / * : % | " < > ?.
 * @param[in] pData      a pointer to the data to write into the file.
 * @param dataSize       number of data bytes to write into the file.
 * @return               on success return number of bytes written into
 *                       the file or negative error code on failure.
 */
int32_t uCellFileWrite(uDeviceHandle_t cellHandle,
                       const char *pFileName,
                       const char *pData,
                       size_t dataSize);

/** Read the contents of a file from the file system. If the file does not exist,
 * error will be return. In order to avoid character loss it is recommended
 * that flow control lines are connected on the interface to the module.
 *
 * @param cellHandle     the handle of the cellular instance.
 * @param[in] pFileName  a pointer to file name to read file contents from the
 *                       file system. File name cannot contain these characters:
 *                       / * : % | " < > ?.
 * @param[out] pData     a pointer to a place to store the stream of bytes.
 * @param dataSize       number of data bytes to read.
 * @return               on success return number of bytes read from file
 *                       or negative error code on failure.
 */
int32_t uCellFileRead(uDeviceHandle_t cellHandle,
                      const char *pFileName,
                      char *pData,
                      size_t dataSize);

/** Read partial contents of a file from the file system, based on given
 * offset and size.  If the file does not exist, error will be returned.
 * In order to avoid character loss it is recommended that flow control
 * lines are connected on the interface to the module.  Note that this
 * functions does NOT support use of tags, i.e. only files from the
 * default "USER" area of the file system can be read in blocks.
 *
 * @param cellHandle     the handle of the cellular instance.
 * @param[in] pFileName  a pointer to file name to read file contents
 *                       from the file system. File name cannot contain
 *                       these characters: / * : % | " < > ?.
 * @param[out] pData     a pointer to a place to store the read data.
 * @param offset         offset in bytes from the beginning of the file.
 * @param dataSize       number of data bytes to read.
 * @return               on success return number of bytes read from file
 *                       or negative error code on failure.
 */
int32_t uCellFileBlockRead(uDeviceHandle_t cellHandle,
                           const char *pFileName,
                           char *pData,
                           size_t offset,
                           size_t dataSize);

/** Read size of file on the file system. If the file does not exists,
 * error will be return.
 *
 * @param cellHandle     the handle of the cellular instance.
 * @param[in] pFileName  a pointer to the file name to read the size of.
 *                       File name cannot contain these characters:
 *                       / : % | " < >.
 * @return               on success return file size or negative error
 *                       code on failure.
 */
int32_t uCellFileSize(uDeviceHandle_t cellHandle,
                      const char *pFileName);

/** Delete a file from the file system. If the file does not exist,
 * error will be return.
 *
 * @param cellHandle     the handle of the cellular instance.
 * @param[in] pFileName  a pointer to the file name to delete from the
 *                       file system. File names cannot contain these
 *                       characters: / * : % | " < > ?.
 * @return               zero on success or negative error code on failure.
 */
int32_t uCellFileDelete(uDeviceHandle_t cellHandle,
                        const char *pFileName);

/** Get the description of file stored on the file system; uCellFileListNext()
 * should be called repeatedly to iterate through subsequent entries in
 * the list. This function is not thread-safe in that there is a single
 * list of names for any given cellHandle: for a re-entrant version see
 * uCellFileListFirst_r() / uCellFileListNext_r() / uCellFileListLast_r().
 *
 * For instance, to print out the names of all stored files on file system:
 *
 * ```
 * char fileName[U_CELL_FILE_NAME_MAX_LENGTH + 1];
 *
 * for (int32_t x = uCellFileListFirst(handle, fileName);
 *      x >= 0;
 *      x = uCellFileListNext(handle, fileName)) {
 *      printf("%s\n", fileName);
 * }
 * ```
 *
 * If a tag has been set using uCellFileSetTag() then only
 * files from the tagged area of the file system will be listed.
 *
 * @param cellHandle      the handle of the cellular instance.
 * @param[out] pFileName  pointer to somewhere to store the result;
 *                        at least #U_CELL_FILE_NAME_MAX_LENGTH + 1 bytes
 *                        of storage must be provided.
 * @return                the total number of file names in the list
 *                        or negative error code.
 */
int32_t uCellFileListFirst(uDeviceHandle_t cellHandle,
                           char *pFileName);

/** Get the subsequent file names in the list. Use uCellFileListFirst()
 * to get the total number of entries in the list and the first result
 * then call this "number of results" times to read out all of
 * the file names in the link list. Calling this "number of results"
 * times will free the memory that held the list after the final call
 * (can be freed with a call to uCellFileListLast()). This function
 * is not thread-safe in that there is a single list for all threads.
 *
 * If a tag has been set using uCellFileSetTag() then only
 * files from the tagged area of the file system will be listed.
 *
 * @param cellHandle      the handle of the cellular instance.
 * @param[out] pFileName  pointer to somewhere to store the result;
 *                        at least #U_CELL_FILE_NAME_MAX_LENGTH + 1
 *                        bytes of storage must be provided..
 * @return                the number of entries remaining *after*
 *                        this one has been read or negative error
 *                        code.
 */
int32_t uCellFileListNext(uDeviceHandle_t cellHandle,
                          char *pFileName);

/** It is good practice to call this to clear up memory from
 * uCellFileListFirst() if you are not going to iterate
 * through the whole list with uCellFileListNext().
 *
 * @param cellHandle the handle of the cellular instance.
 */
void uCellFileListLast(uDeviceHandle_t cellHandle);

/** As uCellFileListFirst() but re-entrant; you must provide
 * storage for the pointer ppRentrant, something like:
 *
 * ```
 * char fileName[U_CELL_FILE_NAME_MAX_LENGTH + 1];
 * void *pReentrant;
 *
 * for (int32_t x = uCellFileListFirst_r(handle, fileName, &pReentrant);
 *      x >= 0;
 *      x = uCellFileListNext_r(fileName, &pReentrant)) {
 *      printf("%s\n", fileName);
 * }
 * ```
 *
 * @param cellHandle      the handle of the cellular instance.
 * @param[out] pFileName  pointer to somewhere to store the result;
 *                        at least #U_CELL_FILE_NAME_MAX_LENGTH + 1 bytes
 *                        of storage must be provided.
 * @param[out] ppRentrant pointer to a place where this function
                          can store its rentrancy pointer.
 * @return                the total number of file names in the list
 *                        or negative error code.
 */
int32_t uCellFileListFirst_r(uDeviceHandle_t cellHandle,
                             char *pFileName, void **ppRentrant);

/** As uCellFileListNext() but re-entrant; you must pass the
 * re-entrancy pointer that was passed to uCellFileListFirst_r()
 * to this function.
 *
 * @param[out] pFileName     pointer to somewhere to store the result;
 *                           at least #U_CELL_FILE_NAME_MAX_LENGTH + 1
 *                           bytes of storage must be provided.
 * @param[in,out] ppRentrant pointer to the rentrancy pointer that
 *                           was passed to uCellFileListFirst_r().
 * @return                   the number of entries remaining *after*
 *                           this one has been read or negative error
 *                           code.
 */
int32_t uCellFileListNext_r(char *pFileName, void **ppRentrant);

/**  As uCellFileListLast() but re-entrant; you must pass the
 * re-entrancy pointer that was passed to uCellFileListFirst_r()
 * to this function.
 *
 * @param[in] ppRentrant pointer to the rentrancy pointer that was
 *                       passed to uCellFileListFirst_r().
 */
void uCellFileListLast_r(void **ppRentrant);

#ifdef __cplusplus
}
#endif

/** @}*/

#endif // _U_CELL_FILE_H_

// End of file
