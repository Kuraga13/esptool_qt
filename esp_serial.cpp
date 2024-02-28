/*  Copyright (C) 2024 Kuraga Tech
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 3.
 */

#include "esptoolqt.h"
#include "espdefines.h"
#include <cmath>

#include <QThread>
#include <QTime>
#include <QDebug>
#include <QSerialPortInfo>
#include "QtZlib/zconf.h"
#include "QtZlib/zlib.h"
#include <QFile>

#if defined(Q_OS_WIN32)
#  include <qt_windows.h>
#endif

using std::vector;
using std::ceil;
using SlipReply = EspToolQt::SlipReply;

std::vector<QString> EspToolQt::getFamilies() {
    std::vector<QString> families;

    for (auto target : available_targets){
        families.push_back(target->CHIP_NAME());
    }
    return families;
}

vector<QString> EspToolQt::getPorts() {
    vector<QString> port_list;

    QList<QSerialPortInfo> available_ports = QSerialPortInfo::availablePorts();
    for (const QSerialPortInfo &port : available_ports)
    {
        port_list.push_back(port.portName());
    }

    return port_list;
}

void EspToolQt::setPortName(QString name) {
    serial->setPortName(name);
}

bool EspToolQt::openPort() {
    return serial->open(QIODevice::ReadWrite);
}

bool EspToolQt::openPort(QString port) {
    serial->setPortName(port);
    return openPort();
}

bool EspToolQt::openPort(QString port, int baud) {
    serial->setBaudRate(baud);
    return openPort(port);
}

void EspToolQt::closePort() {
    serial->close();
}

bool EspToolQt::serialWrite(vector<uint8_t> data, int timeout_ms) {
    serial->clear();
    serial->write(reinterpret_cast<const char*>(data.data()), data.size());
    return serial->waitForBytesWritten(timeout_ms);
}

vector<uint8_t> EspToolQt::serialRead(int timeout_ms) {
    QTime timeout = QTime::currentTime().addMSecs(timeout_ms);
    vector<uint8_t> data;

    while(QTime::currentTime().msecsTo(timeout) > 0)
    {
        serial->waitForReadyRead(5);
        QByteArray byte_array = serial->readAll();
        if ((byte_array.size() == 0) && (data.size() != 0)) break;
        data.insert(data.end(), reinterpret_cast<uint8_t*>(byte_array.begin()), reinterpret_cast<uint8_t*>(byte_array.end()));
    }

    return data;
}

vector<uint8_t> EspToolQt::serialReadOneFrame(int timeout_ms) {
    QTime timeout = QTime::currentTime().addMSecs(timeout_ms);
    vector<uint8_t> data;
    vector<uint8_t> zero;
    bool frame_started = false;
    bool escape_started = false;

    while(QTime::currentTime().msecsTo(timeout) > 0)
    {
        serial->waitForReadyRead(1);
        while(QTime::currentTime().msecsTo(timeout) > 0){
            QByteArray one_byte = serial->read(1);
            if (one_byte.size() == 0) break;
            uint8_t byte = one_byte.at(0);

            //deal with start of frame
            if (!frame_started) {
                if (byte == 0xC0) {
                    frame_started = true;
                    continue;
                } else {
                    continue;
                }
            }

            //deal with end of frame
            if (byte == 0xC0) {
                frame_started = false;
                return data;
            }

            // deal with all other bytes
            if (byte == 0xDB) {
                escape_started = true;
                continue;
            }

            if (escape_started) {
                if (byte == 0xDC) {
                    data.push_back(0xC0);
                    escape_started = false;
                    continue;
                } else if (byte == 0xDD) {
                    data.push_back(0xDB);
                    escape_started = false;
                    continue;
                } else {
                    break;
                }
            }

            data.push_back(byte);
        }
    }

    // return zero length vector if no valid frame found until timeout
    return zero;
}

