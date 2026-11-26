# YunSDR XDMA DKMS packages

Release `2020.2.2.yunsdr.20261126-1` supports x86_64 and AArch64. The DEB (`all`) and RPM (`noarch`) contain the driver source. On the target machine, DKMS compiles `xdma.ko` against the kernel build tree. DKMS autoinstall is enabled for subsequent kernel updates.

## Installation

For the DEB, open the package in GitHub and select **Download raw file**, or use the file in a checkout of the repository. Run the following commands in the directory containing the downloaded file (`XDMA-Driver/` in a checkout). Verify the checksum before installing. The expected architecture is `all`.

```sh
deb=xdma-dkms_2020.2.2.yunsdr.20261126-1_all.deb
echo "67693f53a00a6a9a36a62bdbe9a9876ad2ca246dd3cf29473aa84f285eea237f  $deb" | sha256sum -c - &&
  dpkg-deb -f "$deb" Package Version Architecture
```

If APT reports `Invalid archive signature` or cannot find `control.tar`, run `file "$deb"`. If it reports HTML, download the raw file again and repeat the checksum check.

### NVIDIA Jetson with a Tegra kernel

Use the headers for the **running L4T kernel**. On the tested Jetson AGX Orin with L4T R39.2.2, NVIDIA supplies them in `nvidia-l4t-kernel-headers`. The generic Ubuntu command below would request `linux-headers-6.8.12-1029-tegra`, which APT cannot locate on this system.

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

If APT cannot find the exact NVIDIA package version, check that the NVIDIA APT repository matches the installed L4T release. For a custom BSP kernel, obtain its prepared kernel build tree from the BSP provider. Once the build-tree checks pass, run the block again. On systems enforcing module signatures, complete the Secure Boot steps for your BSP before loading the driver.

```sh
dkms status -m xdma -k "$(uname -r)"
modinfo -n xdma
sudo modprobe xdma
lspci -nnk -d 10ee:
ls -l /dev/xdma*_h2c_* /dev/xdma*_c2h_*
```

DKMS should report `installed` for the running kernel. With a supported board connected, look for `Kernel driver in use: xdma` and the H2C/C2H device nodes. If DKMS fails to compile, locate its build log with `sudo find /var/lib/dkms/xdma -name make.log -print`.

### Debian or Ubuntu with a distribution kernel

```sh
sudo apt install dkms build-essential "linux-headers-$(uname -r)"
sudo apt install ./xdma-dkms_2020.2.2.yunsdr.20261126-1_all.deb
```

**Fedora or RHEL-family systems.** Enable a repository that supplies DKMS, then install the development package matching the running kernel and the XDMA RPM.

```sh
sudo dnf install dkms gcc make "kernel-devel-$(uname -r)" mokutil
sudo dnf install ./xdma-dkms-2020.2.2.yunsdr.20261126-1.noarch.rpm
```

Check that DKMS reports the module as installed for the running kernel. Inspect the path and kernel release selected by the module loader.

```sh
dkms status -m xdma
modinfo -n xdma
modinfo -F vermagic xdma
```

If Secure Boot is enabled, complete the enrollment check below before loading the module. If an older driver is in use, reboot during a maintenance window. Run `sudo modprobe xdma` and inspect the XDMA PCI device's **Kernel driver in use** entry in `lspci -nnk`.

To uninstall, run `sudo apt remove xdma-dkms` on Debian or Ubuntu, or `sudo dnf remove xdma-dkms` on Fedora or RHEL-family systems.

## Secure Boot

On systems that use UEFI shim and Machine Owner Keys (MOK), configure DKMS to sign modules using the target machine's key. The installation log should show the signing command, the public certificate path and a signing entry for `xdma.ko`. See the [DKMS module signing guide](https://github.com/dkms-project/dkms#module-signing).

```sh
mokutil --sb-state
modinfo -F signer xdma
```

Confirm that the signer field is populated. Set `CERT` to the **public certificate path reported by DKMS on this machine**. Ubuntu commonly uses the path below. Another common location is `/var/lib/dkms/mok.pub`.

```sh
CERT=/var/lib/shim-signed/mok/MOK.der
sudo mokutil --test-key "$CERT"
```

If the certificate is pending enrollment or has not been enrolled, complete enrollment. To start a new request, run `sudo mokutil --import "$CERT"` and set a one-time password. Reboot, select **Enroll MOK** in MokManager and confirm with that password. After rebooting, verify the Secure Boot state, enrollment and module load.

```sh
mokutil --sb-state
sudo mokutil --test-key "$CERT"
sudo modprobe xdma
lsmod | grep '^xdma '
```

On ARM64 boards with a vendor-specific trust chain, follow the BSP procedure for adding the module-signing certificate to the kernel's trusted keys. Private signing keys stay on the target machine.

## Rebuilding the packages

In the maintainer’s packaging workspace on host 203, run `./build.sh` from the release directory. The script uses `dpkg-deb` and `rpmbuild` and reads the adjacent `../linux-kernel` source tree. The package version combines the driver version from `xdma/version.h` with the YunSDR snapshot date.

The compatibility change is part of `xdma/xdma_iova_ring.c`. The IOVA mmap code follows the kernel API selection in `cdev_ctrl.c` to set `VM_DONTEXPAND` and `VM_DONTDUMP` on both older and newer kernels. The release directory retains `vm-flags-compat.patch` as a record of the change.

## Validation

| Test | Result |
|---|---|
| x86_64, Ubuntu 6.8.0-138-generic | Native DKMS build, isolated installation and removal, and CMS signature verification passed. |
| AArch64, Jetson 5.15.148-tegra | Cross-build, isolated installation and removal, and CMS signature verification passed. |
| x86_64, Ubuntu 22.04.4, 6.8.0-111-generic | Native DEB installation and DKMS rebuild passed. XDMA bound to 10ee:903f with two H2C and two C2H channels. |
| Jetson AGX Orin, Ubuntu 24.04.5, L4T R39.2.2, 6.8.12-1029-tegra | Native DEB installation and module loading passed. XDMA bound to 10ee:9048 with one H2C and one C2H channel. UEFI Secure Boot was off. DKMS skipped signing because the signing helper was unavailable. |
| Package lifecycle | DEB and RPM installation and removal passed in an x86_64 Ubuntu chroot. The RPM test used a separate RPM database with dependency resolution disabled. |

The maintainer’s release directory contains the test logs. See `verification.log` for the source comparison, build and signature checks, and `package-test.log` for package installation and removal. The signing tests used a temporary certificate. Secure Boot was disabled on the build host. Native Fedora/RHEL installation, module loading under enforced Secure Boot, and DMA transfers on hardware remain unverified.
