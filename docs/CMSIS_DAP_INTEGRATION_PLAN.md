# CMSIS-DAP Integration Plan for MCUViewer

## Executive Summary

This document outlines the detailed plan for integrating CMSIS-DAP probe support into MCUViewer. CMSIS-DAP is an ARM-standardized debug protocol that enables support for a wide range of low-cost debug probes without proprietary libraries.

**Target Hardware**: Raspberry Pi Pico/Pico 2 (RP2040 Cortex-M0+ or RP2350 Cortex-M33 with DapperMime firmware), DAPLink probes, and other CMSIS-DAP v1/v2 devices

**Estimated Effort**: 3-4 weeks full-time development + testing

---

## 1. Background & Motivation

### 1.1 Current Architecture
MCUViewer currently supports two debug probe types:
- **STLink** - Uses libstlink (third_party/stlink/)
- **JLink** - Uses SEGGER JLink SDK (third_party/jlink/)

Both implement standardized interfaces:
- `IDebugProbe` - Memory read/write operations for variable viewing
- `ITraceProbe` - SWO trace capture for trace viewing

### 1.2 Why CMSIS-DAP?

**Advantages**:
- **Open Standard** - No licensing fees or proprietary SDKs
- **Wide Hardware Support** - Works with cheap probes ($4-15)
- **Cross-Platform** - USB HID-based, no driver installation needed
- **Better Performance** - CMSIS-DAP v2 achieves ~10 MB/s over USB bulk transfers
- **Community Support** - Raspberry Pi Pico/Pico 2, DAPLink, and many open-source implementations

**Use Cases**:
- Users with Raspberry Pi Pico or Pico 2 (can be used as debug probe)
- Users with DAPLink probes
- Users wanting to avoid proprietary tool dependencies
- Educational institutions and hobbyists

---

## 2. Technical Architecture

### 2.1 CMSIS-DAP Protocol Overview

CMSIS-DAP defines a command-response protocol over USB:

**Protocol Versions**:
- **v1** - USB HID (64-byte packets, ~1 MB/s)
- **v2** - USB Bulk (512-byte packets, ~10 MB/s) - **Target for MCUViewer**

**Key Commands** (relevant to MCUViewer):
```
DAP_Info              - Get device info (vendor, product, capabilities)
DAP_Connect           - Connect to target (SWD/JTAG mode)
DAP_Disconnect        - Disconnect from target
DAP_Transfer          - Read/write single/multiple AP/DP registers
DAP_TransferBlock     - Bulk read/write operations
DAP_SWJ_Clock         - Set clock frequency
DAP_SWJ_Sequence      - Send SWD/JTAG sequence
DAP_SWD_Configure     - Configure SWD protocol
DAP_ResetTarget       - Reset target device
```

**SWD Memory Access Flow**:
1. Connect via DAP_Connect
2. Configure SWD via DAP_SWD_Configure
3. Read memory:
   - Write address to TAR (Transfer Address Register)
   - Read from DRW (Data Read/Write Register)
4. Repeat for each variable address

### 2.2 USB Communication

**Library Choice**: `libusb` (already used by MCUViewer for STLink)

**Device Detection**:
- Enumerate USB HID devices
- Check for CMSIS-DAP interface descriptor
- Filter by VID/PID or interface string "CMSIS-DAP"

**CMSIS-DAP v1** (HID):
- Endpoint: HID Report
- Packet size: 64 bytes
- Use `libusb_interrupt_transfer()`

**CMSIS-DAP v2** (Bulk):
- Endpoint: Bulk OUT/IN
- Packet size: 512 bytes
- Use `libusb_bulk_transfer()`

### 2.3 Class Structure

New classes to implement:

