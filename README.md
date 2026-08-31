# Android Kernel Driver

> A powerful Android kernel driver supporting Linux kernel 5.1 to 6.12 (Android 12-16), providing advanced memory manipulation capabilities through a custom network protocol family.

## 📑 Table of Contents
[![中文文档](https://img.shields.io/badge/文档-中文-red.svg)](README_CN.md)
- [Features](#-features)
- [Technical Architecture](#-technical-architecture)
- [Requirements](#-requirements)
- [Quick Start](#-quick-start)
- [API Documentation](#-api-documentation)
- [How It Works](#-how-it-works)
- [Performance](#-performance)
- [Compatibility](#-compatibility)
- [Upcoming Features](#-upcoming-features)
- [Security Notice](#-security-notice)
- [Acknowledgments](#-acknowledgments)

## 🌟 Features

### Core Capabilities

| Feature | Description | Implementation | Status |
|---------|-------------|----------------|--------|
| **PTE Physical Memory R/W** | Direct access to arbitrary physical memory | Manual page table walk + PTE remapping | ✅ Implemented |
| **PTE Physical Memory R/W** | Direct access to arbitrary physical memory | Manual page table walk + PTE innear | ✅ Implemented |
| **Memory R/W** | Cross-process memory read/write | use access_process_vm | ✅ Implemented |
| **Module Base Resolution** | Locate loaded module base addresses | VMA traversal + file path matching | ✅ Implemented |
| **Process Discovery** | Find PID by package/process name | Process traversal + name matching | ✅ Implemented |
| **Custom Protocol Family** | Secure user-kernel communication | Dynamic socket family registration | ✅ Implemented |
| **Driver Concealment** | Hide driver traces from system | List deletion + kobject removal | ✅ Implemented |
| **Hardware MMU Translation** | Address translation using AT instruction | ARM64 AT S1E0R instruction | ✅ Implemented |
| **Touch Device Emulation** | Simulate touch input events | uinput/input subsystem | ❌ Not Implemented |
| **Driver Self-Hiding ** |Driver Self-Hiding Hide driver traces from system List deletion + kobject removal| ✅ Implemented|
### Technical Highlights

- ✅ **Multi-version Compatible**: Supports Linux kernel 5.1 to 6.12
- ✅ **ARM64 Optimized**: Hardware-accelerated TLB flush using `TLBI VAALE1IS` instruction
- ✅ **Complete Page Table Walk**: PGD → P4D → PUD → PMD → PTE
- ✅ **Huge Page Detection**: Automatic detection of 1GB and 2MB huge pages
- ✅ **Software TLB Cache**: Optimized address translation performance
- ✅ **Normal-NC Mapping**: Cache-coherent physical memory access

## 🔧 Technical Architecture

```
┌─────────────────────────────────────────────────┐
│              User Space Application              │
│            (Root Access Required)                │
└──────────────────┬──────────────────────────────┘
                   │ Socket Communication
                   │ (Custom Protocol Family)
┌──────────────────▼──────────────────────────────┐
│                   Kernel Driver                  │
│                                                   │
│  ┌───────────────────────────────────────────┐  │
│  │         Socket Interface Layer            │  │
│  │  ┌─────────────┐  ┌──────────────────┐   │  │
│  │  │ getsockopt  │  │   setsockopt     │   │  │
│  │  └─────────────┘  └──────────────────┘   │  │
│  └──────────────────┬───────────────────────┘  │
│                     │                           │
│  ┌──────────────────▼───────────────────────┐  │
│  │          Memory Management Module        │  │
│  │  ┌──────────┐ ┌──────────┐ ┌─────────┐  │  │
│  │  │PTE Mapping│ │Page Walk │ │TLB Mgmt │  │  │
│  │  └──────────┘ └──────────┘ └─────────┘  │  │
│  └──────────────────┬───────────────────────┘  │
│                     │                           │
│  ┌──────────────────▼───────────────────────┐  │
│  │        Process Management Module         │  │
│  │  ┌──────────┐ ┌──────────┐ ┌─────────┐  │  │
│  │  │PID Lookup│ │Module Base│ │VMA Walk │  │  │
│  │  └──────────┘ └──────────┘ └─────────┘  │  │
│  └───────────────────────────────────────────┘  │
│                                                   │
│  ┌───────────────────────────────────────────┐  │
│  │           Concealment Module               │  │
│  │  /proc/modules  /sys/module  /proc/vmalloc│  │
│  └───────────────────────────────────────────┘  │
└───────────────────────────────────────────────────┘
```

## 📋 Requirements

| Item | Requirement |
|------|-------------|
| **Kernel Version** | Linux 5.1 - 6.12 |
| **Architecture** | ARM64 (ARMv8-A) |
| **Compiler** | GCC or Clang (cross-compilation) |
| **Permissions** | Root access |
| **Android Version** | Android 12 - 16 |

## 🚀 Quick Start

### Building

```bash
# 1. Clone the repository
git clone https://github.com/yourusername/your-repo.git
cd your-repo

# 2. Configure build script
# Edit build_all.sh, set the following variables:
# - KERNEL_DIR: kernel source directory
# - PROJECT_DIR: project directory

# 3. Run build script
./build_all.sh

# 4. Check generated kernel module
ls -la *.ko
```

### Installation

```bash
# 1. Push to device
adb push install_driver.sh /data/local/tmp/

# 2. Get root access
adb shell
su

# 3. Load module
cd /data/local/tmp/
./install_driver.sh
```

## 📖 API Documentation

### Operation Codes

| Opcode | Function | Parameters | Description |
|--------|----------|------------|-------------|
| `GET_PROCESS_PID` | Get PID by process name | Process name | Returns PID |
| `ATTACH_PROCESS` | Attach to target process | PID | Must attach first |
| `IS_PROCESS_PID_ALIVE` | Check if process is alive | PID | Returns 1/0 |
| `GET_MODULE_BASE` | Get module base address | Module name | Returns base address |
| `ACCESS_PROCESS_VM` | Standard cross-process R/W | Address and size | Uses kernel API |
| `PTE_PHYS_READ_MEMORY` | PTE-based memory read | Virtual address | Manual page table walk |
| `PTE_PHYS_WRITE_MEMORY` | PTE-based memory write | Virtual address | Manual page table walk |

### Usage Example

> 📝 User space usage examples coming soon, contributions welcome!

## 🔬 How It Works

### PTE Memory R/W Flow

```
1. User space initiates request
   ↓
2. Socket communication to kernel
   ↓
3. Get target process mm_struct
   ↓
4. Manual page table walk (PGD → P4D → PUD → PMD → PTE)
   ↓
5. Get physical address
   ↓
6. Modify PTE to map target physical page
   ↓
7. Flush TLB
   ↓
8. Direct R/W through mapped virtual address
   ↓
9. Return result to user space
```

### Page Table Walk Detail

```
Virtual Address (48-bit)
┌─────┬─────┬─────┬─────┬─────┬────────┐
│ PGD │ P4D │ PUD │ PMD │ PTE │ Offset │
│ 9bit│ 9bit│ 9bit│ 9bit│ 9bit│ 12bit  │
└─────┴─────┴─────┴─────┴─────┴────────┘
  ↓     ↓     ↓     ↓     ↓      ↓
 512   512   512   512   512    4KB
entries entries entries entries entries page
```

## 📊 Performance

| Metric | Value | Description |
|--------|-------|-------------|
| Address Translation | O(1) | With software TLB cache |
| Memory Access Latency | ~100ns | Direct PTE mapping |
| TLB Flush | ~1μs | Hardware-accelerated broadcast |
| Page Table Walk | ~500ns | 4-level page table |
| Cross-page R/W | Automatic | Page-by-page loop |

## 🔄 Compatibility Matrix

| Kernel Version | Android Version | Status |
|---------------|-----------------|--------|
| 6.12 | Android 16 | ✅ Supported |
| 6.6 | Android 15 | ✅ Supported |
| 6.1 | Android 14 | ✅ Supported |
| 5.15 | Android 13 | ✅ Supported |
| 5.10 | Android 13 | ✅ Supported |
| 5.10 | Android 12 | ✅ Supported |

## 🚧 Upcoming Features

### Hardware MMU Translation
- [✅ ] Address translation using ARM64 `AT S1E0R` instruction
- [✅] Handle translation failure SEA (Synchronous External Abort)
- [✅] Optimize translation performance

### Touch Device Emulation
- [ ] Implement virtual touch device
- [ ] Support multi-touch protocol (Type B)
- [ ] Simulate swipe, tap gestures

<<<<<<< HEAD
=======
- [ ]  CFI Bypass

>>>>>>> bab7d981866ee26532c9b27d41280b64bdc581b7
## 🔒 Security Notice

### ⚠️ Warning

This driver provides low-level memory access capabilities that may:

- **Bypass security mechanisms**: Bypass SELinux, Seccomp, etc.
- **Cause system instability**: Incorrect memory operations may crash the kernel
- **Be misused**: May be used to develop malicious software

### ✅ Legitimate Use Cases

- 📚 Educational learning
- 🔬 Security research
- 🐛 Authorized debugging
- 🛡️ Penetration testing (controlled environments)

### ❌ Prohibited Use Cases

- 🚫 Developing malicious software
- 🚫 Unauthorized access
- 🚫 Commercial use (without permission)

## 🙏 Acknowledgments

Special thanks to the following projects and authors:

- **[Linux-android-arm64](https://github.com/lsnbm/Linux-android-arm64/tree/main)** - Provided important technical reference and support

> Standing on the shoulders of giants, grateful for the original author's open-source spirit!

## 📝 License

This project is for **educational purposes only**. Use at your own risk.

## 🤝 Contributing

Contributions are welcome! Please ensure:

1. Code follows Linux kernel coding style
2. Tested on multiple kernel versions
3. Documentation is updated
4. Appropriate comments are added
5. Run `checkpatch.pl` before submitting

## 📧 Contact

For questions or suggestions, please submit an Issue or Pull Request.

---

**⭐ If this project helps you, please give it a Star!**

**🔗 Thank Project: [Linux-android-arm64](https://github.com/lsnbm/Linux-android-arm64/tree/main)**
