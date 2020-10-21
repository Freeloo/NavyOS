/*
 * Copyright (C) 2020  Jordan DALCQ & contributors
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


#include <pthread.h>

#include <Navy/syscall.h>
#include <Navy/macro.h>

__no_return void
pthread_exit(void *retptr)
{
    __unused(retptr);
    syscall(SYS_texit, (uint32_t) retptr, 0, 0);
    for(;;);
}

pthread_t 
pthread_self(void)
{
    return syscall(SYS_gettid, 0, 0, 0);
}
