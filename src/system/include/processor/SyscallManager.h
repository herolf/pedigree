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

#ifndef KERNEL_PROCESSOR_SYSCALLMANAGER_H
#define KERNEL_PROCESSOR_SYSCALLMANAGER_H

#include <processor/state.h>
#include <processor/SyscallHandler.h>
#include <processor/Syscalls.h>

/** @addtogroup kernelprocessor
 * @{ */

/** The syscall manager allows syscall handler registrations and handles syscalls */
class SyscallManager
{
  public:
    /** Get the syscall manager instance
     *\return instance of the syscall manager */
    static SyscallManager &instance();
    /** Register a syscall handler
     *\param[in] Service the service number you want to register
     *\param[in] pHandler the interrupt handler
     *\return true, if successfully registered, false otherwise */
    virtual bool registerSyscallHandler(Service_t Service, SyscallHandler *pHandler) = 0;
    /** Calls a syscall. */
    virtual uintptr_t syscall(Service_t service, uintptr_t function, uintptr_t p1=0, uintptr_t p2=0, uintptr_t p3=0, uintptr_t p4=0,
                              uintptr_t p5=0) = 0;

  protected:
    /** The constructor */
    inline SyscallManager();
    /** The destructor */
    inline virtual ~SyscallManager();

  private:
    /** The copy-constructor
     *\note Not implemented (singleton) */
    SyscallManager(const SyscallManager &);
    /** The copy-constructor
     *\note Not implemented (singleton) */
    SyscallManager &operator = (const SyscallManager &);
};

/** @} */

//
// Part of the Implementation
//
SyscallManager::SyscallManager()
{
}
SyscallManager::~SyscallManager()
{
}

#endif
