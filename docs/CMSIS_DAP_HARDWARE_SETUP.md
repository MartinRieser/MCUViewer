# CMSIS-DAP Hardware Setup Guide

## Quick Start: Raspberry Pi Pico/Pico 2 as Debug Probe

### What You Need
- **Raspberry Pi Pico** (RP2040, ~$4) or **Raspberry Pi Pico 2** (RP2350, ~$5)
- **Target board** (e.g., STM32 Black Pill)
- **USB cable** (USB-A to micro-USB for Pico)
- **Jumper wires** (4 wires minimum: SWCLK, SWDIO, GND, optional SWO)

---

## Step 1: Download DapperMime Firmware

### Pre-built Binaries (Recommended)

**Download Location**:
- https://github.com/majbthrd/DapperMime/releases

**Which file to download**:
- For **Raspberry Pi Pico** (RP2040): `DapperMime-rp2040.uf2`
- For **Raspberry Pi Pico 2** (RP2350): `DapperMime-rp2350.uf2`

### Alternative: Picoprobe Firmware

**Download Location**:
- https://github.com/raspberrypi/picoprobe/releases

**Note**: DapperMime is preferred over Picoprobe for better performance:
- **DapperMime**: CMSIS-DAP v2 (~10 MB/s transfer speed)
- **Picoprobe**: CMSIS-DAP v1 (~1 MB/s transfer speed)

---

## Step 2: Flash Firmware to Pico

1. **Disconnect** the Pico from USB (if connected)
2. **Hold down the BOOTSEL button** on the Pico
3. **While holding BOOTSEL**, connect the Pico to your computer via USB
4. The Pico will appear as a **USB mass storage device** (like a USB drive)
5. **Drag and drop** the `.uf2` file onto the Pico drive
6. The Pico will automatically **reboot** as a CMSIS-DAP debug probe
7. The USB drive will disappear - this is normal!

**Verification**:
- On **Linux**: Run `lsusb` - you should see "CMSIS-DAP" device
- On **Windows**: Check Device Manager - should appear under "Universal Serial Bus devices"
- On **macOS**: System Information → USB - should show CMSIS-DAP device

---

## Step 3: Wire Pico to Target Board

### DapperMime Default Pinout

Connect Pico GPIO pins to your target's SWD interface:

| Pico Pin | Function | Target Pin | Description |
|----------|----------|------------|-------------|
| GP2      | SWCLK    | SWCLK      | SWD Clock (required) |
| GP3      | SWDIO    | SWDIO      | SWD Data (required) |
| GND      | GND      | GND        | Ground (required) |
| GP4      | SWO      | SWO        | Trace output (optional, for SWO trace) |
| GP5      | nRESET   | nRST       | Target reset (optional) |

### Example: Wiring to STM32 Black Pill

Black Pill SWD header (typical 4-pin or 6-pin):

| Black Pill Pin | Pico Pin | Wire Color (suggested) |
|----------------|----------|------------------------|
| SWCLK          | GP2      | Yellow |
| SWDIO          | GP3      | Green |
| GND            | GND      | Black |
| SWO (optional) | GP4      | Blue |

**Important Notes**:
- **Do NOT connect VCC** from Pico to target unless you want Pico to power your target
- Most development boards have their own power supply - only connect signal pins
- Keep wires short (<15 cm) for reliable SWD communication

---

## Step 4: Test Connection

### Using OpenOCD (Quick Test)

```bash
# Install OpenOCD (if not already installed)
# Linux: sudo apt install openocd
# macOS: brew install openocd
# Windows: Download from https://openocd.org

# Test connection to STM32F4 target
openocd -f interface/cmsis-dap.cfg -f target/stm32f4x.cfg
```

**Expected output**:
```
Info : CMSIS-DAP: SWD supported
Info : CMSIS-DAP: Interface ready
Info : clock speed 2000 kHz
Info : SWD DPIDR 0x2ba01477
Info : [stm32f4x.cpu] Cortex-M4 r0p1 processor detected
Info : [stm32f4x.cpu] target has 6 breakpoints, 4 watchpoints
```

### Using MCUViewer (Once CMSIS-DAP Support is Added)

1. Launch MCUViewer
2. Go to **Acquisition Settings**
3. Select **CMSIS-DAP** from probe dropdown
4. Your Pico should appear in the device list
5. Click **Start** to connect

---

## Troubleshooting

### Pico Not Appearing as CMSIS-DAP Device

**Symptom**: After flashing, Pico doesn't show up as CMSIS-DAP device

**Solutions**:
1. Try a different USB cable (some cables are charge-only)
2. Re-flash the firmware (hold BOOTSEL, reconnect, drag `.uf2` again)
3. Check if you downloaded the correct `.uf2` for your board (RP2040 vs RP2350)
4. On Linux: Check `dmesg | tail` for USB errors

### Target Not Detected

**Symptom**: OpenOCD or MCUViewer reports "Target not found"

