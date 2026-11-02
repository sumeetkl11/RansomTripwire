<div align="center">

# 🛡️ RansomTripwire

**Real-Time Ransomware Tripwire & Self-Healing File Shield**

[![Language](https://img.shields.io/badge/C%2B%2B-17-blue?logo=cplusplus)](https://en.cppreference.com/w/cpp/17)
[![Language](https://img.shields.io/badge/Python-3.x-yellow?logo=python)](https://python.org)
[![Build](https://img.shields.io/badge/CMake-4.x-green?logo=cmake)](https://cmake.org)
[![Platform](https://img.shields.io/badge/Platform-Windows-0078D6?logo=windows)](https://microsoft.com/windows)
[![License](https://img.shields.io/badge/License-MIT-green)](LICENSE)
[![Course](https://img.shields.io/badge/CS--3111-Operating%20Systems-orange)](.)

> An OS-level active defense system that monitors a protected file vault in real time,
> detects ransomware behavior via honeypot canary tripwires and write-velocity heuristics,
> evicts the malicious process using Win32 kernel APIs, and self-heals all corrupted files
> from a volatile in-memory shadow cache — in under 200ms.

</div>

---

## 📋 Table of Contents

- [What Is This?](#-what-is-this)
- [Architecture](#-architecture)
- [How It Works](#-how-it-works)
- [Honeypot Canary Strategy](#-honeypot-canary-strategy)
- [IPC Protocol](#-ipc-protocol)
- [Build Prerequisites](#-build-prerequisites)
- [Run Instructions](#️-run-instructions)
- [Attack Simulation Demo](#-attack-simulation-demo)
- [OS Concepts Demonstrated](#-os-concepts-demonstrated)
- [Project Structure](#-project-structure)
- [License](#-license)

---

## 🔍 What Is This?

**RansomTripwire** is a hybrid C++17 / Python 3 active defense engine built as a demonstration of core Operating Systems principles applied to real-world ransomware defense.

It combines three distinct defense layers:

| Layer | Technology | Role |
|---|---|---|
| **Kernel Event Hook** | C++17 + Win32 `ReadDirectoryChangesW` | Non-polling real-time file system notifications |
| **Honeypot Tripwires** | Static canary files in `protected_vault/` | Instant detection on first access |
| **Velocity Heuristic** | Python sliding-window counter | Detects rapid encryption sweeps (≥ 3 writes/sec) |

When a threat is detected, the system:
1. **Terminates** the rogue process via `TerminateProcess` (C++) or `os.kill` (Python)
2. **Restores** all corrupted files from a volatile RAM shadow cache
3. **Reports** everything over a live GUI telemetry stream

---

## 🏗️ Architecture

```
+---------------------------------------------------------------------------------+
|                         PYTHON SUPERVISOR (dashboard.py)                        |
|  - Spawns & supervises C++ child process via anonymous stdout/stdin pipes       |
|  - In-memory volatile shadow cache (dict[Path, bytes]) loaded at startup        |
|  - Evaluates sliding-window velocity heuristic: Velocity = ΔWrites / Δt        |
|  - Eviction Vector B: Terminates attacker PID if Velocity >= 3 writes/sec       |
|  - Microsecond file restoration engine with WinError 32 retry backoff (3x,50ms) |
+----------------------------------------^----------------------------------------+
                                         |
                            ANONYMOUS IPC STREAM PIPE
                 (EVENT:MODIFIED:*, [TRIPWIRE_TRIGGER]:*, [OS_KERNEL]:*)
                                         |
+----------------------------------------v----------------------------------------+
|                          C++ KERNEL WORKER (main.cpp)                           |
|  - Non-polling Win32 directory notification hook: ReadDirectoryChangesW         |
|  - OS Process Table Traversal: CreateToolhelp32Snapshot + Process32First/Next   |
|  - Eviction Vector A: TerminateProcess on canary honeypot breach                |
|  - PID Whitelist: Never terminates self or parent supervisor                    |
+---------------------------------------------------------------------------------+
                                         |
                         PHYSICAL STORAGE (NTFS / FAT32)
                                         |
+----------------------------------------v----------------------------------------+
|                         PROTECTED VAULT (protected_vault/)                      |
|  Canary 1 (Alpha A-Z Trap):    00_manifest.key                                  |
|  Canary 2 (OS Tilde Priority): ~tripwire_vault.key                              |
|  Canary 3 (Omega Z-A Trap):    zz_backup.dat                                    |
|  Protected Records:            student_record_1.txt … student_record_5.txt      |
|  Transient Attack Marker:      .attack_pid  (runtime only, gitignored)          |
+---------------------------------------------------------------------------------+
```

---

## ⚙️ How It Works

### 1. C++ Kernel Engine (`main.cpp`)

The engine uses **`ReadDirectoryChangesW`** — a Windows NT file system filter API that fires a kernel-level interrupt on any file change inside `protected_vault/`. This is **non-polling**: the thread blocks inside the kernel until an event arrives, consuming near-zero CPU.

On every notification:
- If the changed file is a **canary** → immediately evict the attacker process
- Otherwise → stream an `EVENT:MODIFIED:<filename>` message over the IPC pipe to Python

```cpp
// Kernel notification loop — non-polling, zero CPU burn
ReadDirectoryChangesW(hDir, buffer.data(), bufSize, FALSE,
    FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_LAST_WRITE,
    &bytesReturned, NULL, NULL);
```

### 2. Process Eviction (Vector A — C++ `TerminateProcess`)

When a canary is breached, the engine:
1. Reads `.attack_pid` (attacker self-registered their PID)
2. Falls back to `CreateToolhelp32Snapshot` OS Process Table traversal
3. Acquires handle with `PROCESS_TERMINATE` rights via `OpenProcess`
4. Calls `TerminateProcess(hProc, 1)` — a hard Win32 kernel kill

### 3. Python Supervisor (`dashboard.py`)

The supervisor reads the IPC pipe in a background thread and:
- Tracks file write timestamps in a **sliding-window deque**
- Computes write velocity: `Velocity = len(events_in_window) / 1.0`
- If velocity ≥ 3 writes/sec → **Eviction Vector B**: `os.kill(attacker_pid, SIGTERM)`
- Triggers **self-healing rollback** from the volatile RAM shadow cache

### 4. Self-Healing Rollback

On startup, Python reads all vault files into memory:
```python
shadow_cache[file_path] = file_path.read_bytes()
```

On threat detection, it restores every corrupted file from this cache — with WinError 32 retry backoff for Windows handle-release timing:
```
Rollback latency target: < 200ms
Data loss target:         0.0%
```

---

## 🍯 Honeypot Canary Strategy

Three canary files are placed at strategic alphabetical positions in the vault — ensuring they are hit early in any alphabetical ransomware sweep:

| Canary File | Strategy | Why Effective |
|---|---|---|
| `00_manifest.key` | **Alpha trap** — sorts first (A-Z order) | Alphabetical ransomware hits it first |
| `~tripwire_vault.key` | **Tilde trap** — OS filesystem priority | Tilde files sort before letters in many fs implementations |
| `zz_backup.dat` | **Omega trap** — sorts last (Z-A or reverse) | Catches reverse-order encryption sweeps |

Any write to these files = **immediate ransomware detection**.

---

## 📡 IPC Protocol

Messages streamed over the anonymous pipe from C++ → Python follow a strict text protocol:

| Message Format | Meaning |
|---|---|
| `EVENT:MODIFIED:<filename>` | Regular file write — tracked for velocity heuristic |
| `[TRIPWIRE_TRIGGER]:<canary>` | Honeypot canary breached — immediate lockdown |
| `[OS_KERNEL]:TERMINATED:<PID>` | Rogue process evicted by Win32 `TerminateProcess` |

---

## 🔧 Build Prerequisites

| Requirement | Version | Notes |
|---|---|---|
| Windows | 10 / 11 | Win32 API required (`ReadDirectoryChangesW`) |
| Python | 3.10+ | Standard library only — zero pip dependencies |
| CMake | 4.x | Used to compile C++ engine |
| C++ Compiler | MinGW-w64 / MSVC | Bundled with CLion or installed separately |
| Tkinter | Built-in | Included with standard Python on Windows |

---

## 🖥️ Run Instructions

### Step 1 — Compile the C++ Engine

```bash
# Using CLion: Open project → Build → Build engine target
# Or via CMake CLI:
cmake -B cmake-build-debug -S .
cmake --build cmake-build-debug --target engine
```

### Step 2 — Launch the Dashboard (GUI mode)

```bash
python dashboard.py
```

### Step 3 — Launch Headless (CLI mode, no display)

```bash
python dashboard.py --cli
# or
HEADLESS=1 python dashboard.py
```

The GUI will show:
- 🟢 **Green banner** → Shield armed and monitoring
- 🔴 **Red flashing banner** → Threat detected, lockdown active
- Live telemetry stream with color-coded kernel events

---

## 💥 Attack Simulation Demo

Run the attacker in a **separate terminal** while the dashboard is active:

```bash
# Default: alphabetical sweep (hits 00_manifest.key first)
python attack_sim.py

# Reverse alphabetical sweep (hits zz_backup.dat first)
python attack_sim.py --reverse

# Records-first sweep (hits student data before canaries)
python attack_sim.py --records-first
```

**Expected output sequence:**
```
[ADVERSARY]: Commencing rapid sequential encryption sweep...
[ATTACKING #1]: Corrupting 00_manifest.key...
  → Dashboard: [ALERT]: HONEYPOT BREACH DETECTED on canary bait: 00_manifest.key
  → Dashboard: [OS_KERNEL]: Win32 TerminateProcess evicted rogue PID 14872
  → Dashboard: [SELF-HEALING]: Restored 5 protected record(s) in 4.217 ms
[ATTACK_INTERRUPTED]: Permission denied on student_record_1.txt (Access revoked by OS)
```

---

## 📚 OS Concepts Demonstrated

| Concept | Where Applied |
|---|---|
| **File System Notification (I/O Manager Layer)** | `ReadDirectoryChangesW` in `main.cpp` |
| **Process Control Block (PCB) Traversal** | `CreateToolhelp32Snapshot` + `PROCESSENTRY32W` |
| **Inter-Process Communication (IPC)** | Anonymous pipe: `subprocess.Popen(stdout=PIPE)` |
| **Volatile Memory as Defense Cache** | RAM shadow cache `dict[Path, bytes]` |
| **Process Eviction System Call** | `TerminateProcess` (Win32) + `os.kill` (POSIX signal) |
| **Race Condition Mitigation** | WinError 32 retry backoff (handle reclamation delay) |
| **Sliding-Window Heuristic** | Write velocity anomaly detection in Python |
| **PID Whitelisting** | Self + parent whitelisted to prevent friendly-fire eviction |

---

## 📁 Project Structure

```
RansomTripwire/
├── main.cpp                   # C++17 Win32 kernel worker (ReadDirectoryChangesW)
├── dashboard.py               # Python supervisor + Tkinter GUI + self-healing engine
├── attack_sim.py              # Adversary emulator (ransomware sweep simulator)
├── CMakeLists.txt             # CMake build config for engine.exe target
├── HYBRID_ARCHITECTURE.md     # Formal system architecture specification
├── AGENTS.md                  # Development governance & Ponytail code ladder
├── LICENSE                    # MIT License
├── .gitignore                 # Excludes build artifacts and runtime vault state
└── protected_vault/
    ├── 00_manifest.key        # Canary tripwire — Alpha trap (sorts first)
    ├── ~tripwire_vault.key    # Canary tripwire — Tilde OS priority trap
    └── zz_backup.dat          # Canary tripwire — Omega trap (sorts last)
```

---

## 📄 License

MIT License — © 2024 [Sumeet Kanhiya](https://github.com/sumeetkl11)

See [LICENSE](LICENSE) for full terms.

---

<div align="center">
  <sub>Built for CS-3111 Operating Systems · CLO-2 Alignment · 100% offline · Zero external dependencies</sub>
</div>