```cpp
// src/MemoryReader/CmsisDapDebugProbe.hpp
class CmsisDapDebugProbe : public IDebugProbe
{
public:
    CmsisDapDebugProbe(spdlog::logger* logger);
    ~CmsisDapDebugProbe() override;

    // IDebugProbe interface
    bool startAcqusition(const DebugProbeSettings& settings,
                        std::vector<std::pair<uint32_t, uint8_t>>& addressSizeVector,
                        uint32_t samplingFreqency) override;
    bool stopAcqusition() override;
    bool isValid() const override;
    std::string getTargetName() override;
    std::optional<varEntryType> readSingleEntry() override;
    bool readMemory(uint32_t address, uint8_t* buf, uint32_t size) override;
    bool writeMemory(uint32_t address, uint8_t* buf, uint32_t size) override;
    std::string getLastErrorMsg() const override;
    std::vector<std::string> getConnectedDevices() override;

private:
    // USB communication
    libusb_context* usbContext = nullptr;
    libusb_device_handle* deviceHandle = nullptr;
    uint8_t epOut = 0;  // Bulk OUT endpoint
    uint8_t epIn = 0;   // Bulk IN endpoint
    bool isDapV2 = false;  // true = bulk, false = HID

    // DAP protocol helpers
    bool sendCommand(const uint8_t* cmd, size_t cmdLen, uint8_t* resp, size_t respLen);
    bool dapConnect(uint8_t mode);  // 1=SWD, 2=JTAG
    bool dapDisconnect();
    bool dapSetClock(uint32_t freqHz);
    bool dapTransfer(uint8_t apDp, uint8_t reg, uint32_t* data, bool write);
    bool dapTransferBlock(uint32_t count, uint32_t* data, bool write);

    // SWD/CoreSight access
    bool swdReadAP(uint8_t apIndex, uint8_t reg, uint32_t* value);
    bool swdWriteAP(uint8_t apIndex, uint8_t reg, uint32_t value);
    bool swdReadDP(uint8_t reg, uint32_t* value);
    bool swdWriteDP(uint8_t reg, uint32_t value);

    // Memory access via MEM-AP
    bool memApRead32(uint32_t address, uint32_t* value);
    bool memApWrite32(uint32_t address, uint32_t value);
    bool memApReadBlock(uint32_t address, uint8_t* buffer, uint32_t size);

    spdlog::logger* logger;
};

// src/TraceReader/CmsisDapTraceProbe.hpp
class CmsisDapTraceProbe : public ITraceProbe
{
public:
    CmsisDapTraceProbe(spdlog::logger* logger);
    ~CmsisDapTraceProbe() override;

    // ITraceProbe interface
    bool startTrace(const TraceProbeSettings& settings,
                   uint32_t coreFrequency,
                   uint32_t tracePrescaler,
                   uint32_t activeChannelMask,
                   bool shouldReset) override;
    bool stopTrace() override;
    int32_t readTraceBuffer(uint8_t* buffer, uint32_t size) override;
    std::string getTargetName() override;
    std::vector<std::string> getConnectedDevices() override;

private:
    libusb_context* usbContext = nullptr;
    libusb_device_handle* deviceHandle = nullptr;
    uint8_t swoEndpoint = 0;

    // SWO configuration via CMSIS-DAP
    bool configureSWO(uint32_t frequency, uint32_t protocol);
    bool enableSWO();
    bool disableSWO();

    spdlog::logger* logger;
};
```

---

## 3. Implementation Phases

### Phase 1: USB Communication & DAP Protocol Core (Week 1)

**Tasks**:
1. Create skeleton classes `CmsisDapDebugProbe` and `CmsisDapTraceProbe`
2. Implement USB device enumeration
   - Search for CMSIS-DAP devices (HID or bulk interface)
   - Extract serial numbers for device selection
   - Detect CMSIS-DAP v1 vs v2
3. Implement basic DAP commands:
   - `DAP_Info` - Get device capabilities
   - `DAP_Connect` - Establish SWD connection
   - `DAP_Disconnect` - Close connection
   - `DAP_SWJ_Clock` - Set SWD clock frequency
4. Implement command send/receive with error handling
5. Add logging for all DAP transactions

**Deliverables**:
- `src/MemoryReader/CmsisDapDebugProbe.{hpp,cpp}` (partial)
- `src/MemoryReader/CmsisDapProtocol.{hpp,cpp}` (DAP command helpers)
- Successful device enumeration and connection test

**Testing**:
- Unit test: USB device enumeration
- Integration test: Connect/disconnect to real hardware

---

### Phase 2: SWD Protocol & Memory Access (Week 2)

**Tasks**:
1. Implement SWD register access:
   - `DAP_Transfer` for single AP/DP register reads/writes
   - Debug Port (DP) register access
   - Access Port (AP) register access
2. Implement MEM-AP operations:
   - Configure MEM-AP (CSW, TAR registers)
   - 32-bit aligned memory reads via DRW register
   - Handle unaligned reads (byte/halfword)
   - Implement `readMemory()` and `writeMemory()`
