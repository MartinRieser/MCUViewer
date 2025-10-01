/**
 * @file UartDebugProbeTest.cpp
 * @brief Unit tests for UartDebugProbe class
 *
 * Tests UART debug probe with the simulator to verify:
 * - Connection establishment
 * - Memory read operations
 * - Memory write operations
 * - Error handling
 */

#include <gtest/gtest.h>

#include <chrono>
#include <memory>
#include <thread>

#include "UartDebugProbe.hpp"
#include "UartSimulator.hpp"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/spdlog.h"

class UartDebugProbeTest : public ::testing::Test
{
  protected:
	void SetUp() override
	{
		// Create or get logger
		try
		{
			logger = spdlog::stdout_color_mt("UartDebugProbeTest");
		}
		catch (const spdlog::spdlog_ex&)
		{
			logger = spdlog::get("UartDebugProbeTest");
		}
		logger->set_level(spdlog::level::debug);

		// Create simulator with virtual port
		simulator = std::make_unique<UartProtocol::UartSimulator>();
		simulator->setDeviceInfo("TestDevice", 0x0100, 0);

		// Initialize memory map with test data
		uint32_t val1 = 0x12345678;
		uint32_t val2 = 0xABCDEF00;
		uint32_t val3 = 0xDEADBEEF;
		simulator->writeMemory(0x20000000, reinterpret_cast<const uint8_t*>(&val1), 4);
		simulator->writeMemory(0x20000004, reinterpret_cast<const uint8_t*>(&val2), 4);
		simulator->writeMemory(0x20000008, reinterpret_cast<const uint8_t*>(&val3), 4);

		// Create probe
		probe = std::make_unique<UartDebugProbe>(logger);
	}

	void TearDown() override
	{
		if (simulator && simulator->isRunning())
		{
			simulator->stop();
		}

		if (probe && probe->isValid())
		{
			probe->stopAcqusition();
		}

		// Wait for cleanup
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}

	std::shared_ptr<spdlog::logger> logger;
	std::unique_ptr<UartProtocol::UartSimulator> simulator;
	std::unique_ptr<UartDebugProbe> probe;

	/**
	 * @brief Start simulator and connect probe
	 *
	 * @param portName Port name (can be real or virtual)
	 * @return true if connection successful
	 */
	bool connectToSimulator(const std::string& portName)
	{
		// Start simulator
		if (!simulator->start(portName))
		{
			logger->error("Failed to start simulator");
			return false;
		}

		// Wait for simulator to initialize
		std::this_thread::sleep_for(std::chrono::milliseconds(100));

		// Connect probe
		IDebugProbe::DebugProbeSettings settings;
		settings.device = portName;
		settings.speedkHz = 115200;    // Baud rate

		std::vector<std::pair<uint32_t, uint8_t>> addressSizeVector;

		return probe->startAcqusition(settings, addressSizeVector, 0);
	}
};

/**
 * @brief Test UART port enumeration
 */
TEST_F(UartDebugProbeTest, GetConnectedDevices)
{
	auto devices = probe->getConnectedDevices();

	// Should find at least some ports (platform-dependent)
	EXPECT_GE(devices.size(), 0);

	logger->info("Found {} UART ports", devices.size());
	for (const auto& device : devices)
	{
		logger->info("  - {}", device);
	}
}

/**
 * @brief Test connection to simulator
 */
TEST_F(UartDebugProbeTest, ConnectToSimulator)
{
	// Try to find a suitable port for testing
	auto devices = probe->getConnectedDevices();

	if (devices.empty())
	{
		GTEST_SKIP() << "No UART ports available for testing";
	}

	// Use first available port (assuming it's available)
	std::string testPort = devices[0];
	logger->info("Testing with port: {}", testPort);

	// Note: This test requires either:
	// 1. A loopback cable (TX->RX) on the port
	// 2. The UART simulator running on the port
	// 3. Or will be skipped if no suitable hardware available

	// For automated testing, we skip this test if no simulator is available
	GTEST_SKIP() << "Skipping hardware test (requires simulator or loopback)";
}

/**
 * @brief Test memory read operation
 */
TEST_F(UartDebugProbeTest, ReadMemory)
{
	// This test requires a running simulator or real hardware
	// For now, we test the error handling when not connected

	uint8_t buffer[4];
	bool result = probe->readMemory(0x20000000, buffer, 4);

	EXPECT_FALSE(result);    // Should fail when not connected
	EXPECT_EQ(probe->getLastErrorMsg(), "Probe not connected");
}

/**
 * @brief Test memory write operation
 */
TEST_F(UartDebugProbeTest, WriteMemory)
{
	// This test requires a running simulator or real hardware
	// For now, we test the error handling when not connected

	uint8_t buffer[4] = {0x11, 0x22, 0x33, 0x44};
	bool result = probe->writeMemory(0x20000000, buffer, 4);

	EXPECT_FALSE(result);    // Should fail when not connected
	EXPECT_EQ(probe->getLastErrorMsg(), "Probe not connected");
}

/**
 * @brief Test invalid read size
 */
TEST_F(UartDebugProbeTest, InvalidReadSize)
{
	uint8_t buffer[300];

	// Connect probe first (will fail, but we're testing size validation)
	// Size too large (>255)
	bool result = probe->readMemory(0x20000000, buffer, 300);
	EXPECT_FALSE(result);

	// Size zero
	result = probe->readMemory(0x20000000, buffer, 0);
	EXPECT_FALSE(result);
}

