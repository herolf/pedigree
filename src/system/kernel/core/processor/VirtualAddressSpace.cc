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

#include <utilities/utility.h>
#include <processor/VirtualAddressSpace.h>
#include <processor/PhysicalMemoryManager.h>
#include <processor/Processor.h>
#include <Log.h>

void *VirtualAddressSpace::expandHeap(ssize_t incr, size_t flags)
{
  void *Heap = m_HeapEnd;
  PhysicalMemoryManager &PMemoryManager = PhysicalMemoryManager::instance();

  void *newHeapEnd = adjust_pointer(m_HeapEnd, incr);
  
  m_HeapEnd = reinterpret_cast<void*> (reinterpret_cast<uintptr_t>(m_HeapEnd) & ~(PhysicalMemoryManager::getPageSize()-1));

  int i = 0;
  if(incr < 0)
  {
    while (reinterpret_cast<uintptr_t>(newHeapEnd) < reinterpret_cast<uintptr_t>(m_HeapEnd))
    {
        /// \note Should this be m_HeapEnd - getPageSize?
        void *unmapAddr = reinterpret_cast<void*>(m_HeapEnd);
        if(isMapped(unmapAddr))
        {
            // Unmap the virtual address
            physical_uintptr_t phys = 0;
            size_t flags = 0;
            getMapping(unmapAddr, phys, flags);
            unmap(reinterpret_cast<void*>(unmapAddr));

            // Free the physical page
            PMemoryManager.freePage(phys);
        }

        // Drop back a page
        m_HeapEnd = adjust_pointer(m_HeapEnd, -PhysicalMemoryManager::getPageSize());
        i++;
    }

    // Now that we've freed this section, the heap is actually at the end of the original memory...
    Heap = m_HeapEnd;
  }
  else
  {
      while (reinterpret_cast<uintptr_t>(newHeapEnd) > reinterpret_cast<uintptr_t>(m_HeapEnd))
      {
          // Allocate a page
          physical_uintptr_t page = PMemoryManager.allocatePage();

          if (page == 0)
          {
              ERROR("Out of physical memory!");
          
              // Reset the heap pointer
              m_HeapEnd = adjust_pointer(m_HeapEnd, - i * PhysicalMemoryManager::getPageSize());

              // Free the pages that were already allocated
              rollbackHeapExpansion(m_HeapEnd, i);
              return 0;
          }

          // Map the page
          if (map(page, m_HeapEnd, flags) == false)
          {
              // Map failed - probable double mapping. Go to the next page.
              // Free the page
              PMemoryManager.freePage(page);
          }
          else
          {
              // Empty the page.
              memset(m_HeapEnd, 0, PhysicalMemoryManager::getPageSize());
          }

          // Go to the next address
          m_HeapEnd = adjust_pointer(m_HeapEnd, PhysicalMemoryManager::getPageSize());
          i++;
      }
  }

  m_HeapEnd = newHeapEnd;
  return Heap;
}

void VirtualAddressSpace::rollbackHeapExpansion(void *virtualAddress, size_t pageCount)
{
  for (size_t i = 0;i < pageCount;i++)
  {
    // Get the mapping for the current page
    size_t flags;
    physical_uintptr_t physicalAddress;
    getMapping(virtualAddress,
               physicalAddress,
               flags);

    // Free the physical page
    PhysicalMemoryManager::instance().freePage(physicalAddress);

    // Unmap the page from the virtual address space
    unmap(virtualAddress);

    // Go to the next virtual page
    virtualAddress = adjust_pointer(virtualAddress, PhysicalMemoryManager::getPageSize());
  }
}