3. Optimize batch reads:
   - Use `DAP_TransferBlock` for consecutive addresses
   - Implement auto-increment TAR for multi-variable reads
4. Add target identification:
   - Read IDCODE from DP
   - Detect Cortex-M core type

**Deliverables**:
- Fully functional `readMemory()` and `writeMemory()`
- Target device identification in `getTargetName()`
- `getConnectedDevices()` returning serial numbers

**Testing**:
- Unit test: Mock DAP_Transfer responses
- Integration test: Read known memory locations from Black Pill
- Integration test: Read variable values from running firmware

---

### Phase 3: GUI Integration & Configuration (Week 2-3)

**Tasks**:
1. Update GUI probe selection:
   - Add "CMSIS-DAP" to probe dropdown in `GuiAcqusition.cpp`
   - Show connected CMSIS-DAP devices
   - Allow serial number selection
2. Update `ViewerDataHandler`:
   - Factory pattern to create `CmsisDapDebugProbe` instances
   - Handle probe lifecycle (start/stop acquisition)
3. Update configuration files:
   - Add CMSIS-DAP settings to config handler
   - Persist probe selection and parameters
4. Add UI for CMSIS-DAP specific settings:
   - SWD clock frequency slider (100 kHz - 10 MHz)
   - Connection options (connect under reset, etc.)

**Files Modified**:
- `src/Gui/GuiAcqusition.cpp` - Add CMSIS-DAP UI
- `src/DataHandler/ViewerDataHandler.cpp` - Add probe factory
- `src/ConfigHandler/ConfigHandler.cpp` - Add CMSIS-DAP config

**Testing**:
- Manual test: Select CMSIS-DAP probe in GUI
- Integration test: Acquire variables from target
- Verify configuration persistence

---

### Phase 4: SWO Trace Support (Week 3)

**Tasks**:
1. Research CMSIS-DAP SWO commands:
   - `DAP_SWO_Transport` - Configure SWO endpoint
   - `DAP_SWO_Mode` - Set SWO mode (UART/Manchester)
   - `DAP_SWO_Baudrate` - Set SWO frequency
   - `DAP_SWO_Control` - Start/stop SWO capture
   - `DAP_SWO_Data` - Read SWO data
2. Implement `CmsisDapTraceProbe`:
   - Device enumeration (same as debug probe)
   - Configure SWO UART mode
   - Start/stop trace capture
   - Read trace buffer periodically
3. Integrate with TraceReader:
   - Feed SWO data to `TraceReader` for ITM decoding
4. Update GUI for CMSIS-DAP trace:
   - Add CMSIS-DAP to trace probe dropdown
   - Show trace status and buffer utilization

**Deliverables**:
- `src/TraceReader/CmsisDapTraceProbe.{hpp,cpp}` (complete)
- Working SWO trace capture in GUI

**Testing**:
- Integration test: Capture ITM_SendChar() output
- Integration test: Multi-channel ITM trace
- Verify trace synchronization with variable viewer

**Note**: Not all CMSIS-DAP probes support SWO. Check capabilities via `DAP_Info`.

---

### Phase 5: Build System & Dependencies (Week 3)

**Tasks**:
1. Update `CMakeLists.txt`:
   - Add `CmsisDapDebugProbe.cpp` and `CmsisDapTraceProbe.cpp` to `PROJECT_SOURCES`
   - Add compile definition `CMSIS_DAP_AVAILABLE`
   - No new dependencies needed (libusb already used)
2. Platform-specific handling:
   - Linux: No changes (libusb already linked)
   - Windows: Ensure libusb-1.0.dll is bundled
   - macOS: Test with Homebrew libusb
3. Update build documentation:
   - Add CMSIS-DAP to feature list in README.md
   - Update CLAUDE.md with CMSIS-DAP architecture notes

**Files Modified**:
- `CMakeLists.txt` - Add new source files
- `README.md` - Update supported probes list
- `CLAUDE.md` - Add CMSIS-DAP module documentation

---

### Phase 6: Testing & Validation (Week 4)

Comprehensive testing strategy (detailed in Section 4).

---

## 4. Testing Strategy

### 4.1 Unit Tests

**Framework**: Google Test (existing test infrastructure)

**Test Suite**: `test/CmsisDapTest.cpp`

**Test Cases**:

