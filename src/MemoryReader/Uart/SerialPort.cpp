/**
 * @file SerialPort.cpp
 * @brief Cross-platform serial port implementation
 */

#include "SerialPort.hpp"

#include <cstring>

// Platform-specific includes
#ifdef _WIN32
    #include <windows.h>
    #include <setupapi.h>
    #include <devguid.h>
    #include <regstr.h>
    #ifdef _MSC_VER
        #pragma comment(lib, "setupapi.lib")
    #endif
#else
    #include <fcntl.h>
    #include <termios.h>
    #include <unistd.h>
    #include <sys/ioctl.h>
    #include <sys/select.h>
    #include <dirent.h>
    #include <cerrno>
    #ifdef __linux__
        #include <linux/serial.h>
    #endif
#endif

namespace UartProtocol
{

// ===== Constructor / Destructor =====

SerialPort::SerialPort()
#ifdef _WIN32
    : m_handle(INVALID_HANDLE_VALUE), m_isOpen(false)
#else
    : m_handle(-1), m_isOpen(false)
#endif
{
}

SerialPort::~SerialPort()
{
    close();
}

// ===== Move semantics =====

SerialPort::SerialPort(SerialPort&& other) noexcept
    : m_handle(other.m_handle),
      m_portName(std::move(other.m_portName)),
      m_lastError(std::move(other.m_lastError)),
      m_isOpen(other.m_isOpen)
{
#ifdef _WIN32
    other.m_handle = INVALID_HANDLE_VALUE;
#else
    other.m_handle = -1;
#endif
    other.m_isOpen = false;
}

SerialPort& SerialPort::operator=(SerialPort&& other) noexcept
{
    if (this != &other)
    {
        close();
        m_handle = other.m_handle;
        m_portName = std::move(other.m_portName);
        m_lastError = std::move(other.m_lastError);
        m_isOpen = other.m_isOpen;

#ifdef _WIN32
        other.m_handle = INVALID_HANDLE_VALUE;
#else
        other.m_handle = -1;
#endif
        other.m_isOpen = false;
    }
    return *this;
}

// ===== Port enumeration =====

#ifdef _WIN32

std::vector<SerialPortInfo> SerialPort::listPorts()
{
    std::vector<SerialPortInfo> ports;

    // Enumerate COM ports using QueryDosDevice
    for (int i = 1; i <= 256; i++)
    {
        std::string portName = "COM" + std::to_string(i);
        std::string fullPortName = "\\\\.\\\\" + portName;
        HANDLE hPort = CreateFileA(fullPortName.c_str(),
                                   GENERIC_READ | GENERIC_WRITE,
                                   0,
                                   nullptr,
                                   OPEN_EXISTING,
                                   0,
                                   nullptr);

        if (hPort != INVALID_HANDLE_VALUE)
        {
            CloseHandle(hPort);
            SerialPortInfo info;
            info.port = portName;
            info.description = "Serial Port";
            ports.push_back(info);
        }
    }

    return ports;
}

#else    // Linux/macOS

std::vector<SerialPortInfo> SerialPort::listPorts()
{
    std::vector<SerialPortInfo> ports;

    DIR* dir = opendir("/dev");
    if (!dir)
        return ports;

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr)
    {
        std::string name = entry->d_name;

        // Filter for serial port device names
        bool isSerialPort = false;

#ifdef __linux__
        // Linux: ttyUSB*, ttyACM*, ttyS*
        if (name.find("ttyUSB") == 0 || name.find("ttyACM") == 0 || name.find("ttyS") == 0)
            isSerialPort = true;
#elif defined(__APPLE__)
        // macOS: cu.*, tty.* (excluding special devices)
        if ((name.find("cu.") == 0 || name.find("tty.") == 0) && name.find("Bluetooth") == std::string::npos)
            isSerialPort = true;
#endif

        if (isSerialPort)
        {
            SerialPortInfo info;
            info.port = "/dev/" + name;
            info.description = "Serial Port";
            ports.push_back(info);
        }
    }

    closedir(dir);
    return ports;
}

#endif

// ===== Open / Close =====

