/*  Copyright (C) 2024 Kuraga Tech
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 3.
 */

#include "esptoolqt.h"

#include "QDebug"
#include "targets/esp_base.h"
#include "targets/esp32.h"
#include "targets/esp32s3.h"

using std::vector;

// run an arbitrary SPI flash command
uint32_t EspToolQt::runSpiFlashCommand(uint32_t command, std::vector<uint8_t> data, uint32_t read_bits, uint32_t addr, uint32_t addr_len,  uint32_t dummy_len) {

    // SPI_USR register flags
    const uint32_t SPI_USR_COMMAND = 1 << 31;
    const uint32_t SPI_USR_ADDR    = 1 << 30;
    const uint32_t SPI_USR_DUMMY   = 1 << 29;
    const uint32_t SPI_USR_MISO    = 1 << 28;
    const uint32_t SPI_USR_MOSI    = 1 << 27;

    // SPI peripheral "command" bitmasks for SPI_CMD_REG
    const uint32_t SPI_CMD_USR = 1 << 18;

    // SPI shift values
    const uint32_t SPI_USR2_COMMAND_LEN_SHIFT = 28;
    const uint32_t SPI_USR_ADDR_LEN_SHIFT     = 26;

    // remember registers before modifications
    uint32_t old_spi_usr  = read_reg(target->SPI_USR_REG());
    uint32_t old_spi_usr2 = read_reg(target->SPI_USR2_REG());

    uint32_t data_bits = data.size() * 8;

    // ESP32 and later chips have a more sophisticated way to set up "user" commands
    // this function set_data_lengths
    uint32_t mosi_bits = data_bits;
    uint32_t miso_bits = read_bits;
    if (target->SPI_MOSI_DLEN_OFFS()) {
        if (mosi_bits > 0) write_reg(target->SPI_MOSI_DLEN_REG(), mosi_bits - 1);
        if (miso_bits > 0) write_reg(target->SPI_MISO_DLEN_REG(), miso_bits - 1);
        uint32_t length_flags = 0;
        if (dummy_len > 0) length_flags |= dummy_len - 1;
        if (addr_len  > 0) length_flags |= (addr_len - 1) << SPI_USR_ADDR_LEN_SHIFT;
        if (length_flags) {
            if(!write_reg(target->SPI_USR1_REG(), length_flags)) return 0;
        }
    } else {
        // simple method for pre esp32 mcus
        uint32_t SPI_DATA_LEN_REG = target->SPI_USR1_REG();
        uint32_t SPI_MOSI_BITLEN_S = 17;
        uint32_t SPI_MISO_BITLEN_S = 8;
        uint32_t mosi_mask = (mosi_bits == 0) ? 0 : (mosi_bits - 1);
        uint32_t miso_mask = (miso_bits == 0) ? 0 : (miso_bits - 1);
        uint32_t length_flags = (miso_mask << SPI_MISO_BITLEN_S) | (mosi_mask << SPI_MOSI_BITLEN_S);
        if (dummy_len > 0) length_flags |= dummy_len - 1;
        if (addr_len  > 0) length_flags |= (addr_len - 1) << SPI_USR_ADDR_LEN_SHIFT;
        if(!write_reg(SPI_DATA_LEN_REG, length_flags)) return 0;
    }

    uint32_t flags = SPI_USR_COMMAND;
    if (read_bits > 0) flags |= SPI_USR_MISO;
    if (data_bits > 0) flags |= SPI_USR_MOSI;
    if (addr_len  > 0) flags |= SPI_USR_ADDR;
    if (dummy_len > 0) flags |= SPI_USR_DUMMY;

    write_reg(target->SPI_USR_REG(), flags);

    write_reg(target->SPI_USR2_REG(), (7 << SPI_USR2_COMMAND_LEN_SHIFT) | command);

    if (addr && addr_len) write_reg(target->SPI_ADDR_REG(), addr);

    if (data_bits == 0) {
        write_reg(target->SPI_W0_REG(), 0);  // clear data register before we read it
    } else {
        // make data length multiple of 4
        uint32_t padding_required = 4 - data.size() % 4;
        if (padding_required) data.resize(data.size() + padding_required, 0x00);
        uint32_t next_reg = target->SPI_W0_REG();
        for (int i = 0; i < data.size(); i+=4){
            uint32_t x =0;
            x  = data[i];
            x |= data[i+1] << 8;
            x |= data[i+2] << 16;
            x |= data[i+3] << 24;
            write_reg(next_reg, x);
            next_reg += 4;
        }
    }
    write_reg(target->SPI_CMD_REG(), SPI_CMD_USR);

    // wait done
    bool OK = false;
    for(int i = 0; i < 10; i++){
        if ((read_reg(target->SPI_CMD_REG()) & SPI_CMD_USR) == 0) {
            OK = true;
            break;
        }
    }
    if (!OK) return 0;

    uint32_t status = read_reg(target->SPI_W0_REG());

    // restore some SPI controller registers
    write_reg(target->SPI_USR_REG(), old_spi_usr);
    write_reg(target->SPI_USR2_REG(), old_spi_usr2);

    return status;
}

bool EspToolQt::getChipDescription(QString* chip_description) {
    return target->get_chip_description(chip_description, (void*)this);
}

bool EspToolQt::getChipFeatures(QString* features) {
    return target->get_chip_features(features, (void*)this);
}

uint32_t EspToolQt::getCrystalFrequency() {
    return target->get_crystal_freq((void*)this);
}
