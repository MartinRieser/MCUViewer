# UART Integration Tests - Running Instructions

This document provides instructions for running the UART integration tests (Task 1.6).

## Overview

The integration tests verify the complete UART debug probe workflow:
- **Task 1.6.1-1.6.2**: Reading 10 variables with different data types
- **Task 1.6.3**: Writing and verifying 10 variables
- **Task 1.6.4**: Error handling (timeouts, invalid sizes, invalid addresses)
- **Task 1.6.5**: Reconnection after disconnect
- **Task 1.6.6**: Performance measurements (reads/second, latency)

## Prerequisites

### 1. Build the Test Suite

```bash
cd /Users/martinrieser/Documents/MCUViewer
rm -rf build
mkdir build
cd build
cmake .. -DMAKE_TESTS=1
make -j8 MCUViewer_test
```

### 2. Create Virtual Serial Port Pair

The integration tests require a virtual serial port pair to connect the UART simulator and probe.

#### Linux/macOS:
```bash
# Install socat if not already installed
# macOS: brew install socat
# Linux: sudo apt-get install socat

# Create virtual port pair
socat -d -d pty,raw,echo=0 pty,raw,echo=0
```

This will output something like:
```
2025/10/01 12:00:00 socat[12345] N PTY is /dev/pts/5
2025/10/01 12:00:00 socat[12345] N PTY is /dev/pts/6
```

**Important:** Keep this terminal window open while running tests!

#### Windows:
Use `com0com` or similar virtual serial port tools to create a COM port pair (e.g., COM5 and COM6).

### 3. Update Test Port Configuration

Edit `test/Uart/UartIntegrationTest.cpp` and update the port paths at the top:

```cpp
#ifndef SIMULATOR_PORT
#define SIMULATOR_PORT "/dev/pts/6"  // Update with your second PTY
#endif

#ifndef PROBE_PORT
#define PROBE_PORT "/dev/pts/5"      // Update with your first PTY
#endif
```

Or define them when compiling:
```bash
g++ ... -DSIMULATOR_PORT='"/dev/pts/6"' -DPROBE_PORT='"/dev/pts/5"'
```

## Running the Tests

All integration tests are disabled by default (prefixed with `DISABLED_`). This is because they require the virtual serial port setup.

### Run All Integration Tests

```bash
cd build/test
./MCUViewer_test --gtest_filter=UartIntegrationTest.DISABLED_*
```

### Run Individual Tests

```bash
# Task 1.6.2: Read all 10 variables
./MCUViewer_test --gtest_filter=UartIntegrationTest.DISABLED_ReadAllVariables

# Task 1.6.3: Write all 10 variables
./MCUViewer_test --gtest_filter=UartIntegrationTest.DISABLED_WriteAllVariables

# Task 1.6.4: Error cases
./MCUViewer_test --gtest_filter=UartIntegrationTest.DISABLED_ErrorCases

# Task 1.6.5: Reconnection
./MCUViewer_test --gtest_filter=UartIntegrationTest.DISABLED_Reconnection

# Task 1.6.6: Performance
./MCUViewer_test --gtest_filter=UartIntegrationTest.DISABLED_Performance

# All tasks combined
./MCUViewer_test --gtest_filter=UartIntegrationTest.DISABLED_FullIntegration
```

## Expected Results

### Task 1.6.2 - Read All Variables
```
=== Task 1.6.2: Read All Variables Test ===
Connected successfully to simulator
Var 1 (Counter): 0x00000001
Var 2 (Temperature): 25.50°C
Var 3 (Pressure): 1013 hPa
Var 4 (Status): 0xAB
Var 5 (Voltage): 3.30V
Var 6 (Current): 1.50A
Var 7 (Timestamp): 0x123456789ABCDEF0
Var 8 (PosX): -12345
Var 9 (PosY): 67890
Var 10 (ErrorCode): 0x00000000
All 10 variables read successfully!
[       OK ] UartIntegrationTest.DISABLED_ReadAllVariables
```

