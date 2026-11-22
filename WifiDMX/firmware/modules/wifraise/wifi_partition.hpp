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

class WifiPartition {
private:
    inline static const int field_size = 64;
    inline static intptr_t start = 0;
    inline static int length = 0;
    inline static char ssid[field_size]{0};
    inline static char password[field_size]{0};
    inline static char apssid[field_size]{0};
    inline static char appassword[field_size]{0};

    static bool read_bytes(char* buf, int offset) {
        cflash_flags_t flags;
        flags.flags = (CFLASH_OP_VALUE_READ << CFLASH_OP_LSB) | (CFLASH_SECLEVEL_VALUE_SECURE << CFLASH_SECLEVEL_LSB);
        int ret = rom_flash_op(flags, start + XIP_BASE + offset, field_size, (uint8_t*)buf);
        return (ret == 0);
    }

    static bool write() {
        uint8_t flash_buffer[FLASH_SECTOR_SIZE] = {};
        memcpy(flash_buffer + field_size * 0, ssid, strlen(ssid) + 1);
        memcpy(flash_buffer + field_size * 1, password, strlen(password) + 1);
        memcpy(flash_buffer + field_size * 2, apssid, strlen(apssid) + 1);
        memcpy(flash_buffer + field_size * 3, appassword, strlen(appassword) + 1);
        uint32_t status = save_and_disable_interrupts();
        flash_range_erase(start, FLASH_SECTOR_SIZE);
        flash_range_program(start, (const uint8_t *)flash_buffer, FLASH_SECTOR_SIZE);
        restore_interrupts(status);
        return true;
    }

public:
    static void setup() {
        if (get_partition_address("WIFI", &start, &length)) {
            fraise_printf("l WIFI partition found 0x%08x %d\n", start, length);
        } else fraise_printf("l WIFI partition not found!\n");
    }
    static bool is_initialized() {
        return (start != 0) && (length != 0);
    }
    static const char * get_field(int num_field, char (&field)[field_size]) {
        int len = 0;
        if(! is_initialized()) goto error;
        if(!read_bytes(field, field_size * num_field)) goto error;
        if(field[0] <= 0) goto error;
        len = strnlen(field, field_size);
        if(len > 0 && len < field_size - 1) return field;
        error:
            field[0] = 0;
            return field;
    }
    static const char *get_ssid() {
        return get_field(0, ssid);
    }
    static const char *get_password() {
        return get_field(1, password);
    }
    static const char *get_apssid() {
        return get_field(2, apssid);
    }
    static const char *get_appassword() {
        return get_field(3, appassword);
    }
    static bool set(const char *ssid_new, const char *passwd_new) {
        if(! is_initialized()) return false;
        strncpy(ssid, ssid_new, field_size);
        strncpy(password, passwd_new, field_size);
        write();
        return true;
    }
    static bool setap(const char *apssid_new, const char *appasswd_new) {
        if(! is_initialized()) return false;
        strncpy(apssid, apssid_new, field_size);
        strncpy(appassword, appasswd_new, field_size);
        write();
        return true;
    }
    static char get_char(int n) {
        if(! is_initialized()) return 0;
        return 0;
    }
};
