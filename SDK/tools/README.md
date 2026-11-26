# Using yunsdr_ss_util

[中文](README.zh-CN.md)

Use `yunsdr_ss_util` to list YunSDR devices, inspect GPS and RF channel status, or transmit a test tone on a selected TX port. For PCIe boards, [install and load the XDMA driver](../../XDMA-Driver/README.md) first.

## List devices

Start in the repository root and choose the commands for your CPU. Each directory contains the matching `libyunsdr_ss.so`, which the tool can load from the working directory. Run the remaining examples in the same shell.

**x86_64**

```sh
cd SDK/x86_64
util=../tools/yunsdr_ss_util
```

**ARM64 (including NVIDIA Jetson)**

```sh
cd SDK/arm64
util=../tools/yunsdr_ss_util_arm64
```

```sh
"$util" -h
sudo "$util" -l
```

Find your board number in the output of `-l`. These examples read GPS status and RF channel event counters from PCIe bypass device 0:

```sh
sudo "$util" -s 0 -c gps_state
sudo "$util" -s 0 -c chan_evt
```

## Transmit a test tone

This example transmits on TX1 with a 3.6 GHz RF center frequency, a 61.44 MS/s sample rate, a 1.9 MHz tone offset and 70 dB TX gain. Enter frequencies and offsets in Hz, and sample rates in samples per second. Press **Ctrl+C** to stop.

```sh
sudo "$util" -s 0 -F 3600000000 -G 70 -R 61440000 -M 0x1 -o 1900000 -c tx_test
```

Set `-M` to `0x1` for TX1, `0x2` for TX2 or `0x3` for both. An offset of zero transmits at the RF center frequency. For example, to transmit baseband DC on TX1:

```sh
sudo "$util" -s 0 -F 3600000000 -G 90 -R 61440000 -M 0x1 -o 0 -c tx_test
```

## Split devices

For a board with two SDR units, pass `board.unit` to `-s`. To read GPS status from the second unit on the first board, run:

```sh
sudo "$util" -s 0.1 -c gps_state
```

| PCIe board | First SDR unit | Second SDR unit |
| --- | --- | --- |
| 0 | `-s 0.0` (sdr0) | `-s 0.1` (sdr1) |
| 1 | `-s 1.0` (sdr2) | `-s 1.1` (sdr3) |
| 2 | `-s 2.0` (sdr4) | `-s 2.1` (sdr5) |

## Options

| Option | Purpose | Default |
| --- | --- | --- |
| `-h` | Show help for this binary | — |
| `-l` | List devices and check driver status | — |
| `-s board[.unit]` | Select a PCIe bypass device or a split unit | — |
| `-d device` | Select a PCIe device | — |
| `-F Hz` | TX/RX RF center frequency | 2400 MHz |
| `-R samples/s` | TX/RX sample rate | 30.72 MS/s |
| `-g dB` | RX gain | 40 dB |
| `-G dB` | TX gain | 80 dB |
| `-m mask` | RX channel mask | `0x1` |
| `-M mask` | TX channel mask | `0x1` |
| `-o Hz` | Tone offset from the RF center | 1 MHz |
| `-t mV` | Fine-tune the TCXO using a control voltage in millivolts | — |
| `-c command` | Run the named command | — |

## Commands for `-c`

| Command | Action |
| --- | --- |
| `gps_state` | Show GPS status |
| `gps_time` | Calculate absolute time from GPS time and the hardware timestamp |
| `sync_gps` | Fine-tune the oscillator frequency using GPS |
| `get_auxdac1` | Read the current AUX DAC 1 value |
| `chan_evt` | Show RF channel event counters |
| `led` | Pause LED blinking for five seconds to identify a board |
| `reset` | Perform a soft reset |
| `partial_reset` | Perform a partial reset |
| `upgrade_firmware` | Upgrade device firmware |
| `tx_test` | Transmit a tone on the selected TX channels |

Check your binary's `-h` output for firmware options. If it lists `-f firmware_package`, use that option to select a `.fw` file for an upgrade. The `-p raw_firmware` option packages raw firmware using `raw_firmware.json`.
