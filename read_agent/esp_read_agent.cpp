/**
 ******************************************************************************
 * @file           : esp_read_agent.cpp
 * @brief          : Implements ESP flash read-agent helpers.
 * @author         : Kuraga Team
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2026 Kuraga Tech
 * SPDX-License-Identifier: MIT
 *
 ******************************************************************************
 */

#include "esp_read_agent.h"

#include "../esptoolqt.h"

#include <QDebug>

using std::vector;

namespace {

const char *integrityName(EspReadAgentBuilder::IntegrityPacket packet)
{
    switch (packet) {
    case EspReadAgentBuilder::IntegrityPacket::Crc32Reflected:
        return "CRC32_REFLECTED";
    case EspReadAgentBuilder::IntegrityPacket::LegacyMd5Digest:
        return "LEGACY_MD5";
    case EspReadAgentBuilder::IntegrityPacket::None:
        return "NONE";
    }

    return "UNKNOWN";
}

} // namespace

EspReadAgentBuilder::Program EspReadAgentBuilder::buildForCurrentTarget(EspToolQt *espTool)
{
    if (espTool == nullptr || espTool->target == nullptr) {
        return Program{};
    }

    return buildForChipName(espTool->target->CHIP_NAME(), espTool->target->FLASH_SECTOR_SIZE());
}

EspReadAgentBuilder::Program EspReadAgentBuilder::buildForChipName(const QString &chipName,
                                                                   uint32_t flashSectorSize)
{
    Program program;

    if (chipName == QStringLiteral("ESP32-S3")) {
        program.supported = true;
        program.transferBlockSize = flashSectorSize;
        program.agentCrcPacket = IntegrityPacket::Crc32Reflected;
        program.legacyStubIntegrity = IntegrityPacket::LegacyMd5Digest;
        program.name = "esp32s3-stub-read-agent";
    }

    return program;
}

EspReadAgentRunner::EspReadAgentRunner(EspToolQt *espTool)
    : m_espTool(espTool)
{
}

vector<uint8_t> EspReadAgentRunner::readFlash(uint32_t offset, uint32_t size)
{
    const EspReadAgentBuilder::Program program = EspReadAgentBuilder::buildForCurrentTarget(m_espTool);

    if (!validateProgram(program)) {
        return {};
    }

    qInfo() << "[OK] ESP read-agent runner selected:" << program.name
            << "start_marker=0x" << Qt::hex << program.startMarker << Qt::dec
            << "agent_crc=" << integrityName(program.agentCrcPacket)
            << "legacy_stub_integrity=" << integrityName(program.legacyStubIntegrity);

    // Transitional S3 runner: the current ESP stub already takes ownership of
    // target state after upload and returns a final legacy MD5 digest packet
    // for read verification. The final S3 RAM agent must stamp KURAGA_MAGIC
    // on start and return a collision-resistant CRC packet for each read.
    return m_espTool->readFlash(offset, size);
}

bool EspReadAgentRunner::validateProgram(const EspReadAgentBuilder::Program &program) const
{
    if (m_espTool == nullptr || m_espTool->target == nullptr || !m_espTool->isSerialUsable()) {
        qInfo() << "[Error] Target is not connected";
        return false;
    }

    if (!program.supported) {
        qInfo() << "[ERROR] ESP read agent is not available for this target";
        return false;
    }

    if (program.transferBlockSize == 0 || program.agentCrcPacket == EspReadAgentBuilder::IntegrityPacket::None) {
        qInfo() << "[ERROR] ESP read agent has invalid protocol metadata";
        return false;
    }

    return true;
}

#ifdef KT_SELFTEST
#include "kt_selftest/kt_selftest.h"

KT_TEST(esp_read_agent_default_plan_is_disabled,
        "EspReadAgentBuilder default plan is unsupported and has no CRC packet") {
    EspReadAgentBuilder::Program program;

    KT_ASSERT(!program.supported);
    KT_ASSERT_EQ(program.transferBlockSize, static_cast<uint32_t>(0));
    KT_ASSERT_EQ(program.agentCrcPacket, EspReadAgentBuilder::IntegrityPacket::None);
    KT_ASSERT_EQ(program.legacyStubIntegrity, EspReadAgentBuilder::IntegrityPacket::None);
}

KT_TEST(esp_read_agent_s3_plan_has_marker_crc_and_block_size,
        "EspReadAgentBuilder ESP32-S3 plan records marker, CRC, and block size") {
    const auto program = EspReadAgentBuilder::buildForChipName(QStringLiteral("ESP32-S3"), 0x1000);

    KT_ASSERT(program.supported);
    KT_ASSERT_EQ(program.readCommand, static_cast<uint8_t>(0xD2));
    KT_ASSERT_EQ(program.transferBlockSize, static_cast<uint32_t>(0x1000));
    KT_ASSERT_EQ(program.startMarker, EspReadAgentBuilder::KURAGA_MAGIC);
    KT_ASSERT_EQ(program.agentCrcPacket, EspReadAgentBuilder::IntegrityPacket::Crc32Reflected);
    KT_ASSERT_EQ(program.legacyStubIntegrity, EspReadAgentBuilder::IntegrityPacket::LegacyMd5Digest);
    KT_ASSERT(QString::fromLatin1(program.name) == QStringLiteral("esp32s3-stub-read-agent"));
}

KT_TEST(esp_read_agent_non_s3_plan_is_unsupported,
        "EspReadAgentBuilder non-S3 plan stays unsupported") {
    const auto program = EspReadAgentBuilder::buildForChipName(QStringLiteral("ESP32-C3"), 0x1000);

    KT_ASSERT(!program.supported);
    KT_ASSERT_EQ(program.transferBlockSize, static_cast<uint32_t>(0));
    KT_ASSERT_EQ(program.agentCrcPacket, EspReadAgentBuilder::IntegrityPacket::None);
    KT_ASSERT_EQ(program.legacyStubIntegrity, EspReadAgentBuilder::IntegrityPacket::None);
}

KT_TEST(esp_read_agent_null_tool_plan_is_unsupported,
        "EspReadAgentBuilder null tool plan stays unsupported") {
    const auto program = EspReadAgentBuilder::buildForCurrentTarget(nullptr);

    KT_ASSERT(!program.supported);
    KT_ASSERT_EQ(program.agentCrcPacket, EspReadAgentBuilder::IntegrityPacket::None);
}

#endif // KT_SELFTEST
