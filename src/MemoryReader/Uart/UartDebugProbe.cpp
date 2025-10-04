/**
 * @file UartDebugProbe.cpp
 * @brief UART-based debug probe implementation
 */

#include "UartDebugProbe.hpp"

#include <chrono>
#include <thread>

UartDebugProbe::UartDebugProbe(std::shared_ptr<spdlog::logger> logger)
    : logger(logger), serialPort(std::make_unique<UartProtocol::SerialPort>()), targetName("Unknown"), protocolVersion(0), capabilities(0), sequenceNumber(0)
{
	logger->info("UartDebugProbe created");
}

UartDebugProbe::~UartDebugProbe()
{
	if (isRunning)
	{
		stopAcqusition();
	}
}

std::vector<std::string> UartDebugProbe::getConnectedDevices()
{
	std::vector<std::string> devices;

	// Use SerialPort to list all available ports
	auto ports = UartProtocol::SerialPort::listPorts();

	for (const auto& port : ports)
	{
		devices.push_back(port.port);
	}

	logger->info("Found {} UART port(s)", devices.size());

	return devices;
}

bool UartDebugProbe::startAcqusition(const DebugProbeSettings& probeSettings, std::vector<std::pair<uint32_t, uint8_t>>& addressSizeVector, uint32_t samplingFreqency)
{
	std::lock_guard<std::mutex> lock(mtx);

	if (isRunning)
	{
		lastErrorMsg = "Acquisition already running";
		logger->warn("{}", lastErrorMsg);
		return false;
	}

	// Extract port name and baud rate from settings
	std::string portName = probeSettings.device;
	uint32_t baudRate = probeSettings.speedkHz;    // speedkHz is repurposed as baud rate

	// Default to 115200 if not specified
	if (baudRate == 0 || baudRate == 10000)
	{
		baudRate = 115200;
	}

	// Convert to BaudRate enum
	UartProtocol::BaudRate baudRateEnum;
	switch (baudRate)
	{
		case 9600:
			baudRateEnum = UartProtocol::BaudRate::BAUD_9600;
			break;
		case 19200:
			baudRateEnum = UartProtocol::BaudRate::BAUD_19200;
			break;
		case 38400:
			baudRateEnum = UartProtocol::BaudRate::BAUD_38400;
			break;
		case 57600:
			baudRateEnum = UartProtocol::BaudRate::BAUD_57600;
			break;
		case 115200:
			baudRateEnum = UartProtocol::BaudRate::BAUD_115200;
			break;
		case 230400:
			baudRateEnum = UartProtocol::BaudRate::BAUD_230400;
			break;
		case 460800:
			baudRateEnum = UartProtocol::BaudRate::BAUD_460800;
			break;
		case 921600:
			baudRateEnum = UartProtocol::BaudRate::BAUD_921600;
			break;
		default:
			logger->error("Unsupported baud rate: {}", baudRate);
			lastErrorMsg = "Unsupported baud rate";
			return false;
	}

	logger->info("Opening UART port: {} at {} baud", portName, baudRate);

	// Open serial port
	if (!serialPort->open(portName, baudRateEnum))
	{
		lastErrorMsg = "Failed to open serial port: " + serialPort->getLastError();
		logger->error("{}", lastErrorMsg);
		return false;
	}

	// Flush any stale data
	serialPort->flushInput();

	// Send GET_INFO to verify connection
	if (!sendGetInfo())
	{
		// Error message already set by sendGetInfo()
		serialPort->close();
		return false;
	}

	isRunning = true;
	logger->info("UART acquisition started, target: {}", targetName);

	return true;
}

bool UartDebugProbe::stopAcqusition()
{
	std::lock_guard<std::mutex> lock(mtx);

	if (!isRunning)
	{
		return true;
	}

	logger->info("Stopping UART acquisition");

	serialPort->close();
	isRunning = false;

	return true;
}

bool UartDebugProbe::isValid() const
{
	return isRunning && serialPort->isOpen();
}

std::string UartDebugProbe::getTargetName()
{
	return targetName;
}

