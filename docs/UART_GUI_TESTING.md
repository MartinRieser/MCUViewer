# Testing UART with MCUViewer GUI

This guide explains how to test the UART debug probe functionality using the MCUViewer GUI application with the UART simulator (no hardware required).

## Overview

The UART simulator creates a virtual embedded device that responds to MCUViewer's debug commands. This allows you to:
- Test the complete UART workflow without hardware
- View live variable updates in the GUI
- Plot simulated sensor data
- Verify the probe integration

## Setup

### 1. Build MCUViewer and the Simulator

```bash
cd /Users/martinrieser/Documents/MCUViewer/build

# Build with tests enabled to get the simulator
cmake .. -DMAKE_TESTS=1 -DCMAKE_POLICY_VERSION_MINIMUM=3.5
make -j8

# This creates:
# - ./MCUViewer (main GUI application)
# - ./test/UartSimulatorStandalone (simulator program)
```

### 2. Create Virtual Serial Port Pair

#### macOS/Linux:
```bash
# Install socat if needed
brew install socat  # macOS
# sudo apt install socat  # Linux

# Create virtual port pair (keep this terminal open!)
socat -d -d pty,raw,echo=0 pty,raw,echo=0
```

This will output something like:
```
2025/10/01 12:00:00 socat[12345] N PTY is /dev/pts/5
2025/10/01 12:00:00 socat[12345] N PTY is /dev/pts/6
```

**Note the two port paths!** One will be for the simulator, one for MCUViewer.

#### Windows:
- Use `com0com` or similar to create COM port pairs (e.g., COM5 ↔ COM6)

### 3. Start the UART Simulator

In a **new terminal** (keep socat running in the first):

```bash
cd /Users/martinrieser/Documents/MCUViewer/build/test

# Start simulator on one of the PTY ports
./UartSimulatorStandalone /dev/pts/6
```

You should see:
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

Starting UART simulator on port: /dev/pts/6
Press Ctrl+C to stop
============================