```cpp
// Protocol layer tests
TEST(CmsisDapProtocol, DAP_Info_Parsing) {
    // Mock DAP_Info response, verify parsing
}

TEST(CmsisDapProtocol, DAP_Connect_Success) {
    // Mock successful SWD connection
}

TEST(CmsisDapProtocol, DAP_Transfer_SingleRead) {
    // Mock DP/AP register read
}

TEST(CmsisDapProtocol, DAP_TransferBlock_MultiRead) {
    // Mock block transfer
}

// Memory access tests
TEST(CmsisDapMemory, ReadMemory_Aligned) {
    // Test 32-bit aligned read
}

TEST(CmsisDapMemory, ReadMemory_Unaligned_Byte) {
    // Test 8-bit unaligned read
}

TEST(CmsisDapMemory, ReadMemory_Unaligned_Halfword) {
    // Test 16-bit unaligned read
}

TEST(CmsisDapMemory, WriteMemory_Success) {
    // Test memory write
}

// Device enumeration tests
TEST(CmsisDapDevice, EnumerateDevices) {
    // Test USB device listing (requires mock or real HW)
}

TEST(CmsisDapDevice, DetectVersion) {
    // Verify v1 vs v2 detection
}

// Error handling tests
TEST(CmsisDapError, HandleConnectionFailure) {
    // Test error message propagation
}

TEST(CmsisDapError, HandleTimeoutRecovery) {
    // Test USB timeout handling
}
```

**Mock Strategy**:
- Create `MockUsbDevice` class to simulate USB responses
- Pre-record DAP command/response sequences
- Test error conditions (timeouts, NAKs, protocol errors)

---

### 4.2 Integration Tests

**Hardware Required**:
- Raspberry Pi Pico or Pico 2 (RP2040 Cortex-M0+ or RP2350 Cortex-M33) with DapperMime firmware
- STM32F4 Black Pill target board
- USB cables and SWD wiring

**Test Suite**: `test/CmsisDapIntegrationTest.cpp`

**Test Cases**:

#### 4.2.1 Basic Connectivity
```cpp
TEST(CmsisDapIntegration, ConnectDisconnect) {
    CmsisDapDebugProbe probe;
    ASSERT_TRUE(probe.startAcqusition(...));
    ASSERT_TRUE(probe.isValid());
    ASSERT_TRUE(probe.stopAcqusition());
}

TEST(CmsisDapIntegration, EnumerateRealDevices) {
    CmsisDapDebugProbe probe;
    auto devices = probe.getConnectedDevices();
    ASSERT_FALSE(devices.empty());
}

TEST(CmsisDapIntegration, GetTargetName) {
    CmsisDapDebugProbe probe;
    probe.startAcqusition(...);
    std::string target = probe.getTargetName();
    ASSERT_TRUE(target.find("Cortex-M") != std::string::npos);
}
```

#### 4.2.2 Memory Operations
```cpp
TEST(CmsisDapIntegration, ReadFlashMemory) {
    // Read known value from flash (e.g., vector table)
    CmsisDapDebugProbe probe;
    probe.startAcqusition(...);
    uint32_t stackPointer;
    ASSERT_TRUE(probe.readMemory(0x08000000, (uint8_t*)&stackPointer, 4));
    ASSERT_NE(stackPointer, 0);
}

TEST(CmsisDapIntegration, ReadSRAMMemory) {
    // Read/write test pattern to SRAM
    CmsisDapDebugProbe probe;
    probe.startAcqusition(...);
    uint32_t testPattern = 0xDEADBEEF;
    uint32_t readback = 0;
    ASSERT_TRUE(probe.writeMemory(0x20000000, (uint8_t*)&testPattern, 4));
    ASSERT_TRUE(probe.readMemory(0x20000000, (uint8_t*)&readback, 4));
    ASSERT_EQ(readback, testPattern);
}

TEST(CmsisDapIntegration, ReadRunningVariable) {
    // Read variable from test firmware (counter, etc.)
    CmsisDapDebugProbe probe;
    probe.startAcqusition(...);
    uint32_t counter1, counter2;
    probe.readMemory(TEST_COUNTER_ADDR, (uint8_t*)&counter1, 4);
    std::this_thread::sleep_for(100ms);
    probe.readMemory(TEST_COUNTER_ADDR, (uint8_t*)&counter2, 4);
    ASSERT_GT(counter2, counter1);  // Counter should increment
}
```

