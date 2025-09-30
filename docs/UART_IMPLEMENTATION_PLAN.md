# UART Debug Interface Implementation Plan V3
## With Universal Optional Recorder Module (STLink/JLink/UART)

## 1. Overview

This plan adds UART-based debug probe support **plus** a universal **optional oscilloscope-style recorder** that works with **all probe types** (STLink, JLink, UART). The solution includes:

- **Target firmware library** (UART only): Lightweight memory read/write service with optional on-target recorder
- **MCUViewer UART probe**: New `UartDebugProbe` class implementing `IDebugProbe`
- **UART simulator**: Virtual COM port for testing without hardware
- **🆕 Universal Recorder Module**: Optional trigger and recording system for **all probe types**
- **🆕 Non-blocking architecture**: Recorder operates independently - live variables always update

### Key Principle: **Live Variables Always Have Priority**

```
┌──────────────────────────────────────────────┐
│ MCUViewer                                    │
├──────────────────┬───────────────────────────┤
│ Primary Function │ Optional Recorder         │
│ (Never Blocked)  │ (Runs in Background)      │
│                  │                            │
│ • Variable table │ • Trigger setup           │
│ • Live plots     │ • Capture control         │
│ • Continuous     │ • Download capture        │
│   @100Hz         │   @background             │
└────────┬─────────┴────────┬──────────────────┘
         │                  │
         │                  │ Separate threads
    ┌────▼──────────────────▼──────────────────┐
    │ Debug Probe (STLink/JLink/UART)          │
    │  • Thread 1: Live sampling (priority)    │
    │  • Thread 2: Recorder (background)       │
    └──────────────────────────────────────────┘
```

---

## 2. Critical Architectural Decisions

### 2.1 Recorder Design Philosophy

**V2 Problem:** Recorder was tightly integrated, potentially blocking live updates.

**V3 Solution:**
1. **Separate recorder subsystem** - completely independent module
2. **Non-blocking downloads** - buffer retrieval happens in background
3. **Dual-thread architecture** - live updates never wait for recorder
4. **Works with all probes** - unified interface for STLink/JLink/UART
5. **Optional compilation** - can be disabled via CMake flag

### 2.2 Recorder Implementation Strategy by Probe Type

| Probe Type | Recording Strategy | Trigger Location | CPU Overhead | Max Rate |
|------------|-------------------|------------------|--------------|----------|
| **UART** | On-target circular buffer + DMA | Target firmware | <1% | 10 kHz |
| **JLink** | Host-side circular buffer | MCUViewer host | 2-5% | 50 kHz |
| **STLink** | Host-side circular buffer | MCUViewer host | 2-5% | 10 kHz |

**Key insight:** For STLink/JLink, we implement the circular buffer and trigger logic **on the host side** using existing `readMemory()` calls. No target firmware changes needed!

### 2.3 Thread Architecture

```cpp
// ViewerDataHandler - Enhanced with recorder support

class ViewerDataHandler : public DataHandlerBase {
private:
    // ===== THREAD 1: Live Variable Sampling (Existing, High Priority) =====
    void dataHandler() {
        while (!done) {
            if (viewerState == State::RUN) {
                // Read live variables - NEVER BLOCKS
                for (auto& [address, size] : liveVariableList) {
                    debugProbe->readMemory(address, buffer, size);
                }
                updateVariables();
                updatePlots();
            }
            sleep(1.0 / settings.sampleFrequencyHz);
        }
    }

    // ===== THREAD 2: Recorder Operations (NEW, Low Priority, Optional) =====
    void recorderHandler() {
        while (!done) {
            switch (recorderState) {
                case RecorderState::IDLE:
                    sleep(100ms);
                    break;

                case RecorderState::ARMED:
                    // Continuously sample recorder variables
                    if (recorderModule->sampleAndCheckTrigger()) {
                        // Trigger fired!
                        recorderState = RecorderState::TRIGGERED;
                        notifyGUI();
                    }
                    sleep(1.0 / recorderSampleRate);
                    break;

                case RecorderState::TRIGGERED:
                    // Capture post-trigger samples
                    recorderModule->capturePostTrigger();
                    recorderState = RecorderState::READY;
                    break;

                case RecorderState::DOWNLOADING:
                    // Download buffer in chunks (non-blocking)
                    recorderModule->downloadChunk();
                    break;

                case RecorderState::READY:
                    // Buffer ready for viewing/export
                    sleep(100ms);
                    break;
            }
        }
    }

    std::thread dataHandle;           // Existing
    std::thread recorderHandle;       // NEW - optional recorder thread
    std::shared_ptr<RecorderModule> recorderModule;  // NEW
};
```

