/**
 ******************************************************************************
 * @file           : src/serial.cpp
 * @brief          : Handles ESP bootloader serial flashing.
 * @author         : Kuraga Team
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2026 Kuraga Tech
 * SPDX-License-Identifier: MIT
 *
 ******************************************************************************
 */

#include "../esptoolqt.h"
#include "../read_agent/esp_read_agent.h"
#include "defines.h"
#include <cmath>

#include <QThread>
#include <QTime>
#include <QDebug>
#include <QSerialPortInfo>
#include <zlib.h>
#include <QFile>

#include <atomic>

#if defined(Q_OS_WIN32)
#  include <qt_windows.h>
#endif

using std::vector;
using std::ceil;
using SlipReply = EspToolQt::SlipReply;

namespace {
std::atomic<bool> g_esp_diag_enabled{false};

double kbitPerSecond(quint64 bytes, int duration_ms)
{
    if (duration_ms <= 0) duration_ms = 1;
    return (static_cast<double>(bytes) * 8.0 / 1000.0) / (static_cast<double>(duration_ms) / 1000.0);
}
}

void EspToolQt::setDiagEnabled(bool enabled) {
    g_esp_diag_enabled.store(enabled);
}

bool EspToolQt::isDiagEnabled() {
    return g_esp_diag_enabled.load();
}

QVector<QString> EspToolQt::getFamilies() {
    QVector<QString> families;

    for (auto target : available_targets){
        families.push_back(target->CHIP_NAME());
    }
    return families;
}

QVector<QString> EspToolQt::getTargets(QString familie) {
    QVector<QString> targets;

    for (auto target : available_targets){
        if(target->CHIP_NAME() == familie) {
            targets = target->CHIP_TARGETS();
            break;
        }
    }

    return targets;
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
    if (serial == NULL) return false;
    serial->clearError();
    const bool opened = serial->open(QIODevice::ReadWrite);
    if (!opened) {
        qInfo() << "[ERROR] Can't open serial port:" << serialErrorString();
    }
    return opened;
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
    if (serial == NULL) return;
    serial->close();
    esp_target_info.connected = false;
}

bool EspToolQt::hasSerialError() const {
    if (serial == NULL) return true;
    switch (serial->error()) {
    case QSerialPort::NoError:
    case QSerialPort::TimeoutError:
        return false;
    default:
        return true;
    }
}

bool EspToolQt::isSerialUsable() const {
    return serial != NULL && serial->isOpen() && !hasSerialError();
}

QString EspToolQt::serialErrorString() const {
    if (serial == NULL) return QStringLiteral("Serial port is not initialized");
    return serial->errorString();
}

bool EspToolQt::serialWrite(vector<uint8_t> data, int timeout_ms) {
    if (!isSerialUsable()) {
        qInfo() << "[ERROR] Serial port is not usable before write:" << serialErrorString();
        closePort();
        return false;
    }
    serial->clear();
    const qint64 written = serial->write(reinterpret_cast<const char*>(data.data()), data.size());
    if (written < 0) {
        qInfo() << "[ERROR] Serial write failed:" << serialErrorString();
        closePort();
        return false;
    }
    if (!serial->waitForBytesWritten(timeout_ms)) {
        if (!serial->isOpen() || hasSerialError()) {
            qInfo() << "[ERROR] Serial write wait failed:" << serialErrorString();
            closePort();
        }
        return false;
    }
    return true;
}

