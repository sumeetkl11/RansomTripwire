# Project Agent Rules & Governance (AGENTS.md)

## 1. Graphify Node Map
All development in this repository adheres to the following structural node dependencies:
- **Spec Nodes:** `PRD.md`, `TRD.md`, `DATA_DICTIONARY.md`, `HYBRID_ARCHITECTURE.md`
- **Engine Node:** `main.cpp` (Target `engine`, compiled with C++17 and linked to `kernel32.lib`)
- **Supervisor Node:** `dashboard.py` (Spawns `engine.exe`, manages RAM shadow cache, enforces lock retry)
- **Attack Simulator Node:** `attack_sim.py` (Adversary simulation, registers `.attack_pid`, rapid overwrites)
- **Storage Node:** `protected_vault/` (Honeypot canaries: `00_manifest.key`, `~tripwire_vault.key`, `zz_backup.dat`, payloads `student_record_*.txt`)

## 2. Ponytail Zero-Slop Minimal Code Ladder
Every code addition, edit, or refactoring in this repository MUST climb the Ponytail ladder:
1. **Does this need to exist at all?** (YAGNI - reject speculative abstractions).
2. **Standard Library First:**
   - In C++: Use ISO C++17 standard library and native Win32 headers (`<windows.h>`, `<tlhelp32.h>`). Zero third-party SDKs.
   - In Python: Use Python 3 standard library (`subprocess`, `time`, `os`, `sys`, `pathlib`, `collections`, `signal`). Zero `pip` packages.
3. **Native Platform Features Over Custom Code:** Native Win32 API (`ReadDirectoryChangesW`, `TerminateProcess`, `CreateToolhelp32Snapshot`) over simulated or third-party wrappers.
4. **Shortest Working Solution:** Direct, clean, readable code with zero boilerplate classes, factories, or wrapper fluff.
5. **No AI Slop:** No robotic buzzwords, no nested unnecessary abstractions, no decorative code.

## 3. Mandatory Safety Invariants
- **Whitelisting:** Neither C++ nor Python may terminate `dashboard.py` or `engine.exe`.
- **Pipe Flushing:** All IPC output from `main.cpp` must be flushed immediately (`std::flush`).
- **File Lock Resilience:** All rollback file writes must catch `PermissionError` and retry with backoff.
- **Offline Self-Sufficiency:** 100% offline operation without network or external API dependencies.

