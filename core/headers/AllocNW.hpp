#pragma once
#include <cstddef>
#include <cstdlib>
#include <new>

#if defined(_WIN32)
    #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
    #endif
    #include <windows.h>
    #include <memoryapi.h>
#elif define(HAVE_LIBNUMA)
    #include <numa.h>
    #include <numaif.h>
    #include <unistd.h>
#else
  #error "Alloc.hpp requires either Windows NUMA (VirtualAllocExNuma) or Linux libnuma (HAVE_LIBNUMA). Define HAVE_LIBNUMA and link -lnuma for Linux."
#endif

namespace AtomicCScompact::AllocNW
{
    inline void* AlignedAllocP(size_t alignment, size_t size)
    {
        if (alignment == 0)
        {
            alignment = alignof(void*);
        }
    //OS specific
    #if defined(_MSC_VER)
        void* p = _aligned_malloc(size, alignment);
        if(!p)
        {
            throw std::bad_alloc();
        }
        return p;
    #else
        void* p = nullptr;
        int rc = posix_memalign(&p, alignment, size);
        if (rc != 0 || !p)
        {
            throw std::bad_alloc();
        }
        return p;
    #endif
    }


    inline void AlignedFreeP(void* p) noexcept
    {
    //os specefic
    #if defined(_MSC_VER)
        _aligned_free(p);
    #else
        free(p);
    #endif
    }

    inline size_t PageSize()
    {
    //os specific
    #if defined(_WIN32)
        SYSTEM_INFO sysinfo;
        GetSystemInfo(&sysinfo);
        return static_cast<size_t>(sysinfo.dwPageSize);
    #else
        long ps = sysconf(_SC_PAGESIZE);
        if (ps > 0)
        {
            return static_cast<size_t>(ps);
        }
        else
        {
            return 4096u;
        }
    #endif
    }


//os specific
#if defined(HAVE_NUMA)
    inline void* AlignedAllocONnode(size_t alignment, size_t size, int node)
    {
        if (alignment == 0)
        {
            alignment = PageSize();
        }
        size_t ps = PageSize();
        size_t rounded = ((size + ps -1) / ps) * ps;
        if (numa_available() < 0)
        {
            throw std::runtime_error("libnuma::numa_available < 0::Not available");
        }
        if (node < 0 || node > numa_max_node())
        {
            throw std::invalid_argument("libnuma::Invalid node");
        }
        if (!p)
        {
            throw std::bad_alloc();
        }
        return p;
    }

    inline void FreeONNode(void* p, size_t size,) noexcept
    {
        if (!p)
        {
            return;
        }
        size_t ps = PageSize();
        size_t rounded = ((size + ps - 1) / ps) * ps;
        numa_free(p, rounded);
    }

#elif defined(_WIN32)
    inline void* AlignedAllocONnode(size_t alignment, size_t size, int node)
    {
        if (alignment == 0)
        {
            alignment = PageSize();
        }
        size_t ps = PageSize();
        size_t rounded = ((size + ps - 1) / ps) * ps;

        HANDLE HProc = GetCurrentProcess();
        DWORD alloc_type = MEM_RESERVE | MEM_COMMIT;
        DWORD protect = PAGE_READWRITE;

        LPVOID result = VirtualAllocExNuma(HProc, nullptr, rounded, alloc_type, protect, static_cast<DWORD>(node));
        if (!result)
        {
            throw std::bad_alloc();
        }
        
        return result;
    }

    inline void FreeONNode(void* p, size_t) noexcept
    {
        if (!p)
        {
            return;
        }
        VirtualFree(p, 0, MEM_RELEASE);
    }
#endif
}