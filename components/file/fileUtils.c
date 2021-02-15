/**
 * @file
 *
 * File utilities - Support for complex file operations
 *
 * Copyright (C) Sierra Wireless, Inc. Use of this work is subject to license.
 */

#include "fileUtils.h"

// --------------------------------------------------------------------------------------------------
/**
 * Read the entire contents of a file in one operation.
 */
// --------------------------------------------------------------------------------------------------
le_result_t fileUtils_ReadAll
(
    const char* filePath,
    char* buf,
    size_t bufSize
)
{
    le_result_t result;
    size_t fileSize;

    result = le_fs_GetSize(filePath, &fileSize);
    if (LE_OK == result)
    {
        if (bufSize < fileSize)
        {
            LE_ERROR("Insufficient buffer size %zd < %zd", bufSize, fileSize);
            result = LE_OVERFLOW;
        }
        else
        {
            le_fs_FileRef_t fileRef;
            result = le_fs_Open(filePath, LE_FS_RDWR, &fileRef);
            if (LE_OK == result)
            {
                result = le_fs_Read(fileRef, (uint8_t *)buf, &fileSize);
                if (LE_OK != result)
                {
                    LE_ERROR("Failed to read file %s %s", filePath, LE_RESULT_TXT(result));
                }

                result = le_fs_Close(fileRef);
                if (LE_OK != result)
                {
                    LE_ERROR("Error closing file %s", LE_RESULT_TXT(result));
                }
            }
            else
            {
                LE_ERROR("Failed to open file %s %s", filePath, LE_RESULT_TXT(result));
            }
        }
    }
    return result;
}

COMPONENT_INIT
{
}
