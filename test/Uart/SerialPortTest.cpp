#include "SerialPort.hpp"

#include <cassert>
#include <cstdio>
#include <iostream>

using namespace UartProtocol;

void testPortEnumeration()
{
    std::cout << "Testing port enumeration..." << std::endl;

    std::vector<SerialPortInfo> ports = SerialPort::listPorts();

    std::cout << "  Found " << ports.size() << " serial port(s):" << std::endl;
    for (const auto& port : ports)
    {
        std::cout << "    - " << port.port;
        if (!port.description.empty())
            std::cout << " (" << port.description << ")";
        std::cout << std::endl;
    }

    std::cout << "✅ Port enumeration test passed!" << std::endl << std::endl;
}

void testPortOpenClose()
{
    std::cout << "Testing port open/close..." << std::endl;

    SerialPort port;
    assert(!port.isOpen());
    assert(port.getPortName().empty());

    // Try opening a non-existent port (should fail gracefully)
#ifdef _WIN32
    bool result = port.open("COM999");
#else
    bool result = port.open("/dev/ttyNONEXISTENT");
#endif

    assert(!result);
    assert(!port.isOpen());
    std::cout << "  ✓ Non-existent port correctly rejected" << std::endl;

    // Close should be safe even if not open
    port.close();
    assert(!port.isOpen());
    std::cout << "  ✓ Close on unopened port is safe" << std::endl;

    std::cout << "✅ Port open/close test passed!" << std::endl << std::endl;
}

void testMoveSemantics()
{
    std::cout << "Testing move semantics..." << std::endl;

    SerialPort port1;

    // Move constructor
    SerialPort port2(std::move(port1));
    assert(!port1.isOpen());
    assert(!port2.isOpen());
    std::cout << "  ✓ Move constructor works" << std::endl;

    // Move assignment
    SerialPort port3;
    port3 = std::move(port2);
    assert(!port2.isOpen());
    assert(!port3.isOpen());
    std::cout << "  ✓ Move assignment works" << std::endl;

    std::cout << "✅ Move semantics test passed!" << std::endl << std::endl;
}

void testConfiguration()
{
    std::cout << "Testing configuration API..." << std::endl;

    SerialPort port;

    // Test that configuration can be specified (even if port doesn't open)
    // This just verifies the API compiles correctly
#ifdef _WIN32
    port.open("COM999", BaudRate::BAUD_115200, 8, Parity::NONE, StopBits::ONE, FlowControl::NONE);
#else
    port.open("/dev/ttyNONEXISTENT",
              BaudRate::BAUD_115200,
              8,
              Parity::NONE,
              StopBits::ONE,
              FlowControl::NONE);
#endif

    std::cout << "  ✓ Configuration API compiles correctly" << std::endl;

    std::cout << "✅ Configuration test passed!" << std::endl << std::endl;
}

void testReadWriteAPI()
{
    std::cout << "Testing read/write API..." << std::endl;

    SerialPort port;

    // Test that read/write fail gracefully when port is closed
    uint8_t buffer[10];
    size_t bytesRead = port.read(buffer, sizeof(buffer), 100);
    assert(bytesRead == 0);
    std::cout << "  ✓ Read on closed port returns 0" << std::endl;

    uint8_t writeData[] = {0x01, 0x02, 0x03};
    size_t bytesWritten = port.write(writeData, sizeof(writeData));
    assert(bytesWritten == 0);
    std::cout << "  ✓ Write on closed port returns 0" << std::endl;

    assert(!port.flushInput());
    std::cout << "  ✓ flushInput on closed port returns false" << std::endl;

    assert(!port.flushOutput());
    std::cout << "  ✓ flushOutput on closed port returns false" << std::endl;

    assert(port.available() == 0);
    std::cout << "  ✓ available on closed port returns 0" << std::endl;

    std::cout << "✅ Read/write API test passed!" << std::endl << std::endl;
}

void testErrorHandling()
{
    std::cout << "Testing error handling..." << std::endl;

    SerialPort port;

#ifdef _WIN32
    port.open("COM999");
#else
    port.open("/dev/ttyNONEXISTENT");
#endif

    std::string error = port.getLastError();
    assert(!error.empty());
    std::cout << "  ✓ Error message set: \"" << error << "\"" << std::endl;

    std::cout << "✅ Error handling test passed!" << std::endl << std::endl;
}

void testRealPortIfAvailable()
{
    std::cout << "Testing with real serial port (if available)..." << std::endl;

    std::vector<SerialPortInfo> ports = SerialPort::listPorts();
    if (ports.empty())
    {
        std::cout << "  ⚠ No serial ports available - skipping real port test" << std::endl;
        std::cout << "✅ Real port test skipped!" << std::endl << std::endl;
        return;
    }

    SerialPort port;
    bool opened = port.open(ports[0].port, BaudRate::BAUD_115200);

    if (!opened)
    {
        std::cout << "  ⚠ Could not open " << ports[0].port << " (may be in use)" << std::endl;
        std::cout << "  Error: " << port.getLastError() << std::endl;
        std::cout << "✅ Real port test skipped!" << std::endl << std::endl;
        return;
    }

    std::cout << "  ✓ Opened " << port.getPortName() << std::endl;
    assert(port.isOpen());
    assert(port.getPortName() == ports[0].port);

    // Test flush operations
    bool flushed = port.flushInput();
    std::cout << "  ✓ flushInput: " << (flushed ? "success" : "failed") << std::endl;

    flushed = port.flushOutput();
    std::cout << "  ✓ flushOutput: " << (flushed ? "success" : "failed") << std::endl;

    // Test available
    size_t available = port.available();
    std::cout << "  ✓ available: " << available << " bytes" << std::endl;

    // Test non-blocking read (should timeout with no data)
    uint8_t buffer[10];
    size_t bytesRead = port.read(buffer, sizeof(buffer), 100);
    std::cout << "  ✓ Non-blocking read: " << bytesRead << " bytes" << std::endl;

    // Test write
    uint8_t writeData[] = {0xAA, 0x01, 0x02, 0x03};
    size_t bytesWritten = port.write(writeData, sizeof(writeData));
    std::cout << "  ✓ Write: " << bytesWritten << " bytes" << std::endl;
    assert(bytesWritten == sizeof(writeData));

    port.close();
    assert(!port.isOpen());
    std::cout << "  ✓ Port closed" << std::endl;

    std::cout << "✅ Real port test passed!" << std::endl << std::endl;
}

int main()
{
    std::cout << "========================================" << std::endl;
    std::cout << "Serial Port Unit Tests" << std::endl;
    std::cout << "========================================" << std::endl << std::endl;

    try
    {
        testPortEnumeration();
        testPortOpenClose();
        testMoveSemantics();
        testConfiguration();
        testReadWriteAPI();
        testErrorHandling();
        testRealPortIfAvailable();

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
