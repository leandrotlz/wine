/*
 * What processor?
 *
 * Copyright 1995,1997 Morten Welinder
 * Copyright 1997-1998 Marcus Meissner
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include "config.h"
#include "wine/port.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#ifdef HAVE_SYS_TIME_H
# include <sys/time.h>
#endif


#define NONAMELESSUNION
#define NONAMELESSSTRUCT
#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "windef.h"
#include "winbase.h"
#include "winnt.h"
#include "winternl.h"
#include "psapi.h"
#include "ddk/wdm.h"
#include "wine/unicode.h"
#include "kernel_private.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(reg);

#define SHARED_DATA     ((KSHARED_USER_DATA*)0x7ffe0000)

/****************************************************************************
 *		QueryPerformanceCounter (KERNEL32.@)
 *
 * Get the current value of the performance counter.
 * 
 * PARAMS
 *  counter [O] Destination for the current counter reading
 *
 * RETURNS
 *  Success: TRUE. counter contains the current reading
 *  Failure: FALSE.
 *
 * SEE ALSO
 *  See QueryPerformanceFrequency.
 */
BOOL WINAPI QueryPerformanceCounter(PLARGE_INTEGER counter)
{
    NtQueryPerformanceCounter( counter, NULL );
    return TRUE;
}


/****************************************************************************
 *		QueryPerformanceFrequency (KERNEL32.@)
 *
 * Get the resolution of the performance counter.
 *
 * PARAMS
 *  frequency [O] Destination for the counter resolution
 *
 * RETURNS
 *  Success. TRUE. Frequency contains the resolution of the counter.
 *  Failure: FALSE.
 *
 * SEE ALSO
 *  See QueryPerformanceCounter.
 */
BOOL WINAPI QueryPerformanceFrequency(PLARGE_INTEGER frequency)
{
    LARGE_INTEGER counter;
    NtQueryPerformanceCounter( &counter, frequency );
    return TRUE;
}


/***********************************************************************
 * 			GetSystemInfo            	[KERNEL32.@]
 *
 * Get information about the system.
 *
 * RETURNS
 *  Nothing.
 */
VOID WINAPI GetSystemInfo(
	LPSYSTEM_INFO si	/* [out] Destination for system information, may not be NULL */)
{
    NTSTATUS                 nts;
    SYSTEM_CPU_INFORMATION   sci;

    TRACE("si=0x%p\n", si);

    if ((nts = NtQuerySystemInformation( SystemCpuInformation, &sci, sizeof(sci), NULL )) != STATUS_SUCCESS)
    {
        SetLastError(RtlNtStatusToDosError(nts));
        return;
    }

    si->u.s.wProcessorArchitecture  = sci.Architecture;
    si->u.s.wReserved               = 0;
    si->dwPageSize                  = system_info.PageSize;
    si->lpMinimumApplicationAddress = system_info.LowestUserAddress;
    si->lpMaximumApplicationAddress = system_info.HighestUserAddress;
    si->dwActiveProcessorMask       = system_info.ActiveProcessorsAffinityMask;
    si->dwNumberOfProcessors        = system_info.NumberOfProcessors;

    switch (sci.Architecture)
    {
    case PROCESSOR_ARCHITECTURE_INTEL:
        switch (sci.Level)
        {
        case 3:  si->dwProcessorType = PROCESSOR_INTEL_386;     break;
        case 4:  si->dwProcessorType = PROCESSOR_INTEL_486;     break;
        case 5:
        case 6:  si->dwProcessorType = PROCESSOR_INTEL_PENTIUM; break;
        default: si->dwProcessorType = PROCESSOR_INTEL_PENTIUM; break;
        }
        break;
    case PROCESSOR_ARCHITECTURE_PPC:
        switch (sci.Level)
        {
        case 1:  si->dwProcessorType = PROCESSOR_PPC_601;       break;
        case 3:
        case 6:  si->dwProcessorType = PROCESSOR_PPC_603;       break;
        case 4:  si->dwProcessorType = PROCESSOR_PPC_604;       break;
        case 9:  si->dwProcessorType = PROCESSOR_PPC_604;       break;
        case 20: si->dwProcessorType = PROCESSOR_PPC_620;       break;
        default: si->dwProcessorType = 0;
        }
        break;
    case PROCESSOR_ARCHITECTURE_AMD64:
        si->dwProcessorType = PROCESSOR_AMD_X8664;
        break;
    case PROCESSOR_ARCHITECTURE_ARM:
        switch (sci.Level)
        {
        case 4:  si->dwProcessorType = PROCESSOR_ARM_7TDMI;     break;
        default: si->dwProcessorType = PROCESSOR_ARM920;
        }
        break;
    default:
        FIXME("Unknown processor architecture %x\n", sci.Architecture);
        si->dwProcessorType = 0;
    }
    si->dwAllocationGranularity     = system_info.AllocationGranularity;
    si->wProcessorLevel             = sci.Level;
    si->wProcessorRevision          = sci.Revision;
}


