# UART Probe Connection Test - Execution Guide

## Test Information

**Test:** Task 2.3 - UART Probe Connection Test
**Date:** 2025-10-02
**Purpose:** Verify UART probe GUI integration with live variable monitoring
**Prerequisites:** Tasks 2.1 and 2.2 completed (UART probe selection and settings UI)

---

## Quick Start Commands

### Step 1: Create Virtual Serial Port Pair

Open a terminal and run:

```bash
# This creates a virtual serial port pair
socat -d -d pty,raw,echo=0,b115200 pty,raw,echo=0,b115200
```

**Keep this terminal open!** It will display output like:

```
2025/10/02 16:15:30 socat[12345] N PTY is /dev/ttys004
2025/10/02 16:15:30 socat[12345] N PTY is /dev/ttys005
```

**Note down both port names:**
- Port 1 (e.g., `/dev/ttys004`) = For simulator
- Port 2 (e.g., `/dev/ttys005`) = For MCUViewer

---

### Step 2: Start UART Simulator

Open a **second terminal** and run:

```bash
# From MCUViewer repository root
cd build_test/test

# Replace /dev/ttys004 with your Port 1 from Step 1
./UartSimulatorStandalone /dev/ttys004
```

**Expected output:**

```
Initializing test variables:
============================
0x20000000: counter (uint32_t) = 0
0x20000004: temperature (float) = 25.5°C
0x20000008: voltage (float) = 3.3V
0x2000000C: current (float) = 1.5A
0x20000010: status (uint8_t) = 0x0
0x20000014: errorCode (uint32_t) = 0
0x20000018: timestamp (uint64_t) = 0
0x20000020: posX (int32_t) = 0
0x20000024: posY (int32_t) = 0
0x20000028: sineWave (float) = 0

Starting UART simulator on port: /dev/ttys004
Press Ctrl+C to stop
============================

[2025-10-02 16:15:35.123] [UartSimulator] [info] Simulator started successfully!
[2025-10-02 16:15:35.123] [UartSimulator] [info] Waiting for MCUViewer connection...
```

**Keep this terminal open!** The simulator runs until you press Ctrl+C.

---

### Step 3: Launch MCUViewer

Open a **third terminal** and run:

```bash
# From MCUViewer repository root
cd build
./MCUViewer
```

The MCUViewer GUI window will open.

---

## Manual Test Execution

Follow the steps in the GUI:

### Test 2.3.1: Load ELF File

1. In the "General" section, click **"..."** button next to "*.elf file"
2. Navigate to: `<MCUViewer_root>/test/testFiles/MCUViewer_test.elf`
3. Click "Open"

**✅ Verify:** ELF file path appears in the text field

---

### Test 2.3.2: Select UART Probe

1. Scroll to "Debug Probe" section
2. Click the "Debug probe" dropdown
3. Select: **UART**

**✅ Verify:** Dropdown shows "UART" selected

---

### Test 2.3.3: Configure UART Settings

1. In "UART Port" field, enter: `/dev/ttys005` (your Port 2 from Step 1)
2. In "Baud Rate" dropdown, select: **115200**
3. Click the **@** button to refresh device list
4. In "Debug probe S/N" dropdown, select your port

**✅ Verify:**
- UART Port shows `/dev/ttys005`
- Baud Rate shows `115200`
- S/N dropdown populated with available ports

---

### Test 2.3.4: Add Variables

**Option A: From ELF file (if variables are parsed)**

1. Click **"+"** button in variable table area
2. Select variables from the list
3. Click "Add"

**Option B: Add by address (manual)**

For testing, add these variables manually:

| Address      | Size | Type     | Name        |
|--------------|------|----------|-------------|
| 0x20000000   | 4    | uint32_t | counter     |
| 0x20000004   | 4    | float    | temperature |
| 0x20000008   | 4    | float    | voltage     |
| 0x2000000C   | 4    | float    | current     |
| 0x20000010   | 1    | uint8_t  | status      |
| 0x20000028   | 4    | float    | sineWave    |

