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

#include <panic.h>
#include <utilities/utility.h>
#include <processor/Processor.h>
#include <processor/PhysicalMemoryManager.h>
#include <process/Scheduler.h>
#include <process/Process.h>
#include <LockGuard.h>
#include "VirtualAddressSpace.h"

#include <Log.h>

//
// Page Table/Directory entry flags
//
#define PAGE_PRESENT                0x01
#define PAGE_WRITE                  0x02
#define PAGE_USER                   0x04
#define PAGE_WRITE_THROUGH          0x08
#define PAGE_CACHE_DISABLE          0x10
#define PAGE_4MB                    0x80
#define PAGE_GLOBAL                 0x100
#define PAGE_SWAPPED                0x200
#define PAGE_COPY_ON_WRITE          0x400

//
// Macros
//
#define PAGE_DIRECTORY_INDEX(x) ((reinterpret_cast<uintptr_t>(x) >> 22) & 0x3FF)
#define PAGE_TABLE_INDEX(x) ((reinterpret_cast<uintptr_t>(x) >> 12) & 0x3FF)

#define PAGE_DIRECTORY_ENTRY(pageDir, index) (&reinterpret_cast<uint32_t*>(pageDir)[index])
#define PAGE_TABLE_ENTRY(VirtualPageTables, pageDirectoryIndex, index) (&reinterpret_cast<uint32_t*>(adjust_pointer(VirtualPageTables, pageDirectoryIndex * 4096))[index])

#define PAGE_GET_FLAGS(x) (*x & 0xFFF)
#define PAGE_SET_FLAGS(x, f) *x = (*x & ~0xFFF) | f
#define PAGE_GET_PHYSICAL_ADDRESS(x) (*x & ~0xFFF)

// Defined in boot-standalone.s
extern void *pagedirectory;

/** Array of free pages, used during the mapping algorithms in case a new page
    table needs to be mapped, which must be done without relinquishing the lock
    (which means we can't call the PMM!)

    There is one page per processor. */
physical_uintptr_t g_EscrowPages[256]; /// \todo MAX_PROCESSORS

X86KernelVirtualAddressSpace X86KernelVirtualAddressSpace::m_Instance;

VirtualAddressSpace *g_pCurrentlyCloning = 0;

VirtualAddressSpace &VirtualAddressSpace::getKernelAddressSpace()
{
  return X86KernelVirtualAddressSpace::m_Instance;
}

VirtualAddressSpace *VirtualAddressSpace::create()
{
  return new X86VirtualAddressSpace();
}

bool X86VirtualAddressSpace::memIsInHeap(void *pMem)
{
    if(pMem < KERNEL_VIRTUAL_HEAP)
        return false;
    else if(pMem >= getEndOfHeap())
        return false;
    else
        return true;
}
void *X86VirtualAddressSpace::getEndOfHeap()
{
    return reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(KERNEL_VIRTUAL_HEAP) + KERNEL_VIRTUAL_HEAP_SIZE);
}

