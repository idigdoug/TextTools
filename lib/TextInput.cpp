// Copyright (c) Doug Cook.
// Licensed under the MIT License.

#include "pch.h"
#include <TextInput.h>
#include <CodePageInfo.h>
#include "ByteOrderMark.h"
#include "Utility.h"

#include <stdexcept>
#include <assert.h>
#include <stdio.h>

using namespace TextToolsImpl;

static constexpr unsigned ReadMax = 0x1fffffff; // Max value to be used in ReadFile or ReadConsole.
static constexpr unsigned FileBufferSize = 4096;
static constexpr unsigned ConsoleBufferSize = 2048;

constexpr bool
TextInput::IsFlagSet(TextInputFlags flag) const noexcept
{
    return (m_flags & flag) != TextInputFlags::None;
}

void
TextInput::SetCodeConvert(unsigned codePage)
{
    CodeConvert codeConvert(codePage);
    codeConvert.ThrowIfNotSupported();
    m_codeConvert = codeConvert;
}

void
TextInput::FoldCRLF() noexcept
{
    assert(m_charsPos <= m_chars.size());

    if (!IsFlagSet(TextInputFlags::FoldCRLF) || m_charsPos == 0)
    {
        return;
    }

    bool const skipNextCharIfNewline = m_skipNextCharIfNewline;
    m_skipNextCharIfNewline = false;

    auto const pChars = m_chars.data();
    size_t iInput, iOutput;
    if (skipNextCharIfNewline && pChars[0] == L'\n')
    {
        // Last chunk ended on "\r", which we converted to "\n".
        // This chunk starts with "\n", so it was a "\r\n" sequence.
        // We already wrote the corresponding "\n" so skip it.
        iInput = 1;
        iOutput = 0;
    }
    else
    {
        auto const pFirstCR = (char16_t*)wmemchr((WCHAR*)pChars, L'\r', m_charsPos);
        if (!pFirstCR)
        {
            // If there is nothing to skip or convert, we're done.
            return;
        }

        iInput = pFirstCR - pChars;
        iOutput = iInput;
    }

    for (; iInput != m_charsPos; iInput += 1)
    {
        auto const ch = pChars[iInput];
        if (ch != L'\r')
        {
            pChars[iOutput++] = ch;
        }
        else if (iInput + 1 == m_charsPos)
        {
            // "\r" at end of chunk. Assume lone "\r" and convert it to "\n".
            // If next chunk starts with "\n" we will need to skip it.
            m_skipNextCharIfNewline = true;
            pChars[iOutput++] = L'\n';
            break;
        }
        else if (pChars[iInput + 1] != L'\n')
        {
            // Lone "\r", convert to "\n".
            pChars[iOutput++] = L'\n';
        }
        else
        {
            // "\r\n" sequence, ignore the "\r".
        }
    }

    m_charsPos = iOutput;
}

void
TextInput::ConsumeBytes(size_t consumedBytes) noexcept
{
    if (consumedBytes >= m_bytesPos)
    {
        assert(consumedBytes == m_bytesPos);
        m_bytesPos = 0;
    }
    else if (consumedBytes != 0)
    {
        m_bytesPos -= consumedBytes;
        memmove(m_bytes.data(), m_bytes.data() + consumedBytes, m_bytesPos);
    }
}

void
TextInput::Convert()
{
    assert(m_mode == TextInputMode::Bytes || m_mode == TextInputMode::File);
    assert(m_bytesPos <= m_bytes.size());

    m_charsPos = 0;

    size_t consumedBytes = 0;
    LSTATUS status = m_codeConvert.EncodedToUtf16(
        std::string_view(m_bytes.data(), m_bytesPos), consumedBytes,
        m_chars, m_charsPos,
        IsFlagSet(TextInputFlags::InvalidMbcsError) ? MB_ERR_INVALID_CHARS : 0);
    ConsumeBytes(consumedBytes);
    FoldCRLF();

    if (status != ERROR_SUCCESS)
    {
        if (status == ERROR_NO_UNICODE_TRANSLATION)
        {
            throw std::range_error("Input is not valid for encoding " +
                std::to_string(m_codeConvert.CodePage()) +
                ".");
        }
        else
        {
            throw std::runtime_error("MBCS-to-UTF16 conversion error " +
                std::to_string(status) +
                ".");
        }
    }
}

