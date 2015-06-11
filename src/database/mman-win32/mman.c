/* mman-win32 from code.google.com/p/mman-win32 (MIT License). */

#include "mman.h"

#ifdef _WIN32

#include <stdint.h>
#include <windows.h>
#include <errno.h>
#include <io.h>

#ifndef FILE_MAP_EXECUTE
    #define FILE_MAP_EXECUTE    0x0020
#endif

static int __map_mman_error(const DWORD err, const int deferr)
{
    if (err == 0)
        return 0;

    /* TODO: implement. */

    return err;
}

static DWORD __map_mmap_prot_page(const int prot)
{
    DWORD protect = 0;

    if (prot == PROT_NONE)
        return protect;

    if ((prot & PROT_EXEC) != 0)
    {
        protect = ((prot & PROT_WRITE) != 0) ?
        PAGE_EXECUTE_READWRITE : PAGE_EXECUTE_READ;
    }
    else
    {
        protect = ((prot & PROT_WRITE) != 0) ?
        PAGE_READWRITE : PAGE_READONLY;
    }

    return protect;
}

static DWORD __map_mmap_prot_file(const int prot)
{
    DWORD desiredAccess = 0;

    if (prot == PROT_NONE)
        return desiredAccess;

    if ((prot & PROT_READ) != 0)
        desiredAccess |= FILE_MAP_READ;
    if ((prot & PROT_WRITE) != 0)
        desiredAccess |= FILE_MAP_WRITE;
    if ((prot & PROT_EXEC) != 0)
        desiredAccess |= FILE_MAP_EXECUTE;

    return desiredAccess;
}

void* mmap(void* addr, size_t len, int prot, int flags, int fildes, oft__ off)
{
    HANDLE mapping, handle;

    void* map = MAP_FAILED;

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4293)
#endif

    const DWORD access  = __map_mmap_prot_file(prot);
    const DWORD protect = __map_mmap_prot_page(prot);

    const oft__ max = off + (oft__)len;
    const int less = (sizeof(oft__) <= sizeof(DWORD));

    const DWORD maxLow   = less ? (DWORD)max : (DWORD)((max      ) & MAXDWORD);
    const DWORD maxHigh  = less ? (DWORD)0   : (DWORD)((max >> 32) & MAXDWORD);
    const DWORD fileLow  = less ? (DWORD)off : (DWORD)((off      ) & MAXDWORD);
    const DWORD fileHigh = less ? (DWORD)0   : (DWORD)((off >> 32) & MAXDWORD);

#ifdef _MSC_VER
#pragma warning(pop)
#endif

    errno = 0;

    if (len == 0
        /* Unsupported flag combinations */
        || (flags & MAP_FIXED) != 0
        /* Usupported protection combinations */
        || prot == PROT_EXEC)
    {
        errno = EINVAL;
        return MAP_FAILED;
    }

    handle = ((flags & MAP_ANONYMOUS) == 0) ?
        (HANDLE)_get_osfhandle(fildes) : INVALID_HANDLE_VALUE;

    if ((flags & MAP_ANONYMOUS) == 0 && handle == INVALID_HANDLE_VALUE)
    {
        errno = EBADF;
        return MAP_FAILED;
    }

    mapping = CreateFileMapping(handle, NULL, protect, maxHigh, maxLow, NULL);

    if (mapping == NULL)
    {
        errno = __map_mman_error(GetLastError(), EPERM);
        return MAP_FAILED;
    }

    map = MapViewOfFile(mapping, access, fileHigh, fileLow, len);

    CloseHandle(mapping);

    if (map == NULL)
    {
        errno = __map_mman_error(GetLastError(), EPERM);
        return MAP_FAILED;
    }

    return map;
}

int munmap(void* addr, size_t len)
{
    if (UnmapViewOfFile(addr))
        return 0;

    errno = __map_mman_error(GetLastError(), EPERM);

    return -1;
}

int mprotect(void* addr, size_t len, int prot)
{
    DWORD newProtect = __map_mmap_prot_page(prot);
    DWORD oldProtect = 0;

    if (VirtualProtect(addr, len, newProtect, &oldProtect))
        return 0;

    errno = __map_mman_error(GetLastError(), EPERM);

    return -1;
}

int msync(void* addr, size_t len, int flags)
{
    if (FlushViewOfFile(addr, len))
        return 0;

    errno = __map_mman_error(GetLastError(), EPERM);

    return -1;
}

int mlock(const void* addr, size_t len)
{
    if (VirtualLock((LPVOID)addr, len))
        return 0;

    errno = __map_mman_error(GetLastError(), EPERM);

    return -1;
}

int munlock(const void* addr, size_t len)
{
    if (VirtualUnlock((LPVOID)addr, len))
        return 0;

    errno = __map_mman_error(GetLastError(), EPERM);

    return -1;
}

/* www.gitorious.org/git-win32/mainline/source/9ae6b7513158e0b1523766c9ad4a1ad286a96e2c:win32/ftruncate.c */
int ftruncate(int fd, oft__ size)
{
    HANDLE handle;
    DWORD position;
    LARGE_INTEGER li;

    if (fd < 0)
        return -1;

    /* guard against overflow from unsigned to signed */
    if (size >= MAXINT64)
        return -1;

    /* unsigned to signed, splits to high and low */
    li.QuadPart = (LONGLONG)size;

    handle = (HANDLE)_get_osfhandle(fd);
    position = SetFilePointer(handle, li.LowPart, &li.HighPart, FILE_BEGIN);

    if (position == INVALID_SET_FILE_POINTER ||
        SetEndOfFile(handle) == FALSE)
    {
        DWORD error = GetLastError();

        switch (error)
        {
            case ERROR_INVALID_HANDLE:
                errno = EBADF;
                break;
            case ERROR_DISK_FULL:
                errno = ENOSPC;
                break;
            default:
                errno = EIO;
                break;
        }

        return -1;
    }

    return 0;
}

#endif
