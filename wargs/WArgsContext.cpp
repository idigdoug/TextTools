// Copyright (c) Doug Cook.
// Licensed under the MIT License.

#include "pch.h"
#include "WArgsContext.h"

WArgs::Context:: ~Context()
{
    WaitForAllProcessesToExit();
}

WArgs::Context::Context(WArgs const& wargs, bool useStdIn)
    : m_wargs(wargs)
    , m_slotHandles(wargs.m_maxProcs)
    , m_slotCount(wargs.m_maxProcs)
    , m_slotsActive()
    , m_exitCode()
    , m_conin()
    , m_nul()
    , m_hTtyForPrompt()
    , m_hStdInputForChild()
{
    assert(wargs.m_maxProcs <= MAXIMUM_WAIT_OBJECTS);

    if (m_wargs.m_interactive || m_wargs.m_openTty)
    {
        m_conin = OpenInputDevice(L"CONIN$");
    }

    m_hTtyForPrompt = m_wargs.m_interactive
        ? m_conin.get()
        : nullptr;

    if (m_wargs.m_openTty)
    {
        m_hStdInputForChild = m_conin.get();
    }
    else
    {
        auto const hStdInput = useStdIn ? GetStdHandle(STD_INPUT_HANDLE) : nullptr;
        if (hStdInput != nullptr &&
            hStdInput != INVALID_HANDLE_VALUE)
        {
            m_hStdInputForChild = hStdInput;
        }
        else
        {
            m_nul = OpenInputDevice(L"NUL");
            m_hStdInputForChild = m_nul.get();
        }
    }

    return;
}

bool
WArgs::Context::ExitCodeIsFatal() const noexcept
{
    return m_exitCode < 0;
}

int
WArgs::Context::UnsignedExitCode() const noexcept
{
    return abs(m_exitCode);
}

void
WArgs::Context::AccumulateExitCode(ExitCode current) noexcept
{
    if (current != 0)
    {
        if (m_exitCode == 0)
        {
            m_exitCode = current;
        }
        else if (m_exitCode > 0 && current < 0)
        {
            m_exitCode = current;
        }
    }
}

void
WArgs::Context::WaitForAllProcessesToExit()
{
    while (m_slotsActive)
    {
        WaitForProcessExit(true);
    }
}

void
WArgs::Context::StartProcess(_In_ PWSTR commandLine)
{
    assert(!ExitCodeIsFatal());
    assert(commandLine[0] != 0);
    assert(m_hStdInputForChild);

    UINT8 slotIndex;
    if (m_wargs.m_background)
    {
        slotIndex = 0;
    }
    else if (!AcquireSlotIndex(&slotIndex))
    {
        return;
    }

    if (m_wargs.m_interactive)
    {
        assert(m_hTtyForPrompt);

        WCHAR consoleInput[10];
        fprintf(stderr, "%ls?...", commandLine);

        DWORD cchRead = 0;
        if (!ReadConsoleW(m_hTtyForPrompt, &consoleInput, ARRAYSIZE(consoleInput), &cchRead, nullptr) ||
            cchRead == 0 ||
            (consoleInput[0] != L'y' && consoleInput[0] != 'Y'))
        {
            return;
        }
    }
    else if (m_wargs.m_verbose)
    {
        fprintf(stderr, "%ls\n", commandLine);
    }

    WCHAR slotString[3];
    swprintf_s(slotString, L"%0x", slotIndex);
    if (!m_wargs.m_processSlotVar.empty() &&
        !SetEnvironmentVariableW(m_wargs.m_processSlotVar.c_str(), slotString))
    {
        fprintf(stderr, "%hs: error : SetEnvironmentVariableW(%ls) error %u.\n",
            AppName, m_wargs.m_processSlotVar.c_str(), GetLastError());
        AccumulateExitCode(ExitCodeOtherError);
    }
    else
    {
        STARTUPINFOW si = { sizeof(si) };
        si.dwFlags = STARTF_USESTDHANDLES;
        si.hStdInput = m_hStdInputForChild;
        si.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
        si.hStdError = GetStdHandle(STD_ERROR_HANDLE);

        PROCESS_INFORMATION pi = {};
        if (!CreateProcessW(
            nullptr,
            commandLine,
            nullptr,
            nullptr,
            true, // Inherit handles
            0,
            nullptr, // Environment
            nullptr, // Current directory
            &si, &pi))
        {
            fprintf(stderr, "%hs: error : CreateProcessW error %u starting %ls\n",
                AppName, GetLastError(), m_wargs.m_command.c_str());
            AccumulateExitCode(ExitCodeFatalCommandNotFound);
        }
        else
        {
            TextToolsUniqueHandle hProcess(pi.hProcess);
            TextToolsUniqueHandle hThread(pi.hThread);
            if (!m_wargs.m_background)
            {
                SetSlot(slotIndex, std::move(hProcess));
            }
        }
    }
}