std::optional<IDebugProbe::varEntryType> UartDebugProbe::readSingleEntry()
{
	// HSS mode not implemented for UART probe
	return std::nullopt;
}

bool UartDebugProbe::readMemory(uint32_t address, uint8_t* buf, uint32_t size)
{
	std::lock_guard<std::mutex> lock(mtx);

	if (!isValid())
	{
		lastErrorMsg = "Probe not connected";
		return false;
	}

	if (size == 0 || size > 255)
	{
		lastErrorMsg = "Invalid read size (must be 1-255)";
		return false;
	}

	// Create READ_MEMORY packet
	auto packet = UartProtocol::createReadMemoryPacket(address, static_cast<uint16_t>(size), getNextSequence());

	// Send and wait for response
	UartProtocol::Packet response;
	if (!sendAndReceive(packet, response, UartProtocol::CommandCode::READ_MEMORY_RESP))
	{
		lastErrorMsg = "Failed to receive READ_MEMORY response (timeout or invalid packet)";
		return false;
	}

	// Parse response payload
	UartProtocol::ReadMemoryRespPayload respPayload;
	if (!UartProtocol::parseReadMemoryResp(response.payload, respPayload))
	{
		lastErrorMsg = "Failed to parse READ_MEMORY response";
		logger->error("{}", lastErrorMsg);
		return false;
	}

	// Check status
	if (respPayload.status != static_cast<uint8_t>(UartProtocol::ErrorCode::SUCCESS))
	{
		lastErrorMsg = "READ_MEMORY failed: " + UartProtocol::getErrorName(static_cast<UartProtocol::ErrorCode>(respPayload.status));
		logger->error("{}", lastErrorMsg);
		return false;
	}

	// Check data size
	if (respPayload.data.size() != size)
	{
		lastErrorMsg = "READ_MEMORY size mismatch";
		logger->error("{}", lastErrorMsg);
		return false;
	}

	// Copy data to buffer
	std::memcpy(buf, respPayload.data.data(), size);

	return true;
}

bool UartDebugProbe::writeMemory(uint32_t address, uint8_t* buf, uint32_t size)
{
	std::lock_guard<std::mutex> lock(mtx);

	if (!isValid())
	{
		lastErrorMsg = "Probe not connected";
		return false;
	}

	if (size == 0 || size > 255)
	{
		lastErrorMsg = "Invalid write size (must be 1-255)";
		return false;
	}

	// Create data vector
	std::vector<uint8_t> data(buf, buf + size);

	// Create WRITE_MEMORY packet
	auto packet = UartProtocol::createWriteMemoryPacket(address, data, getNextSequence());

	// Send and wait for response
	UartProtocol::Packet response;
	if (!sendAndReceive(packet, response, UartProtocol::CommandCode::WRITE_MEMORY_RESP))
	{
		lastErrorMsg = "Failed to receive WRITE_MEMORY response (timeout or invalid packet)";
		return false;
	}

	// Parse response payload
	if (response.payload.size() < 1)
	{
		lastErrorMsg = "Invalid WRITE_MEMORY response";
		logger->error("{}", lastErrorMsg);
		return false;
	}

	uint8_t status = response.payload[0];

	// Check status
	if (status != static_cast<uint8_t>(UartProtocol::ErrorCode::SUCCESS))
	{
		lastErrorMsg = "WRITE_MEMORY failed: " + UartProtocol::getErrorName(static_cast<UartProtocol::ErrorCode>(status));
		logger->error("{}", lastErrorMsg);
		return false;
	}

	return true;
}

std::string UartDebugProbe::getLastErrorMsg() const
{
	return lastErrorMsg;
}

uint8_t UartDebugProbe::getNextSequence()
{
	return sequenceNumber++;
}

