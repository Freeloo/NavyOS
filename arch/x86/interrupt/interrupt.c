/*
 * Copyright (C) 2020 Jordan DALCQ & contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#include "kernel/log.h"

#include "arch/arch.h"

#include "arch/x86/device/vga.h"
#include "arch/x86/device/keyboard.h"
#include "arch/x86/interrupt/apic.h"
#include "arch/x86/interrupt/pic.h"
#include "arch/x86/interrupt/interrupt.h"
#include "arch/x86/memory/virtual.h"
#include "arch/x86/memory/task.h"

#include <stdlib.h>
#include <stdint.h>

#include <Navy/macro.h>
#include <Navy/syscall.h>

unsigned int tick = 0;

const char *exceptions[32] = {
    "Division by zero",
    "Debug",
    "Non-maskable Interrupt",
    "Breakpoint",
    "Overflow",
    "Bound Range Exceeded",
    "Invalid Opcode",
    "Device Not Available",
    "Double Fault",
    "Coprocessor Segment Overrun",
    "Invalid TSS",
    "Segment Not Present",
    "Stack-Segment Fault",
    "General Protection Fault",
    "Page Fault",
    "Reserved",
    "x87 Floating-Point Exception",
    "Aligment Check",
    "Machine Check",
    "SIMD Floating-Point Exception",
    "Virtualization Exception" "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Security Exception",
    "Reserved"
};

void
register_dump(struct InterruptStackFrame stackframe)
{
    int32_t cr0;
    int32_t cr2;
    int32_t cr3;
    int32_t cr4;

    __asm__ volatile ("mov %%cr0, %0":"=r" (cr0));
    __asm__ volatile ("mov %%cr2, %0":"=r" (cr2));
    __asm__ volatile ("mov %%cr3, %0":"=r" (cr3));
    __asm__ volatile ("mov %%cr4, %0":"=r" (cr4));

    klog(NONE, "CS=%04x      DS=%04x      ES=%04x      FS=%04x       GS=%04x\n",
         stackframe.cs, stackframe.ds, stackframe.es, stackframe.fs, stackframe.gs);
    klog(NONE, "EAX=%08x EBX=%08x ECX=%08x EDX=%08x\n", stackframe.eax,
         stackframe.ebx, stackframe.ecx, stackframe.edx);
    klog(NONE, "EDI=%08x ESI=%08x EBP=%08x ESP=%08x\n", stackframe.edi,
         stackframe.esi, stackframe.ebp, stackframe.esp);
    klog(NONE, "INT=%08x ERR=%08x EIP=%08x FLG=%08x\n", stackframe.intno,
         stackframe.err, stackframe.eip, stackframe.eflags);
    klog(NONE, "CR0=%08x CR2=%08x CR3=%08x CR4=%08x\n", cr0, cr2, cr3, cr4);
}

void
backtrace(uint32_t * ebp)
{
    uint32_t *eip;

    while (ebp)
    {
        eip = ebp + 1;
        if (*eip)
        {
            klog(NONE, "[ 0x%x ]\n", *eip);
        }

        ebp = (uint32_t *) * ebp;
    }
}

unsigned int
fetch_tick(void)
{
    return tick;
}

uint32_t
interrupts_handler(uint32_t esp, struct InterruptStackFrame stackframe)
{
    uint32_t ebp;

    if (stackframe.intno < 32)
    {
        if (stackframe.intno > 1)
        {
            klog(ERROR, "%s (INT: %x, ERR: %08d)\n", exceptions[stackframe.intno],
                 stackframe.intno, stackframe.err);
        }

        disable_vga_cursor();

        klog(NONE, "\n\n === CPU DUMP === \n\n");
        register_dump(stackframe);
        klog(NONE, "\n\n=== BACKTRACE ===\n\n");
        ebp = stackframe.ebp;
        backtrace(&ebp);

        PIC_sendEOI(stackframe.intno);

        while (1)
        {
            keyscan();
            if (getLastKeyCode() == 28)
            {
                break;
            }
        }

        disable_interrupts();
        hlt();
    }

    else if (stackframe.intno < 48)
    {
        uint8_t irq_nbr = stackframe.intno - 32;

        if (irq_nbr == 0)
        {
            tick += 1;
            esp = sched(esp);
        }

        if (irq_nbr == 1)
        {
            keyscan();
        }

    }

    else if (stackframe.intno == 69)
    {
        stackframe.eax = syscall(stackframe.eax, stackframe.ebx, stackframe.ecx, stackframe.edx);
    }

    else if (stackframe.intno == 70)
    {
        esp = sched(esp);
    }

    PIC_sendEOI(stackframe.intno);
    return esp;
}