bool X86VirtualAddressSpace::isAddressValid(void *virtualAddress)
{
  return true;
}
bool X86VirtualAddressSpace::isMapped(void *virtualAddress)
{
  #if defined(ADDITIONAL_CHECKS)
    if (Processor::readCr3() != m_PhysicalPageDirectory)
      panic("VirtualAddressSpace::isMapped(): not in this VirtualAddressSpace");
  #endif

  return doIsMapped(virtualAddress);
}
bool X86VirtualAddressSpace::map(physical_uintptr_t physicalAddress,
                                 void *virtualAddress,
                                 size_t flags)
{
  #if defined(ADDITIONAL_CHECKS)
    if (Processor::readCr3() != m_PhysicalPageDirectory)
      panic("VirtualAddressSpace::map(): not in this VirtualAddressSpace");
  #endif

  return doMap(physicalAddress, virtualAddress, flags);
}
void X86VirtualAddressSpace::getMapping(void *virtualAddress,
                                        physical_uintptr_t &physicalAddress,
                                        size_t &flags)
{
  #if defined(ADDITIONAL_CHECKS)
    if (Processor::readCr3() != m_PhysicalPageDirectory)
      panic("VirtualAddressSpace::getMapping(): not in this VirtualAddressSpace");
  #endif

  doGetMapping(virtualAddress, physicalAddress, flags);
}
void X86VirtualAddressSpace::setFlags(void *virtualAddress, size_t newFlags)
{
  #if defined(ADDITIONAL_CHECKS)
    if (Processor::readCr3() != m_PhysicalPageDirectory)
      panic("VirtualAddressSpace::setFlags(): not in this VirtualAddressSpace");
  #endif

  doSetFlags(virtualAddress, newFlags);
}
void X86VirtualAddressSpace::unmap(void *virtualAddress)
{
  #if defined(ADDITIONAL_CHECKS)
    if (Processor::readCr3() != m_PhysicalPageDirectory)
      panic("VirtualAddressSpace::unmap(): not in this VirtualAddressSpace");
  #endif

  doUnmap(virtualAddress);
}
void *X86VirtualAddressSpace::allocateStack()
{
    void *st  = doAllocateStack(USERSPACE_VIRTUAL_MAX_STACK_SIZE);

    return st;
}
void *X86VirtualAddressSpace::allocateStack(size_t stackSz)
{
    if(stackSz == 0)
        stackSz = USERSPACE_VIRTUAL_MAX_STACK_SIZE;
    void *st  = doAllocateStack(stackSz);

    return st;
}
void X86VirtualAddressSpace::freeStack(void *pStack)
{
  // Add the stack to the list
  m_freeStacks.pushBack(pStack);
}

bool X86VirtualAddressSpace::mapPageStructures(physical_uintptr_t physicalAddress,
                                               void *virtualAddress,
                                               size_t flags)
{
  LockGuard<Spinlock> guard(m_Lock);

  size_t pageDirectoryIndex = PAGE_DIRECTORY_INDEX(virtualAddress);
  uint32_t *pageDirectoryEntry = PAGE_DIRECTORY_ENTRY(m_VirtualPageDirectory, pageDirectoryIndex);

  // Page table present?
  if ((*pageDirectoryEntry & PAGE_PRESENT) != PAGE_PRESENT)
  {
    // TODO: Map the page table into all other address spaces
    *pageDirectoryEntry = physicalAddress | toFlags(flags);

    // Zero the page table
    memset(PAGE_TABLE_ENTRY(m_VirtualPageTables, pageDirectoryIndex, 0),
           0,
           PhysicalMemoryManager::getPageSize());

    return true;
  }
  else
  {
    size_t pageTableIndex = PAGE_TABLE_INDEX(virtualAddress);
    uint32_t *pageTableEntry = PAGE_TABLE_ENTRY(m_VirtualPageTables, pageDirectoryIndex, pageTableIndex);

    // Page frame present?
    if ((*pageTableEntry & PAGE_PRESENT) != PAGE_PRESENT)
    {
      *pageTableEntry = physicalAddress | toFlags(flags);
      return true;
    }
  }
  return false;
}

X86VirtualAddressSpace::~X86VirtualAddressSpace()
{
  PhysicalMemoryManager &physicalMemoryManager = PhysicalMemoryManager::instance();
  VirtualAddressSpace &VAddressSpace = Processor::information().getVirtualAddressSpace();

  // Switch to this virtual address space
  Processor::switchAddressSpace(*this);

  // Get the page table used to map this page directory into the address space
  physical_uintptr_t pageTable = PAGE_GET_PHYSICAL_ADDRESS( PAGE_DIRECTORY_ENTRY(VIRTUAL_PAGE_DIRECTORY, 0x3FE) );

  // Switch to the original virtual address space
  Processor::switchAddressSpace(VAddressSpace);

  // TODO: Free other things, perhaps in VirtualAddressSpace
  //       We can't do this in VirtualAddressSpace destructor though!

  // Free the page table used to map the page directory into the address space and the page directory itself
  physicalMemoryManager.freePage(pageTable);
  physicalMemoryManager.freePage(m_PhysicalPageDirectory);
}

