/*  Copyright (C) 2024 Kuraga Tech
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 3.
 */

#include "esptoolqt.h"

#include <QString>
#include <QCryptographicHash>

#include <QDEbug>
#include <iostream>
#include <map>

#include "espdefines.h"
#include "targets/esp_base.h"
#include "targets/esp8266.h"
#include "targets/esp32.h"
#include "targets/esp32s2.h"
#include "targets/esp32s3.h"
#include "targets/esp32c2.h"
#include "targets/esp32c3.h"
#include "targets/esp32c6.h"
#include "targets/esp32h2.h"
//#include "targets/esp32p4.h"

using std::vector;
using std::string;
using SlipReply = EspToolQt::SlipReply;

EspToolQt::EspToolQt(QObject *parent) : QObject {parent} {
    serial = new QSerialPort(this);
    //available_targets
    available_targets.push_back(new Esp8266(parent));
    available_targets.push_back(new Esp32(parent));
    available_targets.push_back(new Esp32S2(parent));
    available_targets.push_back(new Esp32S3(parent));
    available_targets.push_back(new Esp32C2(parent));
    available_targets.push_back(new Esp32C3(parent));
    available_targets.push_back(new Esp32C6(parent));
    available_targets.push_back(new Esp32H2(parent));
    //available_targets.push_back(new Esp32P4(parent));
}

uint32_t EspToolQt::read_reg(uint32_t address) {
    vector<uint8_t> address_vec;
    address_vec.push_back(address);
    address_vec.push_back(address >> 8);
    address_vec.push_back(address >> 16);
    address_vec.push_back(address >> 24);

    vector<uint8_t> slip = slip_encode(0x0a, address_vec);
    serialWrite(slip);
    vector<uint8_t> reply = serialReadOneFrame();

    SlipReply slip_reply = slip_parse(reply);
    if (slip_reply.valid) {
            return slip_reply.value;
    } else {
            return 0;
    }
}

bool EspToolQt::write_reg(uint32_t address, uint32_t data) {
    vector<uint8_t> data_vec;
    appendU32(&data_vec, address);
    appendU32(&data_vec, data);
    appendU32(&data_vec, 0xFFFFFFFF);
    appendU32(&data_vec, 0);
    return slipCommandSend(ESP_WRITE_REG, data_vec);
}

void EspToolQt::appendU32(std::vector<uint8_t>* vec, uint32_t x) {
    vec->push_back(x);       // 04-07 checksum of data field
    vec->push_back(x >> 8);
    vec->push_back(x >> 16);
    vec->push_back(x >> 24);
}

void EspToolQt::appendVec(std::vector<uint8_t>& append_to, const std::vector<uint8_t>& data) {
    append_to.insert(append_to.end(), data.begin(), data.end());
}

// To calculate checksum, start with seed value 0xEF and XOR each individual byte in the “data to write”. The 8-bit result is stored in the checksum field of the packet header (as a little endian 32-bit value).
uint8_t EspToolQt::calculate_esp_checksum (const std::vector<uint8_t> &vec) {
    uint8_t checksum = 0xEF;
    for (uint8_t x : vec) {
            checksum ^= x;
    }
    return checksum;
}

vector<uint8_t> EspToolQt::calculate_md5_hash (std::vector<uint8_t> &data) {
    QCryptographicHash hash_calc(QCryptographicHash::Md5);
    hash_calc.addData(reinterpret_cast<const char*>(data.data()), data.size());
    QByteArray hash_qb = hash_calc.result();
    vector<uint8_t> hash;
    hash.insert(hash.end(), reinterpret_cast<uint8_t*>(hash_qb.begin()), reinterpret_cast<uint8_t*>(hash_qb.end()));
    return hash;
}

void EspToolQt::progress(float progress) {
    if (serial_progress_enabled) {
        QString str= QString::number(progress, 'f', 2);
        str += "%";
        std::cout << "\r" << "Progress: " << str.toStdString() << std::flush;
        if (progress == 100) std::cout << std::endl << std::flush;
    }
}

uint32_t EspToolQt::flashSizeIdToBytes (uint8_t size_id) {
    uint32_t flash_size;
    std::map<uint8_t, uint32_t>::iterator flash_size_iter;

    std::map<uint8_t, uint32_t> sizes_map;
    sizes_map = {
        {0x12,        256 * 1024},  // 256KB
        {0x13,        512 * 1024},  // 512KB
        {0x14,   1 * 1024 * 1024},  // 1MB
        {0x15,   2 * 1024 * 1024},  // 2MB
        {0x16,   4 * 1024 * 1024},  // 4MB
        {0x17,   8 * 1024 * 1024},  // 8MB
        {0x18,  16 * 1024 * 1024},  // 16MB
        {0x19,  32 * 1024 * 1024},  // 32MB
        {0x1A,  64 * 1024 * 1024},  // 64MB
        {0x1B, 128 * 1024 * 1024},  // 128MB
        {0x1C, 256 * 1024 * 1024},  // 256MB
        {0x20,  64 * 1024 * 1024},  // 64MB
        {0x21, 128 * 1024 * 1024},  // 128MB
        {0x22, 256 * 1024 * 1024},  // 256MB
        {0x32,        256 * 1024},  // 256KB
        {0x33,        512 * 1024},  // 512KB
        {0x34,   1 * 1024 * 1024},  // 1MB
        {0x35,   2 * 1024 * 1024},  // 2MB
        {0x36,   4 * 1024 * 1024},  // 4MB
        {0x37,   8 * 1024 * 1024},  // 8MB
        {0x38,  16 * 1024 * 1024},  // 16MB
        {0x39,  32 * 1024 * 1024},  // 32MB
        {0x3A,  64 * 1024 * 1042},  // 64MB
    };

    flash_size_iter = sizes_map.find(size_id);
    if (flash_size_iter == sizes_map.end()) { return 0; }
    return flash_size_iter->second;
}

uint32_t EspToolQt::getFlashSize() {
    const uint8_t SPIFLASH_RDID = 0x9f;

    uint32_t flash_id = runSpiFlashCommand(SPIFLASH_RDID, {}, 24);
    uint32_t size_id = flash_id >> 16;
    uint32_t flash_size_bytes = flashSizeIdToBytes(size_id);
    double flash_size_megabytes = (double)flash_size_bytes / 1024 / 1024;

    qInfo() << "Flash size:" << flash_size_megabytes << "Mb";

    return flash_size_bytes;
}

uint32_t EspToolQt::readEfuse(uint8_t n) {
    return read_reg(target->EFUSE_RD_REG_BASE() + (4 * n));
}