bool EspToolQt::autoConnect(QString port) {
    esp_target_info.connected = false;
    closePort(); // close port if it was opened

    const vector<uint8_t> sync_sequence_data = {
        0x07, 0x07, 0x12, 0x20,
        0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
        0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
        0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
        0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55
    };

    vector<uint8_t> sync_sequence = slip_encode(ESP_SYNC, sync_sequence_data);

    std::vector<QString> ports;
    
    if (port.isEmpty()) {
        ports = getPorts();
    } else {
        ports.push_back(port);
    }

    QVector<ResetStrategy> resets;
    resets.append(ResetStrategy::classic_reset);
    resets.append(ResetStrategy::usb_jtag_serial_reset);

    bool done = false;
    QString found_port;

    for (auto port = ports.rbegin(); port < ports.rend(); port++) {

        for (auto reset : resets){
            qInfo() << "Try" << *port << "reset_strategy" << reset;
            bool port_opened = openPort(*port,115200);
            if (port_opened) {
                resetToBoot(reset);

                for (int i = 0; i < 4; i++) {
                    serialWrite(sync_sequence, 50);
                    vector<uint8_t> data = serialRead(50);

                    if (data.size() > 50){
                        done = true;
                        found_port = *port;
                        resetStrategy = reset;
                        break;
                    }

                    if (done) break;
                }

                if (done) break;
                closePort();
            } else {
                qInfo() << "Can't open port";
            }
            
        }
        if (done) break;
    }

    if (done){
        qInfo() << "device_found on" << found_port;
    } else {
        qInfo() << "no device found";
        return false;
    }

    // clean rx buffer
    serialRead(200);

    // determine chip id
    uint32_t x = read_reg(0x40001000);
    if (x == 0) {
        qInfo() << "[ERROR] Connection failed. Can't read target id.";
        return false;
    }


    for (auto attempt_target : available_targets){
        if (attempt_target->CHIP_COMPARE_MAGIC_VALUE(x)) {
            qInfo() << "Chip detected:" << attempt_target->CHIP_NAME();
            target = attempt_target;
            break;
        }
    }
    if (x == 0) {
        qInfo() << "[ERROR] Connection failed. Can't detectmine chip fammily.";
        return false;
    }

    stubUpload();

    changeBaud();

    QString chip_description;
    getChipDescription(&chip_description);
    qInfo().noquote() << "Chip is" << chip_description;

    QString chip_features;
    getChipFeatures(&chip_features);
    qInfo().noquote() << "Features:" << chip_features;

    uint32_t CrystalFrequency = getCrystalFrequency();
    QString CrystalFrequencyStr = QString("Crystal is %1MHz").arg(QString::number(CrystalFrequency));
    qInfo().noquote() << CrystalFrequencyStr;

    uint32_t flash_size = getFlashSize();

    esp_target_info.connected = true;
    esp_target_info.com_port = found_port;
    esp_target_info.chip_family = target->CHIP_NAME();
    esp_target_info.chip_description = chip_description;
    esp_target_info.chip_features = chip_features;
    esp_target_info.flash_size = flash_size;

    return true;
}

bool EspToolQt::stubUpload() {
    uint32_t max_packet_size = target->ESP_RAM_BLOCK((void*)this);

    if (!mem_begin(target->stub_text().size(), target->stub_text_start(), max_packet_size)) {
        qInfo() << "STUB upload failed";
        return false;
    }

    if (!mem_data(target->stub_text(), max_packet_size)) {
        qInfo() << "STUB upload failed";
        return false;
    }

    if (!mem_begin(target->stub_data().size(), target->stub_data_start(), max_packet_size)) {
        qInfo() << "STUB upload failed";
        return false;
    }

    if (!mem_data(target->stub_data(), max_packet_size)) {
        qInfo() << "STUB upload failed";
        return false;
    }

    if (!mem_end(target->stub_entry())) {
        qInfo() << "STUB upload failed";
        return false;
    }

    qInfo() << "STUB uploaded successfully";
    return true;
}

bool EspToolQt::changeBaud(uint32_t baud){
    vector<uint8_t> data_field;
    appendU32(&data_field, (uint32_t)baud);
    appendU32(&data_field, (uint32_t)serial->baudRate());
    vector<uint8_t> packet = slip_encode(0x0f, data_field);
    serialWrite(packet);
    vector<uint8_t> reply = serialReadOneFrame();
    serial->setBaudRate(460800);
    QObject().thread()->msleep(50);
    serial->clear();

    // determine chip id
    uint32_t x = read_reg(0x40001000);
    if (target->CHIP_COMPARE_MAGIC_VALUE(x)) {
        qInfo() << "[OK] Baudrate successfuly changed to" << serial->baudRate();
        return true;
    } else {
        qInfo() << "[ERROR] Failed to set baudrate" << serial->baudRate();
        return false;
    }
}