**Solutions**:
1. Verify wiring (especially SWCLK, SWDIO, GND)
2. Check that target board is powered on
3. Try slower SWD clock speed (default is usually 2 MHz, try 100-500 kHz)
4. Ensure target is not in deep sleep or low-power mode
5. Check continuity with multimeter

### Linux USB Permission Denied

**Symptom**: `libusb: error [op_open] libusb requires write access to USB device nodes`

**Solution**: Add udev rule for CMSIS-DAP devices

```bash
# Create udev rule
sudo nano /etc/udev/rules.d/99-cmsis-dap.rules

# Add this line (replace 0xc251 with your VID if different):
SUBSYSTEM=="usb", ATTR{idVendor}=="0xc251", MODE="0666"

# Reload udev rules
sudo udevadm control --reload-rules
sudo udevadm trigger

# Reconnect Pico
```

### SWO Not Working

**Symptom**: Variable viewer works, but SWO trace shows no data

**Possible Causes**:
1. GP4 not connected to target SWO pin
2. Target firmware not configured for SWO output
3. SWO clock frequency mismatch
4. Some CMSIS-DAP implementations don't support SWO (check with `DAP_Info`)

**Note**: SWO support varies by CMSIS-DAP firmware - DapperMime has good SWO support

---

## Alternative CMSIS-DAP Hardware

### DAPLink Probes (~$10-15)

**Where to Buy**:
- AliExpress: Search "CMSIS-DAP debugger" or "DAPLink"
- Amazon: Search "CMSIS-DAP v2" or "ARM debugger"

**Advantages**:
- Purpose-built hardware with proper connectors
- Often includes SWO support
- More robust than DIY Pico solution

**Disadvantages**:
- More expensive than Pico
- Quality varies by manufacturer

### DIY: Another Black Pill as Probe

**Concept**: Flash DAPLink firmware onto a spare STM32F4 Black Pill

**Resources**:
- Search GitHub for "blackpill daplink" or "stm32f4 daplink"
- Community ports available but less mature than Pico firmware

---

## SWD vs JTAG

MCUViewer CMSIS-DAP integration focuses on **SWD** (Serial Wire Debug):

| Feature | SWD | JTAG |
|---------|-----|------|
| Pins required | 2 (SWCLK, SWDIO) + GND | 4 (TCK, TMS, TDI, TDO) + GND |
| Speed | Fast | Slightly slower |
| Supported by CMSIS-DAP | Yes | Yes (but not in MCUViewer Phase 1) |
| Used on ARM Cortex-M | Standard | Legacy |

**For MCUViewer**: SWD is sufficient for all variable viewing and trace functionality.

---

## Power Considerations

### Powering Target from Pico

**Pico can provide 3.3V** to target via VBUS or 3V3(OUT) pin:

| Pico Pin | Max Current | Use Case |
|----------|-------------|----------|
| VBUS (USB 5V) | 500 mA (USB limit) | Power hungry targets |
| 3V3(OUT) | 300 mA | Small targets (STM32F4 typical: 50-100 mA) |

**Recommendation**: Use external power for target board to avoid USB current limits and voltage drops.

---

## Reference: Pico Pinout Diagram

```
                     Raspberry Pi Pico
                    ┌─────────────────┐
                    │  ●           ●  │ USB
                    │                 │
            GP0  ─1 │                 │ 40─ VBUS (5V)
            GP1  ─2 │                 │ 39─ VSYS
            GND  ─3 │                 │ 38─ GND
    [SWCLK] GP2  ─4 │                 │ 37─ 3V3_EN
    [SWDIO] GP3  ─5 │                 │ 36─ 3V3(OUT)
     [SWO] GP4  ─6 │                 │ 35─ ADC_VREF
   [nRESET] GP5  ─7 │                 │ 34─ GP28 (ADC2)
                 ⋮  │                 │  ⋮
                    │   [BOOTSEL]     │
                    │     ●           │
                    └─────────────────┘
```

**Key Pins for CMSIS-DAP (DapperMime default)**:
- **GP2** → SWCLK
- **GP3** → SWDIO
- **GP4** → SWO (optional)
- **GP5** → nRESET (optional)
- **GND** → GND (required)

---

## Next Steps

Once your Pico is set up as a CMSIS-DAP probe:

1. **Test with OpenOCD** to verify hardware connection
2. **Wait for MCUViewer CMSIS-DAP integration** (see `CMSIS_DAP_INTEGRATION_PLAN.md`)
3. **Prepare test firmware** with known variables for testing
4. **Flash test firmware** to your target (Black Pill)
5. **Launch MCUViewer** and start debugging!

---

## Additional Resources

- **DapperMime GitHub**: https://github.com/majbthrd/DapperMime
- **Picoprobe GitHub**: https://github.com/raspberrypi/picoprobe
- **CMSIS-DAP Specification**: https://arm-software.github.io/CMSIS_5/DAP/html/index.html
- **OpenOCD Documentation**: https://openocd.org/doc/html/index.html
- **Raspberry Pi Pico Datasheet**: https://datasheets.raspberrypi.com/pico/pico-datasheet.pdf

---

**Document Version**: 1.0
**Last Updated**: 2025-09-30