bool UartDebugProbe::sendAndReceive(const std::vector<uint8_t>& requestPacket, UartProtocol::Packet& responsePacket, UartProtocol::CommandCode expectedCmd, int timeoutMs)
{
	// Send packet
	size_t written = serialPort->write(requestPacket.data(), requestPacket.size());
	if (written != requestPacket.size())
	{
		lastErrorMsg = "Failed to write packet";
		logger->error("{}", lastErrorMsg);
		return false;
	}

	// Wait for response
	if (!receivePacket(responsePacket, timeoutMs))
	{
		// Error message already set by receivePacket()
		return false;
	}

	// Verify response command
	if (responsePacket.cmd != expectedCmd)
	{
		lastErrorMsg = "Unexpected response command: " + UartProtocol::getCommandName(responsePacket.cmd);
		logger->error("{}", lastErrorMsg);
		return false;
	}

	return true;
}

bool UartDebugProbe::receivePacket(UartProtocol::Packet& packet, int timeoutMs)
{
	std::vector<uint8_t> buffer;
	auto startTime = std::chrono::steady_clock::now();

	// Read until we have a complete packet or timeout
	while (true)
	{
		// Check timeout
		auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - startTime);
		if (elapsed.count() >= timeoutMs)
		{
			lastErrorMsg = "Timeout waiting for response";
			logger->error("{}", lastErrorMsg);
			return false;
		}

		// Read available data
		uint8_t byte;
		size_t bytesRead = serialPort->read(&byte, 1, RESPONSE_POLL_INTERVAL_MS);
		if (bytesRead == 1)
		{
			buffer.push_back(byte);

			// Check if we have enough data for header
			if (buffer.size() >= UartProtocol::HEADER_SIZE)
			{
				// Check start byte
				if (buffer[0] != UartProtocol::START_BYTE)
				{
					// Invalid start byte, discard first byte and continue
					buffer.erase(buffer.begin());
					continue;
				}

				// Extract length (little-endian)
				uint16_t length = static_cast<uint16_t>(buffer[2]) | (static_cast<uint16_t>(buffer[3]) << 8);

				// Check if we have complete packet
				size_t expectedSize = UartProtocol::HEADER_SIZE + length + UartProtocol::CRC_SIZE;
				if (buffer.size() >= expectedSize)
				{
					// Try to deserialize
					std::vector<uint8_t> packetData(buffer.begin(), buffer.begin() + expectedSize);
					if (UartProtocol::deserialize(packetData, packet))
					{
						return true;
					}
					else
					{
						lastErrorMsg = "Failed to deserialize packet (CRC error)";
						logger->error("{}", lastErrorMsg);
						return false;
					}
				}
			}
		}
	}

	return false;
}

bool UartDebugProbe::sendGetInfo()
{
	logger->info("Sending GET_INFO command");

	// Create GET_INFO packet
	auto packet = UartProtocol::createGetInfoPacket(getNextSequence());

	// Send and wait for response
	UartProtocol::Packet response;
	if (!sendAndReceive(packet, response, UartProtocol::CommandCode::GET_INFO_RESP))
	{
		lastErrorMsg = "GET_INFO failed";
		return false;
	}

	// Parse response payload
	UartProtocol::GetInfoRespPayload respPayload;
	if (!UartProtocol::parseGetInfoResp(response.payload, respPayload))
	{
		lastErrorMsg = "Failed to parse GET_INFO response";
		logger->error("{}", lastErrorMsg);
		return false;
	}

	// Check status
	if (respPayload.status != static_cast<uint8_t>(UartProtocol::ErrorCode::SUCCESS))
	{
		lastErrorMsg = "GET_INFO failed: " + UartProtocol::getErrorName(static_cast<UartProtocol::ErrorCode>(respPayload.status));
		logger->error("{}", lastErrorMsg);
		return false;
	}

	// Store device information
	targetName = respPayload.deviceName;
	protocolVersion = respPayload.protocolVersion;
	capabilities = respPayload.capabilities;

	logger->info("Target device: {}", targetName);
	logger->info("Protocol version: {}.{}", (protocolVersion >> 8) & 0xFF, protocolVersion & 0xFF);
	logger->info("Capabilities: 0x{:08X}", capabilities);

	return true;
}