void EspToolQt::disconnect() {
    serial->close();
    qInfo() << "Serial port closed";
}

vector<uint8_t> EspToolQt::slip_encode (uint8_t command, vector<uint8_t>data, uint32_t checksum) {
    uint16_t data_length = data.size();

    // https://docs.espressif.com/projects/esptool/en/latest/esp32/advanced-topics/serial-protocol.html
    vector<uint8_t> slip = {0x00};      // 00 direction: 0 for command and 1 for response
    slip.push_back(command);        // 01 command
    slip.push_back(data_length);    // 02-03 size of data field
    slip.push_back(data_length >> 8);
    slip.push_back(checksum);       // 04-07 checksum of data field
    slip.push_back(checksum >> 8);
    slip.push_back(checksum >> 16);
    slip.push_back(checksum >> 24);

    slip.insert(slip.end(), data.begin(), data.end());  // data

    vector<uint8_t> encoded_slip;
    encoded_slip.push_back(0xC0);  // start of frame

    // within the packet, all occurrences of 0xC0 and 0xDB are replaced with 0xDB 0xDC and 0xDB 0xDD, respectively.
    for (uint8_t i : slip) {
        if (i == 0xC0) {
            encoded_slip.push_back(0xDB);
            encoded_slip.push_back(0xDC);
        } else if (i == 0xDB) {
            encoded_slip.push_back(0xDB);
            encoded_slip.push_back(0xDD);
        } else {
            encoded_slip.push_back(i);
        }
    }

    encoded_slip.push_back(0xC0);  // end of frame
    return encoded_slip;
}

bool EspToolQt::slipCommandSend (uint8_t command, std::vector<uint8_t>data_field, uint32_t checksum, uint32_t timeout_ms) {
    vector<uint8_t> packet = slip_encode(command, data_field, checksum);
    serialWrite(packet);
    vector<uint8_t> reply = serialReadOneFrame(timeout_ms);
    SlipReply slip_reply = slip_parse(reply);
    if (slip_reply.valid && slip_reply.command == command && slip_reply.data[0] == 0)
    {
        return true;
    } else {
        return false;
    }
}

std::vector<uint8_t> EspToolQt::slip_raw_encode (std::vector<uint8_t>& unencoded_vec) {
    vector<uint8_t> encoded_slip;
    encoded_slip.push_back(0xC0);  // start of frame

    // within the packet, all occurrences of 0xC0 and 0xDB are replaced with 0xDB 0xDC and 0xDB 0xDD, respectively.
    for (uint8_t i : unencoded_vec) {
        if (i == 0xC0) {
            encoded_slip.push_back(0xDB);
            encoded_slip.push_back(0xDC);
        } else if (i == 0xDB) {
            encoded_slip.push_back(0xDB);
            encoded_slip.push_back(0xDD);
        } else {
            encoded_slip.push_back(i);
        }
    }

    encoded_slip.push_back(0xC0);  // end of frame
    return encoded_slip;
}

bool EspToolQt::slip_raw_send (std::vector<uint8_t>& data, int timeout_ms) {
    vector <uint8_t> message;
    message = slip_raw_encode(data);
    return serialWrite(message, timeout_ms);
}