---

## 3. Universal Recorder Module Design

### 3.1 Recorder Module Interface (Probe-Agnostic)

```cpp
/**
 * @file RecorderModule.hpp
 * @brief Universal oscilloscope-style recorder for all debug probes
 *
 * This module provides trigger and circular buffer recording capabilities
 * for STLink, JLink, and UART probes. It operates independently from live
 * variable sampling and never blocks the main data acquisition thread.
 */

class RecorderModule {
public:
    enum class RecorderState {
        IDLE,           // Not recording
        ARMED,          // Waiting for trigger
        TRIGGERED,      // Trigger fired, capturing post-trigger
        READY,          // Capture complete, ready for download
        DOWNLOADING     // Transferring data to host
    };

    enum class TriggerType {
        EDGE,           // Rising/falling edge
        LEVEL,          // Threshold crossing
        WINDOW,         // Enter/exit range
        LOGIC,          // Boolean combination
        PATTERN,        // Sequence of events
        EXTERNAL        // GPIO or software trigger
    };

    struct TriggerConfig {
        uint8_t triggerId;
        TriggerType type;
        uint32_t varAddress;
        uint8_t condition;      // Type-specific
        double value1;          // Threshold
        double value2;          // Window upper bound
        bool enablePreTrigger;
        uint8_t preTriggerPercent;  // 0-100
    };

    struct RecorderConfig {
        uint32_t bufferSamples;     // Total buffer size
        uint32_t sampleRateHz;      // Sampling frequency
        std::vector<uint32_t> addresses;  // Variables to record
        std::vector<uint8_t> sizes;       // Variable sizes
        bool compressionEnabled;
    };

    /**
     * @brief Create recorder module for specific probe type
     * @param probe Debug probe instance (STLink/JLink/UART)
     * @param logger Logger instance
     */
    RecorderModule(std::shared_ptr<IDebugProbe> probe, spdlog::logger* logger);

    /**
     * @brief Configure recorder parameters
     * @param config Recorder configuration
     * @return true on success
     */
    bool configure(const RecorderConfig& config);

    /**
     * @brief Setup trigger condition
     * @param config Trigger configuration
     * @return true on success
     */
    bool setupTrigger(const TriggerConfig& config);

    /**
     * @brief Arm trigger and start recording
     * @param mode Single-shot, normal, auto, continuous
     * @return true on success
     */
    bool arm(TriggerMode mode);

    /**
     * @brief Disarm trigger and stop recording
     */
    void disarm();

    /**
     * @brief Force trigger immediately
     */
    void forceTrigger();

    /**
     * @brief Sample variables and check trigger condition
     * @return true if trigger fired
     *
     * Called repeatedly from recorder thread. Non-blocking.
     */
    bool sampleAndCheckTrigger();

    /**
     * @brief Capture remaining post-trigger samples
     *
     * Called after trigger fires. Blocks briefly to complete capture.
     */
    void capturePostTrigger();

    /**
     * @brief Download buffer chunk (non-blocking)
     * @return true if more chunks available
     *
     * Downloads buffer in small chunks to avoid blocking.
     * Call repeatedly until returns false.
     */
    bool downloadChunk();

    /**
     * @brief Get captured data
     * @param offset Offset from trigger point (negative = pre-trigger)
     * @param numSamples Number of samples to retrieve
     * @return Vector of timestamped samples
     */
    std::vector<Sample> getData(int32_t offset, uint32_t numSamples);

    /**
     * @brief Get recorder state
     */
    RecorderState getState() const { return state; }

    /**
     * @brief Get capture statistics
     */
    struct Stats {
        uint32_t totalSamples;
        uint32_t preTriggerSamples;
        uint32_t postTriggerSamples;
        double triggerTimestamp;
        uint32_t bufferOverruns;
        double actualSampleRate;
    };
    Stats getStats() const;

    /**
     * @brief Export captured data to CSV
     * @param filename Output file path
     * @return true on success
     */
    bool exportToCSV(const std::string& filename);

private:
    // Probe-specific implementations
    std::shared_ptr<IDebugProbe> probe;
    std::unique_ptr<RecorderBackend> backend;  // STLink/JLink/UART specific

    // Circular buffer (host-side for STLink/JLink)
    CircularBuffer<Sample, 100000> circularBuffer;  // Configurable size

    // Trigger evaluation
    TriggerConfig triggerConfig;
    TriggerEvaluator triggerEvaluator;

    // State management
    RecorderState state = RecorderState::IDLE;
    size_t triggerSampleIndex = 0;
    size_t preTriggerSamples = 0;
    size_t postTriggerSamples = 0;

    // Download state
    size_t downloadOffset = 0;
    const size_t downloadChunkSize = 1024;  // Samples per chunk

    spdlog::logger* logger;
};
```