X86VirtualAddressSpace::X86VirtualAddressSpace()
  : VirtualAddressSpace(USERSPACE_VIRTUAL_HEAP), m_PhysicalPageDirectory(0), m_VirtualPageDirectory(VIRTUAL_PAGE_DIRECTORY),
    m_VirtualPageTables(VIRTUAL_PAGE_TABLES), m_pStackTop(USERSPACE_VIRTUAL_STACK), m_freeStacks(), m_Lock(false, true)
{
  // Allocate a new page directory
  PhysicalMemoryManager &physicalMemoryManager = PhysicalMemoryManager::instance();
  m_PhysicalPageDirectory = physicalMemoryManager.allocatePage();
  physical_uintptr_t pageTable = physicalMemoryManager.allocatePage();

  // Get the current address space
  VirtualAddressSpace &virtualAddressSpace = Processor::information().getVirtualAddressSpace();

  // Map the page directory and page table into the address space (at a temporary location)
  virtualAddressSpace.map(m_PhysicalPageDirectory,
                          KERNEL_VIRTUAL_TEMP1,
                          VirtualAddressSpace::Write | VirtualAddressSpace::KernelMode);
  virtualAddressSpace.map(pageTable,
                          KERNEL_VIRTUAL_TEMP2,
                          VirtualAddressSpace::Write | VirtualAddressSpace::KernelMode);

  // Initialise the page directory
  memset(KERNEL_VIRTUAL_TEMP1,
         0,
         0xC00);

  // Initialise the page table
  memset(KERNEL_VIRTUAL_TEMP2,
         0,
         0x1000);

  // Copy the kernel address space to the new address space
  memcpy(adjust_pointer(KERNEL_VIRTUAL_TEMP1, 0xC00),
         adjust_pointer(X86KernelVirtualAddressSpace::m_Instance.m_VirtualPageDirectory, 0xC00),
         0x3F8);

  // Map the page tables into the new address space
  *reinterpret_cast<uint32_t*>(adjust_pointer(KERNEL_VIRTUAL_TEMP1, 0xFFC)) = m_PhysicalPageDirectory | PAGE_PRESENT | PAGE_WRITE;

  // Map the page directory into the new address space
  *reinterpret_cast<uint32_t*>(adjust_pointer(KERNEL_VIRTUAL_TEMP1, 0xFF8)) = pageTable | PAGE_PRESENT | PAGE_WRITE;
  *reinterpret_cast<uint32_t*>(adjust_pointer(KERNEL_VIRTUAL_TEMP2, 0xFFC)) = m_PhysicalPageDirectory | PAGE_PRESENT | PAGE_WRITE;

  // Unmap the page directory and page table from the address space (from the temporary location)
  virtualAddressSpace.unmap(KERNEL_VIRTUAL_TEMP1);
  virtualAddressSpace.unmap(KERNEL_VIRTUAL_TEMP2);
}

X86VirtualAddressSpace::X86VirtualAddressSpace(void *Heap,
                                               physical_uintptr_t PhysicalPageDirectory,
                                               void *VirtualPageDirectory,
                                               void *VirtualPageTables,
                                               void *VirtualStack)
  : VirtualAddressSpace(Heap), m_PhysicalPageDirectory(PhysicalPageDirectory),
    m_VirtualPageDirectory(VirtualPageDirectory), m_VirtualPageTables(VirtualPageTables),
    m_pStackTop(VirtualStack), m_freeStacks(), m_Lock(false, true)
{
}