SlipReply EspToolQt::slip_parse (vector<uint8_t> parsed_vec) {
    SlipReply slip;

    // check the frame.size() > 4
    if (parsed_vec.size() < 4) {
        slip.valid = false;
        return slip;
    }

    // check the frame is reply frame
    if (parsed_vec[0] != 1) {
        slip.valid = false;
        return slip;
    }

    // check the size of frame is valid
    uint16_t data_size = (parsed_vec[2]) | (parsed_vec[3] << 8);
    uint16_t expected_frame_size = data_size + 8;
    if (parsed_vec.size() != expected_frame_size) {
        slip.valid = false;
        return slip;
    }

    // FRAME IS VALID, ALL CHECKS PASSED
    slip.valid = true;
    slip.command = parsed_vec[1];
    slip.value = (parsed_vec[4]) | (parsed_vec[5] << 8) | (parsed_vec[6] << 16) | (parsed_vec[7] << 24);
    slip.data.insert(slip.data.begin(), parsed_vec.begin() + 8, parsed_vec.end());

    //        qInfo() << "ParsedVec:" << parsed_vec; ;
    //        qInfo() << "SlipReply is Valid: " << slip.valid;
    //        qInfo() << "Slip Command:" << slip.command;
    //        qInfo() << "Slip Value:" << slip.value;
    //        qInfo() << "Slip Data:" << slip.data;
    return slip;
}

bool EspToolQt::mem_begin(uint32_t size_of_data, uint32_t memory_offset, uint32_t max_packet_size) {
    vector<uint8_t> data_field;
    uint32_t number_of_data_packets = ceil((float)size_of_data / float(max_packet_size));
    appendU32(&data_field, size_of_data);
    appendU32(&data_field, number_of_data_packets);
    appendU32(&data_field, max_packet_size);
    appendU32(&data_field, memory_offset);
    vector<uint8_t> packet = slip_encode(ESP_MEM_BEGIN, data_field);
    serialWrite(packet);
    vector<uint8_t> reply = serialReadOneFrame();
    SlipReply slip_reply = slip_parse(reply);
    if (slip_reply.valid && slip_reply.command == ESP_MEM_BEGIN && slip_reply.data[0] == 0)
    {
        return true;
    } else {
        return false;
    }

}

bool EspToolQt::mem_data_one_block(uint32_t sequence_number, const vector<uint8_t>& data) {
    uint32_t checksum = calculate_esp_checksum(data);
    vector<uint8_t> data_field;
    appendU32(&data_field, data.size());
    appendU32(&data_field, sequence_number);
    uint32_t zero = 0;
    appendU32(&data_field, zero);
    appendU32(&data_field, zero);
    data_field.insert(data_field.end(), data.begin(), data.end());
    vector<uint8_t> packet = slip_encode(ESP_MEM_DATA, data_field, checksum);
    serialWrite(packet);
    vector<uint8_t> reply = serialReadOneFrame();
    SlipReply slip_reply = slip_parse(reply);
    if (slip_reply.valid && slip_reply.command == ESP_MEM_DATA && slip_reply.data[0] == 0)
    {
        return true;
    } else {
        return false;
    }

}

bool EspToolQt::mem_data(vector<uint8_t> data, uint32_t max_packet_size) {

    vector<uint8_t> tmp_vec;
    tmp_vec.reserve(max_packet_size);
    uint32_t frame_n = 0;
    for (uint32_t i = 0; i < data.size(); i++) {
        tmp_vec.push_back(data[i]);
        if (tmp_vec.size() >= max_packet_size) {
            if (!mem_data_one_block(frame_n, tmp_vec)) {
                qInfo() << "Flash Write Failed";
                return false;
            }
            frame_n++;
            tmp_vec.clear();
        }
    }
    if (tmp_vec.size() != 0) {
        if (!mem_data_one_block(frame_n, tmp_vec)) {
            qInfo() << "Flash Write Failed";
            return false;
        }
    }
    return true;
}

bool EspToolQt::mem_end(uint32_t entry_address) {
    vector<uint8_t> data_field;
    uint32_t zero = 0;
    appendU32(&data_field, zero);
    appendU32(&data_field, entry_address);
    if (!slipCommandSend(ESP_MEM_END, data_field)) return false;

    vector<uint8_t> ohai = serialReadOneFrame();
    vector<uint8_t> expected_ohai = {0x4F, 0x48, 0x41, 0x49};
    return (ohai == expected_ohai);
}