#### 4.2.3 Multi-Variable Acquisition
```cpp
TEST(CmsisDapIntegration, AcquireMultipleVariables) {
    // Simulate MCUViewer use case: read multiple variables
    CmsisDapDebugProbe probe;
    std::vector<std::pair<uint32_t, uint8_t>> vars = {
        {0x20000000, 4},  // int32
        {0x20000004, 2},  // int16
        {0x20000006, 1},  // uint8
        {0x20000008, 4},  // float
    };
    probe.startAcqusition(..., vars, 1000);  // 1 kHz sampling

    // Read multiple samples
    for (int i = 0; i < 100; i++) {
        uint8_t buf[4];
        for (auto& var : vars) {
            ASSERT_TRUE(probe.readMemory(var.first, buf, var.second));
        }
        std::this_thread::sleep_for(1ms);
    }
}
```

#### 4.2.4 SWO Trace Tests
```cpp
TEST(CmsisDapIntegration, SWO_BasicCapture) {
    CmsisDapTraceProbe probe;
    ASSERT_TRUE(probe.startTrace(..., 64000000, 1, 0xFF, false));

    uint8_t buffer[1024];
    int bytesRead = probe.readTraceBuffer(buffer, 1024);
    ASSERT_GT(bytesRead, 0);  // Should receive some SWO data

    probe.stopTrace();
}

TEST(CmsisDapIntegration, SWO_ITM_Channel0) {
    // Target firmware sends known pattern on ITM channel 0
    CmsisDapTraceProbe probe;
    probe.startTrace(...);

    // Read and decode ITM packets
    uint8_t buffer[1024];
    int bytesRead = probe.readTraceBuffer(buffer, 1024);

    // Verify ITM packet structure (check for 0x01 sync, channel ID, etc.)
    ASSERT_TRUE(containsITMPackets(buffer, bytesRead));
}
```

---

### 4.3 System Tests

**Manual Test Plan** (perform with real hardware):

#### Test 1: Full Variable Viewer Workflow
1. Connect Raspberry Pi Pico/Pico 2 (DapperMime) to Black Pill
2. Flash test firmware with known variables to Black Pill
3. Launch MCUViewer
4. Select CMSIS-DAP probe from dropdown
5. Load ELF file
6. Add variables to plot
7. Start acquisition
8. Verify real-time plot updates
9. Verify variable values are correct
10. Stop acquisition
11. Export data to CSV, verify contents

**Expected Result**: Variables update smoothly, values match expected range

#### Test 2: High-Speed Acquisition
1. Configure acquisition at maximum speed (10 kHz+)
2. Monitor CPU usage and memory
3. Verify no dropped samples or glitches
4. Compare performance with STLink probe

**Expected Result**: Stable high-speed acquisition with <50% CPU usage

#### Test 3: SWO Trace Viewer
1. Flash firmware with ITM_SendChar() calls
2. Select CMSIS-DAP trace probe
3. Configure SWO frequency (2 MHz)
4. Start trace capture
5. Verify ITM messages appear in trace viewer
6. Test multi-channel ITM trace

**Expected Result**: All ITM messages captured correctly

#### Test 4: Probe Hotplug
1. Start MCUViewer
2. Connect CMSIS-DAP probe (should appear in list)
3. Disconnect probe during acquisition
4. Verify graceful error handling
5. Reconnect probe
6. Verify it reappears in list

**Expected Result**: No crash, clear error messages

#### Test 5: Multi-Platform Testing
- Test on Linux (Ubuntu 22.04)
- Test on Windows 10/11
- Test on macOS (Intel and Apple Silicon)

**Expected Result**: Consistent behavior across platforms

---

### 4.4 Performance Benchmarks

**Metrics to Measure**:
1. **Memory read latency**
   - Single 32-bit read time
   - Batch read throughput (KB/s)
2. **Sampling frequency**
   - Maximum reliable sampling rate for 1 variable
   - Maximum reliable sampling rate for 10 variables
3. **SWO throughput**
   - Maximum SWO bandwidth before data loss
4. **CPU overhead**
   - % CPU usage during acquisition

**Target Performance** (CMSIS-DAP v2):
- Single read latency: <5 ms
- Batch throughput: >500 KB/s
- Max sampling rate (1 var): 5 kHz
- Max sampling rate (10 vars): 1 kHz
- SWO bandwidth: 2 Mbps without loss
- CPU overhead: <30% during 1 kHz acquisition