bool X86VirtualAddressSpace::doIsMapped(void *virtualAddress)
{
#ifndef TRACK_LOCKS
  LockGuard<Spinlock> guard(m_Lock);
#endif

  virtualAddress = reinterpret_cast<void*> (reinterpret_cast<uintptr_t>(virtualAddress) & ~0xFFF);

  size_t pageDirectoryIndex = PAGE_DIRECTORY_INDEX(virtualAddress);
  uint32_t *pageDirectoryEntry = PAGE_DIRECTORY_ENTRY(m_VirtualPageDirectory, pageDirectoryIndex);

  // Is a page table or 4MB page present?
  if ((*pageDirectoryEntry & PAGE_PRESENT) != PAGE_PRESENT)
    return false;

  // Is it a 4MB page?
  if ((*pageDirectoryEntry & PAGE_4MB) == PAGE_4MB)
    return true;

  size_t pageTableIndex = PAGE_TABLE_INDEX(virtualAddress);
  uint32_t *pageTableEntry = PAGE_TABLE_ENTRY(m_VirtualPageTables, pageDirectoryIndex, pageTableIndex);

  // Is a page present?
  return ((*pageTableEntry & PAGE_PRESENT) == PAGE_PRESENT);
}
bool X86VirtualAddressSpace::doMap(physical_uintptr_t physicalAddress,
                                   void *virtualAddress,
                                   size_t flags)
{
  // Check if we have an allocated escrow page - if we don't, allocate it.
  if (g_EscrowPages[Processor::id()] == 0)
  {
    g_EscrowPages[Processor::id()] = PhysicalMemoryManager::instance().allocatePage();
    if (g_EscrowPages[Processor::id()] == 0)
    {
      // Still 0, we have problems.
      FATAL("Out of memory");
    }
  }

  LockGuard<Spinlock> guard(m_Lock);

  size_t Flags = toFlags(flags);
  size_t pageDirectoryIndex = PAGE_DIRECTORY_INDEX(virtualAddress);
  uint32_t *pageDirectoryEntry = PAGE_DIRECTORY_ENTRY(m_VirtualPageDirectory, pageDirectoryIndex);

  // Is a page table present?
  if ((*pageDirectoryEntry & PAGE_PRESENT) != PAGE_PRESENT)
  {
    // We need a page, but calling the PMM could cause reentrancy issues. We
    // use our alotted page in the escrow cache then set it to zero so that
    // it will be replenished next time it needs to be used.
    uint32_t page = g_EscrowPages[Processor::id()];
    g_EscrowPages[Processor::id()] = 0;

    // Map the page
    *pageDirectoryEntry = page | PAGE_USER | ((Flags & ~(PAGE_GLOBAL | PAGE_SWAPPED | PAGE_COPY_ON_WRITE)) | PAGE_WRITE);

    // Zero the page table
    memset(PAGE_TABLE_ENTRY(m_VirtualPageTables, pageDirectoryIndex, 0),
           0,
           PhysicalMemoryManager::getPageSize());

    // If we map within the kernel space, we need to add this page table to the
    // other address spaces!
    //
    // Also, we don't want to do this if the processor isn't initialised...
    VirtualAddressSpace &VAS = Processor::information().getVirtualAddressSpace();
    if (Processor::m_Initialised == 2 && virtualAddress >= KERNEL_SPACE_START)
    {
        for (size_t i = 0; i < Scheduler::instance().getNumProcesses(); i++)
        {
            Process *p = Scheduler::instance().getProcess(i);

            X86VirtualAddressSpace *x86VAS = reinterpret_cast<X86VirtualAddressSpace*> (p->getAddressSpace());
            if (x86VAS == &VAS)
                continue;

            Processor::switchAddressSpace(*p->getAddressSpace());

            pageDirectoryEntry = PAGE_DIRECTORY_ENTRY(x86VAS->m_VirtualPageDirectory, pageDirectoryIndex);
            *pageDirectoryEntry = page | PAGE_WRITE | PAGE_USER | (Flags & ~(PAGE_GLOBAL | PAGE_SWAPPED | PAGE_COPY_ON_WRITE));
        }
        if (g_pCurrentlyCloning)
        {
            X86VirtualAddressSpace *x86VAS = reinterpret_cast<X86VirtualAddressSpace*> (g_pCurrentlyCloning);
            if (x86VAS != &VAS)
            {
                Processor::switchAddressSpace(*g_pCurrentlyCloning);

                pageDirectoryEntry = PAGE_DIRECTORY_ENTRY(x86VAS->m_VirtualPageDirectory, pageDirectoryIndex);
                *pageDirectoryEntry = page | PAGE_WRITE | PAGE_USER | (Flags & ~(PAGE_GLOBAL | PAGE_SWAPPED | PAGE_COPY_ON_WRITE));
            }
        }
        if (&VAS != &getKernelAddressSpace())
        {
            Processor::switchAddressSpace(getKernelAddressSpace());
            X86VirtualAddressSpace *x86VAS = reinterpret_cast<X86VirtualAddressSpace*> (&getKernelAddressSpace());

            pageDirectoryEntry = PAGE_DIRECTORY_ENTRY(x86VAS->m_VirtualPageDirectory, pageDirectoryIndex);
            *pageDirectoryEntry = page | PAGE_WRITE | PAGE_USER | (Flags & ~(PAGE_GLOBAL | PAGE_SWAPPED | PAGE_COPY_ON_WRITE));

        }
        Processor::switchAddressSpace(VAS);
    }

  }
  // The other corner case is when a table has been used for the kernel before
  // but is now being mapped as USER mode. We need to ensure that the
  // directory entry has the USER flags set.
  else if ( (Flags & PAGE_USER) && ((*pageDirectoryEntry & PAGE_USER) != PAGE_USER))
  {
    *pageDirectoryEntry |= PAGE_USER;
  }

  size_t pageTableIndex = PAGE_TABLE_INDEX(virtualAddress);
  uint32_t *pageTableEntry = PAGE_TABLE_ENTRY(m_VirtualPageTables, pageDirectoryIndex, pageTableIndex);

  // Is a page already present
  if ((*pageTableEntry & PAGE_PRESENT) == PAGE_PRESENT)
    return false;

  // Map the page
  *pageTableEntry = physicalAddress | Flags;

  // Flush the TLB (if we are marking a page as swapped-out)
  if ((Flags & PAGE_SWAPPED) == PAGE_SWAPPED)
    Processor::invalidate(virtualAddress);

  return true;
}
void X86VirtualAddressSpace::doGetMapping(void *virtualAddress,
                                          physical_uintptr_t &physicalAddress,
                                          size_t &flags)
{
  // Get a pointer to the page-table entry (Also checks whether the page is actually present
  // or marked swapped out)
  uint32_t *pageTableEntry = 0;
  if (getPageTableEntry(virtualAddress, pageTableEntry) == false)
    panic("VirtualAddressSpace::getMapping(): function misused");

  // Extract the physical address and the flags
  physicalAddress = PAGE_GET_PHYSICAL_ADDRESS(pageTableEntry);
  flags = fromFlags(PAGE_GET_FLAGS(pageTableEntry));
}
void X86VirtualAddressSpace::doSetFlags(void *virtualAddress, size_t newFlags)
{
  LockGuard<Spinlock> guard(m_Lock);

  // Get a pointer to the page-table entry (Also checks whether the page is actually present
  // or marked swapped out)
  uint32_t *pageTableEntry = 0;
  if (getPageTableEntry(virtualAddress, pageTableEntry) == false)
    panic("VirtualAddressSpace::setFlags(): function misused");

  // Set the flags
  PAGE_SET_FLAGS(pageTableEntry, toFlags(newFlags));

  // TODO: Might need a TLB flush
}
void X86VirtualAddressSpace::doUnmap(void *virtualAddress)
{
  LockGuard<Spinlock> guard(m_Lock);

  // Get a pointer to the page-table entry (Also checks whether the page is actually present
  // or marked swapped out)
  uint32_t *pageTableEntry = 0;
  if (getPageTableEntry(virtualAddress, pageTableEntry) == false)
    panic("VirtualAddressSpace::unmap(): function misused");

  // Invalidate the TLB entry
  Processor::invalidate(virtualAddress);

  // Unmap the page
  *pageTableEntry = 0;
}
void *X86VirtualAddressSpace::doAllocateStack(size_t sSize)
{
  // LockGuard<Spinlock> guard(m_Lock);
    m_Lock.acquire();

  // Get a virtual address for the stack
  void *pStack = 0;
  if (m_freeStacks.count() != 0)
  {
    pStack = m_freeStacks.popBack();

    m_Lock.release();
  }
  else
  {
    pStack = m_pStackTop;
    m_pStackTop = adjust_pointer(m_pStackTop, -sSize);

    m_Lock.release();

    // Map in the top page, then everything else will be demand paged
    uintptr_t stackBottom = reinterpret_cast<uintptr_t>(pStack) - sSize;
    for(size_t j = 0; j < sSize; j += 0x1000)
    {
        physical_uintptr_t phys = PhysicalMemoryManager::instance().allocatePage();
        bool b = map(phys,
                 reinterpret_cast<void*>(stackBottom + j),
                 VirtualAddressSpace::Write);
        if (!b)
            WARNING("map() failed in doAllocateStack");
    }
  }
  return pStack;
}