void
TextInput::ReadBytesFromFile()
{
    auto const cbRemainingBuffer = m_bytes.size() - m_bytesPos;
    ReadBytesFromFile(cbRemainingBuffer < ReadMax
        ? (DWORD)cbRemainingBuffer
        : ReadMax);
}

void
TextInput::ReadBytesFromFile(DWORD cbMaxToRead)
{
    assert(m_inputHandle);
    assert(m_bytes.size() > m_bytesPos);
    assert(m_bytes.size() - m_bytesPos >= cbMaxToRead);

    DWORD cbRead = 0;
    if (!ReadFile(m_inputHandle, m_bytes.data() + m_bytesPos, cbMaxToRead, &cbRead, nullptr))
    {
        auto lastError = GetLastError();
        if (lastError != ERROR_BROKEN_PIPE)
        {
            throw std::runtime_error("ReadFile error " + std::to_string(lastError));
        }
    }

    m_bytesPos += cbRead;

    if (cbRead == 0)
    {
        m_inputOwner.reset();
        m_inputHandle = nullptr;
    }
}

void
TextInput::ReadCharsFromConsole()
{
    assert(m_inputHandle);
    assert(ConsoleBufferSize <= m_chars.size());
    assert(m_charsPos == 0);

    CONSOLE_READCONSOLE_CONTROL control = {};
    control.nLength = sizeof(control);
    control.dwCtrlWakeupMask = IsFlagSet(TextInputFlags::ConsoleCtrlZ)
        ? 1u << 26
        : 0u;

    DWORD const cchMaxToRead =
        m_chars.size() < ReadMax
        ? (DWORD)m_chars.size()
        : ReadMax;
    DWORD cchRead = 0;
    if (!ReadConsoleW(m_inputHandle, m_chars.data(), cchMaxToRead, &cchRead, &control))
    {
        auto lastError = GetLastError();
        throw std::runtime_error("ReadConsoleW error " + std::to_string(lastError));
    }

    m_charsPos = cchRead;

    if (cchRead == 0)
    {
        m_inputOwner.reset();
        m_inputHandle = nullptr;
    }
}

void
TextInput::OpenHandle(
    TextToolsUniqueHandle inputOwner,
    _In_ HANDLE inputHandle,
    unsigned codePage,
    TextInputFlags flags)
{
    auto const fileType = GetFileType(inputHandle);
    if (fileType == FILE_TYPE_UNKNOWN)
    {
        auto lastError = GetLastError();
        if (lastError != ERROR_SUCCESS)
        {
            throw std::runtime_error("GetFileType error " + std::to_string(lastError));
        }
    }

    Close();

    // Note: Even if the text ends up having a BOM or Console, we want to validate the codePage parameter.
    SetCodeConvert(codePage);
    m_mode = TextInputMode::File;
    m_flags = flags;

    m_inputOwner = std::move(inputOwner);
    m_inputHandle = inputHandle;

    if (IsFlagSet(TextInputFlags::CheckConsole) && fileType == FILE_TYPE_CHAR)
    {
        DWORD consoleMode;
        if (GetConsoleMode(m_inputHandle, &consoleMode))
        {
            EnsureSize(m_chars, ConsoleBufferSize);
            m_codeConvert = CodeConvert(CodePageUtf16LE);
            m_mode = TextInputMode::Console;
            goto Done; // Note: Don't consume BOM from console.
        }
    }

    EnsureSize(m_bytes, FileBufferSize);

    if (IsFlagSet(TextInputFlags::ConsumeBom))
    {
        ReadBytesFromFile(4);
        for (auto& bomInfo : ByteOrderMark::Standard)
        {
            for (;;)
            {
                auto match = bomInfo.Match({ m_bytes.data(), m_bytesPos });
                if (match == ByteOrderMatch::Yes)
                {
                    ConsumeBytes(bomInfo.Size);
                    m_codeConvert = CodeConvert(bomInfo.CodePage);
                    goto Done;
                }
                else if (match == ByteOrderMatch::No)
                {
                    break;
                }
                else if (!m_inputHandle)
                {
                    // NeedMoreData but EOF. Don't goto Done.  Consider a 2-byte file with
                    // UTF16 BOM. UTF32 will want more data but we want UTF16 to match.
                    break;
                }
                else
                {
                    assert(bomInfo.Size > m_bytesPos);
                    ReadBytesFromFile(bomInfo.Size - (DWORD)m_bytesPos);
                }
            }
        }
    }

Done:

    ReadNextChars();
    return;
}

