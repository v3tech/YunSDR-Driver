# yunsdr_ss_util 使用指南

[English](README.md)

`yunsdr_ss_util` 用来列出 YunSDR 设备、查看 GPS 和射频通道状态，也可以在指定 TX 端口发射测试单音。使用 PCIe 板卡前，请先[安装并加载 XDMA 驱动](../../XDMA-Driver/README.zh-CN.md)。

## 列出设备

从仓库根目录开始，按 CPU 架构选择下面一组命令。工作目录里有对应的 `libyunsdr_ss.so`，工具可以直接加载它。后续示例在同一终端执行。

**x86_64**

```sh
cd SDK/x86_64
util=../tools/yunsdr_ss_util
```

**ARM64（包括 NVIDIA Jetson）**

```sh
cd SDK/arm64
util=../tools/yunsdr_ss_util_arm64
```

```sh
"$util" -h
sudo "$util" -l
```

从 `-l` 的输出中找到板卡编号。下面以 PCIe bypass 设备 0 为例，读取 GPS 状态和通道事件计数：

```sh
sudo "$util" -s 0 -c gps_state
sudo "$util" -s 0 -c chan_evt
```

## 单音发射

下面的命令从 TX1 发射，中心频率为 3.6 GHz，采样率为 61.44 MS/s，单音偏移为 1.9 MHz，发射增益为 70 dB。频率和偏移按 Hz 输入，采样率按每秒采样数输入。按 **Ctrl+C** 结束测试。

```sh
sudo "$util" -s 0 -F 3600000000 -G 70 -R 61440000 -M 0x1 -o 1900000 -c tx_test
```

`-M` 选择发射端口。仅用 TX1 时取 `0x1`，仅用 TX2 时取 `0x2`，同时使用 TX1 和 TX2 时取 `0x3`。将 `-o` 设为 `0` 可在中心频率上发射。下面是 TX1 的零偏移（基带 DC）示例：

```sh
sudo "$util" -s 0 -F 3600000000 -G 90 -R 61440000 -M 0x1 -o 0 -c tx_test
```

## Split 设备

一块板卡包含两个 SDR 单元时，`-s` 使用 `板卡编号.单元编号`。例如，查看第一块板卡上第二个单元的 GPS 状态：

```sh
sudo "$util" -s 0.1 -c gps_state
```

| PCIe 板卡编号 | 第一个 SDR 单元 | 第二个 SDR 单元 |
| --- | --- | --- |
| 0 | `-s 0.0` (sdr0) | `-s 0.1` (sdr1) |
| 1 | `-s 1.0` (sdr2) | `-s 1.1` (sdr3) |
| 2 | `-s 2.0` (sdr4) | `-s 2.1` (sdr5) |

## 参数

| 参数 | 用途 | 默认值 |
| --- | --- | --- |
| `-h` | 显示当前程序的帮助 | — |
| `-l` | 列出设备并检查驱动状态 | — |
| `-s board[.unit]` | 选择 PCIe bypass 设备或 Split 逻辑单元 | — |
| `-d device` | 选择 PCIe 设备 | — |
| `-F Hz` | 收发射频中心频率 | 2400 MHz |
| `-R samples/s` | 收发采样率 | 30.72 MS/s |
| `-g dB` | 接收增益 | 40 dB |
| `-G dB` | 发射增益 | 80 dB |
| `-m mask` | 接收通道掩码 | `0x1` |
| `-M mask` | 发射通道掩码 | `0x1` |
| `-o Hz` | 单音相对中心频率的偏移 | 1 MHz |
| `-t mV` | 以毫伏为单位调节 TCXO 控制电压，微调频率 | — |
| `-c command` | 执行指定命令 | — |

## `-c` 命令

| 命令 | 作用 |
| --- | --- |
| `gps_state` | 查看 GPS 状态 |
| `gps_time` | 用 GPS 时间和硬件时间戳计算绝对时间 |
| `sync_gps` | 使用 GPS 微调振荡器频率 |
| `get_auxdac1` | 读取 AUX DAC 1 的当前值 |
| `chan_evt` | 查看射频通道事件计数 |
| `led` | 暂停 LED 闪烁 5 秒，方便识别板卡 |
| `reset` | 执行软复位 |
| `partial_reset` | 执行局部复位 |
| `upgrade_firmware` | 升级设备固件 |
| `tx_test` | 在选定的 TX 通道发射单音 |

固件升级参数以当前程序的 `-h` 输出为准。如果帮助中列出 `-f firmware_package`，用它选择升级所需的 `.fw` 文件；`-p raw_firmware` 用于配合 `raw_firmware.json` 打包原始固件。