bool X86VirtualAddressSpace::getPageTableEntry(void *virtualAddress,
                                               uint32_t *&pageTableEntry)
{
  // Not a public-facing function - locking shouldn't be needed.

  size_t pageDirectoryIndex = PAGE_DIRECTORY_INDEX(virtualAddress);
  uint32_t *pageDirectoryEntry = PAGE_DIRECTORY_ENTRY(m_VirtualPageDirectory, pageDirectoryIndex);

  // Is a page table or 4MB page present?
  if ((*pageDirectoryEntry & PAGE_PRESENT) != PAGE_PRESENT)
    return false;
  if ((*pageDirectoryEntry & PAGE_4MB) == PAGE_4MB)
    return false;

  size_t pageTableIndex = PAGE_TABLE_INDEX(virtualAddress);
  pageTableEntry = PAGE_TABLE_ENTRY(m_VirtualPageTables, pageDirectoryIndex, pageTableIndex);

  // Is a page present?
  if ((*pageTableEntry & PAGE_PRESENT) != PAGE_PRESENT &&
      (*pageTableEntry & PAGE_SWAPPED) != PAGE_SWAPPED)
    return false;

  return true;
}

uint32_t X86VirtualAddressSpace::toFlags(size_t flags)
{
  uint32_t Flags = 0;
  if ((flags & KernelMode) == KernelMode)
    Flags |= PAGE_GLOBAL;
  else
    Flags |= PAGE_USER;
  if ((flags & Write) == Write)
    Flags |= PAGE_WRITE;
  if ((flags & WriteThrough) == WriteThrough)
    Flags |= PAGE_WRITE_THROUGH;
  if ((flags & CacheDisable) == CacheDisable)
    Flags |= PAGE_CACHE_DISABLE;
  if ((flags & Swapped) == Swapped)
    Flags |= PAGE_SWAPPED;
  else
    Flags |= PAGE_PRESENT;
  if ((flags & CopyOnWrite) == CopyOnWrite)
    Flags |= PAGE_COPY_ON_WRITE;
  return Flags;
}
size_t X86VirtualAddressSpace::fromFlags(uint32_t Flags)
{
  size_t flags = Execute;
  if ((Flags & PAGE_USER) != PAGE_USER)
    flags |= KernelMode;
  if ((Flags & PAGE_WRITE) == PAGE_WRITE)
    flags |= Write;
  if ((Flags & PAGE_WRITE_THROUGH) == PAGE_WRITE_THROUGH)
    flags |= WriteThrough;
  if ((Flags & PAGE_CACHE_DISABLE) == PAGE_CACHE_DISABLE)
    flags |= CacheDisable;
  if ((Flags & PAGE_SWAPPED) == PAGE_SWAPPED)
    flags |= Swapped;
  if ((Flags & PAGE_COPY_ON_WRITE) == PAGE_COPY_ON_WRITE)
    flags |= CopyOnWrite;
  return flags;
}

