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

#ifndef KERNEL_PROCESSOR_MEMORYMAPPEDIO_H
#define KERNEL_PROCESSOR_MEMORYMAPPEDIO_H

#include <processor/types.h>
#include <processor/IoBase.h>
#include <processor/MemoryRegion.h>
#include <processor/Processor.h>
#include <utilities/utility.h>

/** @addtogroup kernelprocessor
 * @{ */

/** The MemoryMappedIo handles special MemoryRegions for I/O to hardware devices
 *\brief Memory mapped I/O range */
class MemoryMappedIo : public IoBase,
                       public MemoryRegion
{
  public:
    /** The default constructor */
  inline MemoryMappedIo(const char *pName, uintptr_t offset=0, uintptr_t padding=1)
    : IoBase(), MemoryRegion(pName), m_Offset(offset), m_Padding(padding) {}
    /** The destructor frees the allocated ressources */
    inline virtual ~MemoryMappedIo(){}

    //
    // IoBase Interface
    //
    inline virtual size_t size() const;
    inline virtual uint8_t read8(size_t offset = 0);
    inline virtual uint16_t read16(size_t offset = 0);
    inline virtual uint32_t read32(size_t offset = 0);
    inline virtual uint64_t read64(size_t offset = 0);
    inline virtual void write8(uint8_t value, size_t offset = 0);
    inline virtual void write16(uint16_t value, size_t offset = 0);
    inline virtual void write32(uint32_t value, size_t offset = 0);
    inline virtual void write64(uint64_t value, size_t offset = 0);
    inline virtual operator bool() const;

    //
    // MemoryRegion Interface
    //

  private:
    /** The copy-constructor
     *\note NOT implemented */
    MemoryMappedIo(const MemoryMappedIo &);
    /** The assignment operator
     *\note NOT implemented */
    MemoryMappedIo &operator = (const MemoryMappedIo &);

    /** MemoryRegion only supports allocation on a page boundary.
        This variable adds an offset onto each access to make up for this
        (if required) */
    uintptr_t m_Offset;

    /** It is possible that registers may not follow one another directly in memory,
        instead being padded to some boundary. */
    size_t m_Padding;
};

/** @} */

//
// Part of the implementation
//
size_t MemoryMappedIo::size() const
{
  return MemoryRegion::size();
}
uint8_t MemoryMappedIo::read8(size_t offset)
{
  #if defined(ADDITIONAL_CHECKS)
    if (offset >= size())
      Processor::halt();
  #endif

  return *reinterpret_cast<volatile uint8_t*>(adjust_pointer(virtualAddress(), (offset*m_Padding)+m_Offset));
}
uint16_t MemoryMappedIo::read16(size_t offset)
{
  #if defined(ADDITIONAL_CHECKS)
    if ((offset + 1) >= size())
      Processor::halt();
  #endif

  return *reinterpret_cast<volatile uint16_t*>(adjust_pointer(virtualAddress(), (offset*m_Padding)+m_Offset));
}
uint32_t MemoryMappedIo::read32(size_t offset)
{
  #if defined(ADDITIONAL_CHECKS)
    if ((offset + 3) >= size())
      Processor::halt();
  #endif

  return *reinterpret_cast<volatile uint32_t*>(adjust_pointer(virtualAddress(), (offset*m_Padding)+m_Offset));
}
uint64_t MemoryMappedIo::read64(size_t offset)
{
  #if defined(ADDITIONAL_CHECKS)
    if ((offset + 7) >= size())
      Processor::halt();
  #endif

  return *reinterpret_cast<volatile uint64_t*>(adjust_pointer(virtualAddress(), (offset*m_Padding)+m_Offset));
}
void MemoryMappedIo::write8(uint8_t value, size_t offset)
{
  #if defined(ADDITIONAL_CHECKS)
    if (offset >= size())
      Processor::halt();
  #endif

  *reinterpret_cast<volatile uint8_t*>(adjust_pointer(virtualAddress(), (offset*m_Padding)+m_Offset)) = value;
}
void MemoryMappedIo::write16(uint16_t value, size_t offset)
{
  #if defined(ADDITIONAL_CHECKS)
    if ((offset + 1) >= size())
      Processor::halt();
  #endif

  *reinterpret_cast<volatile uint16_t*>(adjust_pointer(virtualAddress(), (offset*m_Padding)+m_Offset)) = value;
}
void MemoryMappedIo::write32(uint32_t value, size_t offset)
{
  #if defined(ADDITIONAL_CHECKS)
    if ((offset + 3) >= size())
      Processor::halt();
  #endif

  *reinterpret_cast<volatile uint32_t*>(adjust_pointer(virtualAddress(), (offset*m_Padding)+m_Offset)) = value;
}
void MemoryMappedIo::write64(uint64_t value, size_t offset)
{
  #if defined(ADDITIONAL_CHECKS)
    if ((offset + 7) >= size())
      Processor::halt();
  #endif

  *reinterpret_cast<volatile uint64_t*>(adjust_pointer(virtualAddress(), (offset*m_Padding)+m_Offset)) = value;
}
MemoryMappedIo::operator bool() const
{
  return MemoryRegion::operator bool();
}

#endif
