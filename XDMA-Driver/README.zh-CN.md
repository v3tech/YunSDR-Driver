# YunSDR XDMA DKMS 软件包

版本为 2020.2.2.yunsdr.20261126-1。DEB 的 all 和 RPM 的 noarch 表示包内为驱动源码。目标机上的 DKMS 使用该机的工具链和内核构建目录生成 xdma.ko，并通过 AUTOINSTALL 随内核更新重建。安装前在交付目录执行 `sha256sum -c SHA256SUMS`。

## 安装和卸载

Ubuntu / Debian，在目标内核下运行：

```sh
sudo apt install dkms build-essential "linux-headers-$(uname -r)" mokutil
sudo apt install ./xdma-dkms_2020.2.2.yunsdr.20261126-1_all.deb
```

Jetson 等厂商内核应使用 BSP 提供的匹配头文件。先确认 `/lib/modules/$(uname -r)/build/Makefile` 可读。

Fedora / RHEL 系，在已启用提供 DKMS 的软件源的目标机上运行：

```sh
sudo dnf install dkms gcc make "kernel-devel-$(uname -r)" mokutil
sudo dnf install ./xdma-dkms-2020.2.2.yunsdr.20261126-1.noarch.rpm
```

查看 `dkms status -m xdma`、`modinfo -n xdma` 和 `modinfo -F vermagic xdma`。如已从其他渠道安装同名驱动，应先核对模块路径和 DKMS 版本。生产设备可在维护窗口重启，随后用 `sudo modprobe xdma` 和 `lspci -nnk` 检查驱动绑定。卸载使用 `sudo apt remove xdma-dkms` 或 `sudo dnf remove xdma-dkms`。

## 安全启动

在使用 UEFI shim/MOK 的系统，使用具备模块签名功能的 DKMS 和目标机自己的签名密钥。安装日志应显示 Sign command、Public certificate (MOK) 和 Signing module。先执行：

```sh
mokutil --sb-state
modinfo -F signer xdma
```

确认 signer 非空，再把下例中的证书路径设为 **本机 DKMS 日志中列出的公钥证书**。Ubuntu 常见路径为 /var/lib/shim-signed/mok/MOK.der，其他发行版常见 /var/lib/dkms/mok.pub。

```sh
CERT=/var/lib/shim-signed/mok/MOK.der
sudo mokutil --test-key "$CERT"
```

如果证书尚未受信任，执行 `sudo mokutil --import "$CERT"`，设置一次性口令。重启后在 MokManager 中选择 Enroll MOK 并确认。再次启动后检查 SecureBoot enabled、证书已登记以及模块实际加载：

```sh
mokutil --sb-state
sudo mokutil --test-key "$CERT"
sudo modprobe xdma
lsmod | grep '^xdma '
```

使用自定义信任链的 ARM64 板卡应按 BSP 要求把模块签名证书加入内核信任链。包内没有私钥，包文件的 SHA256 与内核模块签名分别验证。签名配置见 https://github.com/dkms-project/dkms#module-signing 。

## 构建与验证范围

203 上直接运行 ./build.sh 可从相邻 ../linux-kernel 重新制作两个包，需要 dpkg-deb 和 rpmbuild。驱动版本号基于 version.h，另加 YunSDR 快照日期。兼容修改已合入源文件 xdma/xdma_iova_ring.c，打包脚本直接复制源码。IOVA mmap 沿用同目录 cdev_ctrl.c 的新旧内核 API 选择，保留 VM_DONTEXPAND 和 VM_DONTDUMP 标志。vm-flags-compat.patch 留作变更记录。

verification.log 记录两个包解出的源码比较、x86_64 Ubuntu 6.8.0-138-generic 原生构建、AArch64 Jetson 5.15.148-tegra 交叉构建，以及两份模块用临时测试证书完成 CMS 签名校验、隔离安装和卸载。package-test.log 记录 x86_64 Ubuntu chroot 内的 DEB 和 RPM 包管理器安装、卸载，RPM 在独立数据库中跳过依赖解析。chroot 中 /sys/devices 不可用，日志中的 modprobe 输出不用于硬件验证。未验证 Fedora/RHEL 原生安装、Secure Boot 开启后的真实加载或板卡 DMA。