std::vector<uint8_t> EspToolQt::readFlash(uint32_t offset, uint32_t size) {
    QTime start = QTime::currentTime();

    vector<uint8_t> received_data;
    vector<uint8_t> zero;

    // check that target is connected
    if (target == NULL || serial == NULL) {
        qInfo() << "[Error] Target is not connected";
        return zero;
    }

    progress(0);

    vector<uint8_t> data_field;
    appendU32(&data_field, offset);
    appendU32(&data_field, size);
    appendU32(&data_field, target->FLASH_SECTOR_SIZE());
    appendU32(&data_field, (uint32_t)1);
    vector<uint8_t> packet = slip_encode(0xD2, data_field);
    serialWrite(packet);
    vector<uint8_t> reply = serialReadOneFrame(); // read reply to command
    if (reply.size() == 0) return zero;

    while (received_data.size() < size) {
        progress((float)received_data.size() / (float)size * 100);

        vector<uint8_t> reply = serialReadOneFrame();
        if (reply.size() == 0) return zero;

        received_data.insert(received_data.end(), reply.begin(), reply.end());

        // check data size validity
        if (received_data.size() < size && reply.size() != target->FLASH_SECTOR_SIZE()) {
            qInfo() << "Inbound data packet too small";
            return zero;
        }

        vector<uint8_t> ack;
        appendU32(&ack, received_data.size());
        slip_raw_send(ack);
    }

    progress(100);
    int duration = start.msecsTo(QTime::currentTime());

    vector<uint8_t> md5_from_esp = serialReadOneFrame();
    // qInfo() << "md5_from_esp" << Qt::hex << md5_from_esp;

    vector<uint8_t> md5_calculated = calculate_md5_hash(received_data);
    // qInfo() << "md5_calculated" << Qt::hex << md5_calculated;

    if (md5_from_esp == md5_calculated) {
        qInfo() << "[OK] MD5 Check Passed";
    } else {
        qInfo() << "[ERROR] MD5 Check Failed";
        return zero;
    }

    float speed = ((float)received_data.size() * 8 / 1000) / ((float)duration / 1000);
    qInfo() << "[OK] Effective read speed [kbit/s]:" << speed;

    return received_data;
}

bool EspToolQt::flashBegin(uint32_t size_of_data, uint32_t number_of_data_packets, uint32_t max_packet_size, uint32_t memory_offset, bool compressed) {
    vector<uint8_t> data_field;
    appendU32(&data_field, size_of_data);
    appendU32(&data_field, number_of_data_packets);
    appendU32(&data_field, max_packet_size);
    appendU32(&data_field, memory_offset);
    return slipCommandSend((compressed) ? ESP_FLASH_DEFL_BEGIN : ESP_FLASH_BEGIN, data_field);
}

bool EspToolQt::flashDataOneBlock(uint32_t sequence_number, std::vector<uint8_t> &data, bool compressed) {
    vector<uint8_t> data_field;
    uint32_t block_size = target->FLASH_WRITE_SIZE();

    // resize block for not compressed write
    if (!compressed && (data.size() < block_size)) {
        data.resize(block_size, 0xFF);
    }

    appendU32(&data_field, data.size());
    appendU32(&data_field, sequence_number);
    appendU32(&data_field, 0);
    appendU32(&data_field, 0);
    appendVec(data_field, data);
    uint32_t hash = calculate_esp_checksum(data);

    return slipCommandSend((compressed) ? ESP_FLASH_DEFL_DATA : ESP_FLASH_DATA, data_field, hash, 5000);
}

// https://stackoverflow.com/questions/4538586/how-to-compress-a-buffer-with-zlib
std::vector<uint8_t> compress_vector(const std::vector<uint8_t>& source) {
    std::vector<uint8_t> destination;
    unsigned long source_length = source.size();
    unsigned long destination_length = compressBound(source_length);
    destination.resize(destination_length);

    compress2((Bytef *) destination.data(), &destination_length, (Bytef *) source.data(), source_length, Z_BEST_COMPRESSION);
    destination.resize(destination_length);

    return destination;
}