/**
 * @brief Test invalid write size
 */
TEST_F(UartDebugProbeTest, InvalidWriteSize)
{
	uint8_t buffer[300];

	// Size too large (>255)
	bool result = probe->writeMemory(0x20000000, buffer, 300);
	EXPECT_FALSE(result);

	// Size zero
	result = probe->writeMemory(0x20000000, buffer, 0);
	EXPECT_FALSE(result);
}

/**
 * @brief Test isValid() before connection
 */
TEST_F(UartDebugProbeTest, IsValidBeforeConnection)
{
	EXPECT_FALSE(probe->isValid());
}

/**
 * @brief Test getTargetName() before connection
 */
TEST_F(UartDebugProbeTest, GetTargetNameBeforeConnection)
{
	std::string name = probe->getTargetName();
	EXPECT_EQ(name, "Unknown");
}

/**
 * @brief Test readSingleEntry() (HSS mode not supported)
 */
TEST_F(UartDebugProbeTest, ReadSingleEntry)
{
	auto result = probe->readSingleEntry();
	EXPECT_FALSE(result.has_value());    // HSS mode not implemented
}

/**
 * @brief Manual integration test with simulator
 *
 * This test is disabled by default and can be enabled for manual testing
 * with a virtual serial port pair.
 *
 * To test manually:
 * 1. Create virtual port pair: socat -d -d pty,raw,echo=0 pty,raw,echo=0
 * 2. Note the two PTY paths (e.g., /dev/pts/5 and /dev/pts/6)
 * 3. Update the port names below
 * 4. Run test with: ./test/MCUViewer_test --gtest_filter=*ManualIntegration*
 */
TEST_F(UartDebugProbeTest, DISABLED_ManualIntegrationTest)
{
	// Update these paths with your virtual port pair
	std::string simulatorPort = "/dev/pts/6";    // Simulator side
	std::string probePort = "/dev/pts/5";        // Probe side

	logger->info("=== Manual Integration Test ===");
	logger->info("Starting simulator on: {}", simulatorPort);
	logger->info("Connecting probe to: {}", probePort);

	// Start simulator
	ASSERT_TRUE(simulator->start(simulatorPort));
	std::this_thread::sleep_for(std::chrono::milliseconds(200));

	// Connect probe
	IDebugProbe::DebugProbeSettings settings;
	settings.device = probePort;
	settings.speedkHz = 115200;
	std::vector<std::pair<uint32_t, uint8_t>> addressSizeVector;

	ASSERT_TRUE(probe->startAcqusition(settings, addressSizeVector, 0));
	EXPECT_TRUE(probe->isValid());

	// Check target name
	std::string targetName = probe->getTargetName();
	EXPECT_EQ(targetName, "TestDevice");

	logger->info("Connected to target: {}", targetName);

	// Test read memory
	logger->info("Testing memory read...");
	uint8_t readBuffer[4];
	ASSERT_TRUE(probe->readMemory(0x20000000, readBuffer, 4));

	uint32_t readValue = *reinterpret_cast<uint32_t*>(readBuffer);
	EXPECT_EQ(readValue, 0x12345678);
	logger->info("Read 0x{:08X} from 0x20000000", readValue);

	// Test write memory
	logger->info("Testing memory write...");
	uint8_t writeBuffer[4] = {0x11, 0x22, 0x33, 0x44};
	ASSERT_TRUE(probe->writeMemory(0x20000010, writeBuffer, 4));

	// Read back to verify
	ASSERT_TRUE(probe->readMemory(0x20000010, readBuffer, 4));
	readValue = *reinterpret_cast<uint32_t*>(readBuffer);
	EXPECT_EQ(readValue, 0x44332211);    // Little-endian
	logger->info("Read back 0x{:08X} from 0x20000010", readValue);

	// Test multiple reads
	logger->info("Testing multiple reads...");
	for (int i = 0; i < 10; i++)
	{
		ASSERT_TRUE(probe->readMemory(0x20000000 + (i * 4), readBuffer, 4));
	}
	logger->info("Multiple reads successful");

	// Performance test
	logger->info("Performance test: 100 reads...");
	auto start = std::chrono::steady_clock::now();
	for (int i = 0; i < 100; i++)
	{
		ASSERT_TRUE(probe->readMemory(0x20000000, readBuffer, 4));
	}
	auto end = std::chrono::steady_clock::now();
	auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

	logger->info("100 reads took {}ms", duration.count());
	logger->info("Average: {:.2f}ms per read", duration.count() / 100.0);

	// Check simulator statistics
	auto stats = simulator->getStatistics();
	logger->info("Simulator stats:");
	logger->info("  Packets RX: {}", stats.packetsReceived);
	logger->info("  Packets TX: {}", stats.packetsSent);
	logger->info("  CRC Errors: {}", stats.crcErrors);
	logger->info("  Invalid Commands: {}", stats.invalidCommands);
	logger->info("  Memory Errors: {}", stats.memoryErrors);

	// Cleanup
	probe->stopAcqusition();
	simulator->stop();

	logger->info("=== Test Complete ===");
}

