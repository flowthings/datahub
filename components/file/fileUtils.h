/**
 * @file
 *
 * File utilities - wrappers to support different underlying file APIs on Linux
 * and RTOS targets
 *
 * Copyright (C) Sierra Wireless, Inc. Use of this work is subject to license.
 */

#ifndef FILE_UTILS_H_INCLUDE_GUARD
#define FILE_UTILS_H_INCLUDE_GUARD

#include "legato.h"


//--------------------------------------------------------------------------------------------------
/**
 * File reference
 */
//--------------------------------------------------------------------------------------------------
typedef void * FileRef_t;


//--------------------------------------------------------------------------------------------------
/**
 * Create or open an existing file.
 *
 * @return
 *  - LE_OK             The function succeeded
 *  - LE_BAD_PARAMETER  A parameter is invalid
 *  - LE_OVERFLOW       The file path is too long
 *  - LE_NOT_FOUND      The file does not exists or a directory in the path does not exist
 *  - LE_NOT_PERMITTED  Access denied to the file or to a directory in the path
 *  - LE_UNSUPPORTED    The prefix cannot be added and the function is unusable
 *  - LE_FAULT          The function failed
 *
 * @note: Function will assert if a pointer argument is NULL
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED le_result_t FileOpen
(
    const char *filePath,     ///< [IN]  File path
    const char *modeStr,      ///< [IN]  Open mode - behavior same as for fopen
    FileRef_t *fileRefPtr     ///< [OUT] File reference, on success
);


//--------------------------------------------------------------------------------------------------
/**
 * Close an open file
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED void FileClose
(
    FileRef_t fp              ///< [IN]  File reference
);


//--------------------------------------------------------------------------------------------------
/**
 * Read from an open file, starting at the current file position.
 *
 * @return
 *  - LE_OK             The function succeeded
 *  - LE_BAD_PARAMETER  A parameter is invalid
 *  - LE_FAULT          Unspecified error
 *
 * @note:
 *  - Function will assert if a pointer argument is NULL
 *  - A read of less than the requested number of bytes is not an error
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED le_result_t FileRead
(
    FileRef_t fp,             ///< [IN]  File reference
    char *buf,                ///< [OUT] Destination buffer
    size_t *len               ///< [IN/OUT]  Number of bytes requested / read
);


//--------------------------------------------------------------------------------------------------
/**
 * Write data to an open file, starting from the current file position
 *
 * @return
 *  - LE_OK             The function succeeded
 *  - LE_BAD_PARAMETER  A parameter is invalid
 *  - LE_UNDERFLOW      The write succeed but was not able to write all bytes
 *  - LE_FAULT          Unspecified error
 *
 * @note:
 *  - Function will assert if a pointer argument is NULL
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED le_result_t FileWrite
(
    FileRef_t fp,             ///< [IN] File reference
    const char *buf,          ///< [IN] Source buffer
    size_t len                ///< [IN] Number of bytes to write
);


//--------------------------------------------------------------------------------------------------
/**
 * Read the entire contents of a file in one operation
 *
 * @return
 *  - LE_OK             The function succeeded
 *  - LE_BAD_PARAMETER  A parameter is invalid
 *  - LE_OVERFLOW       File contents are larger than supplied buffer
 *  - LE_FAULT          Unspecified error
 *
 * @note:
 *  - Function will assert if a pointer argument is NULL
 *  - An error is returned if the entire file contents cannot fit in the buffer
 *  - A read of less than the buffer size is not an error
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED le_result_t FileReadAll
(
    const char *filePath,     ///< [IN]  File path
    char* dataBuffer,         ///< [OUT] Destination buffer
    size_t bufferSize         ///< [IN]  Size of destination buffer
);


//--------------------------------------------------------------------------------------------------
/**
 * Write the entire contents of a file in one operation.  File is created if it does not exist
 * and truncated if it does.
 *
 * @return
 *  - LE_OK             The function succeeded
 *  - LE_BAD_PARAMETER  A parameter is invalid
 *  - LE_FAULT          Unspecified error
 *
 * @note:
 *  - Function will assert if a pointer argument is NULL
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED le_result_t FileWriteAll
(
    const char *filePath,     ///< [IN]  File path
    const char* dataBuffer,   ///< [IN]  Buffer of data to write
    size_t dataLength         ///< [IN]  Data length
);


//--------------------------------------------------------------------------------------------------
/**
 * Remove a file
 *
 * @return
 *  - LE_OK             The function succeeded
 *  - LE_BAD_PARAMETER  A parameter is invalid
 *  - LE_NOT_FOUND      The file does not exist or a directory in the path does not exist
 *  - LE_NOT_PERMITTED  Insufficient access permissions to delete file
 *  - LE_FAULT          Unspecified error
 *
 * @note:
 *  - Function will assert if a pointer argument is NULL
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED le_result_t FileRemove
(
    const char *filePath      ///< [IN]  File path
);


//--------------------------------------------------------------------------------------------------
/**
 * Get the size of a file
 *
 * @return
 *  - LE_OK             The function succeeded
 *  - LE_BAD_PARAMETER  A parameter is invalid
 *  - LE_FAULT          Unspecified error
 *
 * @note:
 *  - Function will assert if a pointer argument is NULL
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED le_result_t FileSize
(
    const char *filePath,     ///< [IN]  File path
    size_t *sizePtr           ///< [OUT] Pointer to file size
);

#endif // FILE_UTILS_H_INCLUDE_GUARD