bool EspToolQt::flashData(const uint32_t memory_offset, const std::vector<uint8_t>& data, bool compress) {
    uint32_t max_packet_size = target->FLASH_WRITE_SIZE();

    // qInfo() << (compress ? "[OK] Compressed flash upload started" : "[OK] Flash upload started");

    // compress data if needed
    vector<uint8_t> compressed_data;
    if (compress) compressed_data = compress_vector(data);
    const vector<uint8_t>& upload = compress ? compressed_data : data;

    // calculate number of required packets
    uint32_t number_of_data_packets = ceil((float)upload.size() / float(max_packet_size));
    if (!flashBegin(data.size(), number_of_data_packets, max_packet_size, memory_offset, compress)) {
        return false;
    }

    // write flash
    vector<uint8_t> tmp_vec;
    tmp_vec.reserve(max_packet_size);
    uint32_t frame_n = 0;
    for (uint32_t i = 0; i < upload.size(); i++) {
        tmp_vec.push_back(upload[i]);
        if (tmp_vec.size() >= max_packet_size) {
            if (!flashDataOneBlock(frame_n, tmp_vec, compress)) {
                return false;
            }
            frame_n++;
            tmp_vec.clear();
        }
    }
    if (tmp_vec.size() != 0) {
        if (!flashDataOneBlock(frame_n, tmp_vec, compress)) {
            return false;
        }
    }
    
    // Stub only writes each block to flash after 'ack'ing the receive,
    // so do a final dummy operation which will not be 'ack'ed
    // until the last block has actually been written out to flash
    read_reg(target->CHIP_DETECT_MAGIC_REG_ADDR());

    return true;
}

bool EspToolQt::verifyFlashPr(uint32_t memory_offset, std::vector<uint8_t> data) {

    // check md5 of written data
    vector<uint8_t> md5_read_command;
    appendU32(&md5_read_command, memory_offset);
    appendU32(&md5_read_command, data.size());
    appendU32(&md5_read_command, 0);
    appendU32(&md5_read_command, 0);
    vector<uint8_t> md5_read_command_frame = slip_encode (0x13, md5_read_command);
    serialWrite(md5_read_command_frame);
    // read reply with custom timeout. md5 calculation takes some time
    vector<uint8_t> reply = serialReadOneFrame((uint32_t)5000 * (uint32_t)ceil((float)data.size()/((float)1024 * 1024)));
    SlipReply slip_reply = slip_parse(reply);

    // check that we have successfully read md5 from device
    if (slip_reply.valid != true || slip_reply.data.size() < 18) {
        qInfo() << "[ERROR] Failed to get md5 from device";
        return false;
    }

    // pop two status bytes from end of frame
    slip_reply.data.pop_back();
    slip_reply.data.pop_back();

    // md5 hash from esp
    vector<uint8_t> md5_from_esp = slip_reply.data;

    // md5 hash calculated
    vector<uint8_t> md5_calculated = calculate_md5_hash(data);

    if (md5_from_esp == md5_calculated) {
        return true;
    } else {
        return false;
    }
}

