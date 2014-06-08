/*
 * Copyright (c) 2008-2014, Pedigree Developers
 *
 * Please see the CONTRIB file in the root of the source tree for a full
 * list of contributors.
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

#ifndef MEMORY_MAPPED_FILE_H
#define MEMORY_MAPPED_FILE_H

#include "File.h"

#include <processor/PageFaultHandler.h>
#include <process/MemoryPressureManager.h>
#include <utilities/Tree.h>
#include <utilities/List.h>
#include <process/Mutex.h>
#include <process/Process.h>
#include <LockGuard.h>

/** \addtogroup vfs
    @{ */

/** \file
    \brief Memory-mapped file interface

    Provides a mechanism for mapping Files into the address space.
    
    \todo Handle writing of files, not just reading. */

class MemoryMapManager;

/**
 * Generic base for a memory mapped file or object.
 *
 * Provides the interface for implementation, while centralising common
 * functionality (such as extents, CoW flags, shared state, etc)
 */
class MemoryMappedObject
{
    friend class MemoryMapManager;

    private:
        /** Default constructor, don't use. */
        MemoryMappedObject();

    public:
        /** Permissions to assign to a mapping when it is created. */
        typedef int Permissions;

        static const int None = 0x0;
        static const int Read = 0x1;
        static const int Write = 0x2;
        static const int Exec = 0x4;

        /** Constructor - bring up common metadata. */
        MemoryMappedObject(uintptr_t address, bool bCopyOnWrite, size_t length, Permissions perms) :
            m_bCopyOnWrite(bCopyOnWrite), m_Address(address), m_Length(length), m_Permissions(perms)
        {}

        virtual ~MemoryMappedObject();

        /**
         * Clones the existing metadata of this object into another.
         *
         * Returns a MemoryMappedObject that exactly matches this one,
         * but which can be used for reference in a new address space.
         * Used for address space clones.
         *
         * Note that mappings are automatically cloned - only clone
         * metadata in this method.
         */
        virtual MemoryMappedObject *clone() = 0;

        /**
         * Splits the metadata of this object at the given address and
         * creates a new MemoryMappedObject that begins at the page of
         * the split.
         */
        virtual MemoryMappedObject *split(uintptr_t at) = 0;

        /**
         * Removes pages from the start of this MemoryMappedObject.
         *
         * To remove pages from the middle or end of a MemoryMappedObject,
         * use split() and then remove()/unmap() on the returned object).
         *
         * \return true if the remove() has effectively removed the
         *         entire MemoryMappedObject, false otherwise.
         */
        virtual bool remove(size_t length) = 0;

        /**
         * Sets permissions on this object.
         *
         * This may trigger unmaps if the new permissions.
         */
        virtual void setPermissions(Permissions perms) = 0;

        /**
         * Sync back the given page to a backing store, if one exists.
         */
        virtual void sync(uintptr_t at, bool async)
        {}

        /**
         * If the given page is dirty (and this is a CoW mapping),
         * restore it to the file's actual content.
         */
        virtual void invalidate(uintptr_t at)
        {}

        /**
         * Unmaps existing mappings in this object from the address space.
         *
         * Implementations are expected to track these as necessary for
         * the implementation.
         */
        virtual void unmap() = 0;

        /**
         * Trap entry
         *
         * Implement this in your implementation to actually perform
         * the mapping of memory into the address space.
         * \return true if the trap was successful, false otherwise.
         */
        virtual bool trap(uintptr_t address, bool bWrite) = 0;

        /**
         * Release memory that can be released.
         *
         * Default implementation returns 'no pages released'.
         */
        virtual bool compact()
        {
            return false;
        }

        /**
         * Determines if the given address is within this object's mapping.
         */
        bool matches(uintptr_t address)
        {
            return (m_Address <= address) && (address < (m_Address + m_Length));
        }

        /**
         * Getter for base address.
         */
        uintptr_t address() const
        {
            return m_Address;
        }

        /**
         * Getter for length.
         */
        size_t length() const
        {
            return m_Length;
        }

    protected:

        /**
         * Is this a Copy-on-Write mapping?
         *
         * A non-copy-on-write mapping will cause writes to hit the
         * backing store directly. This makes sense for some types of
         * mapping and not for others.
         */
        bool m_bCopyOnWrite;

        /**
         * Base address of this mapping.
         *
         * The region from base -> base+length will trap into this
         * object when an access attempt is made that faults.
         */
        uintptr_t m_Address;

        /**
         * Size of the object.
         *
         * To clarify, this is the size of the mapping itself, not of
         * the entire backing object.
         */
        size_t m_Length;

        /**
         * Permissions for mappings created by this object.
         *
         * For example, 'None' would mean that a trap will NEVER succeed,
         * and that no premapping will be done. 'Read' might mean that
         * writes will never magically map in a page for writing to.
         *
         * 'Exec' only works on systems that support this (eg, x86_64).
         */
        Permissions m_Permissions;
};

/**
 * Anonymous memory map object.
 *
 * Anonymous memory maps do not map back to an actual file; they back
 * onto a common page full of zeroes with forced copy-on-write. This
 * is perfect for mapping in large .bss sections in binaries or for
 * getting huge amounts of zeroed memory.
 */
class AnonymousMemoryMap : public MemoryMappedObject
{
    public:
        AnonymousMemoryMap(uintptr_t address, size_t length, Permissions perms);

        virtual ~AnonymousMemoryMap()
        {}