### 3.2 Recorder Backend (Probe-Specific Implementation)

```cpp
/**
 * @brief Abstract backend interface for probe-specific recording
 */
class RecorderBackend {
public:
    virtual ~RecorderBackend() = default;

    /**
     * @brief Read variables for recording
     * @param addresses Variable addresses
     * @param sizes Variable sizes
     * @param values Output buffer
     * @return true on success
     */
    virtual bool readVariables(const std::vector<uint32_t>& addresses,
                               const std::vector<uint8_t>& sizes,
                               std::vector<uint32_t>& values) = 0;

    /**
     * @brief Setup hardware-accelerated recording (UART only)
     * @param config Recorder configuration
     * @return true if supported and configured
     */
    virtual bool setupHardwareRecording(const RecorderConfig& config) {
        return false;  // Not supported by default
    }

    /**
     * @brief Check if trigger fired on target (UART only)
     * @return true if trigger fired
     */
    virtual bool checkHardwareTrigger() {
        return false;  // Not supported
    }

    /**
     * @brief Download buffer from target (UART only)
     * @return Buffer data
     */
    virtual std::vector<uint8_t> downloadHardwareBuffer() {
        return {};  // Not supported
    }
};

/**
 * @brief STLink recorder backend (software circular buffer on host)
 */
class StlinkRecorderBackend : public RecorderBackend {
public:
    StlinkRecorderBackend(std::shared_ptr<StlinkDebugProbe> probe)
        : probe(probe) {}

    bool readVariables(const std::vector<uint32_t>& addresses,
                       const std::vector<uint8_t>& sizes,
                       std::vector<uint32_t>& values) override {
        // Use existing readMemory() - no target changes needed
        for (size_t i = 0; i < addresses.size(); i++) {
            uint32_t value = 0;
            if (!probe->readMemory(addresses[i], (uint8_t*)&value, sizes[i]))
                return false;
            values[i] = value;
        }
        return true;
    }

private:
    std::shared_ptr<StlinkDebugProbe> probe;
};

/**
 * @brief JLink recorder backend (software circular buffer on host)
 */
class JlinkRecorderBackend : public RecorderBackend {
public:
    JlinkRecorderBackend(std::shared_ptr<JlinkDebugProbe> probe)
        : probe(probe) {}

    bool readVariables(const std::vector<uint32_t>& addresses,
                       const std::vector<uint8_t>& sizes,
                       std::vector<uint32_t>& values) override {
        // Similar to STLink - use readMemory()
        for (size_t i = 0; i < addresses.size(); i++) {
            uint32_t value = 0;
            if (!probe->readMemory(addresses[i], (uint8_t*)&value, sizes[i]))
                return false;
            values[i] = value;
        }
        return true;
    }

private:
    std::shared_ptr<JlinkDebugProbe> probe;
};

/**
 * @brief UART recorder backend (hardware-accelerated on target)
 */
class UartRecorderBackend : public RecorderBackend {
public:
    UartRecorderBackend(std::shared_ptr<UartDebugProbe> probe)
        : probe(probe) {}

    bool readVariables(const std::vector<uint32_t>& addresses,
                       const std::vector<uint8_t>& sizes,
                       std::vector<uint32_t>& values) override {
        // For software mode, use readMemory
        if (!hardwareMode) {
            for (size_t i = 0; i < addresses.size(); i++) {
                uint32_t value = 0;
                if (!probe->readMemory(addresses[i], (uint8_t*)&value, sizes[i]))
                    return false;
                values[i] = value;
            }
            return true;
        }
        return false;  // Hardware mode doesn't need host sampling
    }

    bool setupHardwareRecording(const RecorderConfig& config) override {
        // Use UART-specific recorder commands
        hardwareMode = probe->setupRecorder(config);
        return hardwareMode;
    }

    bool checkHardwareTrigger() override {
        if (!hardwareMode) return false;
        return probe->getTriggerStatus() == TriggerState::FIRED;
    }

    std::vector<uint8_t> downloadHardwareBuffer() override {
        if (!hardwareMode) return {};
        return probe->downloadBuffer();
    }

private:
    std::shared_ptr<UartDebugProbe> probe;
    bool hardwareMode = false;
};
```

