# Hybrid Architecture Specification: Real-Time Ransomware Tripwire & Self-Healing File Shield

**Project:** Real-Time Ransomware Tripwire & Self-Healing File Shield (RTS-Shield)  
**Academic Alignment:** CS-3111 Operating Systems (CLO-2 Alignment)  
**Architecture:** Decoupled Win32 Kernel Worker (C++17) + Supervisor Controller (Python 3)  

---

## 1. System Overview & Data Flow

```text
+---------------------------------------------------------------------------------+
|                         PYTHON SUPERVISOR (dashboard.py)                         |
|  - Spawns & supervises C++ child process via anonymous stdout/stdin pipes       |
|  - In-memory volatile shadow cache (dict[Path, bytes]) loaded at startup        |
|  - Evaluates sliding-window velocity heuristic: Velocity = Delta Writes / Delta t |
|  - Eviction Vector B: Terminates attacker PID if Velocity >= 3 writes/sec        |
|  - Microsecond file restoration engine with WinError 32 retry backoff (3x, 50ms)|
+----------------------------------------^----------------------------------------+
                                         |
                            ANONYMOUS IPC STREAM PIPE
                 (EVENT:MODIFIED:*, [TRIPWIRE_TRIGGER]:*, [OS_KERNEL]:*)
                                         |
+----------------------------------------v----------------------------------------+
|                          C++ KERNEL WORKER (main.cpp)                           |
|  - Non-polling Win32 directory notification hook: ReadDirectoryChangesW        |
|  - Academic distinction: ReadDirectoryChangesW provides FILE_NOTIFY_INFORMATION |
|    (Action, FileName) but DOES NOT supply caller Process IDs (PIDs)            |
|  - OS Process Table Traversal: CreateToolhelp32Snapshot + Process32First/Next   |
|  - Eviction Vector A: TerminateProcess system call on canary honeypot breach    |
|  - PID Whitelist Invariant: Never terminates self or parent supervisor          |
+---------------------------------------------------------------------------------+
                                         |
                         PHYSICAL STORAGE (NTFS / FAT32)
                                         |
+----------------------------------------v----------------------------------------+
|                          PROTECTED VAULT (protected_vault/)                     |
|  - Canary 1 (Alpha A-Z Trap):       00_manifest.key                             |
|  - Canary 2 (OS Priority Tilde):    ~tripwire_vault.key                         |
|  - Canary 3 (Omega Z-A Trap):       zz_backup.dat                               |
|  - Protected Academic Records:      student_record_1.txt ... 5.txt              |
|  - Transient Adversary Marker:      .attack_pid                                 |
+---------------------------------------------------------------------------------+
```

---

## 2. Low-Level C++ Subsystem Specification

### 2.1. Directory Notification Hook (`ReadDirectoryChangesW`)
* **Handle Acquisition:**
  Acquires a directory handle via `CreateFileW` with flags:
  `FILE_LIST_DIRECTORY`, `FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE`, `OPEN_EXISTING`, and `FILE_FLAG_BACKUP_SEMANTICS`.
* **Kernel Notification Loop:**
  Invokes `ReadDirectoryChangesW` in a non-polling asynchronous loop watching:
  * `FILE_NOTIFY_CHANGE_FILE_NAME`
  * `FILE_NOTIFY_CHANGE_LAST_WRITE`
* **Academic OS Principle:**
  `ReadDirectoryChangesW` writes `FILE_NOTIFY_INFORMATION` structures into a buffer. These structures convey file actions and wchar filename strings. The Windows NT file system filter layer does **not** attach calling thread or process IDs to these notification packets.

### 2.2. OS Process Table Enumeration & Eviction
* **Process Table Snapshot:**
  When a honeypot canary is accessed, `main.cpp` takes a snapshot of the operating system Process Table using:
  `CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0)`.
* **PCB Traversal:**
  Traverses `PROCESSENTRY32W` structures using `Process32FirstW` and `Process32NextW` to find the adversary process (e.g. matching `python.exe` / `attack_sim.py`).
* **Whitelisting & Execution Eviction:**
  * Strictly whitelists `GetCurrentProcessId()` (self) and `th32ParentProcessID` (`dashboard.py`).
  * Acquires handle with `PROCESS_TERMINATE` rights via `OpenProcess`.
  * Evicts malicious execution context via `TerminateProcess(hProcess, 1)`.
  * Emits `[OS_KERNEL]:TERMINATED:<PID>` to the IPC stream.

---

## 3. High-Level Python Subsystem Specification

### 3.1. Subprocess Supervision & Stream Pipe
* Spawns `engine.exe` as a child process using `subprocess.Popen(..., stdout=subprocess.PIPE, text=True, bufsize=1)`.
* Reads pipe output line-by-line in real time.

### 3.2. Volatile RAM Shadow Cache
* On startup, reads exact binary byte streams of all files in `protected_vault/` into an in-memory dictionary:
  `shadow_cache[file_path] = binary_payload`
* Completely non-persistent and zero-database; cleared on shutdown.

### 3.3. Heuristic Velocity Evaluation & Dual Eviction
* Maintains sliding-window write timestamps:
  $$\text{Velocity} = \frac{\Delta \text{WriteEvents}}{\Delta t}$$
* If $\text{Velocity} \ge 3 \text{ events/sec}$, flags an anomaly.
* Terminates the recorded adversary PID (from `.attack_pid` or process table) using `os.kill(attacker_pid, signal.SIGTERM)`.

### 3.4. Windows File-Lock Mitigation (WinError 32)
* When an adversary process terminates, Windows kernel handle reclamation is asynchronous and may take a few milliseconds.
* Rollback writes are wrapped in a retry loop: up to 3 attempts with 50ms delay upon encountering `PermissionError` (`WinError 32: ERROR_SHARING_VIOLATION`), ensuring 100% resilient restoration.

---

## 4. IPC Communication Protocol

Messages streamed over the anonymous pipe follow a strict text protocol:
* **File Modification Event:** `EVENT:MODIFIED:<filename>`
* **Canary Trap Breach:** `[TRIPWIRE_TRIGGER]:<canary_name>`
* **Process Eviction Notice:** `[OS_KERNEL]:TERMINATED:<PID>`