Steps for each variable:
1. Click "Add Variable" or similar button
2. Enter address (e.g., `0x20000000`)
3. Select size (e.g., `4`)
4. Select type (e.g., `uint32_t`)
5. Enter name (e.g., `counter`)
6. Click "OK" or "Add"

**✅ Verify:** All 6 variables appear in the table with values showing "---" (not connected)

---

### Test 2.3.5: Start Acquisition

1. Set "Sampling [Hz]" to `10` (10 Hz = 100ms update rate)
2. Set "Max points" to `1000`
3. Set "Max view points" to `500`
4. Click the **START** button

**✅ Verify:**
- Button changes to "STOP" (or turns red)
- Status shows "Running" or "Acquiring"
- Variables start showing values

**Watch the simulator terminal** - you should see output like:

```
[2025-10-02 16:20:00.000] [UartSimulator] [info] Statistics: RX=50, TX=50, CRC_ERR=0, MEM_ERR=0
[2025-10-02 16:20:05.000] [UartSimulator] [info] Statistics: RX=100, TX=100, CRC_ERR=0, MEM_ERR=0
```

---

### Test 2.3.6: Verify Variable Updates

**In the MCUViewer variable table, observe:**

1. **counter** - Should increment: 0 → 1 → 2 → 3 → ... (increases by 1 every 100ms)
2. **temperature** - Should vary slightly: 25.0 → 25.1 → 25.2 → ... → 25.9 → 25.0 (cycles)
3. **voltage** - Should stay near 3.3V with small variations: 3.29 ↔ 3.31
4. **current** - Should oscillate smoothly: ~1.0A → ~2.0A → ~1.0A (sine wave)
5. **status** - Should change periodically: 0 → 1 → 2 → ...
6. **sineWave** - Should oscillate: -1.0 → 0 → +1.0 → 0 → -1.0

**⏱️ Timing Check:**
- Values should update approximately every 100ms (10 Hz)
- Updates should be smooth, no long pauses

**✅ Acceptance Criteria:**
- ✅ All variables update at ~10 Hz
- ✅ No "Read Error" or timeout messages
- ✅ Values change realistically (not stuck at 0 or garbled)
- ✅ Simulator shows RX/TX packets increasing
- ✅ CRC errors = 0, Memory errors = 0

---

### Test 2.3.7: Verify Plots

1. **Add variable to plot:**
   - Right-click on "sineWave" in the table, select "Plot" (or similar)
   - OR click plot icon next to the variable

2. **Observe the plot:**
   - Should show a smooth sine wave oscillating between -1 and +1
   - X-axis = time, Y-axis = value
   - Waveform should scroll from right to left

3. **Add more variables:**
   - Add "current" to the same plot → should also show sine-like pattern
   - Add "counter" to the plot → should show linear ramp upward
   - Add "temperature" to the plot → should show stepped ramp pattern

**✅ Acceptance Criteria:**
- ✅ Plots display in real-time
- ✅ Waveforms match expected patterns
- ✅ No lag or stuttering
- ✅ Multiple variables can be plotted together

---

### Test 2.3.8: Stop/Start Cycles

**Cycle 1:**
1. Click **STOP** button
2. **✅ Verify:** Variables stop updating (values frozen)
3. Wait 5 seconds
4. Click **START** button
5. **✅ Verify:** Variables resume updating from where they left off

**Cycle 2:**
1. Click **STOP** button
2. **✅ Verify:** No error messages, clean stop
3. Wait 5 seconds
4. Click **START** button
5. **✅ Verify:** Connection re-establishes, data flows again

**Cycle 3:**
1. Click **STOP** button
2. Wait 5 seconds
3. Click **START** button
4. **✅ Verify:** Still works correctly

**✅ Acceptance Criteria:**
- ✅ 3 stop/start cycles complete without errors
- ✅ No crashes or hangs
- ✅ Connection re-establishes cleanly each time
- ✅ No memory leaks (check Activity Monitor if suspicious)

