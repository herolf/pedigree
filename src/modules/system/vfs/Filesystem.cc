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

#include "VFS.h"
#include "Filesystem.h"
#include <Log.h>
#include <utilities/utility.h>
#include <processor/Processor.h>
#include <syscallError.h>

#include "File.h"
#include "Directory.h"
#include "Symlink.h"

Filesystem::Filesystem() :
#ifdef CRIPPLE_HDD
    m_bReadOnly(true),
#else
    m_bReadOnly(false),
#endif
    m_pDisk(0), m_nAliases(0)
{
}

File *Filesystem::find(String path, File *pStartNode)
{
    if (!pStartNode) pStartNode = getRoot();
    File *a = findNode(pStartNode, path);
    return a;
}

bool Filesystem::createFile(String path, uint32_t mask, File *pStartNode)
{
    if (!pStartNode) pStartNode = getRoot();
    File *pFile = findNode(pStartNode, path);

    if (pFile)
    {
        SYSCALL_ERROR(FileExists);
        return false;
    }

    String filename;
    File *pParent = findParent(path, pStartNode, filename);

    // Check the parent existed.
    if (!pParent)
    {
        SYSCALL_ERROR(DoesNotExist);
        return false;
    }

    // Now make the file.
    return createFile(pParent, filename, mask);
}

bool Filesystem::createDirectory(String path, File *pStartNode)
{
    if (!pStartNode) pStartNode = getRoot();
    File *pFile = findNode(pStartNode, path);

    if (pFile)
    {
        SYSCALL_ERROR(FileExists);
        return false;
    }

    String filename;
    File *pParent = findParent(path, pStartNode, filename);

    // Check the parent existed.
    if (!pParent)
    {
        SYSCALL_ERROR(DoesNotExist);
        return false;
    }

    // Now make the directory.
    createDirectory(pParent, filename);

    return true;
}

bool Filesystem::createSymlink(String path, String value, File *pStartNode)
{
    if (!pStartNode) pStartNode = getRoot();
    File *pFile = findNode(pStartNode, path);

    if (pFile)
    {
        SYSCALL_ERROR(FileExists);
        return false;
    }

    String filename;
    File *pParent = findParent(path, pStartNode, filename);

    // Check the parent existed.
    if (!pParent)
    {
        SYSCALL_ERROR(DoesNotExist);
        return false;
    }

    // Now make the symlink.
    createSymlink(pParent, filename, value);

    return true;
}

bool Filesystem::remove(String path, File *pStartNode)
{
    if (!pStartNode) pStartNode = getRoot();

    File *pFile = findNode(pStartNode, path);

    if (!pFile)
    {
        SYSCALL_ERROR(DoesNotExist);
        return false;
    }

    String filename;
    File *pParent = findParent(path, pStartNode, filename);

    // Check the parent existed.
    if (!pParent)
    {
        FATAL("Filesystem::remove: Massive algorithmic error.");
        return false;
    }

    Directory *pDParent = Directory::fromFile(pParent);
    if (!pDParent)
    {
        FATAL("Filesystem::remove: Massive algorithmic error (2)");
        return false;
    }
    pDParent->m_Cache.remove(filename);
    return remove(pParent, pFile);
}

File *Filesystem::findNode(File *pNode, String path)
{
    if (path.length() == 0)
        return pNode;

    // If the pathname has a leading slash, cd to root and remove it.
    if (path[0] == '/')
    {
        pNode = getRoot();
        path = String(&path[1]);
    }

    // Grab the next filename component.
    size_t i = 0;
    while (path[i] != '/' && path[i] != '\0')
        i = path.nextCharacter(i);

    String restOfPath;
    // Why did the loop exit?
    if (path[i] != '\0')
    {
        restOfPath = path.split(path.nextCharacter(i));
        // restOfPath is now 'path', but starting at the next token, and with no leading slash.
        // Unfortunately 'path' now has a trailing slash, so chomp it off.
        path.chomp();
    }

    // At this point 'path' contains the token to search for. 'restOfPath' contains the path for the next recursion (or nil).

    // If 'path' is zero-lengthed, ignore and recurse.
    if (path.length() == 0)
        return findNode(pNode, restOfPath);

    // Firstly, if the current node is a symlink, follow it.
    while (pNode->isSymlink())
        pNode = Symlink::fromFile(pNode)->followLink();

    // Next, if the current node isn't a directory, die.
    if (!pNode->isDirectory())
    {
        SYSCALL_ERROR(NotADirectory);
        return 0;
    }

    if (!strcmp(path, ".") || (!strcmp(path, "..") && pNode->m_pParent == 0))
    {
        return findNode(pNode, restOfPath);
    }
    else if (!strcmp(path, ".."))
    {
        return findNode(pNode->m_pParent, restOfPath);
    }

    Directory *pDir = Directory::fromFile(pNode);
    if(!pDir)
    {
        // Throw some error...
        return 0;
    }

    // Cache lookup.
    File *pFile;
    if (pDir->m_bCachePopulated)
    {
        pFile = pDir->m_Cache.lookup(path);

        if (pFile)
        {
            // Cache lookup succeeded, recurse and return.
            return findNode(pFile, restOfPath);
        }
        else
        {
            // Cache lookup failed, does not exist.
            return 0;
        }
    }
    else
    {
        // Directory contents not cached - cache them now.
        pDir->cacheDirectoryContents();
        // Then lookup.
        pFile = pDir->m_Cache.lookup(path);

        if (pFile)
        {
            // Cache lookup succeeded, recurse and return.
            return findNode(pFile, restOfPath);
        }
        else
        {
            // Cache lookup failed, does not exist.
            return 0;
        }
    }
}

File *Filesystem::findParent(String path, File *pStartNode, String &filename)
{
    // Work forwards to the end of the path string, attempting to find the last '/'.
    ssize_t lastSlash = -1;
    for (size_t i = 0; i < path.length(); i = path.nextCharacter(i))
        if (path[i] == '/') lastSlash = i;

    // Now, if there were no slashes, the parent node is pStartNode.
    if (lastSlash == -1)
    {
        filename = path;
        return pStartNode;
    }
    else
    {
        // Else split the filename off from the rest of the path and follow it.
        filename = path.split(lastSlash+1);
        // Remove the trailing '/' from path;
        path.chomp();
        return findNode(pStartNode, path);
    }
}
