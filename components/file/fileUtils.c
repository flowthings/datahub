/**
 * @file
 *
 * File utilities - wrappers to support different underlying file APIs on Linux
 * and RTOS targets
 *
 * Copyright (C) Sierra Wireless, Inc. Use of this work is subject to license.
 */

#include "fileUtils.h"


// HL78 is using le_fs.  The WP7x are using posix functions
#ifdef LE_CONFIG_RTOS
#define USE_LE_FS
#else
#undef USE_LE_FS
#endif


//--------------------------------------------------------------------------------------------------
/**
 * Convert fopen style mode string to LE_FS_ io type flags
 */
//--------------------------------------------------------------------------------------------------
#ifdef USE_LE_FS
static le_fs_AccessMode_t ModeString2Mask
(
    const char *modeStr
)
{
    le_fs_AccessMode_t mask = 0;
    int len = strlen(modeStr);

    LE_ASSERT((0 < len) && (len < 3));

    if (2 == len)
    {
        LE_ASSERT('+' == modeStr[1]);
        switch (*modeStr)
        {
            case 'r':
                mask |= LE_FS_RDWR;
                break;
            case 'w':
                mask |= (LE_FS_RDWR | LE_FS_CREAT | LE_FS_TRUNC);
                break;
            case 'a':
                mask |= (LE_FS_RDWR | LE_FS_APPEND | LE_FS_CREAT);
                break;
            default:
                LE_ASSERT(false);
        }
    }
    else
    {
        switch (*modeStr)
        {
            case 'r':
                mask |= LE_FS_RDONLY;
                break;
            case 'w':
                mask |= (LE_FS_WRONLY | LE_FS_CREAT | LE_FS_TRUNC);
                break;
            case 'a':
                mask |= (LE_FS_WRONLY | LE_FS_APPEND | LE_FS_CREAT);
                break;
            default:
                LE_ASSERT(false);
        }
    }
    return mask;
}
#else

//--------------------------------------------------------------------------------------------------
/**
 * Convert posix errno to Legato le_result_t
 */
//--------------------------------------------------------------------------------------------------
static le_result_t FileErrno2Result
(
    int err
)
{
    switch (err)
    {
        case EACCES: return LE_NOT_PERMITTED;
        case ENOENT: return LE_NOT_FOUND;
        default:     return LE_FAULT;
    }
}
#endif


//--------------------------------------------------------------------------------------------------
/**
 * Retrieve the size of a file, by path
 */
//--------------------------------------------------------------------------------------------------
le_result_t FileSize
(
    const char *filePath,
    size_t *sizePtr
)
{
    le_result_t result = LE_OK;

    LE_ASSERT(filePath && sizePtr);
    if (!strlen(filePath))
    {
        return LE_BAD_PARAMETER;
    }
#ifdef USE_LE_FS
    result = le_fs_GetSize(filePath, sizePtr);
#else
    struct stat stats;
    if (stat(filePath, &stats) < 0)
    {
        result = FileErrno2Result(errno);
    }
    else
    {
        *sizePtr = (size_t)stats.st_size;
    }
#endif
    if (LE_OK != result)
    {
        LE_INFO("Failed to read size of %s %s", filePath, LE_RESULT_TXT(result));
    }
    return result;
}


//--------------------------------------------------------------------------------------------------
/**
 * Open a file stream
 */
//--------------------------------------------------------------------------------------------------
le_result_t FileOpen
(
    const char *filePath,
    const char *modeStr,
    FileRef_t *fileRefPtr
)
{
    LE_ASSERT(filePath && modeStr && fileRefPtr);
    if (!strlen(filePath) || !strlen(modeStr))
    {
        return LE_BAD_PARAMETER;
    }
#ifdef USE_LE_FS
    return le_fs_Open(filePath, ModeString2Mask(modeStr), (le_fs_FileRef_t *)fileRefPtr);
#else
    errno = 0;
    *fileRefPtr = (FileRef_t)fopen(filePath, modeStr);
    return (NULL == *fileRefPtr) ? FileErrno2Result(errno) : LE_OK;
#endif
}


//--------------------------------------------------------------------------------------------------
/**
 * Close a file stream
 */
//--------------------------------------------------------------------------------------------------
void FileClose
(
    FileRef_t fp
)
{
    le_result_t result;
#ifdef USE_LE_FS
    result = le_fs_Close((le_fs_FileRef_t)fp);
#else
    result = (fclose((FILE *)fp)) ? FileErrno2Result(errno) : LE_OK;
#endif
    if (LE_OK != result)
    {
        LE_ERROR("Error closing file %s", LE_RESULT_TXT(result));
    }
}