/***********************************************************************
 * 			GetNativeSystemInfo            	[KERNEL32.@]
 */
VOID WINAPI GetNativeSystemInfo(
    LPSYSTEM_INFO si	/* [out] Destination for system information, may not be NULL */)
{
    BOOL is_wow64;

    GetSystemInfo(si); 

    IsWow64Process(GetCurrentProcess(), &is_wow64);
    if (is_wow64)
    {
        if (si->u.s.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_INTEL)
        {
            si->u.s.wProcessorArchitecture = PROCESSOR_ARCHITECTURE_AMD64;
            si->dwProcessorType = PROCESSOR_AMD_X8664;
        }
        else
        {
            FIXME("Add the proper information for %d in wow64 mode\n",
                  si->u.s.wProcessorArchitecture);
        }
    }
}

/***********************************************************************
 * 			IsProcessorFeaturePresent	[KERNEL32.@]
 *
 * Determine if the cpu supports a given feature.
 * 
 * RETURNS
 *  TRUE, If the processor supports feature,
 *  FALSE otherwise.
 */
BOOL WINAPI IsProcessorFeaturePresent (
	DWORD feature	/* [in] Feature number, (PF_ constants from "winnt.h") */) 
{
  if (feature < PROCESSOR_FEATURE_MAX)
    return SHARED_DATA->ProcessorFeatures[feature];
  else
    return FALSE;
}

/***********************************************************************
 *           K32GetPerformanceInfo (KERNEL32.@)
 */
