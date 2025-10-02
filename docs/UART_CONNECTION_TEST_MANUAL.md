# UART Probe Connection Test Manual

## Overview

This manual provides step-by-step instructions for performing Task 2.3: UART Probe Connection Test. This test verifies that the UART debug probe integration works correctly with the MCUViewer GUI for live variable monitoring.

**Goal:** Verify that the UART probe can connect to a simulated target, read variables, and display live data in the variable table and plots.

---

## Prerequisites

### 1. Software Requirements
- MCUViewer built with UART support (`-DUART_SUPPORT=ON`)
- `socat` utility (Linux/macOS) or virtual COM port driver (Windows)

### 2. Test Files
- Test ELF file: `test/testFiles/MCUViewer_test.elf`
- UART Simulator: Built as part of test suite

### 3. Build MCUViewer with UART Support

```bash
# Navigate to your MCUViewer repository root
cd /path/to/MCUViewer
mkdir -p build
cd build
cmake .. -DUART_SUPPORT=ON
make -j8
```

The MCUViewer executable will be at: `./MCUViewer`

---

## Test Setup

### Step 1: Create Virtual Serial Port Pair

The UART simulator needs a virtual serial port pair to communicate with MCUViewer.

#### macOS/Linux:

```bash
# Install socat if not already installed
# macOS: brew install socat
# Linux: sudo apt-get install socat

# Create virtual serial port pair (run in separate terminal)
socat -d -d pty,raw,echo=0,b115200 pty,raw,echo=0,b115200
```

**Output will look like:**
```
2025/10/02 12:34:56 socat[12345] N PTY is /dev/ttys004
2025/10/02 12:34:56 socat[12345] N PTY is /dev/ttys005
```

**Note the two port names!** You'll use:
- `/dev/ttys004` for the simulator
- `/dev/ttys005` for MCUViewer

#### Windows:

Use a virtual COM port driver like `com0com`:
1. Install com0com from https://com0com.sourceforge.net/
2. Create a COM port pair (e.g., COM10 ↔ COM11)
3. Use COM10 for simulator, COM11 for MCUViewer

---

### Step 2: Start UART Simulator

The simulator mimics a target microcontroller responding to UART protocol commands.

#### Option A: Build and Run Standalone Simulator

```bash
# From MCUViewer repository root
cd build_test
cmake .. -DMAKE_TESTS=1 -DUART_SUPPORT=ON
make -j8

# Run standalone simulator (replace with your port from Step 1)
./test/UartSimulatorStandalone /dev/ttys004 115200
```

**Expected output:**
```
UART Simulator Started
Port: /dev/ttys004
Baud Rate: 115200
Device: MCU-Simulator-v1.0
Memory: 65536 bytes

Simulator running... Press Ctrl+C to stop
```

#### Option B: Use Python Script (Alternative)

If you prefer, you can create a simple Python script to run the simulator. However, the C++ standalone version is recommended.

---

### Step 3: Prepare Test Variables in Simulator

The simulator has a 64KB memory map (addresses 0x00000000 - 0x0000FFFF). You can pre-populate it with test data.

For this test, we'll modify the standalone simulator to create some dynamic test variables:

**Variables for testing:**
- `0x20000000` - uint32_t counter (increments every 100ms)
- `0x20000004` - float sine wave (frequency 1 Hz)
- `0x20000008` - uint8_t boolean toggle
- `0x2000000C` - int16_t sawtooth wave

---

## Running the Test

### Test 2.3.1: Load Test ELF File

1. **Launch MCUViewer:**
   ```bash
   # From MCUViewer repository root
   cd build
   ./MCUViewer
   ```

2. **Load ELF file:**
   - In the "General" section, click the "..." button next to "*.elf file"
   - Navigate to: `<MCUViewer_root>/test/testFiles/MCUViewer_test.elf`
   - Select the file

**Expected result:** ✅ ELF file path displayed in text field

---

### Test 2.3.2: Connect to UART Simulator

1. **Select UART probe:**
   - In the "Debug Probe" section, open the "Debug probe" dropdown
   - Select: **UART**

2. **Configure UART settings:**
   - **UART Port:** Enter `/dev/ttys005` (or your slave port from Step 1)
   - **Baud Rate:** Select `115200`

3. **Refresh device list:**
   - Click the **@** button next to "Debug probe S/N"
   - The dropdown should populate with available serial ports

