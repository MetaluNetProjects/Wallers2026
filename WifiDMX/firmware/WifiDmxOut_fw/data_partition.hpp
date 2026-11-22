/**
 * Copyright (c) 2026 metalu.net
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once
#include "partition_info.h"
#include <string>
#include "pico/bootrom.h"
#include "boot/picobin.h"
#include "hardware/flash.h"
#include "fraise.h"

class DataPartition {
private:
    intptr_t partition_start = 0;
    int partition_length = 0;
    char *buffer = nullptr;
    int index = 0;
    int length = 0;

    bool write() {
        int block_start = partition_start + FLASH_SECTOR_SIZE * ((index - 1) / FLASH_SECTOR_SIZE);
        fraise_printf("l DataPartition write flash at %x\n", block_start);
        uint32_t status = save_and_disable_interrupts();
        flash_range_erase(block_start, FLASH_SECTOR_SIZE);
        flash_range_program(block_start, (const uint8_t *)buffer, FLASH_SECTOR_SIZE);
        restore_interrupts(status);
        return true;
    }

public:
    void setup(const char *partname) {
        if (get_partition_address(partname, &partition_start, &partition_length)) {
            fraise_printf("l DataPartition %s found 0x%08x %d\n", partname, partition_start, partition_length);
        } else fraise_printf("l DataPartition %s not found!\n", partname);
    }
    bool is_initialized() {
        return (partition_start != 0) && (partition_length != 0);
    }
    void start_write(int new_size) {
        length = new_size;
        if(buffer) free(buffer);
        buffer = nullptr;
        if(length > (partition_length - 4)) return;
        buffer = (char*)malloc(FLASH_SECTOR_SIZE);
        ((int*)buffer)[0] = length;
        index = 4;
    }
    void write_char(char c) {
        if(buffer == nullptr || index >= length) return;
        buffer[(index++) % FLASH_SECTOR_SIZE] = c;
        if((index % FLASH_SECTOR_SIZE) == 0) write();
        if(index == length) finish_write();
    }
    void finish_write() {
        if(index % FLASH_SECTOR_SIZE != 0) write();
        free(buffer);
        buffer = nullptr;
        fraise_printf("l DataPartition finish write %d bytes\n", length);
    }
    char *get_address() {
        return (char*)((intptr_t)XIP_NOCACHE_NOALLOC_NOTRANSLATE_BASE + partition_start + 4);
    }
    int get_length() {
        int l = ((int*)(XIP_NOCACHE_NOALLOC_NOTRANSLATE_BASE + partition_start))[0];
        if(l == 0xffffffff) l = 0; // partition not yet written
        return l;
    }
    void fraise_receivebytes(const char* data, uint8_t len) {
        char command = fraise_get_uint8();
        data++; len--;
        switch(command) {
            case 1: {
                    int l = fraise_get_uint32();
                    data += 4;
                    len -= 4;
                    start_write(l);
                    while(len--) write_char(*data++);
                    fraise_printf("l DataPartition start write for %d bytes buffer=%x index=%d\n", length, buffer, index);
                }
                break;
            case 2:
                while(len--) write_char(*data++);
                //fraise_printf("l DataPartition written %d / %d bytes\n", index, length);
                break;
            case 10:
                fraise_printf("l DataPartition start:%x maxlength:%d length:%d\n", get_address(), partition_length - 4, get_length());
                break;
        }
    }
};