bool SerialPort::open(const std::string& portName,
                      BaudRate baudRate,
                      uint8_t dataBits,
                      Parity parity,
                      StopBits stopBits,
                      FlowControl flowControl)
{
    // Close previous connection
    close();

    m_portName = portName;

#ifdef _WIN32
    // Windows implementation
    std::string fullPath = "\\\\.\\" + portName;
    m_handle = CreateFileA(fullPath.c_str(),
                           GENERIC_READ | GENERIC_WRITE,
                           0,
                           nullptr,
                           OPEN_EXISTING,
                           0,
                           nullptr);

    if (m_handle == INVALID_HANDLE_VALUE)
    {
        setError("Failed to open port: " + portName);
        return false;
    }

    m_isOpen = true;

    if (!configure(baudRate, dataBits, parity, stopBits, flowControl))
    {
        close();
        return false;
    }

#else
    // Linux/macOS implementation
    m_handle = ::open(portName.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);

    if (m_handle < 0)
    {
        setError("Failed to open port: " + portName + " (" + std::strerror(errno) + ")");
        return false;
    }

    m_isOpen = true;

    if (!configure(baudRate, dataBits, parity, stopBits, flowControl))
    {
        close();
        return false;
    }

#endif

    return true;
}

void SerialPort::close()
{
    if (!m_isOpen)
        return;

#ifdef _WIN32
    if (m_handle != INVALID_HANDLE_VALUE)
    {
        CloseHandle(m_handle);
        m_handle = INVALID_HANDLE_VALUE;
    }
#else
    if (m_handle >= 0)
    {
        ::close(m_handle);
        m_handle = -1;
    }
#endif

    m_isOpen = false;
    m_portName.clear();
}

bool SerialPort::isOpen() const
{
    return m_isOpen;
}

// ===== Configuration =====

#ifdef _WIN32

bool SerialPort::configure(BaudRate baudRate, uint8_t dataBits, Parity parity, StopBits stopBits, FlowControl flowControl)
{
    DCB dcb = {};
    dcb.DCBlength = sizeof(DCB);

    if (!GetCommState(m_handle, &dcb))
    {
        setError("Failed to get comm state");
        return false;
    }

    // Baud rate
    dcb.BaudRate = static_cast<DWORD>(baudRate);

    // Data bits
    dcb.ByteSize = dataBits;

    // Parity
    switch (parity)
    {
        case Parity::NONE:
            dcb.Parity = NOPARITY;
            dcb.fParity = FALSE;
            break;
        case Parity::ODD:
            dcb.Parity = ODDPARITY;
            dcb.fParity = TRUE;
            break;
        case Parity::EVEN:
            dcb.Parity = EVENPARITY;
            dcb.fParity = TRUE;
            break;
        case Parity::MARK:
            dcb.Parity = MARKPARITY;
            dcb.fParity = TRUE;
            break;
        case Parity::SPACE:
            dcb.Parity = SPACEPARITY;
            dcb.fParity = TRUE;
            break;
    }

    // Stop bits
    switch (stopBits)
    {
        case StopBits::ONE:
            dcb.StopBits = ONESTOPBIT;
            break;
        case StopBits::ONE_POINT_FIVE:
            dcb.StopBits = ONE5STOPBITS;
            break;
        case StopBits::TWO:
            dcb.StopBits = TWOSTOPBITS;
            break;
    }

    // Flow control
    switch (flowControl)
    {
        case FlowControl::NONE:
            dcb.fOutxCtsFlow = FALSE;
            dcb.fRtsControl = RTS_CONTROL_DISABLE;
            dcb.fOutX = FALSE;
            dcb.fInX = FALSE;
            break;
        case FlowControl::HARDWARE:
            dcb.fOutxCtsFlow = TRUE;
            dcb.fRtsControl = RTS_CONTROL_HANDSHAKE;
            dcb.fOutX = FALSE;
            dcb.fInX = FALSE;
            break;
        case FlowControl::SOFTWARE:
            dcb.fOutxCtsFlow = FALSE;
            dcb.fRtsControl = RTS_CONTROL_DISABLE;
            dcb.fOutX = TRUE;
            dcb.fInX = TRUE;
            break;
    }

    if (!SetCommState(m_handle, &dcb))
    {
        setError("Failed to set comm state");
        return false;
    }

    // Set timeouts
    COMMTIMEOUTS timeouts = {};
    timeouts.ReadIntervalTimeout = MAXDWORD;
    timeouts.ReadTotalTimeoutMultiplier = 0;
    timeouts.ReadTotalTimeoutConstant = 0;
    timeouts.WriteTotalTimeoutMultiplier = 0;
    timeouts.WriteTotalTimeoutConstant = 0;

    if (!SetCommTimeouts(m_handle, &timeouts))
    {
        setError("Failed to set timeouts");
        return false;
    }

    return true;
}