---

## Test Results Template

After completing all tests, fill out this checklist:

```
UART Probe Connection Test Results
===================================
Date: 2025-10-02
Tester: [Your Name]
Platform: macOS [version]
MCUViewer Version: [git commit hash]

Test Results:
-------------
[ ] 2.3.1: ELF file loaded successfully
[ ] 2.3.2: UART probe selected in GUI
[ ] 2.3.3: UART settings configured (port + baud rate)
[ ] 2.3.4: Variables added to table
[ ] 2.3.5: Acquisition started without errors
[ ] 2.3.6: Variable values update at ~10 Hz
[ ] 2.3.7: Plots display and update in real-time
[ ] 2.3.8: 3 stop/start cycles completed successfully

Performance Metrics:
--------------------
- Sampling rate configured: 10 Hz
- Actual update rate: ~10 Hz (measured)
- Connection time: <1s
- Simulator statistics:
  - Packets RX: [number]
  - Packets TX: [number]
  - CRC errors: 0
  - Memory errors: 0
- Memory usage: [baseline] → [during test] MB
- CPU usage: <5%

Issues Found:
-------------
[List any issues, or write "None"]

Overall Result: [ ] PASS  [ ] FAIL
```

---

## Cleanup

After testing:

1. **Stop MCUViewer:**
   - Click STOP button
   - Close the application

2. **Stop simulator:**
   - Go to simulator terminal
   - Press **Ctrl+C**

3. **Stop socat:**
   - Go to socat terminal
   - Press **Ctrl+C**

---

## Troubleshooting

### Problem: "Cannot open serial port"

**Solutions:**
```bash
# Check if port exists
ls -l /dev/ttys*

# Make sure socat is running
ps aux | grep socat

# Try a different port
# Start fresh: kill socat, restart, note NEW port names
```

### Problem: "Connection timeout"

**Check:**
1. Is simulator running? (`ps aux | grep UartSimulator`)
2. Are port names correct? (MCUViewer uses Port 2, simulator uses Port 1)
3. Same baud rate? (both should be 115200)

### Problem: Variables show "---" or errors

**Check:**
1. Is START button pressed?
2. Check simulator terminal for errors
3. Try adding variables with simpler addresses first (0x20000000)

### Problem: Plots not showing

**Check:**
1. Are variables numeric types? (int, float, etc.)
2. Try zooming out on Y-axis
3. Add variables one at a time

---

## Expected Behavior Summary

| Test Step | Expected Result | Pass/Fail |
|-----------|----------------|-----------|
| Load ELF | File path displayed | [ ] |
| Select UART | Dropdown shows "UART" | [ ] |
| Config settings | Port + baud configured | [ ] |
| Add variables | 6 variables in table | [ ] |
| Start acquisition | Status = "Running" | [ ] |
| Variable updates | All update at 10 Hz | [ ] |
| Plots | Smooth waveforms | [ ] |
| Stop/start #1 | Clean cycle | [ ] |
| Stop/start #2 | Clean cycle | [ ] |
| Stop/start #3 | Clean cycle | [ ] |

**Overall Test: PASS if all steps PASS**

---

## Automated Test Script (Future Work)

For automated testing, we can create a Python script that:
1. Launches socat in background
2. Launches simulator in background
3. Launches MCUViewer (headless or with GUI automation)
4. Sends commands via config file or IPC
5. Verifies variable updates
6. Reports results

This is **Phase 6** work (Task 6.2: Integration Tests).

---

## Next Steps

After successful test:
1. Fill out test results template above
2. Update `docs/UART_IMPLEMENTATION_TASKS.md` - mark Task 2.3 as complete
3. Commit test documentation
4. Proceed to **Phase 3: Recorder Module Foundation**

---

**Document Version:** 1.0
**Last Updated:** 2025-10-02
**Related:** UART_CONNECTION_TEST_MANUAL.md, UART_IMPLEMENTATION_TASKS.md