VirtualAddressSpace *X86VirtualAddressSpace::clone()
{
    // No lock guard in here - we assume that if we're cloning, nothing will be trying
    // to map/unmap memory.
    // Also, we need a way of solving this as we use the map/unmap functions, which
    // themselves try and grab the lock.
    /// \bug This assumption is false!

    VirtualAddressSpace &thisAddressSpace = Processor::information().getVirtualAddressSpace();

    // Create a new virtual address space
    VirtualAddressSpace *pClone = VirtualAddressSpace::create();
    if (pClone == 0)
    {
        WARNING("X86VirtualAddressSpace: Clone() failed!");
        return 0;
    }

    g_pCurrentlyCloning = pClone;

    uintptr_t v = beginCrossSpace(reinterpret_cast<X86VirtualAddressSpace*>(pClone));

    for (uintptr_t i = 0; i < 1024; i++)
    {
        uint32_t *pageDirectoryEntry = PAGE_DIRECTORY_ENTRY(m_VirtualPageDirectory, i);

        if ((*pageDirectoryEntry & PAGE_PRESENT) != PAGE_PRESENT)
            continue;

        for (uintptr_t j = 0; j < 1024; j++)
        {
            uint32_t *pageTableEntry = PAGE_TABLE_ENTRY(m_VirtualPageTables, i, j);

            if ((*pageTableEntry & PAGE_PRESENT) != PAGE_PRESENT)
                continue;

            uint32_t flags = PAGE_GET_FLAGS(pageTableEntry);

            void *virtualAddress = reinterpret_cast<void*> ( ((i*1024)+j)*4096 );
            if (getKernelAddressSpace().isMapped(virtualAddress))
                continue;

            // Page mapped in source address space, but not in kernel.
            /// \todo Copy on write.
            physical_uintptr_t newFrame = PhysicalMemoryManager::instance().allocatePage();

            // Temporarily map in.
            map(newFrame,
                KERNEL_VIRTUAL_TEMP1,
                VirtualAddressSpace::Write | VirtualAddressSpace::KernelMode);

            // Copy across.
            memcpy(KERNEL_VIRTUAL_TEMP1, virtualAddress, 0x1000);

            // Unmap.
            unmap(KERNEL_VIRTUAL_TEMP1);

            // Map in.
            mapCrossSpace(v, newFrame, virtualAddress, fromFlags(flags));
        }
    }

    endCrossSpace();

    g_pCurrentlyCloning = 0;

    return pClone;
}

