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
#ifndef EXT2_FILE_H
#define EXT2_FILE_H

#include "ext2.h"
#include "Ext2Node.h"
#include <vfs/File.h>
#include <utilities/Vector.h>
#include "Ext2Filesystem.h"

/** A File is a file, a directory or a symlink. */
class Ext2File : public File, public Ext2Node
{
private:
    /** Copy constructors are hidden - unused! */
    Ext2File(const Ext2File &file);
    Ext2File& operator =(const Ext2File&);
public:
    /** Constructor, should be called only by a Filesystem. */
    Ext2File(String name, uintptr_t inode_num, Inode *inode,
             class Ext2Filesystem *pFs, File *pParent = 0);
    /** Destructor */
    virtual ~Ext2File();

    void truncate();

    /** Updates inode attributes. */
    void fileAttributeChanged();

protected:
    /** Performs a read-to-cache. */
    uintptr_t readBlock(uint64_t location);
    size_t getBlockSize() const
    {
        return reinterpret_cast<Ext2Filesystem*>(m_pFilesystem)->m_BlockSize;
    }
};

#endif