BOOL WINAPI K32GetPerformanceInfo(PPERFORMANCE_INFORMATION info, DWORD size)
{
    static SIZE_T commitpeak = 0;
#ifdef linux
    FILE *f;
#endif
    MEMORYSTATUSEX mem_status;
    SYSTEM_PROCESS_INFORMATION *spi;
    ULONG bufsize = 0x4000;
    void *buf = NULL;
    NTSTATUS status;

    TRACE( "(%p, %d)\n", info, size );

    if(size < sizeof(PERFORMANCE_INFORMATION))
    {
        SetLastError(ERROR_BAD_LENGTH);
        return FALSE;
    }

    memset(info, 0, sizeof(PERFORMANCE_INFORMATION));

    info->cb                = sizeof(PERFORMANCE_INFORMATION);
    info->PageSize          = system_info.PageSize;

    do {
        bufsize *= 2;
        HeapFree(GetProcessHeap(), 0, buf);
        buf = HeapAlloc(GetProcessHeap(), 0, bufsize);
        if (!buf)
            break;

        status = NtQuerySystemInformation(SystemProcessInformation, buf, bufsize, NULL);
    } while(status == STATUS_INFO_LENGTH_MISMATCH);

    if (status != STATUS_SUCCESS)
    {
        HeapFree(GetProcessHeap(), 0, buf);
    }

    spi = buf;
    while(spi) {
        info->ProcessCount++;
        info->ThreadCount += spi->dwThreadCount;
        info->HandleCount += spi->HandleCount;

        if (spi->NextEntryOffset == 0)
            break;

        spi = (SYSTEM_PROCESS_INFORMATION *)(((PCHAR)spi) + spi->NextEntryOffset);
    }

    HeapFree(GetProcessHeap(), 0, buf);

#ifdef linux
    f = fopen( "/proc/meminfo", "r" );
    if (f)
    {
        char buffer[256];
        unsigned long total, used, free, shared, buffers, cached, commit;

        while (fgets( buffer, sizeof(buffer), f ))
        {
            /* old style /proc/meminfo ... */
            if (sscanf( buffer, "Mem: %lu %lu %lu %lu %lu %lu",
                        &total, &used, &free, &shared, &buffers, &cached ))
            {
                info->CommitTotal += used / system_info.PageSize;
                info->PhysicalTotal += total / system_info.PageSize;
                info->CommitLimit += total / system_info.PageSize;
                info->PhysicalAvailable += (free + buffers + cached) / system_info.PageSize;
                info->SystemCache += cached / system_info.PageSize;
            }
            if (sscanf( buffer, "Swap: %lu %lu %lu", &total, &used, &free ))
            {
                info->CommitTotal += used / system_info.PageSize;
                info->CommitLimit += total / system_info.PageSize;
            }

            /* new style /proc/meminfo ... */
            if (sscanf(buffer, "MemTotal: %lu", &total))
                info->PhysicalTotal = total / (system_info.PageSize / 1024);
            if (sscanf(buffer, "MemFree: %lu", &free))
                info->PhysicalAvailable += free / (system_info.PageSize / 1024);
            if (sscanf(buffer, "CommitLimit: %lu", &commit))
                info->CommitLimit = commit / (system_info.PageSize / 1024);
            if (sscanf(buffer, "Committed_AS: %lu", &commit))
                info->CommitTotal = commit / (system_info.PageSize / 1024);
            if (sscanf(buffer, "Buffers: %lu", &buffers))
                info->PhysicalAvailable += buffers / (system_info.PageSize / 1024);
            if (sscanf(buffer, "Cached: %lu", &cached))
            {
                info->SystemCache = cached / (system_info.PageSize / 1024);
                info->PhysicalAvailable += cached / (system_info.PageSize / 1024);
            }
            if (sscanf(buffer, "Slab: %lu", &used) ||
                sscanf(buffer, "KernelStack: %lu", &used) ||
                sscanf(buffer, "PageTables: %lu", &used))
            {
                info->KernelTotal += used / (system_info.PageSize / 1024);
                info->KernelNonpaged += used / (system_info.PageSize / 1024);
            }
        }

        fclose(f);

        info->CommitPeak = commitpeak = max(commitpeak, info->CommitTotal);
        return TRUE;
    }
#endif

    /* not linux or /proc open failed */

    mem_status.dwLength = sizeof(mem_status);
    if (!GlobalMemoryStatusEx( &mem_status ))
        return FALSE;

    info->cb                = sizeof(PERFORMANCE_INFORMATION);
    info->CommitTotal       = min(((mem_status.ullTotalPageFile - mem_status.ullAvailPageFile) /
                                    system_info.PageSize), MAXDWORD);
    info->CommitLimit       = min((mem_status.ullTotalPageFile / system_info.PageSize), MAXDWORD);
    info->PhysicalTotal     = min((mem_status.ullTotalPhys / system_info.PageSize), MAXDWORD);
    info->PhysicalAvailable = min((mem_status.ullAvailPhys / system_info.PageSize), MAXDWORD);
    info->CommitPeak        = commitpeak = max(commitpeak, info->CommitTotal);

    FIXME("stub\n");

    return TRUE;
}
