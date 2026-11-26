# YunSDR XDMA 驱动安装

[English](README.md)

Ubuntu、Debian 和 NVIDIA Jetson 使用 DEB 包；Fedora 和 RHEL 系发行版使用 RPM 包。两种包都支持 x86_64 和 ARM64，安装时由 DKMS 为当前内核编译驱动。

- DEB：[xdma-dkms_2020.2.2.yunsdr.20261126-1_all.deb](./xdma-dkms_2020.2.2.yunsdr.20261126-1_all.deb?raw=true)
- RPM：[xdma-dkms-2020.2.2.yunsdr.20261126-1.noarch.rpm](./xdma-dkms-2020.2.2.yunsdr.20261126-1.noarch.rpm?raw=true)

如果在 GitHub 文件页面下载，点击 **Download raw file**。在下载目录打开终端，按系统选择下面的一组命令。如果 APT 报 `Invalid archive signature`，用 `file ./xdma-dkms_2020.2.2.yunsdr.20261126-1_all.deb` 查看文件类型。若显示 HTML，说明保存的是网页，需要重新下载原始文件。

## NVIDIA Jetson

使用 Jetson Linux（L4T）时，先检查当前内核的头文件：

```sh
uname -r
cat "/lib/modules/$(uname -r)/build/include/config/kernel.release"
```

两行版本一致就可以直接安装驱动。如果第二条命令报文件不存在，或版本不同，在使用 NVIDIA 官方内核的 Jetson 上安装对应版本的头文件：

```sh
bsp=$(dpkg-query -W -f='${Version}' nvidia-l4t-kernel)
sudo apt install "nvidia-l4t-kernel-headers=$bsp"
```

再次核对两行版本，确认一致后安装驱动。若 APT 找不到指定版本，请检查 NVIDIA 软件源是否与当前 L4T 版本一致。使用自定义内核时，请使用板卡供应商提供的配套头文件。

```sh
sudo apt install dkms build-essential ./xdma-dkms_2020.2.2.yunsdr.20261126-1_all.deb
```

## Ubuntu / Debian（通用内核）

```sh
sudo apt install dkms build-essential "linux-headers-$(uname -r)" ./xdma-dkms_2020.2.2.yunsdr.20261126-1_all.deb
```

## Fedora / RHEL

先启用提供 `dkms` 的软件源，然后安装：

```sh
sudo dnf install dkms gcc make "kernel-devel-$(uname -r)" ./xdma-dkms-2020.2.2.yunsdr.20261126-1.noarch.rpm
```

## 启用 Secure Boot 的系统

如果系统要求内核模块签名，加载驱动前先确认模块已签名，且签名证书已受内核信任。在采用 shim/MOK 的系统上，可用 `mokutil` 查看 Secure Boot 状态，用 `modinfo` 查看模块签名者：

```sh
mokutil --sb-state
modinfo -F signer xdma
```

如果签名者为空，请先配置发行版的 DKMS 模块签名并重新安装驱动。如果模块已签名，但证书尚未登记，请找到 DKMS 安装日志中列出的公钥证书，执行 `sudo mokutil --import /path/to/certificate.der`（替换为实际路径），设置一次性口令。重启后在 MokManager 中选择 **Enroll MOK** 并确认。使用厂商信任链的 Jetson 请按对应 BSP 的模块签名和证书部署说明操作。

## 加载驱动

```sh
dkms status -m xdma -k "$(uname -r)"
sudo modprobe xdma
lspci -nnk -d 10ee:
ls -l /dev/xdma*
```

DKMS 应显示当前内核的状态为 `installed`。连接板卡后，在对应 PCI 设备下可看到 `Kernel driver in use: xdma`。设备节点包括 `/dev/xdma0_control`，以及板卡支持的 H2C、C2H 通道。

## 卸载

Ubuntu、Debian 和 Jetson 使用 `sudo apt remove xdma-dkms`。Fedora 和 RHEL 使用 `sudo dnf remove xdma-dkms`。
