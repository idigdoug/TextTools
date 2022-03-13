// Copyright (c) Doug Cook.
// Licensed under the MIT License.

#pragma once 
#include "WArgs.h"
#include <TextToolsCommon.h>

class WArgs::Context
{
private:

    WArgs const& m_wargs;
    std::vector<TextToolsUniqueHandle> m_slotHandles;
    UINT8 const m_slotCount;
    UINT8 m_slotsActive;
    ExitCode m_exitCode;
    TextToolsUniqueHandle m_conin;
    TextToolsUniqueHandle m_nul;
    HANDLE m_hTtyForPrompt;
    HANDLE m_hStdInputForChild;

public:

    Context(Context const&) = delete;
    void operator=(Context const&) = delete;

    ~Context();

    Context(WArgs const& wargs, bool useStdIn);

    bool
    ExitCodeIsFatal() const noexcept;

    int
    UnsignedExitCode() const noexcept;

    void
    AccumulateExitCode(ExitCode current) noexcept;

    void
    WaitForAllProcessesToExit();

    void
    StartProcess(_In_ PWSTR commandLine);

private:

    TextToolsUniqueHandle
    OpenInputDevice(PCWSTR name) noexcept;

    _Success_(return) bool
    AcquireSlotIndex(_Out_ UINT8* pSlotIndex);

    void
    SetSlot(UINT8 slotIndex, TextToolsUniqueHandle value) noexcept;

    TextToolsUniqueHandle
    ClearSlot(UINT8 slotIndex) noexcept;

    bool
    WaitForProcessExit(bool block);
};