4. **Select the port:**
   - From the "Debug probe S/N" dropdown, select `/dev/ttys005`

**Expected result:** ✅ UART port configured, no error messages

---

### Test 2.3.3: Add Variables to Table

1. **Open variable selection:**
   - Click the **"+"** button in the main variable table area

2. **Add test variables:**
   - Select variables from the parsed ELF file
   - For manual testing, you can also use "Add by address":
     - Address: `0x20000000`, Size: 4, Type: `uint32_t`, Name: `counter`
     - Address: `0x20000004`, Size: 4, Type: `float`, Name: `sine_wave`
     - Address: `0x20000008`, Size: 1, Type: `uint8_t`, Name: `toggle`
     - Address: `0x2000000C`, Size: 2, Type: `int16_t`, Name: `sawtooth`

3. **Confirm variables added:**
   - Variables should appear in the variable table

**Expected result:** ✅ Variables added to table, values show "---" (not connected yet)

---

### Test 2.3.4: Start Acquisition

1. **Set sampling rate:**
   - In "General" section, set "Sampling [Hz]" to `10` (10 Hz = 100ms period)

2. **Click START button:**
   - The button should be enabled (green)
   - Click **START**

3. **Monitor connection:**
   - Status indicator should show "Running"
   - Variables should start showing values

**Expected result:** ✅ Acquisition starts without errors

---

### Test 2.3.5: Verify Variable Values Update in Table

1. **Observe variable table:**
   - Values should update every 100ms (10 Hz)
   - `counter` should increment: 0, 1, 2, 3, ...
   - `sine_wave` should oscillate: -1.0 to +1.0
   - `toggle` should flip: 0, 1, 0, 1, ...
   - `sawtooth` should ramp: 0, 100, 200, ..., 1000, 0, ...

2. **Check update rate:**
   - Values should update smoothly at 10 Hz
   - No delays or freezing

**Expected result:** ✅ Variables update at expected rate with realistic values

**Acceptance criteria:**
- Variables update at configured sample rate (10 Hz)
- No "Read Error" or timeout messages
- Values change over time (not stuck)

---

### Test 2.3.6: Verify Plots Update Correctly

1. **Open plot view:**
   - Variables with numeric types should have plot icons/buttons
   - Click on a variable's plot button to add it to the chart

2. **Add multiple variables to plot:**
   - Add `counter`, `sine_wave`, and `sawtooth` to the same plot

3. **Observe waveforms:**
   - `counter`: Linear ramp upward
   - `sine_wave`: Smooth sine wave oscillating between -1 and +1
   - `sawtooth`: Repeated ramp pattern

4. **Check plot updates:**
   - Waveforms should scroll in real-time
   - X-axis shows time
   - Y-axis shows values

**Expected result:** ✅ Plots display and update in real-time

**Acceptance criteria:**
- Plots render without lag
- Waveforms match expected patterns
- Multiple variables can be plotted simultaneously

---

### Test 2.3.7: Test Stop/Start Multiple Times

1. **Stop acquisition:**
   - Click **STOP** button
   - Variables should stop updating (values frozen)
   - Status should show "Stopped"

2. **Wait 5 seconds**

3. **Restart acquisition:**
   - Click **START** button
   - Variables should resume updating
   - Plots should continue from where they left off

4. **Repeat 3 times:**
   - Stop → Wait → Start
   - Stop → Wait → Start
   - Stop → Wait → Start

**Expected result:** ✅ Clean start/stop cycles with no crashes

**Acceptance criteria:**
- No crashes or hangs during stop/start
- Connection re-establishes cleanly each time
- No memory leaks (check with Activity Monitor / Task Manager)

---

## Test Checklist

Use this checklist to track test completion:

- [ ] **2.3.1:** Test ELF file loads successfully
- [ ] **2.3.2:** UART simulator connection established
- [ ] **2.3.3:** Variables added to table
- [ ] **2.3.4:** Acquisition starts without errors
- [ ] **2.3.5:** Variable values update at configured rate
- [ ] **2.3.6:** Plots display and update correctly
- [ ] **2.3.7:** Stop/start cycles work cleanly (3 iterations)

---

## Troubleshooting

### Issue: "Cannot open serial port"

**Possible causes:**
1. Port name incorrect
2. Port already in use by another application
3. Permissions issue (Linux)

**Solutions:**
```bash
# Linux: Add user to dialout group
sudo usermod -a -G dialout $USER
# Log out and log back in

# Check port availability
ls -l /dev/tty*

# Kill any processes using the port
lsof | grep /dev/ttys005
```

