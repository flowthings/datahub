/**
 * @file
 *
 * File utilities - Support for complex file operations 
 *
 * Copyright (C) Sierra Wireless, Inc. Use of this work is subject to license.
 */

#ifndef FILE_UTILS_H_INCLUDE_GUARD
#define FILE_UTILS_H_INCLUDE_GUARD

#include "legato.h"

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
LE_SHARED le_result_t fileUtils_ReadAll
(
    const char* filePath,     ///< [IN]  File path
    char* dataBuffer,         ///< [OUT] Destination buffer
    size_t bufferSize         ///< [IN]  Size of destination buffer
);

#endif // FILE_UTILS_H_INCLUDE_GUARD