### 3.3 Recording Flow

**Software Recording (STLink/JLink):**
```
┌─────────────────────────────────────────────┐
│ MCUViewer Recorder Thread                   │
│                                              │
│ while (armed) {                              │
│   // Read variables via existing API        │
│   probe->readMemory(addr1, buf1, size1);    │
│   probe->readMemory(addr2, buf2, size2);    │
│                                              │
│   // Store in host circular buffer          │
│   circularBuffer.push({timestamp, values}); │
│                                              │
│   // Evaluate trigger in software           │
│   if (evaluateTrigger(values)) {            │
│     triggerFired = true;                    │
│     capturePostTrigger();                   │
│   }                                          │
│                                              │
│   sleep(1.0 / sampleRate);                  │
│ }                                            │
└─────────────────────────────────────────────┘
```

**Hardware Recording (UART):**
```
┌─────────────────────────────────────────────┐
│ MCUViewer Recorder Thread                   │
│                                              │
│ // Setup hardware recorder once             │
│ probe->setupRecorder(config);               │
│ probe->setupTrigger(triggerConfig);         │
│ probe->armTrigger();                        │
│                                              │
│ while (armed) {                              │
│   // Just check if trigger fired            │
│   if (probe->checkTriggerStatus() == FIRED) {│
│     // Download buffer in background        │
│     downloadBufferInChunks();               │
│   }                                          │
│   sleep(100ms);  // Low frequency check     │
│ }                                            │
└─────────────────────────────────────────────┘
                    │
                    │ UART protocol
                    ▼
┌─────────────────────────────────────────────┐
│ Target Firmware (UART only)                 │
│                                              │
│ // DMA-driven sampling at 10 kHz            │
│ TIM_IRQHandler() {                          │
│   // Hardware samples automatically         │
│   if (evaluateTrigger()) {                  │
│     triggerFired = true;                    │
│     capturePostTrigger();                   │
│   }                                          │
│ }                                            │
│                                              │
│ // Buffer ready - wait for host download    │
└─────────────────────────────────────────────┘
```

---

## 4. Integration with Existing MCUViewer Architecture

### 4.1 Modified Files (Minimal Changes)

**src/DataHandler/ViewerDataHandler.hpp:**
```cpp
class ViewerDataHandler : public DataHandlerBase {
public:
    // Existing interface unchanged...

    // 🆕 Recorder control methods (optional, can be nullptr)
    void setRecorderModule(std::shared_ptr<RecorderModule> recorder);
    std::shared_ptr<RecorderModule> getRecorderModule();

private:
    void dataHandler();  // Existing - no changes
    void recorderHandler();  // 🆕 NEW thread

    std::thread dataHandle;       // Existing
    std::thread recorderHandle;   // 🆕 NEW (optional)
    std::shared_ptr<RecorderModule> recorderModule;  // 🆕 NEW (optional)
};
```