TextInput::TextInput() noexcept
    : m_bytes()
    , m_chars()
    , m_inputOwner()
    , m_inputHandle()
    , m_codeConvert()
    , m_mode()
    , m_flags()
    , m_skipNextCharIfNewline()
    , m_bytesPos()
    , m_charsPos()
{
    return;
}

void
TextInput::Close() noexcept
{
    m_inputOwner.reset();
    m_inputHandle = {};
    m_codeConvert = {};
    m_mode = {};
    m_flags = {};
    m_skipNextCharIfNewline = {};
    m_bytesPos = {};
    m_charsPos = {};
}

TextInputMode
TextInput::Mode() const noexcept
{
    return m_mode;
}

void
TextInput::OpenChars(
    std::u16string_view inputChars,
    TextInputFlags flags)
{
    Close();

    m_codeConvert = CodeConvert(CodePageUtf16LE);
    m_mode = TextInputMode::Chars;
    m_flags = flags;

    if (IsFlagSet(TextInputFlags::ConsumeBom) && inputChars.starts_with(u'\xFEFF'))
    {
        inputChars.remove_prefix(1);
    }

    EnsureSize(m_chars, inputChars.size());
    memcpy(m_chars.data(), inputChars.data(), inputChars.size() * sizeof(char16_t));
    m_charsPos = inputChars.size();

    FoldCRLF();
}

void
TextInput::OpenBytes(
    std::string_view inputBytes,
    unsigned codePage,
    TextInputFlags flags)
{
    Close();

    // Note: Even if the text ends up having a BOM or Console, we want to validate the codePage parameter.
    SetCodeConvert(codePage);
    m_mode = TextInputMode::Bytes;
    m_flags = flags;

    size_t consumedBytes = 0;

    if (IsFlagSet(TextInputFlags::ConsumeBom))
    {
        for (auto& bomInfo : ByteOrderMark::Standard)
        {
            if (bomInfo.Match(inputBytes) == ByteOrderMatch::Yes)
            {
                m_codeConvert = CodeConvert(bomInfo.CodePage);
                consumedBytes = bomInfo.Size;
                break;
            }
        }
    }

    LSTATUS status = m_codeConvert.EncodedToUtf16(
        inputBytes, consumedBytes,
        m_chars, m_charsPos,
        IsFlagSet(TextInputFlags::InvalidMbcsError) ? MB_ERR_INVALID_CHARS : 0);
    FoldCRLF();

    // Copy any bytes that we couldn't convert.
    EnsureSize(m_bytes, inputBytes.size() - consumedBytes);
    memcpy(m_bytes.data(), inputBytes.data() + consumedBytes, inputBytes.size() - consumedBytes);
    m_bytesPos = inputBytes.size() - consumedBytes;

    if (status != ERROR_SUCCESS)
    {
        throw std::range_error("Conversion error " + std::to_string(status));
    }
}

void
TextInput::OpenBorrowedHandle(
    _In_ HANDLE inputHandle,
    unsigned codePage,
    TextInputFlags flags)
{
    OpenHandle({}, inputHandle, codePage, flags);
}

LSTATUS
TextInput::OpenFile(
    _In_ PCWSTR inputFile,
    unsigned codePage,
    TextInputFlags flags)
{
    LSTATUS status;

    HANDLE const inputHandle = CreateFileW(
        inputFile,
        FILE_READ_DATA,
        FILE_SHARE_READ | FILE_SHARE_DELETE,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN,
        nullptr);
    if (inputHandle == INVALID_HANDLE_VALUE)
    {
        status = GetLastError();
    }
    else
    {
        OpenHandle(TextToolsUniqueHandle(inputHandle), inputHandle, codePage, flags);
        status = ERROR_SUCCESS;
    }

    return status;
}

std::u16string_view
TextInput::Chars() const noexcept
{
    return { m_chars.data(), m_charsPos };
}

bool
TextInput::ReadNextChars()
{
    assert(m_mode != TextInputMode::None);
    m_charsPos = 0;

    if (m_mode == TextInputMode::Console)
    {
        while (m_inputHandle && m_charsPos == 0)
        {
            ReadCharsFromConsole();
            FoldCRLF();
        }
    }
    else
    {
        while (m_inputHandle && m_charsPos == 0)
        {
            ReadBytesFromFile();
            Convert();
        }
    }

    return m_charsPos != 0;
}
