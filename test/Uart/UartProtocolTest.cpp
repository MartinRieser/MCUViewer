#include "UartProtocol.hpp"

#include <cassert>
#include <cstdio>
#include <iostream>

using namespace UartProtocol;

void testCRC16()
{
    std::cout << "Testing CRC16-CCITT calculation..." << std::endl;

    // Test vector 1: Empty data
    uint16_t crc1 = calculateCRC16(nullptr, 0);
    assert(crc1 == 0xFFFF);    // Initial value
    std::cout << "  ✓ Empty data: 0x" << std::hex << crc1 << std::dec << std::endl;

    // Test vector 2: Single byte
    uint8_t data2[] = {0xAA};
    uint16_t crc2 = calculateCRC16(data2, sizeof(data2));
    std::cout << "  ✓ Single byte (0xAA): 0x" << std::hex << crc2 << std::dec << std::endl;

    // Test vector 3: READ_MEMORY packet header (without CRC)
    // START=0xAA, CMD=0x01, LENGTH=0x0006, SEQ=0x01, ADDRESS=0x20000000, SIZE=0x0004
    uint8_t data3[] = {0xAA, 0x01, 0x06, 0x00, 0x01, 0x00, 0x00, 0x00, 0x20, 0x04, 0x00};
    uint16_t crc3 = calculateCRC16(data3, sizeof(data3));
    std::cout << "  ✓ READ_MEMORY packet: 0x" << std::hex << crc3 << std::dec << std::endl;

    // Test vector 4: Known CRC (123456789)
    uint8_t data4[] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};
    uint16_t crc4 = calculateCRC16(data4, sizeof(data4));
    std::cout << "  ✓ \"123456789\": 0x" << std::hex << crc4 << std::dec;
    std::cout << " (Expected: 0x29B1)" << std::endl;
    assert(crc4 == 0x29B1);    // Known CRC16-CCITT result

    std::cout << "✅ CRC16 tests passed!" << std::endl << std::endl;
}

void testPacketSerialization()
{
    std::cout << "Testing packet serialization..." << std::endl;

    // Create a simple packet
    Packet packet;
    packet.cmd = CommandCode::READ_MEMORY;
    packet.seq = 1;
    packet.payload = {0x00, 0x00, 0x00, 0x20, 0x04, 0x00};    // address=0x20000000, size=4
    packet.length = static_cast<uint16_t>(packet.payload.size());

    // Serialize
    std::vector<uint8_t> serialized = serialize(packet);

    std::cout << "  Serialized packet (" << serialized.size() << " bytes): ";
    for (uint8_t byte : serialized)
    {
        printf("%02X ", byte);
    }
    std::cout << std::endl;

    // Verify structure
    assert(serialized[0] == START_BYTE);                       // START
    assert(serialized[1] == static_cast<uint8_t>(CommandCode::READ_MEMORY));    // CMD
    assert(serialized[2] == 0x06);                             // LENGTH LSB
    assert(serialized[3] == 0x00);                             // LENGTH MSB
    assert(serialized[4] == 0x01);                             // SEQ
    assert(serialized.size() == HEADER_SIZE + 6 + CRC_SIZE);    // Total size

    std::cout << "✅ Serialization tests passed!" << std::endl << std::endl;
}

void testPacketDeserialization()
{
    std::cout << "Testing packet deserialization..." << std::endl;

    // Create and serialize a packet
    Packet originalPacket;
    originalPacket.cmd = CommandCode::GET_INFO;
    originalPacket.seq = 42;
    originalPacket.length = 0;    // No payload

    std::vector<uint8_t> serialized = serialize(originalPacket);

    // Deserialize
    Packet deserializedPacket;
    bool success = deserialize(serialized, deserializedPacket);

    assert(success);
    assert(deserializedPacket.start == START_BYTE);
    assert(deserializedPacket.cmd == CommandCode::GET_INFO);
    assert(deserializedPacket.seq == 42);
    assert(deserializedPacket.length == 0);
    assert(deserializedPacket.payload.empty());

    std::cout << "  ✓ Deserialized GET_INFO packet (seq=" << (int)deserializedPacket.seq << ")" << std::endl;

    std::cout << "✅ Deserialization tests passed!" << std::endl << std::endl;
}

