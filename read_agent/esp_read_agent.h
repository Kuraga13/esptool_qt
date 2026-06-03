/**
 ******************************************************************************
 * @file           : esp_read_agent.h
 * @brief          : Declares ESP flash read-agent helpers.
 * @author         : Kuraga Team
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2026 Kuraga Tech
 * SPDX-License-Identifier: MIT
 *
 ******************************************************************************
 */

#ifndef ESP_READ_AGENT_H
#define ESP_READ_AGENT_H

#include <cstdint>
#include <vector>

#include <QString>

class EspToolQt;

class EspReadAgentBuilder
{
public:
    static constexpr uint32_t KURAGA_MAGIC = 0x4B544147u; // "KTAG"

    enum class IntegrityPacket {
        None,
        Crc32Reflected,
        LegacyMd5Digest
    };

    struct Program {
        bool supported = false;
        uint8_t readCommand = 0xD2;
        uint32_t transferBlockSize = 0;
        uint32_t startMarker = KURAGA_MAGIC;
        IntegrityPacket agentCrcPacket = IntegrityPacket::None;
        IntegrityPacket legacyStubIntegrity = IntegrityPacket::None;
        const char *name = "generic-stub-read";
    };

    static Program buildForCurrentTarget(EspToolQt *espTool);
    static Program buildForChipName(const QString &chipName, uint32_t flashSectorSize);
};

class EspReadAgentRunner
{
public:
    explicit EspReadAgentRunner(EspToolQt *espTool);

    std::vector<uint8_t> readFlash(uint32_t offset, uint32_t size);

private:
    bool validateProgram(const EspReadAgentBuilder::Program &program) const;

    EspToolQt *m_espTool;
};

#endif // ESP_READ_AGENT_H