**src/DataHandler/ViewerDataHandler.cpp:**
```cpp
ViewerDataHandler::ViewerDataHandler(...) : DataHandlerBase(...) {
    dataHandle = std::thread(&ViewerDataHandler::dataHandler, this);

    // Recorder thread starts later when recorder is enabled
    // Does NOT start automatically!
}

void ViewerDataHandler::setRecorderModule(std::shared_ptr<RecorderModule> recorder) {
    recorderModule = recorder;

    // Start recorder thread only when module is set
    if (recorder && !recorderHandle.joinable()) {
        recorderHandle = std::thread(&ViewerDataHandler::recorderHandler, this);
    }
}

void ViewerDataHandler::recorderHandler() {
    if (!recorderModule) return;

    while (!done) {
        switch (recorderModule->getState()) {
            case RecorderState::IDLE:
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                break;

            case RecorderState::ARMED:
                // Sample and check trigger (non-blocking)
                if (recorderModule->sampleAndCheckTrigger()) {
                    logger->info("Recorder trigger fired!");
                }
                std::this_thread::sleep_for(std::chrono::microseconds(
                    1000000 / recorderModule->getSampleRate()));
                break;

            case RecorderState::TRIGGERED:
                // Capture remaining post-trigger samples
                recorderModule->capturePostTrigger();
                break;

            case RecorderState::DOWNLOADING:
                // Download in chunks (non-blocking)
                recorderModule->downloadChunk();
                break;

            case RecorderState::READY:
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                break;
        }
    }
}
```

### 4.2 New Files

```
src/Recorder/
├── RecorderModule.hpp          # Main recorder interface
├── RecorderModule.cpp          # Implementation
├── RecorderBackend.hpp         # Probe-specific backends
├── StlinkRecorderBackend.cpp   # STLink implementation
├── JlinkRecorderBackend.cpp    # JLink implementation
├── UartRecorderBackend.cpp     # UART implementation
├── TriggerEvaluator.hpp        # Trigger condition evaluation
├── TriggerEvaluator.cpp
├── CircularBuffer.hpp          # Host-side circular buffer (template)
└── RecorderCompression.hpp     # Optional compression

src/Gui/
├── GuiRecorderControl.hpp      # Recorder control panel
├── GuiRecorderControl.cpp
├── GuiRecorderView.hpp         # Oscilloscope view
└── GuiRecorderView.cpp
```

### 4.3 GUI Integration

**GuiRecorderControl - Control Panel:**
```
┌──────────────────────────────────────────────────┐
│ Recorder Control                         [X]     │
├──────────────────────────────────────────────────┤
│ Status: ●IDLE  [Enable Recorder]                │
│                                                   │
│ Probe: [●STLink  ○JLink  ○UART]                 │
│ Mode:  [●Software  ○Hardware (UART only)]       │
│                                                   │
│ ─── Recording Parameters ───                     │
│ Buffer Size:  [10000__] samples                  │
│ Sample Rate:  [1000___] Hz                       │
│ Pre-trigger:  [80_____]%                         │
│                                                   │
│ ─── Variables to Record ───                      │
│ ☑ motor_speed    (0x20000000)                   │
│ ☑ motor_current  (0x20000004)                   │
│ ☐ fault_flag     (0x20000008)                   │
│                                                   │
│ ─── Trigger Setup ───                            │
│ Variable:  [motor_current ▼]                    │
│ Type:      [Level ▼]                             │
│ Condition: [Above ▼]  Value: [5.0__]            │
│                                                   │
│ [Configure]  [Arm Trigger]  [Force Trigger]     │
│                                                   │
│ ⚠ Note: Live variables continue updating        │
│   during recording and download!                 │
└──────────────────────────────────────────────────┘
```

**Key UI Features:**
1. **Enable/Disable recorder** without affecting live operation
2. **Software mode** works with all probes (host-side circular buffer)
3. **Hardware mode** available for UART (faster, lower host CPU)
4. **Clear indication** that live variables are not affected

---

## 5. UART Protocol (Unchanged from V2)