#else    // Linux/macOS

bool SerialPort::configure(BaudRate baudRate, uint8_t dataBits, Parity parity, StopBits stopBits, FlowControl flowControl)
{
    struct termios tty;
    std::memset(&tty, 0, sizeof(tty));

    if (tcgetattr(m_handle, &tty) != 0)
    {
        setError("Failed to get terminal attributes: " + std::string(std::strerror(errno)));
        return false;
    }

    // Baud rate
    speed_t speed;
    switch (baudRate)
    {
        case BaudRate::BR_9600:
            speed = B9600;
            break;
        case BaudRate::BR_19200:
            speed = B19200;
            break;
        case BaudRate::BR_38400:
            speed = B38400;
            break;
        case BaudRate::BR_57600:
            speed = B57600;
            break;
        case BaudRate::BR_115200:
            speed = B115200;
            break;
        case BaudRate::BR_230400:
            speed = B230400;
            break;
#ifdef B460800
        case BaudRate::BR_460800:
            speed = B460800;
            break;
#endif
#ifdef B921600
        case BaudRate::BR_921600:
            speed = B921600;
            break;
#endif
        default:
            setError("Unsupported baud rate");
            return false;
    }

    cfsetispeed(&tty, speed);
    cfsetospeed(&tty, speed);

    // Control modes
    tty.c_cflag |= (CLOCAL | CREAD);    // Enable receiver, ignore modem control lines

    // Data bits
    tty.c_cflag &= ~CSIZE;
    switch (dataBits)
    {
        case 5:
            tty.c_cflag |= CS5;
            break;
        case 6:
            tty.c_cflag |= CS6;
            break;
        case 7:
            tty.c_cflag |= CS7;
            break;
        case 8:
            tty.c_cflag |= CS8;
            break;
        default:
            setError("Invalid data bits");
            return false;
    }

    // Parity
    switch (parity)
    {
        case Parity::NONE:
            tty.c_cflag &= ~PARENB;
            break;
        case Parity::ODD:
            tty.c_cflag |= PARENB;
            tty.c_cflag |= PARODD;
            break;
        case Parity::EVEN:
            tty.c_cflag |= PARENB;
            tty.c_cflag &= ~PARODD;
            break;
        default:
            setError("Unsupported parity mode");
            return false;
    }

    // Stop bits
    switch (stopBits)
    {
        case StopBits::ONE:
            tty.c_cflag &= ~CSTOPB;
            break;
        case StopBits::TWO:
            tty.c_cflag |= CSTOPB;
            break;
        default:
            setError("Unsupported stop bits");
            return false;
    }

    // Flow control
    switch (flowControl)
    {
        case FlowControl::NONE:
            tty.c_cflag &= ~CRTSCTS;
            tty.c_iflag &= ~(IXON | IXOFF | IXANY);
            break;
        case FlowControl::HARDWARE:
            tty.c_cflag |= CRTSCTS;
            tty.c_iflag &= ~(IXON | IXOFF | IXANY);
            break;
        case FlowControl::SOFTWARE:
            tty.c_cflag &= ~CRTSCTS;
            tty.c_iflag |= (IXON | IXOFF);
            break;
    }

    // Raw mode
    tty.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    tty.c_oflag &= ~OPOST;
    tty.c_iflag &= ~(INLCR | ICRNL);

    // Non-blocking reads
    tty.c_cc[VMIN] = 0;
    tty.c_cc[VTIME] = 0;

    if (tcsetattr(m_handle, TCSANOW, &tty) != 0)
    {
        setError("Failed to set terminal attributes: " + std::string(std::strerror(errno)));
        return false;
    }

    return true;
}

#endif

// ===== Read / Write =====

#ifdef _WIN32