**Comparison**:
- Run same benchmarks with STLink probe
- Document performance delta

---

### 4.5 Error Condition Tests

**Scenarios to Test**:
1. Target not connected (SWD wires disconnected)
2. Target in low-power mode
3. USB cable disconnected during operation
4. Invalid memory address access
5. SWD clock too fast for target
6. Probe firmware incompatibility
7. Multiple CMSIS-DAP probes connected
8. Concurrent access from other tools (OpenOCD, pyOCD)

**Expected Behavior**:
- Clear error messages in GUI
- Graceful recovery when possible
- No crashes or hangs
- Proper resource cleanup

---

### 4.6 Regression Tests

**Ensure Existing Functionality Works**:
1. STLink probe still works (variable viewer + trace)
2. JLink probe still works (variable viewer + trace)
3. ELF file parsing unchanged
4. Plot rendering unchanged
5. Configuration save/load unchanged

**Test Matrix**:
| Probe Type | Variable Viewer | SWO Trace | ELF Loading | Config Save |
|------------|----------------|-----------|-------------|-------------|
| STLink     | ✓              | ✓         | ✓           | ✓           |
| JLink      | ✓              | ✓         | ✓           | ✓           |
| CMSIS-DAP  | ✓              | ✓         | ✓           | ✓           |

---

## 5. Test Firmware

### 5.1 Test Firmware Requirements

Create minimal STM32 firmware for testing:

**Features**:
- Counter variable incrementing at 1 Hz
- Float variable with sine wave (0.5 Hz)
- Array of 10 integers
- Struct with mixed types
- ITM_SendChar() output on channel 0
- ITM_SendValue() output on channel 1

**File**: `test/testFirmware/cmsis_dap_test/main.c`

```c
#include "stm32f4xx.h"
#include <math.h>

// Test variables (place at known addresses via linker script)
volatile uint32_t test_counter = 0;
volatile float test_sine = 0.0f;
volatile int16_t test_array[10] = {0};
volatile struct {
    uint8_t flag;
    uint16_t value;
    float temperature;
} test_struct = {0};

void SysTick_Handler(void) {
    static uint32_t tick = 0;
    tick++;

    // Update test variables
    test_counter = tick;
    test_sine = sinf(tick * 0.01f);

    for (int i = 0; i < 10; i++) {
        test_array[i] = (int16_t)(tick + i);
    }

    test_struct.flag = tick % 2;
    test_struct.value = tick % 1000;
    test_struct.temperature = 25.0f + sinf(tick * 0.1f) * 5.0f;

    // Send ITM trace
    ITM_SendChar('A' + (tick % 26));
}

int main(void) {
    SystemCoreClockUpdate();
    SysTick_Config(SystemCoreClock / 1000);  // 1 ms tick

    while (1) {
        __WFI();
    }
}
```

**Build System**:
- Add CMakeLists.txt for test firmware
- Output ELF with debug symbols
- Document variable addresses in `test/testFirmware/cmsis_dap_test/README.md`

---

## 6. Documentation Updates

### 6.1 User Documentation

**README.md Updates**:
```markdown
## Supported Debug Probes
- STLink v2/v3
- JLink (all versions)
- **CMSIS-DAP v1/v2** (NEW!)
  - Raspberry Pi Pico/Pico 2 (RP2040 Cortex-M0+ or RP2350 Cortex-M33 with DapperMime/Picoprobe firmware)
  - DAPLink probes
  - Generic CMSIS-DAP adapters

## Hardware Setup - CMSIS-DAP
1. Flash your Raspberry Pi Pico or Pico 2 with DapperMime firmware:
   ```
   https://github.com/majbthrd/DapperMime
   ```
2. Connect Pico to target:
   - GP2 → SWCLK
   - GP3 → SWDIO
   - GP4 → SWO (optional, for trace)
   - GND → GND
3. Connect Pico to PC via USB
4. Launch MCUViewer, select "CMSIS-DAP" from probe list
```

### 6.2 Developer Documentation

