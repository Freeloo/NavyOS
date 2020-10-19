/*
 * Copyright (C) 2020 Jordan DALCQ & contributors
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

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
static const char *LOG_MSG[] = {
    "\033[34mLOG\033[39m", "\033[31mERROR\033[39m", "\033[33mWARNING\033[39m",
    "\033[35mOK\033[39m", "\033[41mPANIC\033[49m"
};

void
klog(Level level, const char *format, ...)
{
    char output[512];
    va_list ap;
    va_start(ap, format);

    if (level == PANIC)
    {
        debug_print("\n");
    }

    if (level != NONE)
    {
        debug_print("[ ");
        debug_print(LOG_MSG[level]);
        debug_print(" ] ");
    }

    vs_printf(output, format, ap);

    debug_print(output);
    va_end(ap);
}