vector<uint8_t> EspToolQt::serialRead(int timeout_ms) {
    QTime timeout = QTime::currentTime().addMSecs(timeout_ms);
    vector<uint8_t> data;

    if (!isSerialUsable()) {
        qInfo() << "[ERROR] Serial port is not usable before read:" << serialErrorString();
        closePort();
        return data;
    }

    while(QTime::currentTime().msecsTo(timeout) > 0)
    {
        if (isCancelled()) return data;
        if (!serial->waitForReadyRead(5) && (!serial->isOpen() || hasSerialError())) {
            qInfo() << "[ERROR] Serial read wait failed:" << serialErrorString();
            closePort();
            return data;
        }
        QByteArray byte_array = serial->readAll();
        if (!serial->isOpen() || hasSerialError()) {
            qInfo() << "[ERROR] Serial read failed:" << serialErrorString();
            closePort();
            return data;
        }
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

    if (!isSerialUsable()) {
        qInfo() << "[ERROR] Serial port is not usable before frame read:" << serialErrorString();
        closePort();
        return zero;
    }

    while(QTime::currentTime().msecsTo(timeout) > 0)
    {
        if (isCancelled()) return zero;
        if (!serial->waitForReadyRead(1) && (!serial->isOpen() || hasSerialError())) {
            qInfo() << "[ERROR] Serial frame wait failed:" << serialErrorString();
            closePort();
            return zero;
        }
        while(QTime::currentTime().msecsTo(timeout) > 0){
            if (isCancelled()) return zero;
            QByteArray one_byte = serial->read(1);
            if (!serial->isOpen() || hasSerialError()) {
                qInfo() << "[ERROR] Serial frame read failed:" << serialErrorString();
                closePort();
                return zero;
            }
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
    return autoConnect(port, 460800);
}

bool EspToolQt::syncWithRomBootloader(int attempts) {
    const vector<uint8_t> sync_sequence_data = {
        0x07, 0x07, 0x12, 0x20,
        0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
        0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
        0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
        0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55
    };
    vector<uint8_t> sync_sequence = slip_encode(ESP_SYNC, sync_sequence_data);

    for (int i = 0; i < attempts; ++i) {
        if (isCancelled()) return false;
        serial->clear(QSerialPort::Input);
        if (!serialWrite(sync_sequence, 100)) {
            if (!isSerialUsable()) return false;
            qInfo() << "ESP sync write timed out, reading response anyway";
        }

        for (int frame = 0; frame < 8; ++frame) {
            vector<uint8_t> reply = serialReadOneFrame(100);
            if (isCancelled()) return false;
            SlipReply slip_reply = slip_parse(reply);
            if (slip_reply.valid &&
                slip_reply.command == ESP_SYNC &&
                slip_reply.data.size() >= 2 &&
                slip_reply.data[0] == 0 &&
                slip_reply.data[1] == 0) {
                return true;
            }
        }
        QThread::msleep(100);
    }

    return false;
}

static bool isEspressifUsbPort(const QString &portName)
{
    const auto infos = QSerialPortInfo::availablePorts();
    for (const QSerialPortInfo &info : infos) {
        if (info.portName() != portName) continue;
        return info.hasVendorIdentifier() && info.vendorIdentifier() == 0x303A;
    }
    return false;
}

bool EspToolQt::autoConnect(QString port, uint32_t baud) {
    esp_target_info.connected = false;
    target = NULL;
    lastConnectError.clear();
    closePort(); // close port if it was opened

    std::vector<QString> ports;
    
    if (port.isEmpty()) {
        ports = getPorts();
    } else {
        ports.push_back(port);
    }

    bool done = false;
    QString found_port;
    QString last_sync_error;

    for (auto port = ports.rbegin(); port < ports.rend(); port++) {
        if (isCancelled()) {
            closePort();
            return false;
        }

        QVector<ResetStrategy> resets;
        if (isEspressifUsbPort(*port)) {
            resets.append(ResetStrategy::usb_jtag_serial_reset);
            resets.append(ResetStrategy::classic_reset);
        } else {
            resets.append(ResetStrategy::classic_reset);
            resets.append(ResetStrategy::usb_jtag_serial_reset);
        }

        for (auto reset : resets){
            if (isCancelled()) {
                closePort();
                return false;
            }
            qInfo() << "Try" << *port << "reset_strategy" << reset;
            bool port_opened = openPort(*port,115200);
            if (port_opened) {
                resetToBoot(reset);

                if (syncWithRomBootloader()) {
                    done = true;
                    found_port = *port;
                    resetStrategy = reset;
                    break;
                }

                last_sync_error = QStringLiteral("Couldn't sync to ESP ROM bootloader on %1 with reset strategy %2")
                                      .arg(*port)
                                      .arg(static_cast<int>(reset));
                qInfo() << last_sync_error;
                serial->clear(QSerialPort::AllDirections);
                serialRead(200);
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
        lastConnectError = last_sync_error.isEmpty()
            ? QStringLiteral("Couldn't sync to ESP ROM bootloader. Check port and wiring.")
            : last_sync_error;
        qInfo() << "no device found";
        qInfo() << lastConnectError;
        qInfo() << "ESP manual boot hint: if this is a development board, hold BOOT, pulse RESET, then retry connect.";
        return false;
    }

    // clean rx buffer
    serialRead(200);
    if (isCancelled()) {
        closePort();
        target = NULL;
        return false;
    }

    // determine chip id
    uint32_t x = read_reg(0x40001000);
    if (x == 0) {
        lastConnectError = QStringLiteral("ESP sync succeeded, but target id register could not be read.");
        qInfo() << "[ERROR] Connection failed. Can't read target id.";
        closePort();
        target = NULL;
        return false;
    }


    for (auto attempt_target : available_targets){
        if (attempt_target->CHIP_COMPARE_MAGIC_VALUE(x)) {
            qInfo() << "Chip detected:" << attempt_target->CHIP_NAME();
            target = attempt_target;
            break;
        }
    }
    if (target == NULL) {
        qInfo() << "[ERROR] Connection failed. Can't determine chip family. Unknown chip id:"
                << Qt::hex << x;
        lastConnectError = QStringLiteral("Connection failed. Unknown ESP chip id: 0x%1")
                               .arg(x, 0, 16);
        closePort();
        target = NULL;
        return false;
    }

    if (!stubUpload()) {
        closePort();
        target = NULL;
        return false;
    }
    if (isCancelled()) {
        closePort();
        target = NULL;
        return false;
    }

    if (!changeBaud(baud)) {
        closePort();
        target = NULL;
        return false;
    }

    QString chip_description;
    getChipDescription(&chip_description);
    qInfo().noquote() << "Chip is" << chip_description;

    QString chip_features;
    getChipFeatures(&chip_features);
    qInfo().noquote() << "Features:" << chip_features;

    // This function is implemented not for all families. Skip Mac if not implemented.
    vector<uint8_t> mac;
    getChipBaseMac(&mac);
    if (mac.size() == 6) {
        QString mac_string = QString("MAC: %1:%2:%3:%4:%5:%6")
            .arg(QString::number(mac[0], 16).toUpper().rightJustified(2, '0'))
            .arg(QString::number(mac[1], 16).toUpper().rightJustified(2, '0'))
            .arg(QString::number(mac[2], 16).toUpper().rightJustified(2, '0'))
            .arg(QString::number(mac[3], 16).toUpper().rightJustified(2, '0'))
            .arg(QString::number(mac[4], 16).toUpper().rightJustified(2, '0'))
            .arg(QString::number(mac[5], 16).toUpper().rightJustified(2, '0'));
        qInfo() << mac_string;
    }

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
    if (isCancelled()) {
        closePort();
        return false;
    }
    serial->setBaudRate(baud);
    QObject().thread()->msleep(50);
    serial->clear();

    // determine chip id
    uint32_t x = read_reg(0x40001000);
    if (target->CHIP_COMPARE_MAGIC_VALUE(x)) {
        qInfo() << "[OK] Baudrate successfully changed to" << serial->baudRate();
        return true;
    } else {
        qInfo() << "[ERROR] Failed to set baudrate" << serial->baudRate();
        return false;
    }
}

void EspToolQt::disconnect() {
    closePort();
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
    if (isCancelled()) {
        closePort();
        return false;
    }
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
    if (isCancelled()) {
        closePort();
        return false;
    }
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
    if (isCancelled()) {
        closePort();
        return false;
    }
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
        if (isCancelled()) {
            closePort();
            return false;
        }
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
    if (isCancelled()) {
        closePort();
        return false;
    }
    vector<uint8_t> expected_ohai = {0x4F, 0x48, 0x41, 0x49};
    return (ohai == expected_ohai);
}

std::vector<uint8_t> EspToolQt::readFlash(uint32_t offset, uint32_t size) {
    QTime start = QTime::currentTime();
    const bool diag = isDiagEnabled();
    int command_reply_ms = 0;
    int data_frames_ms = 0;
    int ack_send_ms = 0;
    int md5_frame_ms = 0;
    int host_md5_ms = 0;
    quint64 frame_count = 0;
    quint64 ack_count = 0;
    quint64 payload_bytes = 0;

    vector<uint8_t> received_data;
    vector<uint8_t> zero;

    // check that target is connected
    if (target == NULL || serial == NULL || !serial->isOpen()) {
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
    QTime lap = QTime::currentTime();
    vector<uint8_t> reply = serialReadOneFrame(); // read reply to command
    command_reply_ms = lap.msecsTo(QTime::currentTime());
    if (isCancelled()) {
        closePort();
        return zero;
    }
    if (reply.size() == 0) return zero;

    while (received_data.size() < size) {
        if (isCancelled()) {
            closePort();
            return zero;
        }
        progress((float)received_data.size() / (float)size * 100);
        if (progress_bytes_enabled)
            emit progress_bytes_signal(received_data.size(), size);

        lap = QTime::currentTime();
        vector<uint8_t> reply = serialReadOneFrame();
        data_frames_ms += lap.msecsTo(QTime::currentTime());
        if (isCancelled()) {
            closePort();
            return zero;
        }
        if (reply.size() == 0) return zero;
        frame_count++;
        payload_bytes += static_cast<quint64>(reply.size());

        received_data.insert(received_data.end(), reply.begin(), reply.end());

        // check data size validity
        if (received_data.size() < size && reply.size() != target->FLASH_SECTOR_SIZE()) {
            qInfo() << "Inbound data packet too small";
            return zero;
        }

        vector<uint8_t> ack;
        appendU32(&ack, received_data.size());
        lap = QTime::currentTime();
        slip_raw_send(ack);
        ack_send_ms += lap.msecsTo(QTime::currentTime());
        ack_count++;
    }

    progress(100);
    int transfer_ms = start.msecsTo(QTime::currentTime());
    if (transfer_ms <= 0) transfer_ms = 1;

    lap = QTime::currentTime();
    vector<uint8_t> md5_from_esp = serialReadOneFrame();
    md5_frame_ms = lap.msecsTo(QTime::currentTime());
    if (isCancelled()) {
        closePort();
        return zero;
    }
    // qInfo() << "md5_from_esp" << Qt::hex << md5_from_esp;

    lap = QTime::currentTime();
    vector<uint8_t> md5_calculated = calculate_md5_hash(received_data);
    host_md5_ms = lap.msecsTo(QTime::currentTime());
    // qInfo() << "md5_calculated" << Qt::hex << md5_calculated;

    if (md5_from_esp == md5_calculated) {
        qInfo() << "[OK] MD5 Check Passed";
    } else {
        qInfo() << "[ERROR] MD5 Check Failed";
        return zero;
    }

    int total_ms = start.msecsTo(QTime::currentTime());
    if (total_ms <= 0) total_ms = 1;
    float speed = ((float)received_data.size() * 8 / 1000) / ((float)transfer_ms / 1000);
    qInfo() << "[OK] Effective read speed [kbit/s]:" << speed;
    if (diag) {
        const uint32_t sector_size = target ? target->FLASH_SECTOR_SIZE() : 0;
        qInfo().noquote() << QString("[esp-diag] read offset=0x%1 size=%2 sector=%3 frames=%4 acks=%5 payload=%6 transfer_ms=%7 total_ms=%8 cmd_reply_ms=%9 data_frames_ms=%10 ack_send_ms=%11 md5_frame_ms=%12 host_md5_ms=%13 payload_kbit_s=%14")
            .arg(QString::number(offset, 16).toUpper())
            .arg(size)
            .arg(sector_size)
            .arg(frame_count)
            .arg(ack_count)
            .arg(payload_bytes)
            .arg(transfer_ms)
            .arg(total_ms)
            .arg(command_reply_ms)
            .arg(data_frames_ms)
            .arg(ack_send_ms)
            .arg(md5_frame_ms)
            .arg(host_md5_ms)
            .arg(kbitPerSecond(payload_bytes, transfer_ms), 0, 'f', 2);
    }

    return received_data;
}

std::vector<uint8_t> EspToolQt::readFlashWithAgent(uint32_t offset, uint32_t size) {
    EspReadAgentRunner runner(this);
    return runner.readFlash(offset, size);
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
    const bool diag = isDiagEnabled();
    int compress_ms = 0;
    int flash_begin_ms = 0;
    int flash_packets_ms = 0;
    int final_wait_ms = 0;
    quint64 packet_count = 0;

    // qInfo() << (compress ? "[OK] Compressed flash upload started" : "[OK] Flash upload started");

    // compress data if needed
    vector<uint8_t> compressed_data;
    QTime lap = QTime::currentTime();
    if (compress) compressed_data = compress_vector(data);
    compress_ms = lap.msecsTo(QTime::currentTime());
    const vector<uint8_t>& upload = compress ? compressed_data : data;

    // calculate number of required packets
    uint32_t number_of_data_packets = ceil((float)upload.size() / float(max_packet_size));
    lap = QTime::currentTime();
    if (!flashBegin(data.size(), number_of_data_packets, max_packet_size, memory_offset, compress)) {
        return false;
    }
    flash_begin_ms = lap.msecsTo(QTime::currentTime());

    // write flash
    vector<uint8_t> tmp_vec;
    tmp_vec.reserve(max_packet_size);
    uint32_t frame_n = 0;
    for (uint32_t i = 0; i < upload.size(); i++) {
        if (isCancelled()) {
            closePort();
            return false;
        }
        tmp_vec.push_back(upload[i]);
        if (tmp_vec.size() >= max_packet_size) {
            lap = QTime::currentTime();
            if (!flashDataOneBlock(frame_n, tmp_vec, compress)) {
                return false;
            }
            flash_packets_ms += lap.msecsTo(QTime::currentTime());
            packet_count++;
            frame_n++;
            tmp_vec.clear();
        }
    }
    if (tmp_vec.size() != 0) {
        lap = QTime::currentTime();
        if (!flashDataOneBlock(frame_n, tmp_vec, compress)) {
            return false;
        }
        flash_packets_ms += lap.msecsTo(QTime::currentTime());
        packet_count++;
    }
    
    // Stub only writes each block to flash after 'ack'ing the receive,
    // so do a final dummy operation which will not be 'ack'ed
    // until the last block has actually been written out to flash
    lap = QTime::currentTime();
    read_reg(target->CHIP_DETECT_MAGIC_REG_ADDR());
    final_wait_ms = lap.msecsTo(QTime::currentTime());

    if (diag) {
        const quint64 logical_size = static_cast<quint64>(data.size());
        const quint64 wire_size = static_cast<quint64>(upload.size());
        const double ratio = logical_size == 0 ? 1.0 : static_cast<double>(wire_size) / static_cast<double>(logical_size);
        qInfo().noquote() << QString("[esp-diag] flashData offset=0x%1 logical=%2 wire=%3 ratio=%4 compressed=%5 max_packet=%6 packets=%7 compress_ms=%8 begin_ms=%9 packet_ms=%10 final_wait_ms=%11 wire_kbit_s=%12")
            .arg(QString::number(memory_offset, 16).toUpper())
            .arg(logical_size)
            .arg(wire_size)
            .arg(ratio, 0, 'f', 4)
            .arg(compress ? "yes" : "no")
            .arg(max_packet_size)
            .arg(packet_count)
            .arg(compress_ms)
            .arg(flash_begin_ms)
            .arg(flash_packets_ms)
            .arg(final_wait_ms)
            .arg(kbitPerSecond(wire_size, flash_packets_ms + flash_begin_ms + final_wait_ms), 0, 'f', 2);
    }

    return true;
}

VerifyBlockResult EspToolQt::verifyFlashBlockMd5Detailed(uint32_t memory_offset, const std::vector<uint8_t>& data) {

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
    if (isCancelled()) {
        closePort();
        return VerifyBlockResult::Error;
    }
    SlipReply slip_reply = slip_parse(reply);

    // check that we have successfully read md5 from device
    if (slip_reply.valid != true || slip_reply.data.size() < 18) {
        qInfo() << "[ERROR] Failed to get md5 from device";
        return VerifyBlockResult::Error;
    }

    // pop two status bytes from end of frame
    slip_reply.data.pop_back();
    slip_reply.data.pop_back();

    // md5 hash from esp
    vector<uint8_t> md5_from_esp = slip_reply.data;

    // md5 hash calculated
    std::vector<uint8_t> copy = data;
    vector<uint8_t> md5_calculated = calculate_md5_hash(copy);

    if (md5_from_esp == md5_calculated) {
        return VerifyBlockResult::Match;
    }

    return VerifyBlockResult::Mismatch;
}

bool EspToolQt::verifyFlashPr(uint32_t memory_offset, std::vector<uint8_t> data) {
    return verifyFlashBlockMd5Detailed(memory_offset, data) == VerifyBlockResult::Match;
}

bool EspToolQt::verifyFlashBlockMd5(uint32_t memory_offset, const std::vector<uint8_t>& data) {
    std::vector<uint8_t> copy = data;
    return verifyFlashPr(memory_offset, copy);
}

// #define ESP_TOOL_UPLOAD_DEBUG
bool EspToolQt::flashUpload(uint32_t memory_offset, std::vector<uint8_t> data, bool compressed) {
    QTime start = QTime::currentTime();
    bool upload_result = true;
    const bool diag = isDiagEnabled();
    int block_upload_verify_ms = 0;
    quint64 logical_uploaded = 0;
    quint64 block_count = 0;

    // check that target is connected
    if (target == NULL || serial == NULL || !serial->isOpen()) {
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
        if (isCancelled()) {
            closePort();
            return false;
        }
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
            if (isCancelled()) {
                closePort();
                return false;
            }
            if (attempt != 0) qInfo() << "Retry data block";
            QTime block_lap = QTime::currentTime();
            upload_result = flashData(offset, block, compressed);
            if (upload_result == true) {
                upload_result = verifyFlashPr(offset, block);
            }
            block_upload_verify_ms += block_lap.msecsTo(QTime::currentTime());
            if (upload_result == true) break;
        }

        // stop writing process if one block failed
        if (upload_result == false) {
            qInfo().noquote() << QString("[ERROR] Flash failed at memory range [0x%1-0x%2]")
                .arg(QString::number(offset, 16).toUpper()).arg(QString::number(offset + current_block_size, 16).toUpper());
            break;
        }

        // update progress bar
        quint64 written = offset - memory_offset + current_block_size;
        logical_uploaded = written;
        block_count++;
        emit progress_signal(static_cast<int>(written * 100 / total_length));
        if (progress_bytes_enabled)
            emit progress_bytes_signal(written, static_cast<quint64>(total_length));
    }

    if (!upload_result) {
        return false;
    }

    // get duration of write for speed test
    int duration = start.msecsTo(QTime::currentTime());
    if (duration <= 0) duration = 1;
    float speed = ((float)data.size() * 8 / 1000) / ((float)duration / 1000);
    qInfo() << "[OK] Effective speed [kbit/s]:" << speed;
    if (diag) {
        qInfo().noquote() << QString("[esp-diag] flashUpload offset=0x%1 logical=%2 blocks=%3 block_size=%4 compressed=%5 total_ms=%6 block_upload_verify_ms=%7 logical_kbit_s=%8")
            .arg(QString::number(memory_offset, 16).toUpper())
            .arg(logical_uploaded)
            .arg(block_count)
            .arg(block_size)
            .arg(compressed ? "yes" : "no")
            .arg(duration)
            .arg(block_upload_verify_ms)
            .arg(kbitPerSecond(logical_uploaded, duration), 0, 'f', 2);
    }
    return true;
}

// #define ESP_TOOL_VERIFY_DEBUG
bool EspToolQt::verifyFlash(uint32_t memory_offset, std::vector<uint8_t> data) {
    bool verify_result = true;
    
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
        if (isCancelled()) {
            closePort();
            return false;
        }
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
            if (isCancelled()) {
                closePort();
                return false;
            }
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
        quint64 verified = offset - memory_offset + current_block_size;
        emit progress_signal(static_cast<int>(verified * 100 / total_length));
        if (progress_bytes_enabled)
            emit progress_bytes_signal(verified, static_cast<quint64>(total_length));
    }
    
    return verify_result;
}