//--------------------------------------------------------------------------------------------------
/**
 * Read from file
 */
//--------------------------------------------------------------------------------------------------
le_result_t FileRead
(
    FileRef_t fp,
    char *buf,
    size_t *len
)
{
    le_result_t result = LE_OK;

    LE_ASSERT(fp && buf && len);
#ifdef USE_LE_FS
    result = le_fs_Read((le_fs_FileRef_t)fp, (uint8_t *)buf, len);
#else
    FILE *f = (FILE *)fp;
    size_t count = 0;

    do
    {
        clearerr(f);
        count += fread((void *)(buf + count), 1, *len - count, f);
        if (feof(f))
        {
            // end of file
            break;
        }
        if (ferror(f))
        {
            result = FileErrno2Result(errno);
            break;
        }
    } while (count < *len);
    *len = count;
#endif
    return result;
}


//--------------------------------------------------------------------------------------------------
/**
 * Write to file
 */
//--------------------------------------------------------------------------------------------------
le_result_t FileWrite
(
    FileRef_t fp,
    const char *buf,
    size_t len
)
{
    le_result_t result = LE_OK;

    LE_ASSERT(fp && buf);
#ifdef USE_LE_FS
    result = le_fs_Write((le_fs_FileRef_t)fp, (const uint8_t *)buf, len);
#else
    FILE *f = (FILE *)fp;
    size_t count = 0;

    do
    {
        clearerr(f);
        count += fwrite((const void *)(buf + count), 1, len - count, f);
        if (ferror(f))
        {
            result = FileErrno2Result(errno);
            break;
        }
    } while (count < len);
#endif
    return result;
}


//--------------------------------------------------------------------------------------------------
/**
 * Read the entire contents of a file in one operation.
 */
//--------------------------------------------------------------------------------------------------
le_result_t FileReadAll
(
    const char *filePath,
    char *buf,
    size_t bufSize
)
{
    le_result_t result;
    size_t fileSize;

    result = FileSize(filePath, &fileSize);
    if (LE_OK == result)
    {
        if (bufSize < fileSize)
        {
            LE_ERROR("Insufficient buffer size %u < %u", bufSize, fileSize);
            result = LE_OVERFLOW;
        }
        else
        {
            FileRef_t fp;
            result = FileOpen(filePath, "r", &fp);
            if (LE_OK == result)
            {
                result = FileRead(fp, buf, &fileSize);
                if (LE_OK != result)
                {
                    LE_ERROR("Failed to read file %s %s", filePath, LE_RESULT_TXT(result));
                }
                FileClose(fp);
            }
            else
            {
                LE_ERROR("Failed to open file %s %s", filePath, LE_RESULT_TXT(result));
            }
        }
    }
    return result;
}


//--------------------------------------------------------------------------------------------------
/**
 * Write the entire contents of a file in one operation.  File is created if it does not exist
 * and truncated if it does.
 */
//--------------------------------------------------------------------------------------------------
le_result_t FileWriteAll
(
    const char *filePath,
    const char *buf,
    size_t len
)
{
    FileRef_t fp;
    le_result_t result = FileOpen(filePath, "w", &fp);

    if (LE_OK == result)
    {
        result = FileWrite(fp, buf, len);
        if (LE_OK != result)
        {
            LE_ERROR("Failed to write file %s %s", filePath, LE_RESULT_TXT(result));
        }
        FileClose(fp);
    }
    else
    {
        LE_ERROR("Failed to open file %s %s", filePath, LE_RESULT_TXT(result));
    }
    return result;
}


//--------------------------------------------------------------------------------------------------
/**
 * Remove a file by path
 */
//--------------------------------------------------------------------------------------------------
le_result_t FileRemove
(
    const char* filePath
)
{
    le_result_t result;

    LE_ASSERT(filePath);
    if (!strlen(filePath))
    {
        return LE_BAD_PARAMETER;
    }
#ifdef USE_LE_FS
    result = le_fs_Delete(filePath);
#else
    result = (remove(filePath)) ? FileErrno2Result(errno) : LE_OK;
#endif
    if (LE_OK != result)
    {
        LE_INFO("Failed to remove file %s %s", filePath, LE_RESULT_TXT(result));
    }
    return result;
}

COMPONENT_INIT
{
}