**CLAUDE.md Updates**:
```markdown
### Core Modules (updated)
- **src/MemoryReader/** - Debug probe interfaces
  - `StlinkDebugProbe.cpp` - STLink probe implementation
  - `JlinkDebugProbe.cpp` - JLink probe implementation
  - **`CmsisDapDebugProbe.cpp` - CMSIS-DAP probe implementation (NEW)**
  - **`CmsisDapProtocol.cpp` - CMSIS-DAP protocol layer (NEW)**
- **src/TraceReader/** - SWO trace data handling
  - `StlinkTraceProbe.cpp` - STLink trace implementation
  - `JlinkTraceProbe.cpp` - JLink trace implementation
  - **`CmsisDapTraceProbe.cpp` - CMSIS-DAP trace implementation (NEW)**

### CMSIS-DAP Architecture
- Protocol: ARM CMSIS-DAP v1 (HID) and v2 (bulk USB)
- USB library: libusb (already used for STLink)
- SWD access: Via DAP_Transfer commands to DP/AP registers
- Memory read: MEM-AP (CSW/TAR/DRW registers)
- SWO trace: DAP_SWO_* commands (if probe supports)
```

### 6.3 Code Comments

All new code should have:
- Doxygen-style function comments
- Protocol command references (e.g., "// See CMSIS-DAP spec section 5.5.4")
- Explanation of SWD/CoreSight register operations

---

## 7. Known Limitations & Future Work

### 7.1 Limitations

1. **No HSS mode support** - CMSIS-DAP does not have equivalent to JLink's High-Speed Sampling
2. **SWO probe-dependent** - Not all CMSIS-DAP probes expose SWO endpoint
3. **Slower than native probes** - CMSIS-DAP has higher latency than STLink/JLink proprietary protocols
4. **No JTAG support initially** - Phase 1 focuses on SWD only

### 7.2 Future Enhancements

1. **JTAG support** - Add DAP_JTAG_* commands for JTAG targets
2. **Target reset control** - Expose reset options in GUI
3. **Batch optimization** - Implement pipelined transfers for lower latency
4. **Probe firmware detection** - Detect and warn about outdated DapperMime/DAPLink versions
5. **Custom VID/PID filter** - Allow users to specify custom CMSIS-DAP VID/PID pairs
6. **Multi-target support** - Support probes with multiple SWD ports

---

## 8. Risk Assessment

| Risk | Impact | Probability | Mitigation |
|------|--------|-------------|------------|
| CMSIS-DAP v1/v2 detection fails | High | Low | Comprehensive USB descriptor parsing |
| SWD timing issues at high speeds | Medium | Medium | Conservative default clock, user-adjustable |
| Incompatible probe firmware | Medium | Medium | Version detection + user warnings |
| USB permission issues (Linux) | Medium | High | Document udev rules, include in installer |
| Performance lower than expected | Low | Medium | Focus on v2 bulk mode, batch transfers |
| SWO not working on all probes | Low | High | Check capabilities via DAP_Info, disable if unsupported |

---

## 9. Success Criteria

The CMSIS-DAP integration is considered successful when:

1. ✓ MCUViewer can enumerate CMSIS-DAP v1 and v2 devices
2. ✓ Variable viewer works with at least 10 variables at 1 kHz sampling
3. ✓ Memory read/write operations are stable and accurate
4. ✓ SWO trace capture works on supported probes (Pico DapperMime)
5. ✓ GUI integration is seamless (no UI regressions)
6. ✓ All unit and integration tests pass
7. ✓ Tested on Linux, Windows, and macOS
8. ✓ Documentation updated (README, CLAUDE.md)
9. ✓ No regressions in STLink/JLink functionality
10. ✓ Code passes clang-format check (Allman style)

---

## 10. Implementation Checklist

### Phase 1: USB & DAP Core
- [ ] Create `CmsisDapDebugProbe.hpp/.cpp` skeleton
- [ ] Implement USB device enumeration (libusb)
- [ ] Detect CMSIS-DAP v1 vs v2
- [ ] Implement `sendCommand()` (HID and bulk modes)
- [ ] Implement DAP_Info
- [ ] Implement DAP_Connect/DAP_Disconnect
- [ ] Implement DAP_SWJ_Clock
- [ ] Add unit tests for protocol layer
- [ ] Test connection with real hardware

### Phase 2: SWD & Memory
- [ ] Implement DAP_Transfer (DP/AP register access)
- [ ] Implement SWD DP read/write helpers
- [ ] Implement SWD AP read/write helpers
- [ ] Implement MEM-AP configuration
- [ ] Implement `memApRead32()`
- [ ] Implement `memApReadBlock()`
- [ ] Implement `readMemory()` with unaligned support
- [ ] Implement `writeMemory()`
- [ ] Implement target identification (IDCODE)
- [ ] Add unit tests for memory operations
- [ ] Test with Black Pill hardware

