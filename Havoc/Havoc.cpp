    // SPDX-License-Identifier: GPL-3.0-or-later

    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
    #include <string>
    #include <memory>
    #include <filesystem>
    #include <algorithm>
    #include <cctype>
    #include <type_traits>

    struct HandleCloser {
        void operator()(HANDLE h) const noexcept {
            if (h && h != INVALID_HANDLE_VALUE) { CloseHandle(h); }
        }
    };

    struct RemoteMemory {
        HANDLE process = nullptr;
        void* address = nullptr;
        ~RemoteMemory() { if (process && address) { VirtualFreeEx(process, address, 0, MEM_RELEASE); } }
    };

    static bool EndsWith(const std::wstring &str, const std::wstring &suffix) {
        if (str.length() < suffix.length()) { return false; }
        return std::equal(suffix.rbegin(), suffix.rend(), str.rbegin(), [](wchar_t a, wchar_t b) { return towlower(a) == towlower(b); });
    }

    // Command line: "Path to .dll" "Path to target executable"
    int wmain(int argc, wchar_t** argv) {
        std::wstring dllPath;
        std::wstring targetPath;
        std::wstring targetDirectory;

        if (argc < 3) { return 1; }

        dllPath = argv[1];
        if (!EndsWith(dllPath, L".dll")) { return 1; }
        targetPath = argv[2];   
        if (!EndsWith(targetPath, L".exe")) { return 1; }
        targetDirectory = std::filesystem::path(targetPath).parent_path().wstring();

        PROCESS_INFORMATION processInfo {};
        STARTUPINFOW startupInfo { sizeof(startupInfo) };

        HANDLE hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
        HANDLE hInheritableStdout = nullptr;
        if (hStdout == INVALID_HANDLE_VALUE) { return 2; }
        if (!DuplicateHandle(GetCurrentProcess(), hStdout, GetCurrentProcess(), &hInheritableStdout, 0, TRUE, DUPLICATE_SAME_ACCESS)) { return 2; }

        startupInfo.dwFlags |= STARTF_USESTDHANDLES;
        startupInfo.hStdOutput = hInheritableStdout;
        startupInfo.hStdError = hInheritableStdout;

        if (!CreateProcessW(targetPath.c_str(), nullptr, nullptr, nullptr, TRUE, CREATE_SUSPENDED, nullptr, targetDirectory.c_str(), &startupInfo, &processInfo)) {  
            CloseHandle(hInheritableStdout);
            return 3;
        }

        CloseHandle(hInheritableStdout);

        std::unique_ptr<std::remove_pointer<HANDLE>::type, HandleCloser> hProcess(processInfo.hProcess);
        std::unique_ptr<std::remove_pointer<HANDLE>::type, HandleCloser> hThread(processInfo.hThread);

        RemoteMemory remoteMem { hProcess.get(), VirtualAllocEx(hProcess.get(), nullptr, (dllPath.size() + 1) * sizeof(wchar_t), MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE) };
        if (!remoteMem.address) { return 4; }

        if (!WriteProcessMemory(hProcess.get(), remoteMem.address, dllPath.c_str(), (dllPath.size() + 1) * sizeof(wchar_t), nullptr)) { return 5; }

        HMODULE kernel32Base = GetModuleHandleW(L"kernel32.dll");
        if (!kernel32Base) { return 6; }

        typedef HMODULE (__stdcall *LoadLibraryWFn)(LPCWSTR);
        auto pLoadLibraryW = reinterpret_cast<LoadLibraryWFn>(GetProcAddress(kernel32Base, "LoadLibraryW"));
        if (!pLoadLibraryW) { return 6; }

        std::unique_ptr<std::remove_pointer<HANDLE>::type, HandleCloser> remoteThread(CreateRemoteThread(hProcess.get(), nullptr, 0, (LPTHREAD_START_ROUTINE)pLoadLibraryW, remoteMem.address, 0, nullptr));
        if (!remoteThread) { return 7; }

        DWORD threadExitCode = 0;
        if (WaitForSingleObject(remoteThread.get(), INFINITE) != WAIT_OBJECT_0 || !GetExitCodeThread(remoteThread.get(), &threadExitCode)) { return 8; }

        if (threadExitCode == 0) { return 9; }

        if (ResumeThread(hThread.get()) == static_cast<DWORD>(-1)) { TerminateProcess(hProcess.get(), 1); }

        return 0;
    }