### Task 1.6.3 - Write All Variables
```
=== Task 1.6.3: Write All Variables Test ===
Connected successfully to simulator
Var 1: Wrote 0x99999999, read back 0x99999999
Var 2: Wrote -10.25°C, read back -10.25°C
...
All 10 variables written and verified successfully!
[       OK ] UartIntegrationTest.DISABLED_WriteAllVariables
```

### Task 1.6.4 - Error Cases
```
=== Task 1.6.4: Error Cases Test ===
Test 1: Reading from invalid address...
Test 2: Reading with zero size...
  Correctly rejected zero-size read
Test 3: Reading with oversized buffer...
  Correctly rejected oversized read
...
Error cases tested successfully!
[       OK ] UartIntegrationTest.DISABLED_ErrorCases
```

### Task 1.6.5 - Reconnection
```
=== Task 1.6.5: Reconnection Test ===
Initial connection...
Reading variables before disconnect...
  Read value: 0x00000001
Disconnecting...
  Correctly failed to read while disconnected
Reconnecting...
Reading variables after reconnect...
  Read value: 0x00000001
Testing multiple reconnection cycles...
  Cycle 1/3
  Cycle 2/3
  Cycle 3/3
Reconnection test completed successfully!
[       OK ] UartIntegrationTest.DISABLED_Reconnection
```

### Task 1.6.6 - Performance
```
=== Task 1.6.6: Performance Test ===
Connected successfully to simulator

Test 1: 1000 reads of 4-byte variable...
  Duration: 2543ms
  Success: 1000/1000 (100.0%)
  Failures: 0
  Average latency: 2.54ms per read
  Throughput: 393.2 reads/second

Test 2: 100 cycles of reading all 10 variables...
  Duration: 2521ms
  Success: 1000/1000 (100.0%)
  Failures: 0
  Average latency: 2.52ms per read
  Throughput: 396.7 reads/second

Test 3: 100 write operations...
  Duration: 256ms
  Success: 100/100
  Average latency: 2.56ms per write
  Throughput: 390.6 writes/second

Simulator final statistics:
  Total packets RX: 2200
  Total packets TX: 2200
  CRC errors: 0
  Invalid commands: 0
  Memory errors: 0

Performance test completed successfully!
[       OK ] UartIntegrationTest.DISABLED_Performance
```

## Acceptance Criteria

For Task 1.6 to be considered complete:

✅ **100% success rate** over 1000 reads (no failures)
✅ **<10ms latency** per read at 115200 baud
✅ **Error handling verified** (invalid size, timeout)
✅ **Reconnection works** (multiple disconnect/reconnect cycles)
✅ **All data types** read/written correctly (uint8, uint16, uint32, uint64, int32, float)

## Troubleshooting

### Issue: "Failed to start simulator" or "Failed to connect probe"

**Solution:**
- Verify socat is running and showing two PTY paths
- Make sure the port paths in the test match the socat output
- Check that no other program is using those ports
- Try recreating the virtual port pair

### Issue: Tests timeout

**Solution:**
- Increase timeout in test code
- Check baud rate is set to 115200 on both sides
- Verify no hardware flow control is enabled

### Issue: CRC errors

**Solution:**
- This usually indicates data corruption on the serial link
- For virtual ports this should not happen
- Check socat parameters match test expectations

### Issue: Permission denied on /dev/pts/*

**Solution (Linux):**
```bash
# Add your user to dialout group
sudo usermod -a -G dialout $USER
# Log out and back in
```

## Notes

- All tests are **disabled by default** to avoid failures in CI without virtual ports
- Tests can be enabled by removing the `DISABLED_` prefix from test names
- The simulator runs in a background thread during tests
- Tests clean up resources in TearDown() to prevent leaks
- Virtual serial ports provide reliable testing without hardware

## Next Steps

After completing Task 1.6, proceed to:
- **Task 2.1**: Add UART option to probe selection in GUI
- **Task 2.2**: UART-specific settings UI
- **Task 2.3**: UART probe connection test with live variable table