void X86VirtualAddressSpace::revertToKernelAddressSpace()
{
    // Again, similar to clone(), we don't grab the lock. We should, but we don't.

    for (uintptr_t i = 0; i < 1024; i++)
    {
        uint32_t *pageDirectoryEntry = PAGE_DIRECTORY_ENTRY(m_VirtualPageDirectory, i);

        if ((*pageDirectoryEntry & PAGE_PRESENT) != PAGE_PRESENT)
            continue;

        if (reinterpret_cast<void*>(i*1024*4096) >= KERNEL_SPACE_START)
            continue;

        bool bDidSkip = false;
        for (uintptr_t j = 0; j < 1024; j++)
        {
            uint32_t *pageTableEntry = PAGE_TABLE_ENTRY(m_VirtualPageTables, i, j);

            if ((*pageTableEntry & PAGE_PRESENT) != PAGE_PRESENT)
                continue;

            void *virtualAddress = reinterpret_cast<void*> ( ((i*1024)+j)*4096 );
            if (getKernelAddressSpace().isMapped(virtualAddress))
            {
                bDidSkip = true;
                continue;
            }

            // Grab the physical address for it.
            physical_uintptr_t physicalAddress = PAGE_GET_PHYSICAL_ADDRESS(pageTableEntry);

            // Page mapped in this address space but not in kernel. Unmap it.
            unmap(virtualAddress);

            // And release the physical memory.
            /// \todo There's going to be a caveat with CoW here...
            PhysicalMemoryManager::instance().freePage(physicalAddress);

            // This PTE is no longer valid
            *pageTableEntry = 0;
        }

        // Don't remove the directory entry if we skipped a kernel page
        if(bDidSkip)
            continue;

        // Remove the page table from the directory
        PhysicalMemoryManager::instance().freePage(PAGE_GET_PHYSICAL_ADDRESS(pageDirectoryEntry));
        *pageDirectoryEntry = 0;

        // Invalidate the page table mapping
        uint32_t *pageTable = PAGE_TABLE_ENTRY(m_VirtualPageTables, i, 0);
        Processor::invalidate(pageTable);
    }
}

uintptr_t X86VirtualAddressSpace::beginCrossSpace(X86VirtualAddressSpace *pOther)
{
    map(pOther->m_PhysicalPageDirectory, KERNEL_VIRTUAL_TEMP2, VirtualAddressSpace::KernelMode | VirtualAddressSpace::Write);

    uint32_t *pDir = reinterpret_cast<uint32_t*> (KERNEL_VIRTUAL_TEMP2);
    map(pDir[0], KERNEL_VIRTUAL_TEMP3, VirtualAddressSpace::KernelMode | VirtualAddressSpace::Write);

    return 0x00000000;
}