### Issue: "Connection timeout"

**Possible causes:**
1. Simulator not running
2. Wrong port selected
3. Baud rate mismatch

**Solutions:**
- Verify simulator is running in separate terminal
- Double-check port names match (simulator uses one, MCUViewer uses the other)
- Ensure both use same baud rate (115200)

### Issue: "CRC error" or "Invalid response"

**Possible causes:**
1. Baud rate mismatch
2. Serial port configuration issue
3. Data corruption

**Solutions:**
- Restart simulator and MCUViewer
- Try lower baud rate (115200 → 9600)
- Check socat is running correctly

### Issue: Variables not updating

**Possible causes:**
1. Simulator memory addresses don't match variable addresses
2. Read size mismatch
3. Simulator not responding

**Solutions:**
- Check simulator logs for incoming READ_MEMORY commands
- Verify variable addresses are within simulator memory range (0x00000000 - 0x0000FFFF)
- Enable verbose logging in simulator

### Issue: Plots not displaying

**Possible causes:**
1. Variable type not plottable (e.g., struct, array)
2. Values out of plot range
3. GUI rendering issue

**Solutions:**
- Only plot numeric types (int, float, etc.)
- Check Y-axis range (may need to zoom out)
- Try plotting one variable at a time

---

## Expected Performance

| Metric | Target | Acceptable Range |
|--------|--------|------------------|
| Sample rate | 10 Hz | 5-100 Hz |
| Connection time | < 1s | < 3s |
| Read latency | ~10ms | < 50ms |
| Stop/start time | < 0.5s | < 2s |
| Memory usage | Baseline | < +10 MB during test |
| CPU usage | < 5% | < 15% |

---

## Success Criteria

The test is **PASSED** if:
1. ✅ UART probe connects to simulator successfully
2. ✅ Variables update at configured sample rate (10 Hz)
3. ✅ Plots display real-time waveforms
4. ✅ 3 stop/start cycles complete without errors
5. ✅ No crashes, hangs, or memory leaks
6. ✅ All acceptance criteria met for each subtask

---

## Next Steps

After successful completion of Task 2.3:
- ✅ Mark Task 2.3 as complete in `docs/UART_IMPLEMENTATION_TASKS.md`
- ✅ Commit test results
- ✅ Move to **Phase 3: Recorder Module Foundation**

---

## Notes for Developers

### Debugging Tips

1. **Enable verbose logging:**
   - Simulator: Built with debug prints to console
   - MCUViewer: Check `logs/` directory for detailed logs

2. **Monitor serial traffic:**
   ```bash
   # macOS/Linux: Monitor serial port
   cat /dev/ttys005 | xxd
   ```

3. **Test simulator independently:**
   ```bash
   # Send raw UART commands to simulator
   echo -ne '\xAA\x03\x00\x00\x01\x???\x???' > /dev/ttys005
   ```

4. **Use test suite:**
   ```bash
   # Run UART integration tests
   cd build_test/test
   ./MCUViewer_test --gtest_filter=UartIntegrationTest.*
   ```

### Simulator Features

The simulator supports:
- ✅ READ_MEMORY (0x01) - Read up to 255 bytes
- ✅ WRITE_MEMORY (0x03) - Write up to 255 bytes
- ✅ GET_INFO (0x05) - Device information
- ✅ PING (0x09) - Keepalive
- ✅ CRC16-CCITT validation on all packets
- ✅ 64KB memory map (configurable)
- ✅ Statistics tracking (packets RX/TX, errors)

### Adding Dynamic Variables to Simulator

To make the test more realistic, modify `UartSimulatorStandalone.cpp` to update memory periodically:

```cpp
// In main loop
while (running) {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count();

    // Update counter (address 0x20000000)
    uint32_t counter = elapsed / 100;  // Increment every 100ms
    sim.writeMemory(0x20000000, reinterpret_cast<uint8_t*>(&counter), 4);

    // Update sine wave (address 0x20000004)
    float sine = std::sin(2.0 * M_PI * elapsed / 1000.0);  // 1 Hz
    sim.writeMemory(0x20000004, reinterpret_cast<uint8_t*>(&sine), 4);

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
}
```

---

**Document Version:** 1.0
**Last Updated:** 2025-10-02
**Author:** Claude Code
**Test Phase:** Phase 2 - GUI Integration