The UART protocol specification remains the same as V2, with all trigger and recorder commands. These are **only used when UART probe is selected**.

For STLink/JLink, the recorder module uses existing `readMemory()` calls and implements everything on the host side.

---

## 6. Target Firmware (UART Only, Optional)

### 6.1 Firmware Architecture (Unchanged)

The target firmware for UART remains as designed in V2, with all trigger and recorder capabilities. This is **optional** - users can choose software mode instead.

### 6.2 Firmware Integration Modes

**Mode 1: Software Recording (Default)**
- No target firmware changes needed
- Works with all probes (STLink/JLink/UART)
- Host reads variables and builds circular buffer
- Trigger evaluation on host
- Lower sampling rates (~100-1000 Hz typical)

**Mode 2: Hardware Recording (UART Only, Optional)**
- Requires target firmware integration
- DMA-driven circular buffer on target
- Hardware trigger evaluation
- Higher sampling rates (~1-10 kHz)
- Lower host CPU usage

```c
// Minimal UART integration (software mode)
int main(void) {
    HAL_Init();
    MX_USART2_UART_Init();

    // Only basic UART service needed
    mcuv_uart_init(&huart2, 115200);
    mcuv_uart_set_device_name("MyDevice");

    while (1) {
        mcuv_uart_process();  // Process read/write commands
        do_application_work();
    }
    // NO recorder setup needed - host does everything!
}

// Optional UART integration (hardware mode for high performance)
int main(void) {
    HAL_Init();
    MX_USART2_UART_Init();
    MX_TIM2_Init();  // For sampling timer
    MX_DMA_Init();   // For circular buffer

    mcuv_uart_init(&huart2, 921600);
    mcuv_uart_set_device_name("MyDevice");

    // Enable hardware recorder (optional)
    mcuv_uart_enable_recorder();

    while (1) {
        mcuv_uart_process();
        do_application_work();
    }
}
```

---

## 7. Performance Characteristics

### 7.1 Software Recording Mode (All Probes)

| Probe | Sample Rate | Variables | Host CPU | Probe CPU | Latency |
|-------|-------------|-----------|----------|-----------|---------|
| STLink | 100 Hz | 10 | 2% | 0% | 10 ms |
| STLink | 1 kHz | 5 | 5% | 0% | 1 ms |
| JLink | 100 Hz | 10 | 2% | 0% | 10 ms |
| JLink | 1 kHz | 10 | 4% | 0% | 1 ms |
| JLink | 10 kHz | 5 | 15% | 0% | 100 μs |
| UART | 100 Hz | 10 | 2% | 0% | 10 ms |
| UART | 1 kHz | 5 | 5% | 0% | 1 ms |

**Advantages:**
- ✅ No target changes needed
- ✅ Works with all probes
- ✅ Easy to use
- ✅ Good for most debugging scenarios

**Limitations:**
- ⚠️ Host CPU usage increases with sample rate
- ⚠️ Limited to ~1-10 kHz depending on probe
- ⚠️ USB latency affects timing

### 7.2 Hardware Recording Mode (UART Only)

| Configuration | Sample Rate | Variables | Host CPU | Target CPU | Latency |
|---------------|-------------|-----------|----------|------------|---------|
| Basic | 1 kHz | 10 | <1% | 1% | 1 ms |
| DMA | 1 kHz | 10 | <1% | <0.5% | 1 ms |
| DMA | 10 kHz | 10 | <1% | <1% | 100 μs |
| DMA | 10 kHz | 20 | <1% | 2% | 100 μs |

**Advantages:**
- ✅ Very low host CPU usage
- ✅ Higher sampling rates (1-10 kHz)
- ✅ Precise hardware timing
- ✅ DMA reduces target CPU to <1%
- ✅ Better trigger latency (<10 samples)

**Limitations:**
- ⚠️ Requires target firmware integration
- ⚠️ UART only
- ⚠️ Uses target RAM for buffer

---

## 8. Use Cases and Recommendations

### Use Case 1: General Debugging
**Scenario:** Monitoring variables during development

