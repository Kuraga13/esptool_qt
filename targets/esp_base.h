/*  Copyright (C) 2024 Kuraga Tech
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 3.
 */

#ifndef ESP_BASE_H
#define ESP_BASE_H

#include <QObject>
#include <QString>
#include <QByteArray>

class EspBase : public QObject
{
    Q_OBJECT
private:
    virtual uint32_t CHIP_DETECT_MAGIC_VALUE() = 0;

public:
    explicit EspBase(QObject *parent) : QObject {parent} {}
    virtual QString CHIP_NAME() = 0;
    virtual QVector<QString> CHIP_TARGETS() = 0;

    // This ROM address has a different value on each chip model
    static uint32_t CHIP_DETECT_MAGIC_REG_ADDR() {return 0x40001000;}
    virtual bool CHIP_COMPARE_MAGIC_VALUE(uint32_t x) {return (x == CHIP_DETECT_MAGIC_VALUE()) ? true : false;}

    // STUB
    virtual uint32_t stub_entry() = 0;
    virtual std::vector<uint8_t> stub_text() = 0;
    virtual uint32_t stub_text_start() = 0;
    virtual std::vector<uint8_t> stub_data() = 0;
    virtual uint32_t stub_data_start() = 0;

    // CONSTANTS
    // Maximum block sized for RAM and Flash writes, respectively.
    virtual uint32_t ESP_RAM_BLOCK(void* esp_tool) {return 0x1800;}
    virtual uint32_t FLASH_SECTOR_SIZE() {return 0x1000;}
    virtual uint32_t FLASH_WRITE_SIZE() {return 0x4000;}

    // SPI
    virtual uint32_t SPI_REG_BASE() = 0;
            uint32_t SPI_CMD_REG()  {return SPI_REG_BASE() + 0x00;}
            uint32_t SPI_ADDR_REG() {return SPI_REG_BASE() + 0x04;}
    virtual uint32_t SPI_USR_REG()  = 0;
    virtual uint32_t SPI_USR1_REG() = 0;
    virtual uint32_t SPI_USR2_REG() = 0;
    virtual uint32_t SPI_W0_REG()   = 0;
    virtual bool     SPI_MOSI_DLEN_OFFS() = 0;
    virtual uint32_t SPI_MOSI_DLEN_REG()  = 0;
    virtual uint32_t SPI_MISO_DLEN_REG()  = 0;

    // efuse
    virtual uint32_t EFUSE_RD_REG_BASE() = 0;

    // functions
    virtual bool get_chip_description(QString* description, void* esp_tool) = 0;
    virtual bool get_chip_features(QString* features, void* esp_tool) = 0;
    virtual uint32_t get_crystal_freq(void* esp_tool) = 0;

    // helper function
    static std::vector<uint8_t> ba_to_vec(QByteArray ba) {
        std::vector<uint8_t> vec;
        vec.insert(vec.end(), reinterpret_cast<uint8_t*>(ba.begin()), reinterpret_cast<uint8_t*>(ba.end()));
        return vec; }
};

#endif // ESP_BASE_H
