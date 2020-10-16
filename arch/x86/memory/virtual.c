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


#include "arch/x86/memory/virtual.h"
#include "kernel/memory/physical.h"
#include "kernel/log.h"

#include <stdint.h>
#include <stddef.h>
#include <multiboot2.h>
#include <string.h>
#include <Navy/macro.h>

extern uint32_t __start;
extern uint32_t __end;

static struct PAGE_DIR kernel_page_dir __attribute__((aligned(PAGE_SIZE))) = {};
static struct PAGE_TABLE kernel_page_table[256] __attribute__((aligned(PAGE_SIZE))) = {};
size_t page_table_index = 0;

void
virtual_free(void *address_space, Range range)
{
    size_t i;
    size_t offset;
    size_t dir_index;
    size_t table_index;

    PageDirEntry *dir_entry;
    PageTableEntry *table_entry;
    struct PAGE_TABLE *table;
    struct PAGE_DIR *page_dir = (struct PAGE_DIR *) (address_space);

    for (i = 0; i < range.begin / PAGE_SIZE; i++)
    {
        offset = i * PAGE_SIZE;

        dir_index = __pd_index(range.begin + offset);
        dir_entry = &page_dir->entries[dir_index];

        table = (struct PAGE_TABLE *) (dir_entry->page_framenbr * PAGE_SIZE);

        table_index = __pt_index(range.begin + offset);
        table_entry = &table->entries[table_index];

        if (table_entry->present)
        {
            table_entry->as_uint = 0;
        }
    }
} 

void *kernel_space_address_space()
{
    return &kernel_page_dir;
}


void
init_paging(BootInfo *info)
{
    Range kernel_range;
    Range page_zero;

    size_t i;
    PageDirEntry *dir_entry;

    kernel_range.begin = (uintptr_t) & __start;
    kernel_range.end = (uintptr_t) & __end;

    page_zero.begin = 0;
    page_zero.end = PAGE_SIZE;

    init_bitmap();

    for (i = 0; i < info->memory_map_size; i++)
    {
        MemoryMapEntry *entry = &info->mmap[i];

        if (entry->type == MULTIBOOT_MEMORY_AVAILABLE)
        {
            physical_set_free(entry->range);
        }
    }

    for (i = 0; i < 256; i++)
    {
        dir_entry = &kernel_page_dir.entries[i];
        dir_entry->user_supervisor = 0;
        dir_entry->read_write = 1;
        dir_entry->present = 1;
        dir_entry->page_framenbr = (size_t) &kernel_page_table[i] / PAGE_SIZE;
    }

    set_total_memory(info->memory_usable);

    memory_map_identity(kernel_space_address_space(), kernel_range, MEMORY_NONE); 
    klog(OK, "Kernel mapped\n");
    
    for (i = 0; i < info->modules_size; i++)
    {
        memory_map_identity(kernel_space_address_space(), info->modules[i].range, MEMORY_NONE);
    }

    klog(OK, "Modules mapped\n");

    virtual_free(kernel_space_address_space(), page_zero);
    physical_set_used(page_zero);

    address_space_switch(kernel_space_address_space());
    _asm_init_paging();
    
}

void 
address_space_switch(void *address_space)
{
    _asm_load_pagedir(virtual_to_physical(kernel_space_address_space(), (uintptr_t) address_space));
}

bool
is_virtual_present(void *addr_space, uintptr_t virtual_addr)
{
    struct PAGE_TABLE page_table;
    int page_table_index;
    PageTableEntry page_table_entry;

    struct PAGE_DIR *page_directory = (struct PAGE_DIR *) (addr_space);
    int page_directory_index = __pd_index(virtual_addr);
    PageDirEntry page_dir_entry = page_directory->entries[page_directory_index];

    if (!page_dir_entry.present)
    {
        return false;
    }

    page_table = *(struct PAGE_TABLE *) (page_dir_entry.page_framenbr * PAGE_SIZE);
    page_table_index = __pt_index(virtual_addr);
    page_table_entry = page_table.entries[page_table_index];

    return page_table_entry.present;
}