void testRoundTrip()
{
    std::cout << "Testing round-trip serialization/deserialization..." << std::endl;

    // Test READ_MEMORY packet
    std::vector<uint8_t> readMemPacket = createReadMemoryPacket(0x20000000, 4, 1);
    Packet parsedRead;
    assert(deserialize(readMemPacket, parsedRead));
    assert(parsedRead.cmd == CommandCode::READ_MEMORY);
    assert(parsedRead.seq == 1);
    assert(parsedRead.length == 6);
    std::cout << "  ✓ READ_MEMORY round-trip OK" << std::endl;

    // Test WRITE_MEMORY packet
    std::vector<uint8_t> writeData = {0x12, 0x34, 0x56, 0x78};
    std::vector<uint8_t> writeMemPacket = createWriteMemoryPacket(0x20000004, writeData, 2);
    Packet parsedWrite;
    assert(deserialize(writeMemPacket, parsedWrite));
    assert(parsedWrite.cmd == CommandCode::WRITE_MEMORY);
    assert(parsedWrite.seq == 2);
    assert(parsedWrite.length == 6 + writeData.size());
    std::cout << "  ✓ WRITE_MEMORY round-trip OK" << std::endl;

    // Test GET_INFO packet
    std::vector<uint8_t> getInfoPacket = createGetInfoPacket(3);
    Packet parsedInfo;
    assert(deserialize(getInfoPacket, parsedInfo));
    assert(parsedInfo.cmd == CommandCode::GET_INFO);
    assert(parsedInfo.seq == 3);
    assert(parsedInfo.length == 0);
    std::cout << "  ✓ GET_INFO round-trip OK" << std::endl;

    // Test PING packet
    std::vector<uint8_t> pingPacket = createPingPacket(0x12345678, 4);
    Packet parsedPing;
    assert(deserialize(pingPacket, parsedPing));
    assert(parsedPing.cmd == CommandCode::PING);
    assert(parsedPing.seq == 4);
    assert(parsedPing.length == 4);
    std::cout << "  ✓ PING round-trip OK" << std::endl;

    std::cout << "✅ Round-trip tests passed!" << std::endl << std::endl;
}

void testCRCValidation()
{
    std::cout << "Testing CRC validation..." << std::endl;

    // Create a valid packet
    std::vector<uint8_t> validPacket = createGetInfoPacket(10);

    // Test valid packet
    Packet parsed;
    assert(deserialize(validPacket, parsed));
    std::cout << "  ✓ Valid packet accepted" << std::endl;

    // Corrupt the CRC
    std::vector<uint8_t> corruptedPacket = validPacket;
    corruptedPacket[corruptedPacket.size() - 1] ^= 0xFF;    // Flip all bits in CRC LSB

    // Test corrupted packet
    Packet parsedCorrupted;
    bool success = deserialize(corruptedPacket, parsedCorrupted);
    assert(!success);    // Should fail CRC check
    std::cout << "  ✓ Corrupted packet rejected" << std::endl;

    // Corrupt a payload byte
    std::vector<uint8_t> corruptedPayload = createReadMemoryPacket(0x20000000, 4, 1);
    corruptedPayload[5] ^= 0xFF;    // Corrupt address byte

    // This should fail CRC check
    Packet parsedPayload;
    success = deserialize(corruptedPayload, parsedPayload);
    assert(!success);
    std::cout << "  ✓ Corrupted payload rejected" << std::endl;

    std::cout << "✅ CRC validation tests passed!" << std::endl << std::endl;
}

