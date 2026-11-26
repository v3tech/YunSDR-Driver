# YunSDR XDMA DKMS packages

Release `2020.2.2.yunsdr.20261126-1` supports x86_64 and AArch64. The DEB (`all`) and RPM (`noarch`) contain the driver source. On the target machine, DKMS compiles `xdma.ko` against the kernel build tree. DKMS autoinstall is enabled for subsequent kernel updates.

## Installation

From the release directory, run `sha256sum -c SHA256SUMS` and confirm that both files pass. If the target already uses an XDMA driver, record its path with `modinfo -n xdma` and its DKMS version with `dkms status -m xdma` before installation.

**Debian or Ubuntu.** Install the build tools and headers for the running kernel, then install the DEB.

```sh
sudo apt install dkms build-essential "linux-headers-$(uname -r)" mokutil
sudo apt install ./xdma-dkms_2020.2.2.yunsdr.20261126-1_all.deb
```

For a vendor kernel such as Jetson Linux, use the matching headers from the board support package (BSP) in place of the distribution header package. Verify that `/lib/modules/$(uname -r)/build/Makefile` is readable.

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

On host 203, run `./build.sh` from the release directory. The script uses `dpkg-deb` and `rpmbuild` and reads the adjacent `../linux-kernel` source tree. The package version combines the driver version from `xdma/version.h` with the YunSDR snapshot date.

The compatibility change is part of `xdma/xdma_iova_ring.c`. The IOVA mmap code follows the kernel API selection in `cdev_ctrl.c` to set `VM_DONTEXPAND` and `VM_DONTDUMP` on both older and newer kernels. The release directory retains `vm-flags-compat.patch` as a record of the change.

## Validation

| Test | Result |
|---|---|
| x86_64, Ubuntu 6.8.0-138-generic | Native DKMS build, isolated installation and removal, and CMS signature verification passed. |
| AArch64, Jetson 5.15.148-tegra | Cross-build, isolated installation and removal, and CMS signature verification passed. |
| Package lifecycle | DEB and RPM installation and removal passed in an x86_64 Ubuntu chroot. The RPM test used a separate RPM database with dependency resolution disabled. |

See `verification.log` for the source comparison, build and signature checks, and `package-test.log` for package installation and removal. The signing tests used a temporary certificate. Secure Boot was disabled on the build host. Native Fedora/RHEL installation, module loading under enforced Secure Boot, and DMA transfers on hardware remain unverified.