TextToolsUniqueHandle
WArgs::Context::OpenInputDevice(PCWSTR name) noexcept
{
    HANDLE hDevice = CreateFileW(
        name,
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr,
        OPEN_EXISTING,
        0,
        nullptr);
    if (hDevice == INVALID_HANDLE_VALUE)
    {
        fprintf(stderr, "%hs: error : CreateFile error %u opening '%ls'.\n",
            AppName, GetLastError(), name);
        AccumulateExitCode(ExitCodeFatalOtherError);
        hDevice = nullptr;
    }
    else if (!DuplicateHandle(
        GetCurrentProcess(), hDevice,
        GetCurrentProcess(), &hDevice,
        0,
        true, // Make inheritable.
        DUPLICATE_CLOSE_SOURCE | DUPLICATE_SAME_ACCESS))
    {
        fprintf(stderr, "%hs: error : DuplicateHandle error %u opening '%ls'.\n",
            AppName, GetLastError(), name);
        AccumulateExitCode(ExitCodeFatalOtherError);
        hDevice = nullptr;
    }

    return TextToolsUniqueHandle(hDevice);
}

_Success_(return) bool
WArgs::Context::AcquireSlotIndex(_Out_ UINT8* pSlotIndex)
{
    UINT8 slotIndex;
    bool ok;
    if (!WaitForProcessExit(m_slotsActive == m_slotCount))
    {
        slotIndex = 0;
        ok = false;
    }
    else
    {
        assert(m_slotsActive < m_slotCount);
        for (slotIndex = 0; slotIndex != m_slotCount; slotIndex += 1)
        {
            if (!m_slotHandles[slotIndex])
            {
                break;
            }
        }

        assert(slotIndex < m_slotCount);
        ok = true;
    }

    *pSlotIndex = slotIndex;
    return ok;
}

void
WArgs::Context::SetSlot(UINT8 slotIndex, TextToolsUniqueHandle value) noexcept
{
    assert(slotIndex < m_slotCount);
    assert(value);
    assert(!m_slotHandles[slotIndex]);
    assert(m_slotsActive < m_slotCount);
    m_slotHandles[slotIndex] = std::move(value);
    m_slotsActive += 1;
}

TextToolsUniqueHandle
WArgs::Context::ClearSlot(UINT8 slotIndex) noexcept
{
    assert(slotIndex < m_slotCount);
    assert(m_slotHandles[slotIndex]);
    assert(m_slotsActive != 0);
    m_slotsActive -= 1;
    return std::move(m_slotHandles[slotIndex]);
}

bool
WArgs::Context::WaitForProcessExit(bool block)
{
    HANDLE handles[MAXIMUM_WAIT_OBJECTS];
    UINT8 handleIndexToSlotIndex[MAXIMUM_WAIT_OBJECTS];
    DWORD timeout = block ? INFINITE : 0;

    // Loop until no objects signaled.
    while (m_slotsActive)
    {
        UINT8 handleIndex = 0;
        for (UINT8 slotIndex = 0; slotIndex != m_slotCount; slotIndex++)
        {
            auto const slotHandle = m_slotHandles[slotIndex].get();
            if (slotHandle)
            {
                handles[handleIndex] = slotHandle;
                handleIndexToSlotIndex[handleIndex] = slotIndex;
                handleIndex += 1;
            }
        }
        assert(handleIndex == m_slotsActive);

        auto const waitResult = WaitForMultipleObjects(handleIndex, handles, false, timeout);
        timeout = 0; // Block only first time through the loop.

        if (waitResult > WAIT_OBJECT_0 + handleIndex)
        {
            switch (waitResult)
            {
            case WAIT_TIMEOUT:
                break;
            case WAIT_FAILED:
                fprintf(stderr, "%hs: error : WaitForMultipleObjects failed with code %u.\n",
                    AppName, GetLastError());
                AccumulateExitCode(ExitCodeFatalOtherError);
                break;
            default:
                fprintf(stderr, "%hs: error : WaitForMultipleObjects returned unexpected result %u.\n",
                    AppName, waitResult);
                AccumulateExitCode(ExitCodeFatalOtherError);
                break;
            }
            break;
        }

        auto slotIndex = handleIndexToSlotIndex[waitResult - WAIT_OBJECT_0];
        auto hProcess = ClearSlot(slotIndex);

        DWORD processExitCode = 0;
        if (!GetExitCodeProcess(hProcess.get(), &processExitCode))
        {
            fprintf(stderr, "%hs: warning : GetExitCodeProcess failed with code %u.\n",
                AppName, GetLastError());
        }
        else if (processExitCode == 255)
        {
            fprintf(stderr, "%hs: error : process exit code %u (0x%x).\n",
                AppName, processExitCode, processExitCode);
            AccumulateExitCode(ExitCodeFatalCommandError);
        }
        else if (processExitCode != 0)
        {
            fprintf(stderr, "%hs: warning : process exit code %u (0x%x).\n",
                AppName, processExitCode, processExitCode);
            AccumulateExitCode(ExitCodeCommandError);
        }
    }

    return m_exitCode >= 0;
}
