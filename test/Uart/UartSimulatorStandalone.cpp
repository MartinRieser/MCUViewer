/**
 * @file UartSimulatorStandalone.cpp
 * @brief Standalone UART simulator for testing MCUViewer GUI
 *
 * This program runs a UART simulator that can be used to test the MCUViewer
 * GUI application without real hardware.
 *
 * Usage:
 *   ./UartSimulatorStandalone <serial_port> [options]
 *
 * Example:
 *   ./UartSimulatorStandalone /dev/pts/6
 *
 * The simulator will run until Ctrl+C is pressed.
 */

#include <csignal>
#include <cstring>
#include <iostream>
#include <thread>

#include "UartSimulator.hpp"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/spdlog.h"

// Global simulator instance for signal handler
UartProtocol::UartSimulator* g_simulator = nullptr;

void signalHandler(int signal)
{
	if (signal == SIGINT)
	{
		std::cout << "\n\nReceived Ctrl+C, stopping simulator...\n";
		if (g_simulator && g_simulator->isRunning())
		{
			g_simulator->stop();
		}
		exit(0);
	}
}

void printUsage(const char* programName)
{
	std::cout << "UART Simulator - Standalone Test Program\n";
	std::cout << "=========================================\n\n";
	std::cout << "Usage: " << programName << " <serial_port> [options]\n\n";
	std::cout << "Arguments:\n";
	std::cout << "  serial_port    Serial port to use (e.g., /dev/pts/6 or COM5)\n\n";
	std::cout << "Options:\n";
	std::cout << "  -h, --help     Show this help message\n";
	std::cout << "  -v, --verbose  Enable verbose logging\n\n";
	std::cout << "Example:\n";
	std::cout << "  " << programName << " /dev/pts/6\n";
	std::cout << "  " << programName << " COM5 -v\n\n";
	std::cout << "Setup:\n";
	std::cout << "  1. Create virtual serial port pair:\n";
	std::cout << "     macOS/Linux: socat -d -d pty,raw,echo=0 pty,raw,echo=0\n";
	std::cout << "     Windows: Use com0com or similar tool\n";
	std::cout << "  2. Run simulator on one port (e.g., /dev/pts/6)\n";
	std::cout << "  3. Connect MCUViewer to the other port (e.g., /dev/pts/5)\n\n";
}

