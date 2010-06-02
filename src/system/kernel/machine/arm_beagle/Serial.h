/*
 * Copyright (c) 2010 Matthew Iselin
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

#ifndef MACHINE_ARM_BEAGLE_SERIAL_H
#define MACHINE_ARM_BEAGLE_SERIAL_H

#include <machine/Serial.h>

class ArmBeagleSerial : public Serial
{
  public:
    ArmBeagleSerial();
    virtual ~ArmBeagleSerial();
  
    virtual void setBase(uintptr_t nBaseAddr);
    virtual char read();
    virtual char readNonBlock();
    virtual void write(char c);

    /*
    struct serial
    {
      volatile uint32_t dr;
    } PACKED;
    */
    
    /**
     * The serial device's registers.
     */
    // volatile serial *m_pRegs;
    
    // uintptr_t tmp;
};

#endif