size_t SerialPort::read(uint8_t* buffer, size_t size, int32_t timeoutMs)
{
    if (!m_isOpen)
        return 0;

    // Set timeout
    COMMTIMEOUTS timeouts = {};
    if (timeoutMs == 0)
    {
        // Non-blocking
        timeouts.ReadIntervalTimeout = MAXDWORD;
        timeouts.ReadTotalTimeoutMultiplier = 0;
        timeouts.ReadTotalTimeoutConstant = 0;
    }
    else if (timeoutMs < 0)
    {
        // Blocking
        timeouts.ReadIntervalTimeout = 0;
        timeouts.ReadTotalTimeoutMultiplier = 0;
        timeouts.ReadTotalTimeoutConstant = 0;
    }
    else
    {
        // Timeout
        timeouts.ReadIntervalTimeout = MAXDWORD;
        timeouts.ReadTotalTimeoutMultiplier = MAXDWORD;
        timeouts.ReadTotalTimeoutConstant = timeoutMs;
    }

    SetCommTimeouts(m_handle, &timeouts);

    DWORD bytesRead = 0;
    if (!ReadFile(m_handle, buffer, static_cast<DWORD>(size), &bytesRead, nullptr))
    {
        setError("Read failed");
        return 0;
    }

    return static_cast<size_t>(bytesRead);
}

size_t SerialPort::write(const uint8_t* data, size_t size)
{
    if (!m_isOpen)
        return 0;

    DWORD bytesWritten = 0;
    if (!WriteFile(m_handle, data, static_cast<DWORD>(size), &bytesWritten, nullptr))
    {
        setError("Write failed");
        return 0;
    }

    return static_cast<size_t>(bytesWritten);
}

#else    // Linux/macOS

size_t SerialPort::read(uint8_t* buffer, size_t size, int32_t timeoutMs)
{
    if (!m_isOpen)
        return 0;

    if (timeoutMs == 0)
    {
        // Non-blocking read
        ssize_t bytesRead = ::read(m_handle, buffer, size);
        return (bytesRead > 0) ? static_cast<size_t>(bytesRead) : 0;
    }
    else if (timeoutMs < 0)
    {
        // Blocking read
        ssize_t bytesRead = ::read(m_handle, buffer, size);
        return (bytesRead > 0) ? static_cast<size_t>(bytesRead) : 0;
    }
    else
    {
        // Read with timeout using select()
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(m_handle, &readfds);

        struct timeval timeout;
        timeout.tv_sec = timeoutMs / 1000;
        timeout.tv_usec = (timeoutMs % 1000) * 1000;

        int result = select(m_handle + 1, &readfds, nullptr, nullptr, &timeout);

        if (result < 0)
        {
            setError("Select failed: " + std::string(std::strerror(errno)));
            return 0;
        }
        else if (result == 0)
        {
            // Timeout
            return 0;
        }
        else
        {
            // Data available
            ssize_t bytesRead = ::read(m_handle, buffer, size);
            return (bytesRead > 0) ? static_cast<size_t>(bytesRead) : 0;
        }
    }
}

size_t SerialPort::write(const uint8_t* data, size_t size)
{
    if (!m_isOpen)
        return 0;

    ssize_t bytesWritten = ::write(m_handle, data, size);
    return (bytesWritten > 0) ? static_cast<size_t>(bytesWritten) : 0;
}

#endif

// ===== Buffer operations =====

bool SerialPort::flushInput()
{
    if (!m_isOpen)
        return false;

#ifdef _WIN32
    return PurgeComm(m_handle, PURGE_RXCLEAR) != 0;
#else
    return tcflush(m_handle, TCIFLUSH) == 0;
#endif
}

bool SerialPort::flushOutput()
{
    if (!m_isOpen)
        return false;

#ifdef _WIN32
    return FlushFileBuffers(m_handle) != 0;
#else
    return tcdrain(m_handle) == 0;
#endif
}

size_t SerialPort::available() const
{
    if (!m_isOpen)
        return 0;

#ifdef _WIN32
    COMSTAT comStat;
    DWORD errors;
    if (ClearCommError(m_handle, &errors, &comStat))
        return static_cast<size_t>(comStat.cbInQue);
    return 0;
#else
    int bytes = 0;
    if (ioctl(m_handle, FIONREAD, &bytes) == 0)
        return static_cast<size_t>(bytes);
    return 0;
#endif
}

// ===== Error handling =====

void SerialPort::setError(const std::string& error)
{
    m_lastError = error;
}

std::string SerialPort::getLastError() const
{
    return m_lastError;
}

std::string SerialPort::getPortName() const
{
    return m_portName;
}

}    // namespace UartProtocol
