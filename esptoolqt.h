/*  Copyright (C) 2024 Kuraga Tech
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 3.
 */

#ifndef ESP_TOOL_QT_H
#define ESP_TOOL_QT_H

#include <QObject>
#include <QSerialPort>
#include <vector>
#include "targets/esp_base.h"

enum ResetStrategy { classic_reset, usb_jtag_serial_reset };

struct EspTargetInfo {
    bool connected;
    QString chip_family;
    QString chip_description;
    QString chip_features;
    uint32_t flash_size;
};

class EspToolQt : public QObject
{
    Q_OBJECT
private:

    // stub upload private helpers
    bool mem_begin(uint32_t size_of_data, uint32_t memory_offset, uint32_t max_packet_size);
    bool mem_data_one_block(uint32_t sequence_number, const std::vector<uint8_t>& data);
    bool mem_data(std::vector<uint8_t> data, uint32_t max_packet_size);
    bool mem_end(uint32_t entry_address);

    // write flash private helpers
    bool flashBegin(uint32_t size_of_data, uint32_t number_of_data_packets, uint32_t max_packet_size, uint32_t memory_offset, bool compressed);
    bool flashDataOneBlock(uint32_t sequence_number, std::vector<uint8_t> &data, bool compressed);
    bool flashData(const uint32_t memory_offset, const std::vector<uint8_t>& data, bool compress);

public:
    // helpers
    void appendU32(std::vector<uint8_t>*, uint32_t);
    void appendVec(std::vector<uint8_t>& append_to, const std::vector<uint8_t>& data);
    uint32_t flashSizeIdToBytes (uint8_t size_id);


    explicit EspToolQt(QObject *parent = nullptr);
    QSerialPort* serial = NULL;
    std::vector<EspBase*> available_targets;
    EspBase* target = NULL;
    EspTargetInfo esp_target_info;

    struct SlipReply {bool valid = false; uint8_t command = 0; uint32_t value = 0; std::vector<uint8_t> data;};

    void resetToBoot(ResetStrategy);
    void resetFromBoot();
    ResetStrategy resetStrategy = ResetStrategy::classic_reset;

    // esp_serial
    static std::vector<QString> getPorts();
    std::vector<QString> getFamilies();
    void setPortName(QString);
    bool openPort();
    bool openPort(QString);
    bool openPort(QString port, int baud);
    void closePort();
    bool serialWrite(std::vector<uint8_t>, int timeout_ms = 1000);
    std::vector<uint8_t> serialRead(int timeout_ms = 1000);
    std::vector<uint8_t> serialReadOneFrame(int timeout_ms = 1000);
    bool autoConnect(QString port = NULL);

    // stub upload
    bool stubUpload();

    // chip info
    bool getChipDescription(QString* chip_description);
    bool getChipFeatures(QString* features);
    uint32_t getCrystalFrequency();

    bool changeBaud(uint32_t baud = 460800);
    uint32_t getFlashSize();
    void disconnect();
    std::vector<uint8_t> slip_encode (uint8_t command, std::vector<uint8_t>data, uint32_t checksum = 0);
    bool slipCommandSend (uint8_t command, std::vector<uint8_t>data_field, uint32_t checksum = 0, uint32_t timeout_ms = 1000);

    std::vector<uint8_t> slip_raw_encode (std::vector<uint8_t>&);
    bool slip_raw_send (std::vector<uint8_t>&, int timeout_ms = 1000);
    SlipReply slip_parse (std::vector<uint8_t>);
    uint8_t calculate_esp_checksum (const std::vector<uint8_t> &);
    std::vector<uint8_t> calculate_md5_hash (std::vector<uint8_t> &);

    uint32_t read_reg(uint32_t);
    bool write_reg(uint32_t address, uint32_t data);
    uint32_t readEfuse(uint8_t);
    uint32_t runSpiFlashCommand(uint32_t command, std::vector<uint8_t> data = {}, uint32_t read_bits = 0, uint32_t addr = 0, uint32_t addr_len = 0,  uint32_t dummy_len = 0);

    // read flash
    std::vector<uint8_t> readFlash(uint32_t memory_offset, uint32_t size);

    // write flash
    bool flashUpload(uint32_t memory_offset, std::vector<uint8_t> data, bool compressed = true);

    // verify flash
    bool verifyFlash(uint32_t memory_offset, std::vector<uint8_t> data);

    void progress(float);
    bool serial_progress_enabled = false;

signals:
    void progress_signal(int);
};

#endif // ESP_TOOL_QT_H
