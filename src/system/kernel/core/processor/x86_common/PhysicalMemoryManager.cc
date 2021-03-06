/*
 * Copyright (c) 2008 James Molloy, Jörg Pfähler, Matthew Iselin
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <Log.h>
#include <panic.h>
#include <utilities/utility.h>
#include <processor/MemoryRegion.h>
#include <processor/Processor.h>
#include "PhysicalMemoryManager.h"
#include <LockGuard.h>
#include <utilities/Cache.h>

#if defined(X86)
#include "../x86/VirtualAddressSpace.h"
#elif defined(X64)
#include "../x64/VirtualAddressSpace.h"
#include "../x64/utils.h"
#endif

#if defined(TRACK_PAGE_ALLOCATIONS)
#include <commands/AllocationCommand.h>
#endif

X86CommonPhysicalMemoryManager X86CommonPhysicalMemoryManager::m_Instance;

PhysicalMemoryManager &PhysicalMemoryManager::instance()
{
    return X86CommonPhysicalMemoryManager::instance();
}

physical_uintptr_t X86CommonPhysicalMemoryManager::allocatePage()
{
    m_Lock.acquire();

    physical_uintptr_t ptr;

    ptr = m_PageStack.allocate(0);
    if(!ptr)
    {
    /// \bug If caches are compacted, we end up with a massive deadlock somewhere
#if 0
        CacheManager::instance().compactAll();

        ptr = m_PageStack.allocate(0);
        if(!ptr)
        {
#endif
            m_Lock.release();
            FATAL_NOLOCK("Out of physical memory!");
#if 0
        }
#endif
    }
    
    m_Lock.release();

#if defined(TRACK_PAGE_ALLOCATIONS)             
    if (Processor::m_Initialised == 2)
    {
        if (!g_AllocationCommand.isMallocing())
        {
            g_AllocationCommand.allocatePage(ptr);
        }
    }
#endif

    return ptr;
}
void X86CommonPhysicalMemoryManager::freePage(physical_uintptr_t page)
{
    LockGuard<Spinlock> guard(m_Lock);

    m_PageStack.free(page);

#if defined(TRACK_PAGE_ALLOCATIONS)             
    if (Processor::m_Initialised == 2)
    {
        if (!g_AllocationCommand.isMallocing())
        {
            m_Lock.release();
            g_AllocationCommand.freePage(page);
            m_Lock.acquire();
        }
    }
#endif
}
void X86CommonPhysicalMemoryManager::freePageUnlocked(physical_uintptr_t page)
{
    if(!m_Lock.acquired())
        FATAL("X86CommonPhysicalMemoryManager::freePageUnlocked called without an acquired lock");

    m_PageStack.free(page);

    // g_AllocationCommand.freePage uses our lock.
    
#if 0 // defined(TRACK_PAGE_ALLOCATIONS)             
    if (Processor::m_Initialised == 2)
    {
        if (!g_AllocationCommand.isMallocing())
        {
            g_AllocationCommand.freePage(page);
        }
    }
#endif
}
bool X86CommonPhysicalMemoryManager::allocateRegion(MemoryRegion &Region,
                                                    size_t cPages,
                                                    size_t pageConstraints,
                                                    size_t Flags,
                                                    physical_uintptr_t start)
{
    LockGuard<Spinlock> guard(m_RegionLock);

    // Allocate a specific physical memory region (always physically continuous)
    if (start != static_cast<physical_uintptr_t>(-1))
    {
        if ((pageConstraints & continuous) != continuous)
            panic("PhysicalMemoryManager::allocateRegion(): function misused");

        // Remove the memory from the range-lists (if desired/possible)
        if ((pageConstraints & nonRamMemory) == nonRamMemory)
        {
            Region.setNonRamMemory(true);
            if (m_PhysicalRanges.allocateSpecific(start, cPages * getPageSize()) == false)
            {
                if ((pageConstraints & force) != force)
                    return false;
                else
                    Region.setForced(true);
            }
        }
        else
        {
            if (start < 0x100000 &&
                (start + cPages * getPageSize()) < 0x100000)
            {
                if (m_RangeBelow1MB.allocateSpecific(start, cPages * getPageSize()) == false)
                    return false;
            }
            else if (start < 0x1000000 &&
                     (start + cPages * getPageSize()) < 0x1000000)
            {
                if (m_RangeBelow16MB.allocateSpecific(start, cPages * getPageSize()) == false)
                    return false;
            }
            else if (start < 0x1000000)
            {
                ERROR("PhysicalMemoryManager: Memory region neither completely below nor above 1MB");
                return false;
            }
            else
            {
                // Ensure that free() does not attempt to free the given memory...
                Region.setNonRamMemory(true);
                Region.setForced(true);
            }
        }

        // Allocate the virtual address space
        uintptr_t vAddress;

        if (m_MemoryRegions.allocate(cPages * PhysicalMemoryManager::getPageSize(),
                                     vAddress)
            == false)
        {
            WARNING("AllocateRegion: MemoryRegion allocation failed.");
            return false;
        }

        // Map the physical memory into the allocated space
        VirtualAddressSpace &virtualAddressSpace =  Processor::information().getVirtualAddressSpace();
        for (size_t i = 0;i < cPages;i++)
            if (virtualAddressSpace.map(start + i * PhysicalMemoryManager::getPageSize(),
                                        reinterpret_cast<void*>(vAddress + i * PhysicalMemoryManager::getPageSize()),
                                        Flags)
                == false)
            {
                m_MemoryRegions.free(vAddress, cPages * PhysicalMemoryManager::getPageSize());
                WARNING("AllocateRegion: VirtualAddressSpace::map failed.");
                return false;
            }

        // Set the memory-region's members
        Region.m_VirtualAddress = reinterpret_cast<void*>(vAddress);
        Region.m_PhysicalAddress = start;
        Region.m_Size = cPages * PhysicalMemoryManager::getPageSize();
//       NOTICE("MR: Allocated " << Hex << vAddress << " (phys " << static_cast<uintptr_t>(start) << "), size " << (cPages*4096));

        // Add to the list of memory-regions
        PhysicalMemoryManager::m_MemoryRegions.pushBack(&Region);
        return true;
    }
    else
    {
        // If we need continuous memory, switch to below16 if not already
        if ((pageConstraints & continuous) == continuous)
            if ((pageConstraints & addressConstraints) != below1MB &&
                (pageConstraints & addressConstraints) != below16MB)
                pageConstraints = (pageConstraints & ~addressConstraints) | below16MB;

        // Allocate the virtual address space
        uintptr_t vAddress;
        if (m_MemoryRegions.allocate(cPages * PhysicalMemoryManager::getPageSize(),
                                     vAddress)
            == false)
        {
            WARNING("AllocateRegion: MemoryRegion allocation failed.");
            return false;
        }


        uint32_t start = 0;
        VirtualAddressSpace &virtualAddressSpace = Processor::information().getVirtualAddressSpace();

        if ((pageConstraints & addressConstraints) == below1MB ||
            (pageConstraints & addressConstraints) == below16MB)
        {
            // Allocate a range
            if ((pageConstraints & addressConstraints) == below1MB)
            {
                if (m_RangeBelow1MB.allocate(cPages * getPageSize(), start) == false)
                    return false;
            }
            else if ((pageConstraints & addressConstraints) == below16MB)
            {
                if (m_RangeBelow16MB.allocate(cPages * getPageSize(), start) == false)
                    return false;
            }

            // Map the physical memory into the allocated space
            for (size_t i = 0;i < cPages;i++)
                if (virtualAddressSpace.map(start + i * PhysicalMemoryManager::getPageSize(),
                                            reinterpret_cast<void*>(vAddress + i * PhysicalMemoryManager::getPageSize()),
                                            Flags)
                    == false)
                {
                    WARNING("AllocateRegion: VirtualAddressSpace::map failed.");
                    return false;
                }
        }
        else
        {
            // Map the physical memory into the allocated space
            for (size_t i = 0;i < cPages;i++)
            {
                physical_uintptr_t page = m_PageStack.allocate(pageConstraints & addressConstraints);
                if (virtualAddressSpace.map(page,
                                            reinterpret_cast<void*>(vAddress + i * PhysicalMemoryManager::getPageSize()),
                                            Flags)
                    == false)
                {
                    WARNING("AllocateRegion: VirtualAddressSpace::map failed.");
                    return false;
                }
            }
        }

        // Set the memory-region's members
        Region.m_VirtualAddress = reinterpret_cast<void*>(vAddress);
        Region.m_PhysicalAddress = start;
        Region.m_Size = cPages * PhysicalMemoryManager::getPageSize();

        // Add to the list of memory-regions
        PhysicalMemoryManager::m_MemoryRegions.pushBack(&Region);
        return true;
    }
    return false;
}

void X86CommonPhysicalMemoryManager::initialise(const BootstrapStruct_t &Info)
{
    NOTICE("memory-map:");

    // Fill the page-stack (usable memory above 16MB)
    // NOTE: We must do the page-stack first, because the range-lists already need the
    //       memory-management
    MemoryMapEntry_t *MemoryMap = reinterpret_cast<MemoryMapEntry_t*>(Info.mmap_addr);
    while (reinterpret_cast<uintptr_t>(MemoryMap) < (Info.mmap_addr + Info.mmap_length))
    {
        NOTICE(" " << Hex << MemoryMap->address << " - " << (MemoryMap->address + MemoryMap->length) << ", type: " << MemoryMap->type);

        if (MemoryMap->type == 1)
        {
            for (uint64_t i = MemoryMap->address;i < (MemoryMap->length + MemoryMap->address);i += getPageSize())
            {
                // Worry about regions > 4 GB once we've got regions under 4 GB completely done.
                // We can't do anything over 4 GB because the PageStack class uses
                // VirtualAddressSpace to map in its stack! Done in initialise64
                if(i >= 0x100000000ULL)
                    break;
                if (i >= 0x1000000)
                    m_PageStack.free(i);
            }
        }

        MemoryMap = adjust_pointer(MemoryMap, MemoryMap->size + 4);
    }

    // Fill the range-lists (usable memory below 1/16MB & ACPI)
    MemoryMap = reinterpret_cast<MemoryMapEntry_t*>(Info.mmap_addr);
    while (reinterpret_cast<uintptr_t>(MemoryMap) < (Info.mmap_addr + Info.mmap_length))
    {
        if (MemoryMap->type == 1)
        {
            if (MemoryMap->address < 0x100000)
            {
                // NOTE: Assumes that the entry/entries starting below 1MB don't cross the
                //       1MB barrier
                if ((MemoryMap->address + MemoryMap->length) >= 0x100000)
                    panic("PhysicalMemoryManager: strange memory-map");

                m_RangeBelow1MB.free(MemoryMap->address, MemoryMap->length);
            }
            else if (MemoryMap->address < 0x1000000)
            {
                uint64_t upperBound = MemoryMap->address + MemoryMap->length;
                if (upperBound >= 0x1000000)upperBound = 0x1000000;

                m_RangeBelow16MB.free(MemoryMap->address, upperBound - MemoryMap->address);
            }
        }
#if defined(ACPI)                                               
        else if (MemoryMap->type == 3 || MemoryMap->type == 4)
        {
            m_AcpiRanges.free(MemoryMap->address, MemoryMap->length);
        }
#endif

        MemoryMap = adjust_pointer(MemoryMap, MemoryMap->size + 4);
    }

    // Remove the pages used by the kernel from the range-list (below 16MB)
    extern void *init;
    extern void *end;
    if (m_RangeBelow16MB.allocateSpecific(reinterpret_cast<uintptr_t>(&init) - reinterpret_cast<uintptr_t>(KERNEL_VIRTUAL_ADDRESS),
                                          reinterpret_cast<uintptr_t>(&end) - reinterpret_cast<uintptr_t>(&init))
        == false)
    {
        panic("PhysicalMemoryManager: could not remove the kernel image from the range-list");
    }

    // Print the ranges
#if defined(VERBOSE_MEMORY_MANAGER)             
    NOTICE("free memory ranges (below 1MB):");
    for (size_t i = 0;i < m_RangeBelow1MB.size();i++)
        NOTICE(" " << Hex << m_RangeBelow1MB.getRange(i).address << " - " << (m_RangeBelow1MB.getRange(i).address + m_RangeBelow1MB.getRange(i).length));
    NOTICE("free memory ranges (below 16MB):");
    for (size_t i = 0;i < m_RangeBelow16MB.size();i++)
        NOTICE(" " << Hex << m_RangeBelow16MB.getRange(i).address << " - " << (m_RangeBelow16MB.getRange(i).address + m_RangeBelow16MB.getRange(i).length));
#if defined(ACPI)                               
    NOTICE("ACPI ranges:");
    for (size_t i = 0;i < m_AcpiRanges.size();i++)
        NOTICE(" " << Hex << m_AcpiRanges.getRange(i).address << " - " << (m_AcpiRanges.getRange(i).address + m_AcpiRanges.getRange(i).length));
#endif
#endif

    // Initialise the free physical ranges
    m_PhysicalRanges.free(0, 0x100000000ULL);
    MemoryMap = reinterpret_cast<MemoryMapEntry_t*>(Info.mmap_addr);
    while (reinterpret_cast<uintptr_t>(MemoryMap) < (Info.mmap_addr + Info.mmap_length))
    {
        // Only map if the variable fits into a uintptr_t - no overflow!
        if((MemoryMap->address) > ((uintptr_t) -1))
        {
            WARNING("Memory region " << MemoryMap->address << " not used.");
        }
        else if(MemoryMap->address >= 0x100000000ULL)
        {
            // Skip >= 4 GB for now, done in initialise64
            break;
        }
        else if (m_PhysicalRanges.allocateSpecific(MemoryMap->address, MemoryMap->length) == false)
            panic("PhysicalMemoryManager: Failed to create the list of ranges of free physical space");

        MemoryMap = adjust_pointer(MemoryMap, MemoryMap->size + 4);
    }

    // Print the ranges
#if defined(VERBOSE_MEMORY_MANAGER)             
    NOTICE("physical memory ranges:");
    for (size_t i = 0;i < m_PhysicalRanges.size();i++)
    {
        NOTICE(" " << Hex << m_PhysicalRanges.getRange(i).address << " - " << (m_PhysicalRanges.getRange(i).address + m_PhysicalRanges.getRange(i).length));
    }
#endif

    // Initialise the range of virtual space for MemoryRegions
    m_MemoryRegions.free(reinterpret_cast<uintptr_t>(KERNEL_VIRTUAL_MEMORYREGION_ADDRESS),
                         KERNEL_VIRTUAL_MEMORYREGION_SIZE);
}
#ifdef X64
void X86CommonPhysicalMemoryManager::initialise64(const BootstrapStruct_t &Info)
{
    NOTICE("64-bit memory-map:");

    // Fill the page-stack (usable memory above 16MB)
    // NOTE: We must do the page-stack first, because the range-lists already need the
    //       memory-management
    MemoryMapEntry_t *MemoryMap = reinterpret_cast<MemoryMapEntry_t*>(Info.mmap_addr);
    while (reinterpret_cast<uintptr_t>(MemoryMap) < (Info.mmap_addr + Info.mmap_length))
    {
        if(MemoryMap->address >= 0x100000000ULL)
        {
            NOTICE(" " << Hex << MemoryMap->address << " - " << (MemoryMap->address + MemoryMap->length) << ", type: " << MemoryMap->type);

            if (MemoryMap->type == 1)
            {
                for (uint64_t i = MemoryMap->address;i < (MemoryMap->length + MemoryMap->address);i += getPageSize())
                {
                    m_PageStack.free(i);
                }

                m_PhysicalRanges.free(MemoryMap->address, MemoryMap->length);
            }
        }
        
        MemoryMap = adjust_pointer(MemoryMap, MemoryMap->size + 4);
    }

    // Fill the range-lists (usable memory below 1/16MB & ACPI)
#if defined(ACPI)
    MemoryMap = reinterpret_cast<MemoryMapEntry_t*>(Info.mmap_addr);
    while (reinterpret_cast<uintptr_t>(MemoryMap) < (Info.mmap_addr + Info.mmap_length))
    {
        if ((MemoryMap->type == 3 || MemoryMap->type == 4) && MemoryMap->address >= 0x100000000ULL)
        {
            m_AcpiRanges.free(MemoryMap->address, MemoryMap->length);
        }

        MemoryMap = adjust_pointer(MemoryMap, MemoryMap->size + 4);
    }
    
#if defined(VERBOSE_MEMORY_MANAGER)
    // Print the ranges
    NOTICE("ACPI ranges (x64 added):");
    for (size_t i = 0;i < m_AcpiRanges.size();i++)
        NOTICE(" " << Hex << m_AcpiRanges.getRange(i).address << " - " << (m_AcpiRanges.getRange(i).address + m_AcpiRanges.getRange(i).length));
#endif
#endif

    // Initialise the free physical ranges
    MemoryMap = reinterpret_cast<MemoryMapEntry_t*>(Info.mmap_addr);
    while (reinterpret_cast<uintptr_t>(MemoryMap) < (Info.mmap_addr + Info.mmap_length))
    {
        // Only map if the variable fits into a uintptr_t - no overflow!
        if((MemoryMap->address) > ((uintptr_t) -1))
        {
            WARNING("Memory region " << MemoryMap->address << " not used.");
        }
        else if ((MemoryMap->address >= 0x100000000ULL) && (m_PhysicalRanges.allocateSpecific(MemoryMap->address, MemoryMap->length) == false))
            panic("PhysicalMemoryManager: Failed to create the list of ranges of free physical space");

        MemoryMap = adjust_pointer(MemoryMap, MemoryMap->size + 4);
    }

    // Print the ranges
#if defined(VERBOSE_MEMORY_MANAGER)             
    NOTICE("physical memory ranges, 64-bit added:");
    for (size_t i = 0;i < m_PhysicalRanges.size();i++)
    {
        NOTICE(" " << Hex << m_PhysicalRanges.getRange(i).address << " - " << (m_PhysicalRanges.getRange(i).address + m_PhysicalRanges.getRange(i).length));
    }
#endif
}
#endif

void X86CommonPhysicalMemoryManager::initialisationDone()
{
    extern void *init;
    extern void *code;

    m_Lock.acquire();

    // Unmap & free the .init section
    VirtualAddressSpace &kernelSpace = VirtualAddressSpace::getKernelAddressSpace();
    size_t count = (reinterpret_cast<uintptr_t>(&code) - reinterpret_cast<uintptr_t>(&init)) / getPageSize();
    for (size_t i = 0;i < count;i++)
    {
        void *vAddress = adjust_pointer(reinterpret_cast<void*>(&init), i * getPageSize());

        // Get the physical address
        size_t flags;
        physical_uintptr_t pAddress;
        kernelSpace.getMapping(vAddress, pAddress, flags);

        // Unmap the page
        kernelSpace.unmap(vAddress);
    }

    // Free the physical page
    m_RangeBelow16MB.free(reinterpret_cast<uintptr_t>(&init) - reinterpret_cast<uintptr_t>(KERNEL_VIRTUAL_ADDRESS), count * getPageSize());
    m_Lock.release();
}

X86CommonPhysicalMemoryManager::X86CommonPhysicalMemoryManager()
    : m_PageStack(), m_RangeBelow1MB(), m_RangeBelow16MB(), m_PhysicalRanges(),
#if defined(ACPI)                               
      m_AcpiRanges(),
#endif                                              
      m_MemoryRegions(), m_Lock(false, true), m_RegionLock(false, true)
{
}
X86CommonPhysicalMemoryManager::~X86CommonPhysicalMemoryManager()
{
}

void X86CommonPhysicalMemoryManager::unmapRegion(MemoryRegion *pRegion)
{
    LockGuard<Spinlock> guard(m_RegionLock);

    for (Vector<MemoryRegion*>::Iterator it = PhysicalMemoryManager::m_MemoryRegions.begin();
         it != PhysicalMemoryManager::m_MemoryRegions.end();
         it++)
    {
        if (*it == pRegion)
        {

            size_t cPages = pRegion->size() / PhysicalMemoryManager::getPageSize();
            uintptr_t start = reinterpret_cast<uintptr_t> (pRegion->virtualAddress());
            physical_uintptr_t phys = pRegion->physicalAddress();
            VirtualAddressSpace &virtualAddressSpace = VirtualAddressSpace::getKernelAddressSpace();

            if (pRegion->getNonRamMemory())
            {
                if (!pRegion->getForced())
                    m_PhysicalRanges.free(phys, pRegion->size());
            }
            else
            {
                if (phys < 0x100000 &&
                    (phys + cPages * getPageSize()) < 0x100000)
                {
                    m_RangeBelow1MB.free(phys, cPages * getPageSize());
                }
                else if (phys < 0x1000000 &&
                         (phys + cPages * getPageSize()) < 0x1000000)
                {
                    m_RangeBelow16MB.free(phys, cPages * getPageSize());
                }
                else if (phys < 0x1000000)
                {
                    ERROR("PhysicalMemoryManager: Memory region neither completely below nor above 1MB");
                    return;
                }
            }

            for (size_t i = 0;i < cPages;i++)
            {
                void *vAddr = reinterpret_cast<void*> (start + i * PhysicalMemoryManager::getPageSize());
                if (!virtualAddressSpace.isMapped(vAddr))
                {
                    FATAL("Algorithmic error in PhysicalMemoryManager::unmapRegion");
                }
                physical_uintptr_t pAddr;
                size_t flags;
                virtualAddressSpace.getMapping(vAddr, pAddr, flags);

                if (!pRegion->getNonRamMemory() && pAddr > 0x1000000)
                    m_PageStack.free(pAddr);
                
                virtualAddressSpace.unmap(vAddr);
            }
//            NOTICE("MR: Freed " << Hex << start << ", size " << (cPages*4096));
            m_MemoryRegions.free(start, pRegion->size());
            PhysicalMemoryManager::m_MemoryRegions.erase(it);
            break;
        }
    }
}

size_t g_FreePages = 0;
size_t g_AllocedPages = 0;
physical_uintptr_t X86CommonPhysicalMemoryManager::PageStack::allocate(size_t constraints)
{
    size_t index = 0;
#if defined(X64)                                                    
    if (constraints == X86CommonPhysicalMemoryManager::below4GB)
    index = 0;
    else if (constraints == X86CommonPhysicalMemoryManager::below64GB)
        index = 1;
    else
        index = 2;

    if (index == 2 && m_StackMax[2] == m_StackSize[2])
        index = 1;
    if (index == 1 && m_StackMax[1] == m_StackSize[1])
        index = 0;
#endif

    physical_uintptr_t result = 0;
    if ( (m_StackMax[index] != m_StackSize[index]) && m_StackSize[index])
    {
        if (index == 0)
        {
            m_StackSize[0] -= 4;
            result = *(reinterpret_cast<uint32_t*>(m_Stack[0]) + m_StackSize[0] / 4);
            /// \note Testing.
            g_FreePages --;
            g_AllocedPages ++;
        }
        else
        {
            m_StackSize[index] -= 8;
            result = *(reinterpret_cast<uint64_t*>(m_Stack[index]) + m_StackSize[index] / 8);
        }
    }
    return result;
}
void X86CommonPhysicalMemoryManager::PageStack::free(uint64_t physicalAddress)
{
    // Select the right stack
    size_t index = 0;
    if (physicalAddress >= 0x100000000ULL)
    {
#if defined(X86)
        return;
#elif defined(X64)
        index = 1;
        if (physicalAddress >= 0x1000000000ULL)
            index = 2;
#endif                                          
    }
        // Don't attempt to map address zero.
        if(!m_Stack[index])
            return;

        // Expand the stack if necessary
        if (m_StackMax[index] == m_StackSize[index])
        {
            // Get the kernel virtual address-space
#if defined(X86)                                                        
            X86VirtualAddressSpace &AddressSpace = static_cast<X86VirtualAddressSpace&>(VirtualAddressSpace::getKernelAddressSpace());
#elif defined(X64)                                                      
            X64VirtualAddressSpace &AddressSpace = static_cast<X64VirtualAddressSpace&>(VirtualAddressSpace::getKernelAddressSpace());
#endif

            if(!index)
            {
                if (AddressSpace.mapPageStructures(physicalAddress,
                                                   adjust_pointer(m_Stack[index], m_StackMax[index]),
                                                   VirtualAddressSpace::KernelMode | VirtualAddressSpace::Write)
                    == true)
                    return;
            }
            else
            {
#if defined(X64)
                if (AddressSpace.mapPageStructuresAbove4GB(physicalAddress,
                                                   adjust_pointer(m_Stack[index], m_StackMax[index]),
                                                   VirtualAddressSpace::KernelMode | VirtualAddressSpace::Write)
                    == true)
                    return;
#else
                FATAL("PageStack::free - index > 0 when not built as x86_64");
#endif
            }
 
            m_StackMax[index] += getPageSize();
        }

        if (index == 0)
        {
            *(reinterpret_cast<uint32_t*>(m_Stack[0]) + m_StackSize[0] / 4) = static_cast<uint32_t>(physicalAddress);
            m_StackSize[0] += 4;
            /// \note Testing.
            g_FreePages ++;
            if (g_AllocedPages > 0)
                g_AllocedPages --;
        }
        else
        {
            *(reinterpret_cast<uint64_t*>(m_Stack[index]) + m_StackSize[index] / 8) = physicalAddress;
            m_StackSize[index] += 8;
        }
    }
    X86CommonPhysicalMemoryManager::PageStack::PageStack()
    {
        for (size_t i = 0;i < StackCount;i++)
        {
            m_StackMax[i] = 0;
            m_StackSize[i] = 0;
        }

        // Set the locations for the page stacks in the virtual address space
        m_Stack[0] = KERNEL_VIRTUAL_PAGESTACK_4GB;
#if defined(X64)
        m_Stack[1] = KERNEL_VIRTUAL_PAGESTACK_ABV4GB1;
        m_Stack[2] = KERNEL_VIRTUAL_PAGESTACK_ABV4GB2;
#endif
    }