void testPayloadParsing()
{
    std::cout << "Testing payload parsing..." << std::endl;

    // Test READ_MEMORY_RESP parsing - success case
    std::vector<uint8_t> respPayload1 = {0x00, 0x12, 0x34, 0x56, 0x78};    // status=SUCCESS, data
    ReadMemoryRespPayload parsed1;
    assert(parseReadMemoryResp(respPayload1, parsed1));
    assert(parsed1.status == static_cast<uint8_t>(ErrorCode::SUCCESS));
    assert(parsed1.data.size() == 4);
    assert(parsed1.data[0] == 0x12);
    std::cout << "  ✓ READ_MEMORY_RESP (success) parsed correctly" << std::endl;

    // Test READ_MEMORY_RESP parsing - error case
    std::vector<uint8_t> respPayload2 = {0x05};    // status=MEMORY_ACCESS_ERROR
    ReadMemoryRespPayload parsed2;
    assert(parseReadMemoryResp(respPayload2, parsed2));
    assert(parsed2.status == static_cast<uint8_t>(ErrorCode::MEMORY_ACCESS_ERROR));
    assert(parsed2.data.empty());
    std::cout << "  ✓ READ_MEMORY_RESP (error) parsed correctly" << std::endl;

    // Test GET_INFO_RESP parsing
    std::vector<uint8_t> infoPayload;
    infoPayload.push_back(0x00);                  // status=SUCCESS
    infoPayload.push_back(0x00);                  // version LSB
    infoPayload.push_back(0x01);                  // version MSB (v1.0)
    infoPayload.push_back(0x03);                  // capabilities bits 0-7
    infoPayload.push_back(0x00);                  // capabilities bits 8-15
    infoPayload.push_back(0x00);                  // capabilities bits 16-23
    infoPayload.push_back(0x00);                  // capabilities bits 24-31
    infoPayload.push_back('T');
    infoPayload.push_back('e');
    infoPayload.push_back('s');
    infoPayload.push_back('t');
    infoPayload.push_back('\0');

    GetInfoRespPayload parsed3;
    assert(parseGetInfoResp(infoPayload, parsed3));
    assert(parsed3.status == static_cast<uint8_t>(ErrorCode::SUCCESS));
    assert(parsed3.protocolVersion == 0x0100);
    assert(parsed3.capabilities == 0x03);
    assert(parsed3.deviceName == "Test");
    std::cout << "  ✓ GET_INFO_RESP parsed correctly (device: \"" << parsed3.deviceName << "\")" << std::endl;

    std::cout << "✅ Payload parsing tests passed!" << std::endl << std::endl;
}

void testCommandNames()
{
    std::cout << "Testing command/error name conversion..." << std::endl;

    assert(getCommandName(CommandCode::READ_MEMORY) == "READ_MEMORY");
    assert(getCommandName(CommandCode::PING) == "PING");
    assert(getCommandName(CommandCode::ERROR) == "ERROR");
    assert(getErrorName(ErrorCode::SUCCESS) == "SUCCESS");
    assert(getErrorName(ErrorCode::CRC_ERROR) == "CRC_ERROR");

    std::cout << "  ✓ Command names correct" << std::endl;
    std::cout << "  ✓ Error names correct" << std::endl;

    std::cout << "✅ Name conversion tests passed!" << std::endl << std::endl;
}

int main()
{
    std::cout << "========================================" << std::endl;
    std::cout << "UART Protocol Unit Tests" << std::endl;
    std::cout << "========================================" << std::endl << std::endl;

    try
    {
        testCRC16();
        testPacketSerialization();
        testPacketDeserialization();
        testRoundTrip();
        testCRCValidation();
        testPayloadParsing();
        testCommandNames();

        std::cout << "========================================" << std::endl;
        std::cout << "✅ ALL TESTS PASSED!" << std::endl;
        std::cout << "========================================" << std::endl;

        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "❌ TEST FAILED: " << e.what() << std::endl;
        return 1;
    }
}