int main(int argc, char* argv[])
{
	// Parse arguments
	if (argc < 2)
	{
		printUsage(argv[0]);
		return 1;
	}

	std::string portName;
	bool verbose = false;

	for (int i = 1; i < argc; i++)
	{
		std::string arg = argv[i];
		if (arg == "-h" || arg == "--help")
		{
			printUsage(argv[0]);
			return 0;
		}
		else if (arg == "-v" || arg == "--verbose")
		{
			verbose = true;
		}
		else if (portName.empty())
		{
			portName = arg;
		}
	}

	if (portName.empty())
	{
		std::cerr << "Error: Serial port not specified\n\n";
		printUsage(argv[0]);
		return 1;
	}

	// Setup logger
	auto logger = spdlog::stdout_color_mt("UartSimulator");
	logger->set_level(verbose ? spdlog::level::debug : spdlog::level::info);

	// Create simulator
	UartProtocol::UartSimulator simulator;
	g_simulator = &simulator;

	// Setup signal handler for Ctrl+C
	signal(SIGINT, signalHandler);

	// Configure device info
	simulator.setDeviceInfo("SimulatedDevice", 0x0100, 0);

	// Initialize memory with some test variables
	std::cout << "\nInitializing test variables:\n";
	std::cout << "============================\n";

	// Variable 1: Counter (uint32_t) at 0x20000000
	uint32_t counter = 0;
	simulator.writeMemory(0x20000000, reinterpret_cast<const uint8_t*>(&counter), 4);
	std::cout << "0x20000000: counter (uint32_t) = " << counter << "\n";

	// Variable 2: Temperature (float) at 0x20000004
	float temperature = 25.5f;
	simulator.writeMemory(0x20000004, reinterpret_cast<const uint8_t*>(&temperature), 4);
	std::cout << "0x20000004: temperature (float) = " << temperature << "°C\n";

	// Variable 3: Voltage (float) at 0x20000008
	float voltage = 3.3f;
	simulator.writeMemory(0x20000008, reinterpret_cast<const uint8_t*>(&voltage), 4);
	std::cout << "0x20000008: voltage (float) = " << voltage << "V\n";

	// Variable 4: Current (float) at 0x2000000C
	float current = 1.5f;
	simulator.writeMemory(0x2000000C, reinterpret_cast<const uint8_t*>(&current), 4);
	std::cout << "0x2000000C: current (float) = " << current << "A\n";

	// Variable 5: Status (uint8_t) at 0x20000010
	uint8_t status = 0x00;
	simulator.writeMemory(0x20000010, &status, 1);
	std::cout << "0x20000010: status (uint8_t) = 0x" << std::hex << (int)status << std::dec << "\n";

	// Variable 6: Error code (uint32_t) at 0x20000014
	uint32_t errorCode = 0;
	simulator.writeMemory(0x20000014, reinterpret_cast<const uint8_t*>(&errorCode), 4);
	std::cout << "0x20000014: errorCode (uint32_t) = " << errorCode << "\n";

	// Variable 7: Timestamp (uint64_t) at 0x20000018
	uint64_t timestamp = 0;
	simulator.writeMemory(0x20000018, reinterpret_cast<const uint8_t*>(&timestamp), 8);
	std::cout << "0x20000018: timestamp (uint64_t) = " << timestamp << "\n";

	// Variable 8: Position X (int32_t) at 0x20000020
	int32_t posX = 0;
	simulator.writeMemory(0x20000020, reinterpret_cast<const uint8_t*>(&posX), 4);
	std::cout << "0x20000020: posX (int32_t) = " << posX << "\n";

	// Variable 9: Position Y (int32_t) at 0x20000024
	int32_t posY = 0;
	simulator.writeMemory(0x20000024, reinterpret_cast<const uint8_t*>(&posY), 4);
	std::cout << "0x20000024: posY (int32_t) = " << posY << "\n";

	// Variable 10: Sine wave (float) at 0x20000028
	float sineWave = 0.0f;
	simulator.writeMemory(0x20000028, reinterpret_cast<const uint8_t*>(&sineWave), 4);
	std::cout << "0x20000028: sineWave (float) = " << sineWave << "\n";

	std::cout << "\nStarting UART simulator on port: " << portName << "\n";
	std::cout << "Press Ctrl+C to stop\n";
	std::cout << "============================\n\n";

	// Start simulator
	if (!simulator.start(portName))
	{
		logger->error("Failed to start simulator on port {}", portName);
		return 1;
	}

	logger->info("Simulator started successfully!");
	logger->info("Waiting for MCUViewer connection...");

	// Update variables periodically to simulate real device
	int iteration = 0;
	const double PI = 3.14159265358979323846;

	while (simulator.isRunning())
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(100));

		// Update counter
		counter = iteration;
		simulator.writeMemory(0x20000000, reinterpret_cast<const uint8_t*>(&counter), 4);

		// Update temperature (simulated room temperature with noise)
		temperature = 25.0f + (iteration % 10) * 0.1f;
		simulator.writeMemory(0x20000004, reinterpret_cast<const uint8_t*>(&temperature), 4);

		// Update voltage (simulated 3.3V rail with small variation)
		voltage = 3.3f + (iteration % 5 - 2) * 0.01f;
		simulator.writeMemory(0x20000008, reinterpret_cast<const uint8_t*>(&voltage), 4);

		// Update current (simulated load)
		current = 1.5f + 0.5f * std::sin(iteration * 0.1);
		simulator.writeMemory(0x2000000C, reinterpret_cast<const uint8_t*>(&current), 4);

		// Update status (toggle bits)
		status = (iteration / 10) % 256;
		simulator.writeMemory(0x20000010, &status, 1);

		// Update timestamp
		timestamp = iteration * 100;    // milliseconds
		simulator.writeMemory(0x20000018, reinterpret_cast<const uint8_t*>(&timestamp), 8);

		// Update positions (circular motion)
		posX = static_cast<int32_t>(1000 * std::cos(iteration * 0.05));
		posY = static_cast<int32_t>(1000 * std::sin(iteration * 0.05));
		simulator.writeMemory(0x20000020, reinterpret_cast<const uint8_t*>(&posX), 4);
		simulator.writeMemory(0x20000024, reinterpret_cast<const uint8_t*>(&posY), 4);

		// Update sine wave (1Hz frequency)
		sineWave = std::sin(iteration * 0.1);
		simulator.writeMemory(0x20000028, reinterpret_cast<const uint8_t*>(&sineWave), 4);

		iteration++;

		// Print statistics every 50 iterations (5 seconds)
		if (iteration % 50 == 0)
		{
			auto stats = simulator.getStatistics();
			logger->info("Statistics: RX={}, TX={}, CRC_ERR={}, MEM_ERR={}",
			             stats.packetsReceived, stats.packetsSent,
			             stats.crcErrors, stats.memoryErrors);
		}
	}

	std::cout << "\nSimulator stopped.\n";

	// Print final statistics
	auto stats = simulator.getStatistics();
	std::cout << "\nFinal Statistics:\n";
	std::cout << "=================\n";
	std::cout << "Packets received: " << stats.packetsReceived << "\n";
	std::cout << "Packets sent: " << stats.packetsSent << "\n";
	std::cout << "CRC errors: " << stats.crcErrors << "\n";
	std::cout << "Invalid commands: " << stats.invalidCommands << "\n";
	std::cout << "Memory errors: " << stats.memoryErrors << "\n";

	return 0;
}
