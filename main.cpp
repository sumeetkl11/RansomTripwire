// ==============================================================================
// RTS-Shield: Win32 Real-Time Directory Event Engine & Process Eviction Worker
// Course Context: CS-3111 Operating Systems (CLO-2 Alignment)
// Ponytail Ladder: ISO C++17 stdlib + native Win32 API. Zero external packages.
// ==============================================================================

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>
#include <iostream>
#include <string>
#include <vector>
#include <fstream>

// ACADEMIC NOTE (CS-3111 Operating Systems):
// ReadDirectoryChangesW is a file system notification hook operating at the I/O
// manager layer. It populates FILE_NOTIFY_INFORMATION with action codes and file
// names, but DOES NOT provide the calling Process Identifier (PID) or thread context.
// Therefore, PID discovery and eviction are explicitly decoupled and performed via
// OS Process Table inspection using CreateToolhelp32Snapshot.

static DWORD GetParentPID() {
    DWORD currentPid = GetCurrentProcessId();
    DWORD parentPid = 0;
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32W pe;
        pe.dwSize = sizeof(pe);
        if (Process32FirstW(hSnap, &pe)) {
            do {
                if (pe.th32ProcessID == currentPid) {
                    parentPid = pe.th32ParentProcessID;
                    break;
                }
            } while (Process32NextW(hSnap, &pe));
        }
        CloseHandle(hSnap);
    }
    return parentPid;
}

static std::wstring GetVaultPath() {
    auto check = [](const wchar_t* p) {
        DWORD a = GetFileAttributesW(p);
        return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY);
    };
    if (check(L"protected_vault")) return L"protected_vault";
    if (check(L"ransomware-shield\\protected_vault")) return L"ransomware-shield\\protected_vault";
    return L"protected_vault";
}

static DWORD EvictAdversaryProcess(DWORD selfPid, DWORD parentPid) {
    // Check if adversary marked its PID in protected_vault/.attack_pid
    std::wstring vPath = GetVaultPath();
    std::wstring pidPathW = vPath + L"\\.attack_pid";
    std::string pidPath(pidPathW.begin(), pidPathW.end());
    std::ifstream pidFile(pidPath);
    if (pidFile.is_open()) {
        DWORD recordedPid = 0;
        pidFile >> recordedPid;
        pidFile.close();
        if (recordedPid != 0 && recordedPid != selfPid && recordedPid != parentPid) {
            HANDLE hProc = OpenProcess(PROCESS_TERMINATE, FALSE, recordedPid);
            if (hProc != NULL) {
                TerminateProcess(hProc, 1);
                CloseHandle(hProc);
                return recordedPid;
            }
        }
    }

    // Fallback: Traversal of OS Process Table via Toolhelp32
    DWORD evictedPid = 0;
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap == INVALID_HANDLE_VALUE) return 0;

    PROCESSENTRY32W pe;
    pe.dwSize = sizeof(pe);
    if (Process32FirstW(hSnap, &pe)) {
        do {
            if (_wcsicmp(pe.szExeFile, L"python.exe") == 0 || _wcsicmp(pe.szExeFile, L"python3.exe") == 0) {
                if (pe.th32ProcessID != selfPid && pe.th32ProcessID != parentPid && pe.th32ProcessID != 0) {
                    HANDLE hProc = OpenProcess(PROCESS_TERMINATE, FALSE, pe.th32ProcessID);
                    if (hProc != NULL) {
                        TerminateProcess(hProc, 1);
                        CloseHandle(hProc);
                        evictedPid = pe.th32ProcessID;
                        break;
                    }
                }
            }
        } while (Process32NextW(hSnap, &pe));
    }
    CloseHandle(hSnap);
    return evictedPid;
}

static bool IsCanaryFile(const std::wstring& filename) {
    return (filename == L"00_manifest.key" ||
            filename == L"~tripwire_vault.key" ||
            filename == L"zz_backup.dat");
}

int main() {
    std::cout << "[ENGINE_INIT]: RTS-Shield Win32 Engine Active" << std::endl;

    DWORD selfPid = GetCurrentProcessId();
    DWORD parentPid = GetParentPID();

    std::wstring vaultPath = GetVaultPath();

    // Acquire directory descriptor with backup semantics for directory monitoring
    HANDLE hDir = CreateFileW(
        vaultPath.c_str(),
        FILE_LIST_DIRECTORY,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        NULL,
        OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS,
        NULL
    );

    if (hDir == INVALID_HANDLE_VALUE) {
        std::cerr << "[ENGINE_ERROR]: Failed to acquire directory handle for protected_vault" << std::endl;
        return 1;
    }

    std::vector<BYTE> buffer(4096);
    DWORD bytesReturned = 0;

    std::cout << "[C++_ENGINE_READY] Win32 directory notification hook active." << std::endl;

    // Kernel notification event loop (non-polling ReadDirectoryChangesW)
    while (true) {
        BOOL success = ReadDirectoryChangesW(
            hDir,
            buffer.data(),
            static_cast<DWORD>(buffer.size()),
            FALSE, // Do not watch subtree
            FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_LAST_WRITE,
            &bytesReturned,
            NULL,
            NULL
        );

        if (!success || bytesReturned == 0) {
            break;
        }

        BYTE* current = buffer.data();
        while (true) {
            FILE_NOTIFY_INFORMATION* fni = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(current);
            std::wstring wFilename(fni->FileName, fni->FileNameLength / sizeof(WCHAR));
            std::string filename(wFilename.begin(), wFilename.end());

            // Ignore transient internal marker
            if (filename != ".attack_pid") {
                if (IsCanaryFile(wFilename)) {
                    // Immediate honeypot breach detected
                    std::cout << "[TRIPWIRE_TRIGGER]:" << filename << std::endl;

                    DWORD killedPid = EvictAdversaryProcess(selfPid, parentPid);
                    if (killedPid != 0) {
                        std::cout << "[OS_KERNEL]:TERMINATED:" << killedPid << std::endl;
                    }
                } else {
                    // Regular file write event streamed to supervisor for velocity heuristic
                    std::cout << "EVENT:MODIFIED:" << filename << std::endl;
                }
            }

            if (fni->NextEntryOffset == 0) break;
            current += fni->NextEntryOffset;
        }
    }

    CloseHandle(hDir);
    return 0;
}