// #define ESP_TOOL_UPLOAD_DEBUG
bool EspToolQt::flashUpload(uint32_t memory_offset, std::vector<uint8_t> data, bool compressed) {
    QTime start = QTime::currentTime();
    bool upload_result;

    // check that target is connected
    if (target == NULL || serial == NULL) {
        qInfo() << "[Error] Target is not connected";
        return false;
    }

    // skip zero size writes
    if (data.size() == 0) {
        qInfo() << "[INFO] Zero Sized Write Operation Skipped";
        return true;
    }

    // make data length multiple of 4
    uint32_t padding_required = (4 - data.size() % 4) % 4;
    if (padding_required) data.resize(data.size() + padding_required, 0xFF);

    // split data in 100 blocks
    int total_length = data.size();
    int blocks_per_percent = total_length / 4096 / 100;
    if (blocks_per_percent < 2) blocks_per_percent = 2;
    int block_size = blocks_per_percent * 4096;

    #ifdef ESP_TOOL_UPLOAD_DEBUG
    qInfo() << "[DEBUG] Start of Uploading Process";
    qInfo() << "[DEBUG] Upload data block by block...";
    qInfo() << "[DEBUG] total_length =" << total_length;
    qInfo() << "[DEBUG] blocks_per_percent =" << blocks_per_percent;
    qInfo() << "[DEBUG] block_size =" << block_size;
    #endif // ESP_TOOL_UPLOAD_DEBUG

    // upload data block by block
    for(int offset = memory_offset; offset < memory_offset + total_length; offset += block_size) {
        // amount of data left to write
        int data_left = (total_length - (offset - memory_offset));
        
        // size of current block
        int current_block_size = (data_left >= block_size) ? block_size : data_left;

        // prepare block for writing
        std::vector<uint8_t> block;
        block.insert(block.end(), data.begin() + (offset - memory_offset), data.begin() + (offset - memory_offset) + current_block_size);

        #ifdef ESP_TOOL_UPLOAD_DEBUG
        qInfo() << "[DEBUG] Writing block with offset [bytes]:" << offset;
        qInfo() << "[DEBUG] Data left to write [bytes]:" << data_left;
        qInfo() << "[DEBUG] Current Block Size [bytes]:" << block.size();
        #endif // ESP_TOOL_UPLOAD_DEBUG

        // write block in 3 attempts;
        for(int attempt = 0; attempt < 3; attempt++) {
            if (attempt != 0) qInfo() << "Retry data block";
            upload_result = flashData(offset, block, compressed);
            if (upload_result == true) {
                upload_result = verifyFlashPr(offset, block);
                if (upload_result == true) break;
            }
        }

        // stop writing process if one block failed
        if (upload_result == false) {
            qInfo().noquote() << QString("[ERROR] Flash failed at memory range [0x%1-0x%2]")
                .arg(QString::number(offset, 16).toUpper()).arg(QString::number(offset + current_block_size, 16).toUpper());
            break;
        }

        // update progress bar
        emit progress_signal((offset + current_block_size) * 100 / total_length);
    }

    if (!upload_result) {
        return false;
    }

    // get duration of write for speed test
    int duration = start.msecsTo(QTime::currentTime());
    float speed = ((float)data.size() * 8 / 1000) / ((float)duration / 1000);
    qInfo() << "[OK] Effective speed [kbit/s]:" << speed;
    return true;
}

// #define ESP_TOOL_VERIFY_DEBUG
bool EspToolQt::verifyFlash(uint32_t memory_offset, std::vector<uint8_t> data) {
    bool verify_result;
    
    // split data in 100 blocks
    int total_length = data.size();
    int blocks_per_percent = total_length / 4096 / 100;
    if (blocks_per_percent < 2) blocks_per_percent = 2;
    int block_size = blocks_per_percent * 4096;

    #ifdef ESP_TOOL_VERIFY_DEBUG
    qInfo() << "[DEBUG] Start of Verification Process";
    qInfo() << "[DEBUG] Verifing data block by block...";
    qInfo() << "[DEBUG] total_length =" << total_length;
    qInfo() << "[DEBUG] blocks_per_percent =" << blocks_per_percent;
    qInfo() << "[DEBUG] block_size =" << block_size;
    #endif // ESP_TOOL_VERIFY_DEBUG

    for(int offset = memory_offset; offset < memory_offset + total_length; offset += block_size) {
        // amount of data left to verify
        int data_left = total_length - (offset - memory_offset);
        
        // size of current block
        int current_block_size = (data_left >= block_size) ? block_size : data_left;

        // prepare block for verification 
        std::vector<uint8_t> block;
        block.insert(block.end(), data.begin() + (offset - memory_offset), data.begin() + (offset - memory_offset) + current_block_size);

        #ifdef ESP_TOOL_VERIFY_DEBUG
        qInfo() << "[DEBUG] Verifying block with offset [bytes]:" << offset;
        qInfo() << "[DEBUG] Data left to verify [bytes]:" << data_left;
        qInfo() << "[DEBUG] Current Block Size [bytes]:" << block.size();
        #endif // ESP_TOOL_VERIFY_DEBUG

        // verify block in 3 attempts;
        for(int attempt = 0; attempt < 3; attempt++) {
            if (attempt != 0) qInfo() << "Retry to verify data block";
            verify_result = verifyFlashPr(offset, block);
            if (verify_result == true) break;
        }

        // stop verification process if one block failed
        if (verify_result == false) {
            qInfo().noquote() << QString("Verification failed at memory range [0x%1-0x%2]")
                .arg(QString::number(offset, 16).toUpper()).arg(QString::number(offset + current_block_size, 16).toUpper());
            break;
        }

        // update progress bar
        emit progress_signal((offset + current_block_size) * 100 / total_length);
    }
    
    return verify_result;
}