**Recommendation:** Software mode with STLink/JLink
- No target changes
- 100-500 Hz is plenty
- Live variables always visible

### Use Case 2: Transient Event Capture
**Scenario:** Catching rare motor overcurrent events

**Recommendation:** Software mode with any probe, or hardware mode with UART
- Setup trigger on current > threshold
- 80% pre-trigger buffer
- 1 kHz sampling adequate

### Use Case 3: High-Speed Signal Analysis
**Scenario:** Analyzing control loop oscillations at 5 kHz

**Recommendation:** Hardware mode with UART or JLink software mode
- JLink can handle 10 kHz in software mode
- UART hardware mode preferred for longer captures
- Compression helps with data transfer

### Use Case 4: Production Monitoring
**Scenario:** Field devices with UART interface

**Recommendation:** Hardware mode with UART
- Deploy target with recorder firmware
- Remote trigger and download
- Captures intermittent faults

### Use Case 5: Multi-Board Testing
**Scenario:** Testing 5 boards simultaneously

**Recommendation:** Software mode with multiple STLink/JLink probes
- Each board gets its own recorder
- Independent trigger conditions
- All data synchronized by timestamp

---

## 9. Implementation Phases (Realistic)

### Phase 1: UART Basic Interface (Week 1-2)
- Implement `UartProtocol.hpp` and `SerialPort`
- Basic READ/WRITE_MEMORY commands
- Device enumeration
- **No recorder yet**

### Phase 2: Software Recorder Module (Week 3-4)
- Implement `RecorderModule` class
- Host-side circular buffer
- Trigger evaluation
- Backend interface
- **Works with STLink/JLink via readMemory()**

### Phase 3: STLink/JLink Backends (Week 5)
- `StlinkRecorderBackend` implementation
- `JlinkRecorderBackend` implementation
- Test with existing probes
- Performance optimization

### Phase 4: Recorder GUI (Week 6)
- `GuiRecorderControl` panel
- `GuiRecorderView` oscilloscope display
- Integration with main GUI
- CSV export

### Phase 5: UART Hardware Recording (Week 7-8)
- Extend UART protocol with recorder commands
- `UartRecorderBackend` with hardware support
- Target firmware library (optional)
- DMA integration examples

### Phase 6: Target Firmware Examples (Week 9)
- STM32 example project
- Minimal integration example
- Hardware recorder example
- Documentation

### Phase 7: UART Simulator (Week 10)
- Extend simulator with recorder support
- Scenario-based testing
- Standalone simulator app

### Phase 8: Testing & Documentation (Week 11-12)
- End-to-end testing all probe types
- Performance benchmarking
- User documentation
- Video tutorials

---

## 10. File Summary

### New Files (~35 files)

**Recorder Module (probe-agnostic):**
- `src/Recorder/RecorderModule.hpp` (~250 lines)
- `src/Recorder/RecorderModule.cpp` (~600 lines)
- `src/Recorder/RecorderBackend.hpp` (~150 lines)
- `src/Recorder/StlinkRecorderBackend.cpp` (~200 lines)
- `src/Recorder/JlinkRecorderBackend.cpp` (~200 lines)
- `src/Recorder/UartRecorderBackend.cpp` (~300 lines)
- `src/Recorder/TriggerEvaluator.hpp` (~150 lines)
- `src/Recorder/TriggerEvaluator.cpp` (~300 lines)
- `src/Recorder/CircularBuffer.hpp` (~200 lines, template)
- `src/Recorder/RecorderCompression.hpp` (~100 lines)
- `src/Recorder/RecorderCompression.cpp` (~200 lines)

**UART Probe:**
- `src/MemoryReader/UartDebugProbe.hpp` (~250 lines)
- `src/MemoryReader/UartDebugProbe.cpp` (~600 lines)
- `src/MemoryReader/UartProtocol.hpp` (~200 lines)
- `src/MemoryReader/SerialPort.hpp` (~80 lines)
- `src/MemoryReader/SerialPort.cpp` (~400 lines)
- `src/MemoryReader/UartSimulator.hpp` (~150 lines)
- `src/MemoryReader/UartSimulator.cpp` (~400 lines)