bool X86VirtualAddressSpace::mapCrossSpace(uintptr_t &v, physical_uintptr_t physicalAddress,
                                 void *virtualAddress, size_t flags)
{
    size_t Flags = toFlags(flags);
    size_t pageDirectoryIndex = PAGE_DIRECTORY_INDEX(virtualAddress);

    uint32_t *pageDirectoryEntry = PAGE_DIRECTORY_ENTRY(KERNEL_VIRTUAL_TEMP2, pageDirectoryIndex);
    uint32_t *pDir = reinterpret_cast<uint32_t*> (KERNEL_VIRTUAL_TEMP2);

    if ((*pageDirectoryEntry & PAGE_PRESENT) != PAGE_PRESENT)
    {
        uint32_t page = PhysicalMemoryManager::instance().allocatePage();
        // Set the page.
        *pageDirectoryEntry = page | ((Flags & ~(PAGE_GLOBAL | PAGE_SWAPPED | PAGE_COPY_ON_WRITE)) | PAGE_WRITE);

        // Map it in.
        v = pageDirectoryIndex;
        unmap(KERNEL_VIRTUAL_TEMP3);
        map(pDir[pageDirectoryIndex], KERNEL_VIRTUAL_TEMP3, VirtualAddressSpace::KernelMode | VirtualAddressSpace::Write);

        // Zero.
        memset(KERNEL_VIRTUAL_TEMP3, 0, PhysicalMemoryManager::getPageSize());
    }
    else if ( (Flags & PAGE_USER) && ((*pageDirectoryEntry & PAGE_USER) != PAGE_USER))
    {
        *pageDirectoryEntry |= PAGE_USER;
    }

    // Check we now have the right page table mapped in. If not, map it in.
    if (v != pageDirectoryIndex)
    {
        v = pageDirectoryIndex;
        unmap(KERNEL_VIRTUAL_TEMP3);
        map(pDir[pageDirectoryIndex], KERNEL_VIRTUAL_TEMP3, VirtualAddressSpace::KernelMode | VirtualAddressSpace::Write);
    }

    size_t pageTableIndex = PAGE_TABLE_INDEX(virtualAddress);
    uint32_t *pageTableEntry = & (reinterpret_cast<uint32_t*>(KERNEL_VIRTUAL_TEMP3)[pageTableIndex]);

    // Is a page already present
    if ((*pageTableEntry & PAGE_PRESENT) == PAGE_PRESENT)
        return false;

    // Map the page
    *pageTableEntry = physicalAddress | Flags;

    return true;
}

void X86VirtualAddressSpace::endCrossSpace()
{
    unmap(KERNEL_VIRTUAL_TEMP2);
    unmap(KERNEL_VIRTUAL_TEMP3);
}

bool X86KernelVirtualAddressSpace::isMapped(void *virtualAddress)
{
  return doIsMapped(virtualAddress);
}
bool X86KernelVirtualAddressSpace::map(physical_uintptr_t physicalAddress,
                                       void *virtualAddress,
                                       size_t flags)
{
  return doMap(physicalAddress, virtualAddress, flags);
}
void X86KernelVirtualAddressSpace::getMapping(void *virtualAddress,
                                              physical_uintptr_t &physicalAddress,
                                              size_t &flags)
{
  doGetMapping(virtualAddress, physicalAddress, flags);
}
void X86KernelVirtualAddressSpace::setFlags(void *virtualAddress, size_t newFlags)
{
  doSetFlags(virtualAddress, newFlags);
}
void X86KernelVirtualAddressSpace::unmap(void *virtualAddress)
{
  doUnmap(virtualAddress);
}
void *X86KernelVirtualAddressSpace::allocateStack()
{
  void *pStack = doAllocateStack(KERNEL_STACK_SIZE + 0x1000);

  return pStack;
}
X86KernelVirtualAddressSpace::X86KernelVirtualAddressSpace()
  : X86VirtualAddressSpace(KERNEL_VIRTUAL_HEAP,
                           reinterpret_cast<uintptr_t>(&pagedirectory) - reinterpret_cast<uintptr_t>(KERNEL_VIRTUAL_ADDRESS),
                           KERNEL_VIRUTAL_PAGE_DIRECTORY,
                           VIRTUAL_PAGE_TABLES,
                           KERNEL_VIRTUAL_STACK)
{
  /// \todo MAX_PROCESSORS here.
  for (int i = 0; i < 256; i++)
  {
    g_EscrowPages[i] = 0;
  }
}
X86KernelVirtualAddressSpace::~X86KernelVirtualAddressSpace()
{
}
