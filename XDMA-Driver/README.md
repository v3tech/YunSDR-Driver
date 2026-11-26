# Install the YunSDR XDMA driver

[中文](README.zh-CN.md)

Use the DEB on Ubuntu, Debian or NVIDIA Jetson. Use the RPM on Fedora or a RHEL-based distribution. Both packages support x86_64 and ARM64. DKMS builds the driver for the running kernel during installation.

- DEB: [xdma-dkms_2020.2.2.yunsdr.20261126-1_all.deb](./xdma-dkms_2020.2.2.yunsdr.20261126-1_all.deb?raw=true)
- RPM: [xdma-dkms-2020.2.2.yunsdr.20261126-1.noarch.rpm](./xdma-dkms-2020.2.2.yunsdr.20261126-1.noarch.rpm?raw=true)

If you open the file on GitHub, click **Download raw file**. Open a terminal in the download directory and follow the instructions for your system. If APT reports `Invalid archive signature`, run `file ./xdma-dkms_2020.2.2.yunsdr.20261126-1_all.deb`. An HTML result means you saved the web page. Download the raw file and try again.

## NVIDIA Jetson

On Jetson Linux (L4T), check the kernel release recorded in the headers against the running kernel:

```sh
uname -r
cat "/lib/modules/$(uname -r)/build/include/config/kernel.release"
```

If the two versions match, skip straight to the driver installation below. If the second command reports a missing file or a different version and you use the NVIDIA kernel, install its matching headers:

```sh
bsp=$(dpkg-query -W -f='${Version}' nvidia-l4t-kernel)
sudo apt install "nvidia-l4t-kernel-headers=$bsp"
```

Check the versions again before installing the driver. If APT cannot find the requested version, check that the NVIDIA package repository matches your L4T release. For a custom kernel, use the headers supplied with your board support package (BSP).

```sh
sudo apt install dkms build-essential ./xdma-dkms_2020.2.2.yunsdr.20261126-1_all.deb
```

## Ubuntu or Debian with a distribution kernel

```sh
sudo apt install dkms build-essential "linux-headers-$(uname -r)" ./xdma-dkms_2020.2.2.yunsdr.20261126-1_all.deb
```

## Fedora or RHEL

Enable a repository that provides `dkms`, then install the package:

```sh
sudo dnf install dkms gcc make "kernel-devel-$(uname -r)" ./xdma-dkms-2020.2.2.yunsdr.20261126-1.noarch.rpm
```

## Systems with Secure Boot enabled

If your system enforces module signatures, the driver must be signed with a key the kernel trusts. On systems using shim and Machine Owner Keys (MOK), check the Secure Boot state and module signer before loading the driver:

```sh
mokutil --sb-state
modinfo -F signer xdma
```

If the signer is empty, configure DKMS module signing for your distribution and reinstall the driver. If the module is signed and the certificate has not been enrolled, use the public certificate path from the DKMS installation log. Run `sudo mokutil --import /path/to/certificate.der` with that path and set a one-time password. Reboot, select **Enroll MOK** in MokManager and confirm. For Jetson boards using a vendor trust chain, follow the module-signing and certificate deployment instructions for your BSP.

## Load the driver

```sh
dkms status -m xdma -k "$(uname -r)"
sudo modprobe xdma
lspci -nnk -d 10ee:
ls -l /dev/xdma*
```

DKMS should report `installed` for the running kernel. With the board connected, its PCI entry should show `Kernel driver in use: xdma`. Look for `/dev/xdma0_control` and the H2C and C2H channel nodes supported by your board.

## Uninstall

On Ubuntu, Debian or Jetson, run `sudo apt remove xdma-dkms`. On Fedora or RHEL, run `sudo dnf remove xdma-dkms`.