### Phase 3: GUI Integration
- [ ] Add CMSIS-DAP to probe dropdown
- [ ] Update `ViewerDataHandler` factory
- [ ] Update `ConfigHandler` for CMSIS-DAP settings
- [ ] Add SWD clock frequency slider
- [ ] Test end-to-end variable acquisition
- [ ] Verify configuration persistence

### Phase 4: SWO Trace
- [ ] Create `CmsisDapTraceProbe.hpp/.cpp`
- [ ] Implement DAP_SWO_Transport
- [ ] Implement DAP_SWO_Mode
- [ ] Implement DAP_SWO_Baudrate
- [ ] Implement DAP_SWO_Control
- [ ] Implement `readTraceBuffer()`
- [ ] Integrate with TraceReader
- [ ] Add SWO tests
- [ ] Test with ITM output

### Phase 5: Build System
- [ ] Update CMakeLists.txt (add new sources)
- [ ] Add CMSIS_DAP_AVAILABLE compile definition
- [ ] Test Linux build
- [ ] Test Windows build
- [ ] Test macOS build (Intel + ARM64)
- [ ] Update README.md
- [ ] Update CLAUDE.md

### Phase 6: Testing
- [ ] Write unit tests (protocol, memory, device enum)
- [ ] Write integration tests (connectivity, memory ops, SWO)
- [ ] Create test firmware for Black Pill
- [ ] Perform manual system tests
- [ ] Run performance benchmarks
- [ ] Test error conditions
- [ ] Regression test STLink/JLink
- [ ] Multi-platform testing

### Phase 7: Documentation & Release
- [ ] Update README with CMSIS-DAP setup instructions
- [ ] Update CLAUDE.md with architecture notes
- [ ] Add inline code documentation (Doxygen)
- [ ] Create user guide for Pico setup
- [ ] Test with fresh install (no dev dependencies)
- [ ] Create release notes

---

## 11. Appendix

### 11.1 Reference Materials

**CMSIS-DAP Specification**:
- https://arm-software.github.io/CMSIS_5/DAP/html/index.html

**DapperMime Firmware** (Raspberry Pi Pico/Pico 2):
- https://github.com/majbthrd/DapperMime
- Supports both RP2040 (Cortex-M0+) and RP2350 (Cortex-M33)

**Picoprobe Firmware** (Raspberry Pi Pico/Pico 2):
- https://github.com/raspberrypi/picoprobe
- Official Raspberry Pi firmware, supports both models

**DAPLink Firmware**:
- https://github.com/ARMmbed/DAPLink

**OpenOCD CMSIS-DAP Driver** (reference implementation):
- https://github.com/openocd-org/openocd/blob/master/src/jtag/drivers/cmsis_dap.c

**PyOCD CMSIS-DAP Implementation** (Python reference):
- https://github.com/pyocd/pyOCD/tree/main/pyocd/probe/cmsis_dap_probe.py

**ARM CoreSight Architecture**:
- https://developer.arm.com/documentation/ihi0031/latest/

### 11.2 Hardware Pinouts

**Raspberry Pi Pico/Pico 2 (DapperMime default pinout)**:
```
GP2  → SWCLK
GP3  → SWDIO
GP4  → SWO (optional)
GP5  → nRESET (optional)
GND  → GND
VBUS → Target 3.3V (if powering target)
```

**Black Pill (STM32F411) SWD Header**:
```
Pin 1: VCC (3.3V)
Pin 2: SWCLK
Pin 3: GND
Pin 4: SWDIO
Pin 5: nRST
Pin 6: SWO
```

### 11.3 Useful Commands

**Test CMSIS-DAP probe with OpenOCD**:
```bash
openocd -f interface/cmsis-dap.cfg -f target/stm32f4x.cfg
```

**List USB devices with libusb**:
```bash
lsusb -v | grep -A 20 "CMSIS-DAP"
```

**Monitor SWO with OpenOCD**:
```bash
openocd -f interface/cmsis-dap.cfg -f target/stm32f4x.cfg \
  -c "tpiu config internal swo.log uart off 64000000" \
  -c "itm port 0 on"
```

---

## Revision History

| Version | Date | Author | Changes |
|---------|------|--------|---------|
| 1.0 | 2025-09-30 | Claude | Initial detailed implementation plan |

---

**End of Document**