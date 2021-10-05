If any of the steps fail: check Notes section at the end for common problems and fixes.

Important Note: most of the configurations files in [fuzz/configs](https://github.com/file-citas/via/tree/master/fuzz/configs) are not up to date. All tested and up to date configurations are listed in [benchmarks/eval/target_drvs](https://github.com/file-citas/via/blob/master/benchmarks/eval/target_drvs). To use one of the remaining configurations, please update it according to the configuration options described below.

# Overview
VIA is an in-process userspace Linux-kernel fuzzer, that targets the hardware-OS interface of device drivers.

VIA contains and is based on the following projects:
* llvm: https://github.com/llvm/llvm-project
* lkl: https://github.com/lkl/linux


# Docker
### Build the docker image
```
cd via/docker
docker build . -t via --build-arg NT=<number of build threads (make -j $NT)>
```
### Start fuzzing with the docker image
```
mkdir <tmp_indir>
docker run -v <tmp_indir>:/tmp_indir --env FUZZ_TARGET="rocker" via /via/fuzz/bin/fuzzl__X.elf -detect_leaks=0 /tmp_indir
```

# Installation

## Dependencies
* libconfig-dev
* python3.8
* libllvm10
* llvm-10
* cmake

## Clone Repo
```
git clone https://github.com/file-citas/via.git via
```

## Build clang
```
cd via
cd via/llvm
mkdir build
cd build
cmake -DLLVM_ENABLE_PROJECTS="clang;compiler-rt" -DCMAKE_BUILD_TYPE="Release" -G "Unix Makefiles" ../llvm
make -j<N>
```

make sure this clang instance is used for all builds from now on...

## Setup Environment
```
cd via
export VIA_PATH=$PWD
source setup_env
mkdir fuzz/targets_fuzz
```

## Build the LKL Library
Make sure that `$CC` is set correclty (`source via/fuzz/setup_env` should do this)

(defconfig is applied automatically)
```
cd via/lkl
make -C tools/lkl CC=$CC HOSTCC=$CC FUZZ=1 LLVM_IAS=1 PYTHON=python3.8 -j<N>
make drivers modules ARCH=lkl CC=$CC HOSTCC=$CC FUZZ=1 LLVM_IAS=1 PYTHON=python3.8 -j<N>
find . -name '*ko' -exec cp {} $VIA_PG_PATH/targets_fuzz \;
```

## Build the Harness
```
cd via/fuzz
mkdir bin
mkdir bin/harness
make -j<N>
```

# Start Fuzzing
Note: you can redirect stdout to clean up the output
VIA also supports all the default libfuzzer options: https://llvm.org/docs/LibFuzzer.html#options
```
cd via/fuzz
mkdir tmp_indir
FUZZ_TARGET="virtio_net" ./bin/fuzzl__X.elf -detect_leaks=0 ./tmp_indir
```

# Re-Execute on one particular IO-stream
```
FUZZ_TARGET="virtio_net" ./bin/runl__X.elf <io.bin>
```

# Configuration
(Also: check the comments in `via/fuzz/src/util.c`)

Configuration options are applied via a combination of values read from the environment and the module configuration file (`via/fuzz/configs/<target>.config`)

*IMPORTANT*:
Always set `FUZZ_TARGET=<target>` to select the configuration file.

## Global Configuration
These should be set by `source via/fuzz/setup_env` ...
 * env: `VIA_HARNESS` directory containing the harness libraries; e.g. `via/fuzz/bin/harness`
 * env: `VIA_CONFIGS` directory containing the modules configuration files; e.g. `via/fuzz/configs`
 * env: `VIA_MODULES` directory containing the modules; e.g. `via/fuzz/<targets_fuzz>`
 * env: `VIA_FW` directory containing the firmware files; e.g. `via/fuzz/fw`

## Target Specific Configuration
Each target is configured via a `via/fuzz/configs/<target>.config`
This file must minimally contain:
 * `module`: Name of the module in `<targets_fuzz>`; e.g. `module="virtio_net.ko"`
 * `module_deps`: if the target module depends on other modules, this information can be found in `modules.dep`; to generate `modules.dep` run `make ARCH=lkl CC=$CC HOSTCC=$CC LLVM_IAS=1 FUZZ=1 drivers/ modules_install INSTALL_MOD_PATH=../<moddir>`; e.g. `virtio_net.ko` depends on `moddeps=["virtio.ko", "virtio_ring.ko", "virtio_mmio.ko"]`
 * `harness`: Name of the harness library, for a list of available harness libraries check `via/fuzz/bin/harness/`; e.g. `harness="fuzz_net_icmp.so"`
 * `devtype`: Device Type (0=PCI, 1=VIRTIO, 2=PLATFORM)
 * `fws`: Array of firmware file name (must be present in `via/fuzz/fws`, if the module requires external firmware
 * `barsizes` and `barflags`: size and flags of `MMIO` or `PIO` regions; e.g. `barsizes=[0xfffff, 0xfffff]` and  `barflags=[0x40200, 0x40200]` (note, virtio resources will be configured automatically)

## Device Specific Configuration
 * `plt_name` (PLATFORM): compatible name of device
 * `vid` (PCI, VIRTIO): vendor id
 * `did` (PCI, VIRTIO): device id
 * `svid` (PCI): sub-vendor id
 * `sdid` (PCI): sub-device id
 * `pci_class` (PCI)
 * `revision` (PCI)
 * `vio_nofuzz` (VIRTIO): mmio offsets to exclude from fuzzing
 * `features_set_mask_high/low` (VIRTIO): features to be always set
 * `features_unset_mask_high/low` (VIRTIO): features to be always cleared
 * `drain_irqs` (VIRTIO): continue issuing IRQs after end of io-stream
 * `extra_io` (VIRTIO): extend io-stream with PRNG data
 * `fuzz_dma` (VIRTIO): 0: Simulation Mode, 1: Passthrough Mode
 * `nqueues` (VIRTIO): number of virtio queues

## Debug Configuration
 * env: `FUZZ_TRACE_MOD` (default 0): trace module (un)init
 * env: `FUZZ_TRACE_IO` (default 0): trace io-stream passed on to module
 * env: `FUZZ_TRACE_IRQ` (default 0): trace requested IRQ lines
 * env: `FUZZ_TRACE_MSIIRQ` (default 0): trace requested MSI-IRQ lines
 * env: `FUZZ_TRACE_DMA` (default 0): trace dma allocations, syncs and maps
 * env: `FUZZ_TRACE_DMAINJ` (default 0): trace asan based coherent dma injection
 * env: `FUZZ_TRACE_CONF` (default 0): trace pci device configuration
 * env: `FUZZ_TRACE_WAITERS` (default 0): trace addition and removal of blocked workloads
 * env: `FUZZ_TRACE_BST` (default 0): trace management and verification of mapped dma addresses
 * env: `FUZZ_TRACE_DONECB` (default 0): trace io-stream end callback invocation
 * env: `FUZZ_TRACE_DEVNODES` (default 0): trace allocation of device nodes

## Patch  Configuration
 * `apply_patch` (default 0), overwritten by env:`PATCH`: apply patches to avoid assertions, deadlocks and unbounded allocations
 * `apply_patch_2` (default 0), overwritten by env:`PATCH2`: apply patches for remaining bugs (if available)
 * `apply_hacks` (default 0), overwritten by env:`HACKS`: apply fuzzing optimizations

## Delay Configuration
 * env: `MIN_ALL_DELAY`: sets all options below to one
 * `minimize_delay` (default 0): reduce `delay` and `sleep`
 * `minimize_wq_delay` (default 0): reduce delay in `queue_delayed_work`
 * `minimize_timeout` (default 0): reduce timeout in `schedule_timeout_*`
 * `minimize_timebefore/after` (default 0): reduce delay through in `time_before/_after*`

## Runtime Configuration
 * `loglevel` (default: 0), overwritten by env:`LOGLEVEL`: kernel log level (0=no log, 8=verbose)
 * `target_irqs` (default: 0), overwritten by env:`TARGET_IRQS`: use targeted IRQ injection
 * `fast_irqs` (default: 0), overwritten by env:`FAST_IRQS`: trigger IRQ immediatly after `request_irq`
 * `do_update_hwm` (default: 1), overwritten by env:`UPDATEHWM`: update io-stream size after each iteration
 * `use_bst` (default: 0), overwritten by env: `USEBST`: keep track of mapped dma addresses and valiadate them on sync and unmap

## Network Configuration
 * `interface` (default: NULL): name of network interface
 * `ifindex` (default: 0): network interface index
 * `mtu` (default: -1): minimum transfer unit size

# Adding a new Harness
The fuzzing harness is device specific, therefore such functionality has been separated into different libs.
The harness libs must implement the function `mod_fuzz` (will be called from `LLVMFuzzerTestOneInput`) and can optionally provide `mod_init` (will be called at the end of `LLVMFuzzerInitialize`).
See `src/harness/` for examples.
The harness library to be used is selected in the config file via the `harness` parameter.
To create a new harness, implement the functionality in `src/harness/<new_target_harness>.c` and add `bin/harness/<new_target_harness>.so` to the the `TARGET_HARNESS` list in `Makefile`

# Concurrent IRQ Triggering
Most of the default fuzzing harnesses start an IRQ thread.
If the target blocks on resources which have to be generated asynchronously on the IRQ path,
configure an IRQ callback in `mod_init` via `start_irqthread_cb(irq_cb)` and
call `request_irqs(<max_irqs>)` before invoking the blocking function from the `mod_fuzz` function in the harness lib.
If `<max_irqs>==-1` an unlimited amount of IRQs will be generated.
Call `cancel_irqs()` to stall IRQ generation.

If targeted IRQ injection is enabled, the harness can call `lkl_fuzz_wait_for_wait` to block until a workload of the target module enters a blocked state.
Call `lkl_fuzz_has_waiters` to check the current number of blocked threads. This should also be called before `lkl_fuzz_wait_for_wait` to avoid deadlocks.
Coverage stability will probably suffer if using this, but targeted IRQ injection should help a bit.

# IO-Stream End Callback
If the io-stream generated by the fuzzer is used up a callback function will be invoked.
This callback function can be configured in `mod_init` via `lkl_fuzz_set_done_callback(done_cb)`.
By invoking `start_thread_done_default()` a default callback will be configured, that unloads the module when the IO stream is used up.
Using this functionality correctly can be a bit tricky, since it can lead to deadlocks, so take care when adjusting the default functionality.

# Add new Fuzzing Target
To add a new PCI, PLATFORM or VIRTIO target module:
* Get the device config from the lkl linux source code (see Configuration for required parameters).
* Configure the module in the lkl linux config (update defconfig, since the lkl build process overwrites .config)
* Create a new config file in via/fuzz/configs/.
* If the module has dependencies on other module add those to the config file as well (modules_dep). Otherwise the harness will crash due to undefined symbols.
* Specify the device specific harness lib (see `src/harness` for already available harnesses or create a new one).

# Notes
If you get an error like
```
lib/hijack/xlate.c:558:7: error: use of undeclared identifier 'SIOCGSTAMP
```
add the header
```
#include <linux/sockios.h>
```

---

To avoid leak detection notifications set this asan option (the leaks are an artefact of the harness)
```
export ASAN_OPTIONS=detect_leaks=0
```

---

If the drivers's probe function is not invoked, make sure that all the pci dev config options match the values set in `struct pci_device_id`.
This include sub ids pci class etc.
If you get iomem errors like `error reading iomem 0x9` check the config options `barsizes` and `barflags`.
You can add up to 6 iomem regions, adding more than requested by the driver is ok.

---

If the hwasan build fails, you can disable it; e.g. by removing `X86_64` from `ALL_HWASAN_SUPPORTED_ARCH`

---

You can use gdb for debugging.
Note that symbols are only resolved after the dynamic libraries have been loaded,
so run the harness once to get all the symbols and then restart or continue.
```
FUZZ_TARGET=virtio_balloon gdb --args ./bin/runl__X.elf ./crash-2a7cf9bf19656b33acd7873f5ea251ad9d351b74
FUZZ_TARGET=virtio_balloon gdb --args ./bin/fuzzl__X.elf -detect_leaks=0 ./tmp_indir
```
