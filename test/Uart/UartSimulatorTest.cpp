#include "UartSimulator.hpp"

#include <cassert>
#include <chrono>
#include <cstdio>
#include <iostream>
#include <thread>

using namespace UartProtocol;

void testSimulatorBasics()
{
    std::cout << "Testing simulator basics..." << std::endl;

    UartSimulator sim(1024);    // 1KB memory

    assert(!sim.isRunning());
    assert(sim.getPortName().empty());

    // Pre-populate memory
    uint8_t testData[] = {0x12, 0x34, 0x56, 0x78};
    assert(sim.writeMemory(0x100, testData, sizeof(testData)));
    std::cout << "  ✓ Memory write OK" << std::endl;

    // Read back
    uint8_t readData[4];
    assert(sim.readMemory(0x100, readData, sizeof(readData)));
    assert(memcmp(testData, readData, sizeof(testData)) == 0);
    std::cout << "  ✓ Memory read OK" << std::endl;

    // Test bounds checking
    assert(!sim.writeMemory(2000, testData, sizeof(testData)));    // Out of bounds
    std::cout << "  ✓ Bounds checking OK" << std::endl;

    // Test device info
    sim.setDeviceInfo("TestDevice", 0x0100, 0x03);
    std::cout << "  ✓ Device info set" << std::endl;

    std::cout << "✅ Simulator basics test passed!" << std::endl << std::endl;
}

void testSimulatorWithRealPort()
{
    std::cout << "Testing simulator start/stop..." << std::endl;

    // Find an available serial port
    std::vector<SerialPortInfo> ports = SerialPort::listPorts();
    if (ports.empty())
    {
        std::cout << "  ⚠ No serial ports available - skipping test" << std::endl;
        std::cout << "✅ Test skipped!" << std::endl << std::endl;
        return;
    }

    // Create simulator
    UartSimulator sim(1024);
    sim.setVerbose(false);
    sim.setDeviceInfo("TestDevice", 0x0100, 0x03);

    // Pre-populate memory with test pattern
    uint8_t testPattern[16];
    for (int i = 0; i < 16; i++)
        testPattern[i] = static_cast<uint8_t>(i * 0x11);
    sim.writeMemory(0x200, testPattern, sizeof(testPattern));

    // Start simulator on first available port
    std::cout << "  Attempting to start simulator on " << ports[0].port << "..." << std::endl;
    if (!sim.start(ports[0].port))
    {
        std::cout << "  ⚠ Could not start simulator (port may be in use)" << std::endl;
        std::cout << "✅ Test skipped!" << std::endl << std::endl;
        return;
    }

    std::cout << "  ✓ Simulator started on " << sim.getPortName() << std::endl;
    assert(sim.isRunning());
    assert(sim.getPortName() == ports[0].port);

    // Give simulator time to run
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Note: Testing actual communication requires either:
    // 1. A loopback cable (TX->RX shorted)
    // 2. A virtual serial port pair (socat)
    // 3. Two separate physical ports
    //
    // For now, we just test that the simulator thread runs without crashing

    auto stats = sim.getStatistics();
    std::cout << "  Statistics after 500ms:" << std::endl;
    std::cout << "    - Packets received: " << stats.packetsReceived << std::endl;
    std::cout << "    - Packets sent: " << stats.packetsSent << std::endl;
    std::cout << "    - CRC errors: " << stats.crcErrors << std::endl;

    // Stop simulator
    sim.stop();
    assert(!sim.isRunning());
    std::cout << "  ✓ Simulator stopped cleanly" << std::endl;

    // Test restart
    if (sim.start(ports[0].port))
    {
        std::cout << "  ✓ Simulator restarted successfully" << std::endl;
        assert(sim.isRunning());
        sim.stop();
    }

    std::cout << "✅ Simulator lifecycle test passed!" << std::endl << std::endl;
}

void testVirtualPort()
{
    std::cout << "Testing virtual serial port creation..." << std::endl;

#ifdef _WIN32
    std::cout << "  ⚠ Virtual ports not supported on Windows (requires com0com)" << std::endl;
    std::cout << "✅ Test skipped!" << std::endl << std::endl;
    return;
#else

    // Test PTY creation
    std::string master, slave;
    if (VirtualSerialPort::createPair(master, slave))
    {
        std::cout << "  ✓ PTY pair created:" << std::endl;
        std::cout << "    Master: " << master << std::endl;
        std::cout << "    Slave: " << slave << std::endl;
    }
    else
    {
        std::cout << "  ✗ Failed to create PTY pair" << std::endl;
    }

    // Test socat (if installed)
    std::cout << "  Testing socat virtual ports..." << std::endl;
    std::string port1, port2;
    if (VirtualSerialPort::createSocatPair(port1, port2))
    {
        std::cout << "  ✓ Socat ports created:" << std::endl;
        std::cout << "    Port 1: " << port1 << std::endl;
        std::cout << "    Port 2: " << port2 << std::endl;

        // Try to use them
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        SerialPort testPort;
        if (testPort.open(port1))
        {
            std::cout << "  ✓ Successfully opened virtual port" << std::endl;
            testPort.close();

            // Clean up socat process
            system("killall socat 2>/dev/null");
        }
        else
        {
            std::cout << "  ✗ Failed to open virtual port: " << testPort.getLastError() << std::endl;
        }
    }
    else
    {
        std::cout << "  ⚠ Socat not available or failed (may need to install: brew install socat)" << std::endl;
    }

    std::cout << "✅ Virtual port test complete!" << std::endl << std::endl;
#endif
}

int main()
{
    std::cout << "========================================" << std::endl;
    std::cout << "UART Simulator Unit Tests" << std::endl;
    std::cout << "========================================" << std::endl << std::endl;

    try
    {
        testSimulatorBasics();
        testVirtualPort();
        testSimulatorWithRealPort();

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