**GUI:**
- `src/Gui/GuiRecorderControl.hpp` (~100 lines)
- `src/Gui/GuiRecorderControl.cpp` (~500 lines)
- `src/Gui/GuiRecorderView.hpp` (~120 lines)
- `src/Gui/GuiRecorderView.cpp` (~600 lines)

**Target Firmware (UART only, optional):**
- `firmware/inc/mcuv_uart.h` (~150 lines)
- `firmware/inc/mcuv_uart_protocol.h` (~150 lines)
- `firmware/inc/mcuv_recorder.h` (~180 lines) *optional*
- `firmware/src/mcuv_uart_protocol.c` (~600 lines)
- `firmware/src/mcuv_uart_memory.c` (~300 lines)
- `firmware/src/mcuv_uart_driver.c` (~400 lines)
- `firmware/src/mcuv_recorder.c` (~800 lines) *optional*
- `firmware/port/stm32/mcuv_uart_stm32.c` (~200 lines)
- `firmware/port/stm32/mcuv_dma_stm32.c` (~300 lines) *optional*
- `firmware/examples/minimal_uart.c` (~100 lines)
- `firmware/examples/hardware_recorder.c` (~200 lines) *optional*

**Tests:**
- `test/RecorderModuleTest.cpp` (~300 lines)
- `test/UartProtocolTest.cpp` (~200 lines)

### Modified Files (~10 files)

- `src/DataHandler/ViewerDataHandler.hpp` (~20 lines added)
- `src/DataHandler/ViewerDataHandler.cpp` (~100 lines added)
- `src/Gui/Gui.hpp` (~10 lines)
- `src/Gui/Gui.cpp` (~20 lines)
- `src/Gui/GuiAcqusition.cpp` (~50 lines)
- `src/MemoryReader/IDebugProbe.hpp` (~5 lines)
- `CMakeLists.txt` (~30 lines)
- `README.md` (~100 lines)

**Total new code: ~7500 lines**
**Total modifications: ~335 lines**

---

## 11. Key Advantages of V3 Design

### vs V2:
✅ **Non-blocking** - Live variables never affected by recorder
✅ **Universal** - Recorder works with STLink/JLink/UART
✅ **Optional** - Can be disabled/excluded from build
✅ **Backwards compatible** - Existing code unchanged
✅ **Practical** - No target changes needed for STLink/JLink
✅ **Flexible** - Software mode (easy) or hardware mode (fast)

### vs Traditional Approaches:
✅ **No expensive equipment** - Use existing probes
✅ **Pre-trigger capture** - See what led to the event
✅ **Trigger flexibility** - Complex conditions not possible with hardware
✅ **Remote capable** - UART over network/wireless
✅ **Software updates** - Trigger logic can be enhanced without hardware changes

---

## 12. Success Criteria

✅ **Non-interference**: Live variable refresh rate unaffected when recorder enabled
✅ **Universal support**: Recorder works with STLink, JLink, and UART probes
✅ **Software mode**: Achieve 100 Hz recording with 10 variables, <5% host CPU
✅ **Hardware mode**: Achieve 1 kHz recording with 10 variables, <1% target CPU
✅ **Pre-trigger**: 50-95% pre-trigger buffer configurable and working
✅ **Trigger latency**: <100ms in software mode, <10 samples in hardware mode
✅ **Data integrity**: 99.9% success rate over 1-hour test
✅ **Ease of use**: Enable recorder with <10 clicks in GUI
✅ **Documentation**: Complete guide with video tutorials

---

## 13. Conclusion

**V3 design achieves the best of all worlds:**

1. **For STLink/JLink users**: Get oscilloscope-style recording with no target changes whatsoever
2. **For UART users**: Choose between simple software mode or high-performance hardware mode
3. **For all users**: Live variables always work - recorder is truly optional
4. **For developers**: Clean modular design that doesn't pollute existing code

The key innovation is the **dual-thread architecture with separate recorder backends**, allowing the same recorder functionality to work across all probe types while maintaining complete independence from the core acquisition loop.

This design is **production-ready**, **backwards-compatible**, and **future-proof**.