        virtual MemoryMappedObject *clone();
        virtual MemoryMappedObject *split(uintptr_t at);
        virtual bool remove(size_t length);

        virtual void setPermissions(MemoryMappedObject::Permissions perms);

        virtual void unmap();

        virtual bool trap(uintptr_t address, bool bWrite);

    private:
        static physical_uintptr_t m_Zero;

        /** List of existing virtual addresses we've mapped in. */
        List<void *> m_Mappings;
};

/**
 * File map object.
 *
 * File maps actually provide a backing file for a memory region. These
 * are used for loading binaries or for opening files for reading without
 * the overhead of read/write syscalls (assuming the syscall is more
 * expensive than a page fault).
 */
class MemoryMappedFile : public MemoryMappedObject
{
    public:
        MemoryMappedFile(uintptr_t address, size_t length, size_t offset, File *backing, bool bCopyOnWrite, Permissions perms);

        virtual ~MemoryMappedFile()
        {
        }

        virtual MemoryMappedObject *clone();
        virtual MemoryMappedObject *split(uintptr_t at);
        virtual bool remove(size_t length);

        virtual void setPermissions(MemoryMappedObject::Permissions perms);

        virtual void sync(uintptr_t at, bool async);
        virtual void invalidate(uintptr_t at);

        virtual void unmap();

        virtual bool trap(uintptr_t address, bool bWrite);

        /**
         * Syncs back all dirty pages to their respective backing store.
         *
         * Compacting begins by doing a pass to find any pages that can be
         * synced and then unpinned, allowing a future Cache eviction.
         * Should that pass successfully free memory, the compact will be
         * considered successful.
         *
         * If that pass is not successful, read-only pages are evicted. At this
         * stage, this is done in a linear fashion; there is no LRU or other
         * type of algorithm at play.
         * \todo Improve this.
         *
         * \return true if at least one page was released, false otherwise.
         */
        virtual bool compact();

    private:
        /** Backing file. */
        File *m_pBacking;

        /** Offset within the file that this mapping begins at. */
        size_t m_Offset;

        /** Shared files share the same physical page even on write. */
        bool m_bShared;

        /** List of existing mappings. */
        Tree<uintptr_t, physical_uintptr_t> m_Mappings;
};

/**
 * This class is a multiplexing trap handler, to handle traps for
 * MemoryMappedObjects, dispatching them to the right place.
 */
class MemoryMapManager : public MemoryTrapHandler, public MemoryPressureHandler
{
    public:
        /** Singleton instance */
        static MemoryMapManager &instance()
        {
            return m_Instance;
        }

        /**
         * Map in the given File.
         */
        MemoryMappedObject *mapFile(File *pFile, uintptr_t &address, size_t length, MemoryMappedObject::Permissions perms, size_t offset = 0, bool bCopyOnWrite = true);

        /**
         * Create a new anonymous memory mapping.
         */
        MemoryMappedObject *mapAnon(uintptr_t &address, size_t length, MemoryMappedObject::Permissions perms);

        /**
         * Registers the current address space's mappings with the target
         * process.
         * \param pTarget The process to clone into.
         */
        void clone(Process *pTarget);

        /**
         * Removes the given range from whatever objects might own them,
         * and will cross object boundaries if necessary.
         *
         * If this will result in a MemoryMappedObject being completely
         * unmapped, it will be removed.
         *
         * \return number of objects affected by this call.
         */
        size_t remove(uintptr_t base, size_t length);

        /**
         * Adjusts permissions across the given range, crossing object
         * boundaries if necessary.
         *
         * \return number of objects affected by this call.
         */
        size_t setPermissions(uintptr_t base, size_t length, MemoryMappedObject::Permissions perms);

        /**
         * Returns true if at least one memory mapped object is in the range
         * given, false otherwise.
         */
        bool contains(uintptr_t base, size_t length);

        /**
         * Syncs memory mapped objects within the given range back to
         * their backing store, if they have one.
         */
        void sync(uintptr_t base, size_t length, bool async);

        /**
         * Takes any pages that differ from the backing file (ie, copied
         * on write) and restores them to point to the backing file.
         */
        void invalidate(uintptr_t base, size_t length);

        /**
         * Removes the mappings for the given object from the address space.
         */
        void unmap(MemoryMappedObject* pObj);

        /**
         * Removes all mappings from this address space.
         */
        void unmapAll();

        /**
         * Trap handler, called when a fault takes place.
         */
        bool trap(uintptr_t address, bool bIsWrite);

        /**
         * Trigger a compact in all address spaces.
         *
         * This may switch in and out of several address spaces.
         */
        virtual bool compact();

        virtual const String getMemoryPressureDescription()
        {
          return String("Unmap safe pages from memory mapped files.");
        }

    private:
        /** Default and only constructor. Registers with PageFaultHandler. */
        MemoryMapManager();
        ~MemoryMapManager();

        bool sanitiseAddress(uintptr_t &address, size_t length);

        enum Ops
        {
            Sync,
            Invalidate,
        };

        void op(Ops what, uintptr_t base, size_t length, bool async);

        /** Singleton instance. */
        static MemoryMapManager m_Instance;

        typedef List<MemoryMappedObject*> MmObjectList;

        /** Cache of virtual address spaces -> MmObjectLists. */
        Tree<VirtualAddressSpace*, MmObjectList*> m_MmObjectLists;

        /** Lock for the cache. */
        Mutex m_Lock;
};

/** @} */

#endif