Simulator started successfully!
Waiting for MCUViewer connection...
```

**Keep this running!**

### 4. Launch MCUViewer

In a **third terminal**:

```bash
cd /Users/martinrieser/Documents/MCUViewer/build
./MCUViewer
```

## Using UART in MCUViewer GUI

### Step 1: Configure Acquisition Settings

1. In MCUViewer, go to **Options → Acquisition Settings**
2. Under **Debug Probe**, select **UART** from the dropdown
3. Set the following:
   - **Serial Port**: Select the *other* PTY port (e.g., `/dev/pts/5`)
   - **Baud Rate**: `115200` (default)
4. Click **Done**

### Step 2: Add Variables Manually

Since we don't have a real ELF file for the simulator, add variables manually:

1. Click **Add variable** button
2. Add the following variables:

| Name       | Address      | Type    | Description           |
|------------|--------------|---------|----------------------|
| counter    | 0x20000000   | u32     | Incrementing counter |
| temperature| 0x20000004   | float   | Room temperature     |
| voltage    | 0x20000008   | float   | 3.3V rail            |
| current    | 0x2000000C   | float   | Load current         |
| status     | 0x20000010   | u8      | Status byte          |
| errorCode  | 0x20000014   | u32     | Error code           |
| timestamp  | 0x20000018   | u64     | Timestamp (ms)       |
| posX       | 0x20000020   | i32     | X position           |
| posY       | 0x20000024   | i32     | Y position           |
| sineWave   | 0x20000028   | float   | Sine wave            |

### Step 3: Start Acquisition

1. Click the **STOPPED** button in the main window
2. The button should change to **RUNNING** with a green indicator
3. You should see variable values updating in the table!

### Step 4: Plot Variables

1. **Drag and drop** variables from the table to the plot area
2. Recommended plots to try:
   - **sineWave**: Should show a smooth sine wave at ~1Hz
   - **temperature**: Should show gradual changes
   - **voltage**: Should show 3.3V ± small variations
   - **current**: Should show sinusoidal variation around 1.5A
   - **posX vs posY**: Create a 2D plot to see circular motion

### Step 5: Monitor Simulator

In the simulator terminal, you should see periodic statistics:

```
Statistics: RX=250, TX=250, CRC_ERR=0, MEM_ERR=0
Statistics: RX=500, TX=500, CRC_ERR=0, MEM_ERR=0
```

This shows MCUViewer is successfully communicating with the simulator!

## What the Simulator Does

The standalone simulator automatically updates variables to simulate a real device:

- **counter**: Increments every 100ms
- **temperature**: Simulates room temperature (25°C ± noise)
- **voltage**: Simulates 3.3V rail with small variations
- **current**: Sine wave around 1.5A (simulated load)
- **status**: Slowly incrementing status byte
- **timestamp**: Real-time millisecond counter
- **posX/posY**: Circular motion pattern (radius=1000)
- **sineWave**: Pure sine wave at 1Hz

## Troubleshooting

### "No UART ports found" or UART not available

**Cause**: UART support might not be compiled in, or ports not detected.

**Solution**:
- Verify `/dev/pts/5` exists (from socat output)
- Try refreshing the port list in MCUViewer
- Check socat is still running

### "Failed to connect" or "Connection timeout"

**Cause**:
- Wrong port selected
- Simulator not running
- Port already in use

**Solution**:
1. Verify simulator is running and shows correct port
2. Make sure you're using the *other* port in MCUViewer
3. Check no other program is using the ports

### Variables show "NOT FOUND!"

**Cause**: This is normal - variables are manually added, not from ELF file.

**Solution**: The addresses should work as-is. If you still see "NOT FOUND!", make sure you've typed the addresses correctly with the `0x` prefix.

### No data updating / values frozen

**Cause**:
- Acquisition not started
- Communication error
- Wrong baud rate

**Solution**:
1. Click **STOPPED** to restart acquisition
2. Verify baud rate is 115200 on both sides
3. Check simulator terminal for error messages

### Simulator shows "CRC_ERR" or "MEM_ERR"

**Cause**: Communication errors (shouldn't happen with virtual ports)

**Solution**:
- Restart both simulator and MCUViewer
- Recreate virtual port pair
- Check socat parameters

## Expected Behavior

### ✅ Success Indicators:

1. **Simulator terminal**: Shows "Waiting for MCUViewer connection..." then statistics updates
2. **MCUViewer**:
   - Status shows **RUNNING** (green)
   - Variable values update in real-time
   - Plots show smooth curves
   - No error messages in status bar
3. **Performance**:
   - Update rate: ~10Hz (MCUViewer's default sampling rate)
   - No lag or freezing
   - Smooth plot rendering

### ❌ Common Issues:

- **Simulator**: "Failed to start simulator on port..."
  → Check port path is correct
- **MCUViewer**: Red status indicator
  → Check connection settings
- **Statistics**: High CRC_ERR count
  → Indicates communication problems

## Stopping Everything

1. In MCUViewer: Click **RUNNING** button to stop acquisition
2. Close MCUViewer GUI
3. In simulator terminal: Press **Ctrl+C**
4. In socat terminal: Press **Ctrl+C**

## Next Steps

Once you've verified UART works with the simulator:

1. Test with different baud rates (230400, 460800, 921600)
2. Test disconnecting/reconnecting (stop/start acquisition)
3. Measure performance with more variables
4. Create an actual ELF file with these variables for testing
5. Eventually integrate with real hardware

## Creating a Test ELF File (Optional)

To test with a real ELF file:

1. Create a simple C project with these variables:
```c
uint32_t counter = 0;
float temperature = 25.5f;
float voltage = 3.3f;
float current = 1.5f;
uint8_t status = 0;
uint32_t errorCode = 0;
uint64_t timestamp = 0;
int32_t posX = 0;
int32_t posY = 0;
float sineWave = 0.0f;
```

2. Compile with debug symbols: `gcc -g ...`
3. Use the generated `.elf` file in MCUViewer
4. Variables will auto-populate with correct addresses

## Tips

- **Logging**: Run simulator with `-v` flag for verbose output
- **Multiple Tests**: You can stop/start acquisition without restarting simulator
- **Plotting**: Try 2D plots (posX vs posY) for interesting visualizations
- **Performance**: Monitor simulator statistics to see communication health

## Summary

This setup provides a complete end-to-end test of the UART debug probe without any hardware:

```
┌─────────────┐      ┌──────────────┐      ┌─────────────┐
│  MCUViewer  │◄────►│ Virtual Port │◄────►│  Simulator  │
│     GUI     │      │   (socat)    │      │  (Virtual   │
│             │      │              │      │   Device)   │
└─────────────┘      └──────────────┘      └─────────────┘
    /dev/pts/5           pty pair              /dev/pts/6
```

This validates that Task 1.6 (Integration Test) works not just programmatically, but also through the actual user-facing GUI!
