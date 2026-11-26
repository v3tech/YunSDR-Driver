# YunSDR XDMA DKMS 软件包

版本为 2020.2.2.yunsdr.20261126-1。DEB 的 all 和 RPM 的 noarch 表示包内为驱动源码。目标机上的 DKMS 使用该机的工具链和内核构建目录生成 xdma.ko，并通过 AUTOINSTALL 随内核更新重建。

## 安装和卸载

从 GitHub 下载 DEB 时，在文件页面点击 **Download raw file**，也可使用克隆所得的仓库文件。在 DEB 所在目录执行下列命令（克隆后为 `XDMA-Driver/`）。先确认校验结果为 `OK`，包架构为 `all`。

```sh
deb=xdma-dkms_2020.2.2.yunsdr.20261126-1_all.deb
echo "67693f53a00a6a9a36a62bdbe9a9876ad2ca246dd3cf29473aa84f285eea237f  $deb" | sha256sum -c - &&
  dpkg-deb -f "$deb" Package Version Architecture
```

若 APT 报 `Invalid archive signature` 或提示找不到 `control.tar`，执行 `file "$deb"`。若结果为 HTML，重新下载原始文件并校验。

### NVIDIA Jetson（Tegra 内核）

使用与**当前运行的 L4T 内核**匹配的头文件。实测 Jetson AGX Orin（L4T R39.2.2）由 NVIDIA 的 `nvidia-l4t-kernel-headers` 提供头文件。在这台设备上，通用 Ubuntu 命令会请求 `linux-headers-6.8.12-1029-tegra`，APT 会报找不到包。

```sh
(
  set -eu
  kernel=$(uname -r)
  build="/lib/modules/$kernel/build"
  if [ ! -r "$build/Makefile" ] || [ ! -s "$build/Module.symvers" ] ||
     [ "$(cat "$build/include/config/kernel.release" 2>/dev/null || true)" != "$kernel" ]; then
    bsp=$(dpkg-query -W -f='${Version}' nvidia-l4t-kernel)
    sudo apt install "nvidia-l4t-kernel-headers=$bsp"
  fi
  test -r "$build/Makefile" && test -s "$build/Module.symvers" &&
    test "$(cat "$build/include/config/kernel.release")" = "$kernel" ||
    { echo "Missing or mismatched kernel build tree: $build" >&2; exit 1; }
  sudo apt install dkms build-essential ./xdma-dkms_2020.2.2.yunsdr.20261126-1_all.deb
)
```

若 APT 找不到指定版本的 NVIDIA 头文件，核对 NVIDIA 软件源是否对应已安装的 L4T 版本。自定义 BSP 内核请向 BSP 提供方获取匹配的内核构建目录，准备好后重新执行上面的命令。启用模块签名强制验证的设备，请先按 BSP 流程完成签名和证书信任配置，再加载驱动。

```sh
dkms status -m xdma -k "$(uname -r)"
modinfo -n xdma
sudo modprobe xdma
lspci -nnk -d 10ee:
ls -l /dev/xdma*_h2c_* /dev/xdma*_c2h_*
```

DKMS 对运行内核应显示 `installed`。已接入受支持的板卡时，PCI 设备应显示 `Kernel driver in use: xdma`，并出现 H2C、C2H 节点。若 DKMS 编译失败，可用 `sudo find /var/lib/dkms/xdma -name make.log -print` 定位构建日志。

### Debian / Ubuntu（发行版内核）

```sh
sudo apt install dkms build-essential "linux-headers-$(uname -r)"
sudo apt install ./xdma-dkms_2020.2.2.yunsdr.20261126-1_all.deb
```

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

在 203 的维护者打包目录中，运行 ./build.sh 可从相邻 ../linux-kernel 重新制作两个包，需要 dpkg-deb 和 rpmbuild。驱动版本号基于 version.h，另加 YunSDR 快照日期。兼容修改已合入源文件 xdma/xdma_iova_ring.c，打包脚本直接复制源码。IOVA mmap 沿用同目录 cdev_ctrl.c 的新旧内核 API 选择，保留 VM_DONTEXPAND 和 VM_DONTDUMP 标志。vm-flags-compat.patch 留作变更记录。

实机验证中，Ubuntu 22.04.4 x86_64（6.8.0-111-generic）从 DEB 安装并绑定 10ee:903f，生成两对 H2C/C2H 通道。Jetson AGX Orin（Ubuntu 24.04.5、L4T R39.2.2、6.8.12-1029-tegra）从同一 DEB 原生构建并加载模块，绑定 10ee:9048，生成一对 H2C/C2H 通道。该 Jetson 的 UEFI Secure Boot 已关闭，DKMS 因缺少签名辅助工具而跳过签名。

维护者打包目录中的 verification.log 记录两个包解出的源码比较、x86_64 Ubuntu 6.8.0-138-generic 原生构建、AArch64 Jetson 5.15.148-tegra 交叉构建，以及两份模块用临时测试证书完成 CMS 签名校验、隔离安装和卸载。package-test.log 记录 x86_64 Ubuntu chroot 内的 DEB 和 RPM 包管理器安装、卸载，RPM 在独立数据库中跳过依赖解析。chroot 中 /sys/devices 不可用，日志中的 modprobe 输出不用于硬件验证。未验证 Fedora/RHEL 原生安装、Secure Boot 开启后的真实加载或板卡 DMA。
