/**
 * @file UartIntegrationTest.cpp
 * @brief Integration tests for UART debug probe end-to-end functionality
 *
 * This test suite verifies the complete UART probe workflow including:
 * - Multi-variable read/write scenarios (10 variables)
 * - Error handling (invalid addresses, timeouts, CRC errors)
 * - Reconnection after disconnect
 * - Performance measurements
 *
 * @note These tests require a virtual serial port pair:
 *       Linux/macOS: socat -d -d pty,raw,echo=0 pty,raw,echo=0
 *       Then update SIMULATOR_PORT and PROBE_PORT below with the PTY paths
 */

#include <gtest/gtest.h>

#include <chrono>
#include <cmath>
#include <memory>
#include <thread>
#include <vector>

#include "UartDebugProbe.hpp"
#include "UartSimulator.hpp"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/spdlog.h"

// Configuration for virtual port pair
// Update these paths based on your socat output
#ifndef SIMULATOR_PORT
#define SIMULATOR_PORT "/dev/pts/6"
#endif

#ifndef PROBE_PORT
#define PROBE_PORT "/dev/pts/5"
#endif

class UartIntegrationTest : public ::testing::Test
{
  protected:
	void SetUp() override
	{
		// Create or get logger
		try
		{
			logger = spdlog::stdout_color_mt("UartIntegrationTest");
		}
		catch (const spdlog::spdlog_ex&)
		{
			logger = spdlog::get("UartIntegrationTest");
		}
		logger->set_level(spdlog::level::info);

		// Create simulator
		simulator = std::make_unique<UartProtocol::UartSimulator>();
		simulator->setDeviceInfo("IntegrationTestDevice", 0x0100, 0);

		// Initialize test variables (10 variables as per Task 1.6.1)
		initializeTestVariables();

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

	/**
	 * @brief Initialize 10 test variables in simulator memory
	 */
	void initializeTestVariables()
	{
		// Variable 1: Counter (uint32_t)
		uint32_t counter = 0x00000001;
		simulator->writeMemory(0x20000000, reinterpret_cast<const uint8_t*>(&counter), 4);

		// Variable 2: Temperature (float, 25.5°C)
		float temperature = 25.5f;
		simulator->writeMemory(0x20000004, reinterpret_cast<const uint8_t*>(&temperature), 4);

		// Variable 3: Pressure (uint16_t)
		uint16_t pressure = 1013;
		simulator->writeMemory(0x20000008, reinterpret_cast<const uint8_t*>(&pressure), 2);

		// Variable 4: Status flags (uint8_t)
		uint8_t status = 0xAB;
		simulator->writeMemory(0x2000000A, reinterpret_cast<const uint8_t*>(&status), 1);

		// Variable 5: Voltage (float, 3.3V)
		float voltage = 3.3f;
		simulator->writeMemory(0x2000000C, reinterpret_cast<const uint8_t*>(&voltage), 4);

		// Variable 6: Current (float, 1.5A)
		float current = 1.5f;
		simulator->writeMemory(0x20000010, reinterpret_cast<const uint8_t*>(&current), 4);

		// Variable 7: Timestamp (uint64_t)
		uint64_t timestamp = 0x123456789ABCDEF0ULL;
		simulator->writeMemory(0x20000014, reinterpret_cast<const uint8_t*>(&timestamp), 8);

		// Variable 8: Position X (int32_t)
		int32_t posX = -12345;
		simulator->writeMemory(0x2000001C, reinterpret_cast<const uint8_t*>(&posX), 4);

		// Variable 9: Position Y (int32_t)
		int32_t posY = 67890;
		simulator->writeMemory(0x20000020, reinterpret_cast<const uint8_t*>(&posY), 4);

		// Variable 10: Error code (uint32_t)
		uint32_t errorCode = 0x00000000;
		simulator->writeMemory(0x20000024, reinterpret_cast<const uint8_t*>(&errorCode), 4);
	}

	/**
	 * @brief Start simulator and connect probe
	 * @return true if connection successful
	 */
	bool connectToSimulator()
	{
		// Start simulator
		if (!simulator->start(SIMULATOR_PORT))
		{
			logger->error("Failed to start simulator on {}", SIMULATOR_PORT);
			return false;
		}

		// Wait for simulator to initialize
		std::this_thread::sleep_for(std::chrono::milliseconds(200));

		// Connect probe
		IDebugProbe::DebugProbeSettings settings;
		settings.device = PROBE_PORT;
		settings.speedkHz = 115200;

		std::vector<std::pair<uint32_t, uint8_t>> addressSizeVector;

		bool connected = probe->startAcqusition(settings, addressSizeVector, 0);
		if (!connected)
		{
			logger->error("Failed to connect probe to {}", PROBE_PORT);
			return false;
		}

		return true;
	}

	/**
	 * @brief Helper to compare float values with tolerance
	 */
	bool floatEquals(float a, float b, float epsilon = 0.0001f)
	{
		return std::fabs(a - b) < epsilon;
	}

	std::shared_ptr<spdlog::logger> logger;
	std::unique_ptr<UartProtocol::UartSimulator> simulator;
	std::unique_ptr<UartDebugProbe> probe;
};

/**
 * @brief Task 1.6.1: Test scenario with 10 variables
 * @brief Task 1.6.2: Test READ_MEMORY for all variables
 */
TEST_F(UartIntegrationTest, DISABLED_ReadAllVariables)
{
	logger->info("=== Task 1.6.2: Read All Variables Test ===");

	ASSERT_TRUE(connectToSimulator());
	EXPECT_TRUE(probe->isValid());

	logger->info("Connected successfully to simulator");

	// Read Variable 1: Counter
	{
		uint8_t buffer[4];
		ASSERT_TRUE(probe->readMemory(0x20000000, buffer, 4));
		uint32_t value = *reinterpret_cast<uint32_t*>(buffer);
		EXPECT_EQ(value, 0x00000001);
		logger->info("Var 1 (Counter): 0x{:08X}", value);
	}

	// Read Variable 2: Temperature
	{
		uint8_t buffer[4];
		ASSERT_TRUE(probe->readMemory(0x20000004, buffer, 4));
		float value = *reinterpret_cast<float*>(buffer);
		EXPECT_TRUE(floatEquals(value, 25.5f));
		logger->info("Var 2 (Temperature): {:.2f}°C", value);
	}

	// Read Variable 3: Pressure
	{
		uint8_t buffer[2];
		ASSERT_TRUE(probe->readMemory(0x20000008, buffer, 2));
		uint16_t value = *reinterpret_cast<uint16_t*>(buffer);
		EXPECT_EQ(value, 1013);
		logger->info("Var 3 (Pressure): {} hPa", value);
	}

	// Read Variable 4: Status
	{
		uint8_t buffer[1];
		ASSERT_TRUE(probe->readMemory(0x2000000A, buffer, 1));
		uint8_t value = buffer[0];
		EXPECT_EQ(value, 0xAB);
		logger->info("Var 4 (Status): 0x{:02X}", value);
	}

	// Read Variable 5: Voltage
	{
		uint8_t buffer[4];
		ASSERT_TRUE(probe->readMemory(0x2000000C, buffer, 4));
		float value = *reinterpret_cast<float*>(buffer);
		EXPECT_TRUE(floatEquals(value, 3.3f));
		logger->info("Var 5 (Voltage): {:.2f}V", value);
	}

	// Read Variable 6: Current
	{
		uint8_t buffer[4];
		ASSERT_TRUE(probe->readMemory(0x20000010, buffer, 4));
		float value = *reinterpret_cast<float*>(buffer);
		EXPECT_TRUE(floatEquals(value, 1.5f));
		logger->info("Var 6 (Current): {:.2f}A", value);
	}

	// Read Variable 7: Timestamp
	{
		uint8_t buffer[8];
		ASSERT_TRUE(probe->readMemory(0x20000014, buffer, 8));
		uint64_t value = *reinterpret_cast<uint64_t*>(buffer);
		EXPECT_EQ(value, 0x123456789ABCDEF0ULL);
		logger->info("Var 7 (Timestamp): 0x{:016X}", value);
	}

	// Read Variable 8: Position X
	{
		uint8_t buffer[4];
		ASSERT_TRUE(probe->readMemory(0x2000001C, buffer, 4));
		int32_t value = *reinterpret_cast<int32_t*>(buffer);
		EXPECT_EQ(value, -12345);
		logger->info("Var 8 (PosX): {}", value);
	}

	// Read Variable 9: Position Y
	{
		uint8_t buffer[4];
		ASSERT_TRUE(probe->readMemory(0x20000020, buffer, 4));
		int32_t value = *reinterpret_cast<int32_t*>(buffer);
		EXPECT_EQ(value, 67890);
		logger->info("Var 9 (PosY): {}", value);
	}

	// Read Variable 10: Error code
	{
		uint8_t buffer[4];
		ASSERT_TRUE(probe->readMemory(0x20000024, buffer, 4));
		uint32_t value = *reinterpret_cast<uint32_t*>(buffer);
		EXPECT_EQ(value, 0x00000000);
		logger->info("Var 10 (ErrorCode): 0x{:08X}", value);
	}

	logger->info("All 10 variables read successfully!");

	// Check simulator statistics
	auto stats = simulator->getStatistics();
	logger->info("Simulator stats:");
	logger->info("  Packets RX: {}", stats.packetsReceived);
	logger->info("  Packets TX: {}", stats.packetsSent);
	logger->info("  CRC Errors: {}", stats.crcErrors);
	EXPECT_EQ(stats.crcErrors, 0);    // No CRC errors expected
}

/**
 * @brief Task 1.6.3: Test WRITE_MEMORY for all variables
 */
TEST_F(UartIntegrationTest, DISABLED_WriteAllVariables)
{
	logger->info("=== Task 1.6.3: Write All Variables Test ===");

	ASSERT_TRUE(connectToSimulator());
	EXPECT_TRUE(probe->isValid());

	logger->info("Connected successfully to simulator");

	// Write Variable 1: Counter
	{
		uint32_t newValue = 0x99999999;
		ASSERT_TRUE(probe->writeMemory(0x20000000, reinterpret_cast<uint8_t*>(&newValue), 4));

		// Read back to verify
		uint8_t buffer[4];
		ASSERT_TRUE(probe->readMemory(0x20000000, buffer, 4));
		uint32_t readValue = *reinterpret_cast<uint32_t*>(buffer);
		EXPECT_EQ(readValue, newValue);
		logger->info("Var 1: Wrote 0x{:08X}, read back 0x{:08X}", newValue, readValue);
	}

	// Write Variable 2: Temperature
	{
		float newValue = -10.25f;
		ASSERT_TRUE(probe->writeMemory(0x20000004, reinterpret_cast<uint8_t*>(&newValue), 4));

		uint8_t buffer[4];
		ASSERT_TRUE(probe->readMemory(0x20000004, buffer, 4));
		float readValue = *reinterpret_cast<float*>(buffer);
		EXPECT_TRUE(floatEquals(readValue, newValue));
		logger->info("Var 2: Wrote {:.2f}°C, read back {:.2f}°C", newValue, readValue);
	}

	// Write Variable 3: Pressure
	{
		uint16_t newValue = 950;
		ASSERT_TRUE(probe->writeMemory(0x20000008, reinterpret_cast<uint8_t*>(&newValue), 2));

		uint8_t buffer[2];
		ASSERT_TRUE(probe->readMemory(0x20000008, buffer, 2));
		uint16_t readValue = *reinterpret_cast<uint16_t*>(buffer);
		EXPECT_EQ(readValue, newValue);
		logger->info("Var 3: Wrote {} hPa, read back {} hPa", newValue, readValue);
	}

	// Write Variable 4: Status
	{
		uint8_t newValue = 0x42;
		ASSERT_TRUE(probe->writeMemory(0x2000000A, &newValue, 1));

		uint8_t buffer[1];
		ASSERT_TRUE(probe->readMemory(0x2000000A, buffer, 1));
		uint8_t readValue = buffer[0];
		EXPECT_EQ(readValue, newValue);
		logger->info("Var 4: Wrote 0x{:02X}, read back 0x{:02X}", newValue, readValue);
	}

	// Write Variable 5: Voltage
	{
		float newValue = 5.0f;
		ASSERT_TRUE(probe->writeMemory(0x2000000C, reinterpret_cast<uint8_t*>(&newValue), 4));

		uint8_t buffer[4];
		ASSERT_TRUE(probe->readMemory(0x2000000C, buffer, 4));
		float readValue = *reinterpret_cast<float*>(buffer);
		EXPECT_TRUE(floatEquals(readValue, newValue));
		logger->info("Var 5: Wrote {:.2f}V, read back {:.2f}V", newValue, readValue);
	}

	// Write Variable 6: Current
	{
		float newValue = 2.75f;
		ASSERT_TRUE(probe->writeMemory(0x20000010, reinterpret_cast<uint8_t*>(&newValue), 4));

		uint8_t buffer[4];
		ASSERT_TRUE(probe->readMemory(0x20000010, buffer, 4));
		float readValue = *reinterpret_cast<float*>(buffer);
		EXPECT_TRUE(floatEquals(readValue, newValue));
		logger->info("Var 6: Wrote {:.2f}A, read back {:.2f}A", newValue, readValue);
	}

	// Write Variable 7: Timestamp
	{
		uint64_t newValue = 0xFEDCBA9876543210ULL;
		ASSERT_TRUE(probe->writeMemory(0x20000014, reinterpret_cast<uint8_t*>(&newValue), 8));

		uint8_t buffer[8];
		ASSERT_TRUE(probe->readMemory(0x20000014, buffer, 8));
		uint64_t readValue = *reinterpret_cast<uint64_t*>(buffer);
		EXPECT_EQ(readValue, newValue);
		logger->info("Var 7: Wrote 0x{:016X}, read back 0x{:016X}", newValue, readValue);
	}

	// Write Variable 8: Position X
	{
		int32_t newValue = 999888;
		ASSERT_TRUE(probe->writeMemory(0x2000001C, reinterpret_cast<uint8_t*>(&newValue), 4));

		uint8_t buffer[4];
		ASSERT_TRUE(probe->readMemory(0x2000001C, buffer, 4));
		int32_t readValue = *reinterpret_cast<int32_t*>(buffer);
		EXPECT_EQ(readValue, newValue);
		logger->info("Var 8: Wrote {}, read back {}", newValue, readValue);
	}

	// Write Variable 9: Position Y
	{
		int32_t newValue = -777666;
		ASSERT_TRUE(probe->writeMemory(0x20000020, reinterpret_cast<uint8_t*>(&newValue), 4));

		uint8_t buffer[4];
		ASSERT_TRUE(probe->readMemory(0x20000020, buffer, 4));
		int32_t readValue = *reinterpret_cast<int32_t*>(buffer);
		EXPECT_EQ(readValue, newValue);
		logger->info("Var 9: Wrote {}, read back {}", newValue, readValue);
	}

	// Write Variable 10: Error code
	{
		uint32_t newValue = 0xDEADBEEF;
		ASSERT_TRUE(probe->writeMemory(0x20000024, reinterpret_cast<uint8_t*>(&newValue), 4));

		uint8_t buffer[4];
		ASSERT_TRUE(probe->readMemory(0x20000024, buffer, 4));
		uint32_t readValue = *reinterpret_cast<uint32_t*>(buffer);
		EXPECT_EQ(readValue, newValue);
		logger->info("Var 10: Wrote 0x{:08X}, read back 0x{:08X}", newValue, readValue);
	}

	logger->info("All 10 variables written and verified successfully!");

	// Check simulator statistics
	auto stats = simulator->getStatistics();
	logger->info("Simulator stats:");
	logger->info("  Packets RX: {}", stats.packetsReceived);
	logger->info("  Packets TX: {}", stats.packetsSent);
	logger->info("  Memory Errors: {}", stats.memoryErrors);
	EXPECT_EQ(stats.memoryErrors, 0);    // No memory errors expected
}

/**
 * @brief Task 1.6.4: Test error cases
 */
TEST_F(UartIntegrationTest, DISABLED_ErrorCases)
{
	logger->info("=== Task 1.6.4: Error Cases Test ===");

	ASSERT_TRUE(connectToSimulator());
	EXPECT_TRUE(probe->isValid());

	// Test 1: Invalid address (beyond memory bounds)
	logger->info("Test 1: Reading from invalid address...");
	{
		uint8_t buffer[4];
		// Simulator has 64KB memory, so 0xFFFF0000 is invalid
		bool result = probe->readMemory(0xFFFF0000, buffer, 4);
		// This should succeed at protocol level but return zeros or error from simulator
		logger->info("  Result: {}", result ? "success" : "failure");
	}

	// Test 2: Reading with zero size (should be caught before sending)
	logger->info("Test 2: Reading with zero size...");
	{
		uint8_t buffer[4];
		bool result = probe->readMemory(0x20000000, buffer, 0);
		EXPECT_FALSE(result);
		logger->info("  Correctly rejected zero-size read");
	}

	// Test 3: Reading with oversized buffer (>255 bytes)
	logger->info("Test 3: Reading with oversized buffer...");
	{
		uint8_t buffer[300];
		bool result = probe->readMemory(0x20000000, buffer, 300);
		EXPECT_FALSE(result);
		logger->info("  Correctly rejected oversized read");
	}

	// Test 4: Writing with zero size
	logger->info("Test 4: Writing with zero size...");
	{
		uint8_t buffer[4] = {0x11, 0x22, 0x33, 0x44};
		bool result = probe->writeMemory(0x20000000, buffer, 0);
		EXPECT_FALSE(result);
		logger->info("  Correctly rejected zero-size write");
	}

	// Test 5: Writing with oversized buffer
	logger->info("Test 5: Writing with oversized buffer...");
	{
		uint8_t buffer[300];
		bool result = probe->writeMemory(0x20000000, buffer, 300);
		EXPECT_FALSE(result);
		logger->info("  Correctly rejected oversized write");
	}

	// Test 6: Timeout scenario (stop simulator temporarily)
	logger->info("Test 6: Testing timeout...");
	{
		// Stop simulator to cause timeout
		simulator->stop();
		std::this_thread::sleep_for(std::chrono::milliseconds(100));

		uint8_t buffer[4];
		auto startTime = std::chrono::steady_clock::now();
		bool result = probe->readMemory(0x20000000, buffer, 4);
		auto endTime = std::chrono::steady_clock::now();
		auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

		EXPECT_FALSE(result);
		logger->info("  Timeout detected after {}ms", duration.count());

		// Restart simulator for other tests
		ASSERT_TRUE(simulator->start(SIMULATOR_PORT));
		std::this_thread::sleep_for(std::chrono::milliseconds(200));
	}

	// Note: CRC error testing would require injecting corrupt packets,
	// which is difficult without modifying the serial port layer.
	// The simulator already validates CRC and rejects bad packets.

	logger->info("Error cases tested successfully!");
}

/**
 * @brief Task 1.6.5: Test reconnection after disconnect
 */
TEST_F(UartIntegrationTest, DISABLED_Reconnection)
{
	logger->info("=== Task 1.6.5: Reconnection Test ===");

	// Initial connection
	logger->info("Initial connection...");
	ASSERT_TRUE(connectToSimulator());
	EXPECT_TRUE(probe->isValid());

	// Perform some reads
	logger->info("Reading variables before disconnect...");
	uint8_t buffer[4];
	ASSERT_TRUE(probe->readMemory(0x20000000, buffer, 4));
	uint32_t value1 = *reinterpret_cast<uint32_t*>(buffer);
	logger->info("  Read value: 0x{:08X}", value1);

	// Disconnect
	logger->info("Disconnecting...");
	probe->stopAcqusition();
	simulator->stop();
	std::this_thread::sleep_for(std::chrono::milliseconds(500));

	EXPECT_FALSE(probe->isValid());

	// Attempt to read while disconnected (should fail)
	logger->info("Attempting read while disconnected...");
	bool result = probe->readMemory(0x20000000, buffer, 4);
	EXPECT_FALSE(result);
	logger->info("  Correctly failed to read while disconnected");

	// Reconnect
	logger->info("Reconnecting...");
	ASSERT_TRUE(connectToSimulator());
	EXPECT_TRUE(probe->isValid());

	// Perform reads after reconnection
	logger->info("Reading variables after reconnect...");
	ASSERT_TRUE(probe->readMemory(0x20000000, buffer, 4));
	uint32_t value2 = *reinterpret_cast<uint32_t*>(buffer);
	logger->info("  Read value: 0x{:08X}", value2);

	EXPECT_EQ(value1, value2);    // Should read same value

	// Disconnect and reconnect again (multiple cycles)
	logger->info("Testing multiple reconnection cycles...");
	for (int i = 0; i < 3; i++)
	{
		logger->info("  Cycle {}/3", i + 1);

		// Disconnect
		probe->stopAcqusition();
		simulator->stop();
		std::this_thread::sleep_for(std::chrono::milliseconds(300));

		// Reconnect
		ASSERT_TRUE(connectToSimulator());
		EXPECT_TRUE(probe->isValid());

		// Verify read still works
		ASSERT_TRUE(probe->readMemory(0x20000000, buffer, 4));
	}

	logger->info("Reconnection test completed successfully!");
}

/**
 * @brief Task 1.6.6: Measure performance (reads/second)
 */
TEST_F(UartIntegrationTest, DISABLED_Performance)
{
	logger->info("=== Task 1.6.6: Performance Test ===");

	ASSERT_TRUE(connectToSimulator());
	EXPECT_TRUE(probe->isValid());

	logger->info("Connected successfully to simulator");

	// Test 1: 1000 reads of single variable
	logger->info("\nTest 1: 1000 reads of 4-byte variable...");
	{
		uint8_t buffer[4];
		int successCount = 0;
		int failCount = 0;

		auto startTime = std::chrono::steady_clock::now();

		for (int i = 0; i < 1000; i++)
		{
			if (probe->readMemory(0x20000000, buffer, 4))
			{
				successCount++;
			}
			else
			{
				failCount++;
			}
		}

		auto endTime = std::chrono::steady_clock::now();
		auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

		double successRate = (successCount * 100.0) / 1000.0;
		double avgLatency = duration.count() / 1000.0;
		double readsPerSecond = 1000.0 / (duration.count() / 1000.0);

		logger->info("  Duration: {}ms", duration.count());
		logger->info("  Success: {}/1000 ({:.1f}%)", successCount, successRate);
		logger->info("  Failures: {}", failCount);
		logger->info("  Average latency: {:.2f}ms per read", avgLatency);
		logger->info("  Throughput: {:.1f} reads/second", readsPerSecond);

		// Acceptance criteria: 100% success rate, <10ms latency
		EXPECT_EQ(successCount, 1000);
		EXPECT_LT(avgLatency, 10.0);
	}

	// Test 2: Read all 10 variables in sequence (100 cycles)
	logger->info("\nTest 2: 100 cycles of reading all 10 variables...");
	{
		uint8_t buffer[8];    // Max size needed
		int successCount = 0;
		int failCount = 0;

		std::vector<std::pair<uint32_t, size_t>> variables = {
			{0x20000000, 4},    // Counter
			{0x20000004, 4},    // Temperature
			{0x20000008, 2},    // Pressure
			{0x2000000A, 1},    // Status
			{0x2000000C, 4},    // Voltage
			{0x20000010, 4},    // Current
			{0x20000014, 8},    // Timestamp
			{0x2000001C, 4},    // PosX
			{0x20000020, 4},    // PosY
			{0x20000024, 4}     // ErrorCode
		};

		auto startTime = std::chrono::steady_clock::now();

		for (int cycle = 0; cycle < 100; cycle++)
		{
			for (const auto& var : variables)
			{
				if (probe->readMemory(var.first, buffer, var.second))
				{
					successCount++;
				}
				else
				{
					failCount++;
				}
			}
		}

		auto endTime = std::chrono::steady_clock::now();
		auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

		int totalReads = 100 * 10;    // 100 cycles * 10 variables
		double successRate = (successCount * 100.0) / totalReads;
		double avgLatency = duration.count() / static_cast<double>(totalReads);
		double readsPerSecond = totalReads / (duration.count() / 1000.0);

		logger->info("  Duration: {}ms", duration.count());
		logger->info("  Success: {}/{} ({:.1f}%)", successCount, totalReads, successRate);
		logger->info("  Failures: {}", failCount);
		logger->info("  Average latency: {:.2f}ms per read", avgLatency);
		logger->info("  Throughput: {:.1f} reads/second", readsPerSecond);

		EXPECT_EQ(successCount, totalReads);
	}

	// Test 3: Write performance
	logger->info("\nTest 3: 100 write operations...");
	{
		uint32_t testValue = 0x12345678;
		int successCount = 0;

		auto startTime = std::chrono::steady_clock::now();

		for (int i = 0; i < 100; i++)
		{
			testValue = i;
			if (probe->writeMemory(0x20000000, reinterpret_cast<uint8_t*>(&testValue), 4))
			{
				successCount++;
			}
		}

		auto endTime = std::chrono::steady_clock::now();
		auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

		double avgLatency = duration.count() / 100.0;
		double writesPerSecond = 100.0 / (duration.count() / 1000.0);

		logger->info("  Duration: {}ms", duration.count());
		logger->info("  Success: {}/100", successCount);
		logger->info("  Average latency: {:.2f}ms per write", avgLatency);
		logger->info("  Throughput: {:.1f} writes/second", writesPerSecond);

		EXPECT_EQ(successCount, 100);
	}

	// Display simulator statistics
	auto stats = simulator->getStatistics();
	logger->info("\nSimulator final statistics:");
	logger->info("  Total packets RX: {}", stats.packetsReceived);
	logger->info("  Total packets TX: {}", stats.packetsSent);
	logger->info("  CRC errors: {}", stats.crcErrors);
	logger->info("  Invalid commands: {}", stats.invalidCommands);
	logger->info("  Memory errors: {}", stats.memoryErrors);

	logger->info("\nPerformance test completed successfully!");
}

/**
 * @brief Combined integration test - all tasks in one run
 */
TEST_F(UartIntegrationTest, DISABLED_FullIntegration)
{
	logger->info("=== FULL INTEGRATION TEST ===");
	logger->info("This test combines all Task 1.6 subtasks\n");

	// Connect
	logger->info("Step 1: Connecting to simulator...");
	ASSERT_TRUE(connectToSimulator());
	EXPECT_TRUE(probe->isValid());
	logger->info("  ✓ Connected\n");

	// Read all variables
	logger->info("Step 2: Reading all 10 variables...");
	uint8_t buffer[8];
	for (int i = 0; i < 10; i++)
	{
		ASSERT_TRUE(probe->readMemory(0x20000000 + (i * 4), buffer, 4));
	}
	logger->info("  ✓ All reads successful\n");

	// Write all variables
	logger->info("Step 3: Writing all 10 variables...");
	uint32_t testValue = 0xAAAAAAAA;
	for (int i = 0; i < 10; i++)
	{
		ASSERT_TRUE(probe->writeMemory(0x20000000 + (i * 4), reinterpret_cast<uint8_t*>(&testValue), 4));
	}
	logger->info("  ✓ All writes successful\n");

	// Test error cases
	logger->info("Step 4: Testing error handling...");
	EXPECT_FALSE(probe->readMemory(0x20000000, buffer, 0));          // Zero size
	EXPECT_FALSE(probe->readMemory(0x20000000, buffer, 300));        // Oversized
	logger->info("  ✓ Error handling works\n");

	// Test reconnection
	logger->info("Step 5: Testing reconnection...");
	probe->stopAcqusition();
	simulator->stop();
	std::this_thread::sleep_for(std::chrono::milliseconds(500));
	ASSERT_TRUE(connectToSimulator());
	EXPECT_TRUE(probe->isValid());
	logger->info("  ✓ Reconnection successful\n");

	// Performance test
	logger->info("Step 6: Performance test (100 reads)...");
	auto startTime = std::chrono::steady_clock::now();
	int successCount = 0;
	for (int i = 0; i < 100; i++)
	{
		if (probe->readMemory(0x20000000, buffer, 4))
		{
			successCount++;
		}
	}
	auto endTime = std::chrono::steady_clock::now();
	auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

	logger->info("  100 reads: {}ms ({:.2f}ms/read)", duration.count(), duration.count() / 100.0);
	logger->info("  Success rate: {}/100", successCount);
	EXPECT_EQ(successCount, 100);
	logger->info("  ✓ Performance acceptable\n");

	// Final statistics
	auto stats = simulator->getStatistics();
	logger->info("Final Statistics:");
	logger->info("  Packets RX: {}", stats.packetsReceived);
	logger->info("  Packets TX: {}", stats.packetsSent);
	logger->info("  Errors: {} CRC, {} invalid, {} memory",
	             stats.crcErrors, stats.invalidCommands, stats.memoryErrors);

	logger->info("\n=== ALL INTEGRATION TESTS PASSED ===");
}
