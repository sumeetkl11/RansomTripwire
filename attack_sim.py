#!/usr/bin/env python3
# ==============================================================================
# RTS-Shield: Ransomware Attack Simulator (Adversary Process)
# Course Context: CS-3111 Operating Systems (CLO-2 Alignment)
# Ponytail Ladder: Standard library only (no pip dependencies).
# ==============================================================================

import os
import sys
import time
from pathlib import Path

BASE_DIR = Path(__file__).resolve().parent
VAULT_DIR = BASE_DIR / "protected_vault"
PID_FILE = VAULT_DIR / ".attack_pid"
ENCRYPTED_PAYLOAD = b"[LOCKED_BY_RANSOMWARE_ENCRYPTED_BLOB_X992019488102]"

def simulate_ransomware_attack(mode: str = "alphabetical"):
    print("=" * 65)
    print(" RANSOMWARE ATTACK SIMULATOR (ADVERSARY EMULATOR)")
    print(f" Attacker Process ID (PID): {os.getpid()}")
    print(f" Mode:                      {mode.upper()}")
    print(f" Target:                    {VAULT_DIR}")
    print("=" * 65)

    if not VAULT_DIR.is_dir():
        print(f"[ATTACK_ERROR]: Vault directory {VAULT_DIR} not found.")
        sys.exit(1)

    # Register adversary PID for detection verification
    try:
        PID_FILE.write_text(str(os.getpid()))
        print(f"[ADVERSARY]: Active process registered at {PID_FILE}")
    except Exception as e:
        print(f"[ADVERSARY_WARN]: Could not write PID file: {e}")

    # Discover target files
    all_files = [f for f in VAULT_DIR.iterdir() if f.is_file() and f.name != ".attack_pid"]

    if mode == "records-first":
        records = [f for f in all_files if f.name.startswith("student_record")]
        canaries = [f for f in all_files if not f.name.startswith("student_record")]
        target_files = sorted(records, key=lambda x: x.name) + sorted(canaries, key=lambda x: x.name)
    else:
        target_files = sorted(all_files, key=lambda x: x.name, reverse=(mode == "reverse"))

    print(f"[ADVERSARY]: Discovered {len(target_files)} target file descriptors.")
    print("[ADVERSARY]: Commencing rapid sequential encryption sweep...\n")

    attack_start = time.perf_counter()

    for idx, target in enumerate(target_files, start=1):
        try:
            print(f"[ATTACKING #{idx}]: Corrupting {target.name}...")
            # Simulate ransomware overwrite
            with open(target, "wb") as f:
                f.write(ENCRYPTED_PAYLOAD)

            # Rapid attack pace (30ms between writes = >30 writes/sec velocity)
            time.sleep(0.03)

        except PermissionError:
            print(f"[ATTACK_INTERRUPTED]: Permission denied on {target.name} (Access revoked by OS)")
            break
        except Exception as e:
            print(f"[ATTACK_EXCEPTION]: {e}")
            break

    elapsed = (time.perf_counter() - attack_start) * 1000.0
    print(f"\n[ATTACK_FINISHED]: Cycle completed in {elapsed:.2f} ms.")

if __name__ == "__main__":
    mode_arg = "alphabetical"
    if len(sys.argv) > 1:
        if "--records-first" in sys.argv:
            mode_arg = "records-first"
        elif "--reverse" in sys.argv:
            mode_arg = "reverse"
    simulate_ransomware_attack(mode_arg)