bool
is_virtual_free(void *address_space, uintptr_t addr)
{
    size_t directory_index;
    size_t table_index;
    PageDirEntry page_dir_entry;
    PageTableEntry page_table_entry;
    struct PAGE_TABLE *page_table;

    struct PAGE_DIR *page_dir = (struct PAGE_DIR *) (address_space);
    directory_index = __pd_index(addr);
    page_dir_entry = page_dir->entries[directory_index];

    if (!page_dir_entry.present)
    {
        return false;
    }

    page_table = (struct PAGE_TABLE *) (page_dir_entry.page_framenbr * PAGE_SIZE);
    table_index = __pt_index(addr);
    page_table_entry = page_table->entries[table_index];

    return !page_table_entry.present;
}

void
memory_alloc_identity(void *address_space, uint8_t mode, uintptr_t * out_addr)
{
    size_t i;

    for (i = 1; i < 256 * 1024; i++)
    {
        Range mem_range;

        mem_range.begin = i * PAGE_SIZE;
        mem_range.end = PAGE_SIZE;

        if (!is_virtual_present(address_space, mem_range.begin)
            && !physical_is_used(mem_range))
        {
            physical_set_used(mem_range);
            virtual_map(address_space, mem_range, mem_range.begin, mode);
        }

        if (mode & CLEAR)
        {
            memset((void *) mem_range.begin, 0, PAGE_SIZE);
        }

        *out_addr = mem_range.begin;
    }

    klog(ERROR, "Failed to allocate identity mapped page !\n");
    *out_addr = 0;

    disable_interrupts();
    hlt();
}

int
virtual_map(void *address_space, Range range, uintptr_t addr, uint8_t mode)
{
    size_t i;
    size_t offset;
    size_t dir_index;
    size_t table_index;

    PageDirEntry dir_entry;
    PageTableEntry table_entry;
    struct PAGE_TABLE *table;
    struct PAGE_DIR *page_dir = (struct PAGE_DIR *) (address_space);

    __unused(table_entry);

    for (i = 0; i < get_range_size(range) / PAGE_SIZE; i++)
    {
        offset = i * PAGE_SIZE;
        dir_index = __pd_index(addr + offset);
        dir_entry = page_dir->entries[dir_index];

        table = (struct PAGE_TABLE *) (dir_entry.page_framenbr * PAGE_SIZE);

        if (!dir_entry.present)
        {
            memory_alloc_identity(page_dir, CLEAR, (uintptr_t *) & kernel_page_table);

            dir_entry.present = 1;
            dir_entry.read_write = 1;
            dir_entry.user_supervisor = 1;  
            dir_entry.page_framenbr = (uint32_t) (table) >> 12;
        }

        table_index = __pt_index(addr + offset);
        table_entry = table->entries[table_index];

        table_entry.present = 1;
        table_entry.read_write = 1;
        table_entry.user_supervisor = mode & USER;
        table_entry.page_framenbr = (range.begin + offset) >> 12;
    }

    _asm_reload_pagedir();
    return 0;
}

void 
memory_map_identity(void *address_space, Range range, uint8_t mode)
{
    if (!is_range_page_aligned(range))
    {
        klog(ERROR, "This memory range is not page aligned ! (START: 0%x, LEN: %d)", range.begin, get_range_size(range));
        disable_interrupts();
        hlt();
    }

    physical_set_used(range);
    virtual_map(address_space, range, range.begin, mode);

    if (mode & CLEAR)
    {
        memset((void *) range.begin, 0, get_range_size(range));
    }
}

uintptr_t 
virtual_to_physical(void *address_space, uintptr_t addr)
{
    int page_dir_index;
    int page_table_index;

    PageDirEntry page_dir_entry;
    PageTableEntry page_table_entry;

    struct PAGE_TABLE page_table;

    struct PAGE_DIR *page_dir = (struct PAGE_DIR *) (address_space);

    page_dir_index = __pd_index(addr);
    page_dir_entry = (PageDirEntry) page_dir->entries[page_dir_index];

    if (!page_dir_entry.present)
    {
        return 0;
    }

    page_table = *(struct PAGE_TABLE *) (page_dir_entry.page_framenbr * PAGE_SIZE);

    page_table_index = __pt_index(addr);
    page_table_entry = (PageTableEntry) page_table.entries[page_table_index];

    if (!page_table_entry.present)
    {
        return 0;
    }

    return (page_table_entry.page_framenbr * PAGE_SIZE) + (addr & 0xfff);

}