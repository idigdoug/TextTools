// Copyright (c) Doug Cook.
// Licensed under the MIT License.

#include "pch.h"
#include <ClipboardText.h>
#include <CodePageInfo.h>
#include <TextInput.h>
#include "Utility.h"

#include <assert.h>

namespace TextToolsImpl
{
    class ClipboardOwnership
    {
        bool const m_opened;

    public:

        ClipboardOwnership(ClipboardOwnership const&) = delete;
        void operator=(ClipboardOwnership const&) = delete;

        ~ClipboardOwnership()
        {
            if (m_opened)
            {
                verify(CloseClipboard());
            }
        }

        ClipboardOwnership()
            : m_opened(0 != OpenClipboard(nullptr)) {}

        explicit constexpr
        operator bool() const noexcept
        {
            return m_opened;
        }
    };

    struct GlobalFree_delete
    {
        void
        operator()(HANDLE hMem) const noexcept
        {
            verify(nullptr == GlobalFree(hMem));
        }
    };

    struct GlobalUnlock_delete
    {
        void
        operator()(HANDLE hMem) const noexcept
        {
            verify(GlobalUnlock(hMem));
        }
    };
}

using namespace TextToolsImpl;
using unique_hglobal = std::unique_ptr<void, GlobalFree_delete>;
using unique_hgloballock = std::unique_ptr<void, GlobalUnlock_delete>;

LSTATUS
ClipboardTextGet(std::wstring& value)
{
    LSTATUS status;
    value.clear();

    ClipboardOwnership clipboard;
    if (!clipboard)
    {
        status = GetLastError();
    }
    else if (auto hClipMem = GetClipboardData(CF_UNICODETEXT);
        !hClipMem)
    {
        status = GetLastError();
    }
    else if (auto const pchClip = (PCWSTR)GlobalLock(hClipMem);
        !pchClip)
    {
        status = GetLastError();
    }
    else
    {
        unique_hgloballock hClipMemLock(hClipMem);
        auto const cchClip = pchClip ? wcslen(pchClip) : 0u;
        value.assign(pchClip, cchClip);
        status = ERROR_SUCCESS;
    }

    return status;
}

LSTATUS
TextInput::OpenClipboard(TextInputFlags flags)
{
    LSTATUS status;

    ClipboardOwnership clipboard;
    if (!clipboard)
    {
        status = GetLastError();
    }
    else if (auto hClipMem = GetClipboardData(CF_UNICODETEXT);
        !hClipMem)
    {
        status = GetLastError();
    }
    else if (auto const pchClip = (PCWSTR)GlobalLock(hClipMem);
        !pchClip)
    {
        status = GetLastError();
    }
    else
    {
        unique_hgloballock hClipMemLock(hClipMem);
        static_assert(sizeof(char16_t) == sizeof(wchar_t));
        OpenChars({ pchClip ? (char16_t const*)pchClip : u"" }, flags);
        status = ERROR_SUCCESS;
    }

    return status;
}

LSTATUS
ClipboardTextSet(std::wstring_view value)
{
    LSTATUS status;

    unique_hglobal hClipMem(GlobalAlloc(GMEM_MOVEABLE, (value.size() + 1) * sizeof(value[0])));
    if (!hClipMem)
    {
        status = GetLastError();
    }
    else if (auto const pchClip = (char16_t*)GlobalLock(hClipMem.get());
        !pchClip)
    {
        status = GetLastError();
    }
    else
    {
        unique_hgloballock hClipMemLock(hClipMem.get());
        memcpy(pchClip, value.data(), value.size() * sizeof(value[0]));
        pchClip[value.size()] = 0;
        hClipMemLock.reset();

        ClipboardOwnership clipboard;
        if (!clipboard)
        {
            status = GetLastError();
        }
        else if (!EmptyClipboard())
        {
            status = GetLastError();
        }
        else if (!SetClipboardData(CF_UNICODETEXT, hClipMem.get()))
        {
            status = GetLastError();
        }
        else
        {
            (void)hClipMem.release();
            status = ERROR_SUCCESS;
        }
    }

    return status;
}
