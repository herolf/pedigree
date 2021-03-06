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

#ifndef KERNEL_BOOTSTRAPINFO_H
#define KERNEL_BOOTSTRAPINFO_H

#include <compiler.h>
#include <processor/types.h>
#include <utilities/String.h>

/** @addtogroup kernel
 * @{ */

// Again, if we're passed via grub these multiboot #defines will be valid, otherwise they won't.
#if defined(KERNEL_STANDALONE) || defined(MIPS_COMMON) || defined(ARM_COMMON)
#define MULTIBOOT_FLAG_MEM     0x001
#define MULTIBOOT_FLAG_DEVICE  0x002
#define MULTIBOOT_FLAG_CMDLINE 0x004
#define MULTIBOOT_FLAG_MODS    0x008
#define MULTIBOOT_FLAG_AOUT    0x010
#define MULTIBOOT_FLAG_ELF     0x020
#define MULTIBOOT_FLAG_MMAP    0x040
#define MULTIBOOT_FLAG_CONFIG  0x080
#define MULTIBOOT_FLAG_LOADER  0x100
#define MULTIBOOT_FLAG_APM     0x200
#define MULTIBOOT_FLAG_VBE     0x400
#endif

#ifndef PPC_COMMON

struct BootstrapStruct_t
{
    // If we are passed via grub, this information will be completely different to
    // via the bootstrapper.
    uint32_t flags;

    uint32_t mem_lower;
    uint32_t mem_upper;

    uint32_t boot_device;

    uint32_t cmdline;

    uint32_t mods_count;
    uint32_t mods_addr;

    /* ELF information */
    uint32_t num;
    uint32_t size;
    uint32_t addr;
    uint32_t shndx;

    uint32_t mmap_length;
    uint32_t mmap_addr;

    uint32_t drives_length;
    uint32_t drives_addr;

    uint32_t config_table;

    uint32_t boot_loader_name;

    uint32_t apm_table;

    uint32_t vbe_control_info;
    uint32_t vbe_mode_info;
    uint32_t vbe_mode;
    uint32_t vbe_interface_seg;
    uint32_t vbe_interface_off;
    uint32_t vbe_interface_len;

    inline bool isInitrdLoaded() const
    {
        return (mods_count != 0);
    }
    inline uint8_t *getInitrdAddress() const
    {
        return reinterpret_cast<uint8_t*>(*reinterpret_cast<uint32_t*>(mods_addr));
    }
    inline size_t getInitrdSize() const
    {
        return *reinterpret_cast<uint32_t*>(mods_addr + 4) -
            *reinterpret_cast<uint32_t*>(mods_addr);
    }
    inline size_t isDatabaseLoaded() const
    {
        return (mods_count > 1);
    }
    inline uint8_t *getDatabaseAddress() const
    {
        return reinterpret_cast<uint8_t*>(*reinterpret_cast<uint32_t*>(mods_addr+16));
    }
    inline size_t getDatabaseSize() const
    {
        return *reinterpret_cast<uint32_t*>(mods_addr + 20) -
            *reinterpret_cast<uint32_t*>(mods_addr + 16);
    }
    inline char *getCommandLine() const
    {
        return (flags&MULTIBOOT_FLAG_CMDLINE?reinterpret_cast<char *>(cmdline):0);
    }
} PACKED;

struct MemoryMapEntry_t
{
    uint32_t size;
    uint64_t address;
    uint64_t length;
    uint32_t type;
} PACKED;

#else

struct BootstrapStruct_t
{
    int (*prom)(struct anon*);
    uint32_t initrd_start;
    uint32_t initrd_end;

    /* ELF information */
    uint32_t num;
    uint32_t size;
    uint32_t addr;
    uint32_t shndx;

    inline bool isInitrdLoaded() const
    {
        return true;
    }
    inline uint8_t *getInitrdAddress() const
    {
        return reinterpret_cast<uint8_t*>(initrd_start);
    }
    inline size_t getInitrdSize() const
    {
        return initrd_end - initrd_start;
    }
};

#endif

/** @} */

#endif
