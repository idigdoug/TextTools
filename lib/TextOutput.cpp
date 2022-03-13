// Copyright (c) Doug Cook.
// Licensed under the MIT License.

#include "pch.h"
#include <TextOutput.h>
#include <CodePageInfo.h>
#include "ByteOrderMark.h"
#include "Utility.h"

#include <stdexcept>
#include <assert.h>
#include <stdio.h>

using namespace TextToolsImpl;

unsigned constexpr WriteMax = 1u << 20;
char16_t constexpr BomChar = u'\xFEFF';

constexpr bool
TextOutput::IsFlagSet(TextOutputFlags flag) const noexcept
{
    return (m_flags & flag) != TextOutputFlags::None;
}

void
TextOutput::SetCodeConvert(unsigned codePage)
{
    CodeConvert codeConvert(codePage);
    auto const category = codeConvert.ThrowIfNotSupported();
    m_codeConvert = codeConvert;
    m_codeConvertUtf = category == CodePageCategory::Utf;
}

void
TextOutput::SetFlags(TextOutputFlags flags) noexcept
{
    m_flags = flags;
    m_wc2mbFlags = m_codeConvertUtf
        ? (IsFlagSet(TextOutputFlags::InvalidUtf16Error) ? WC_ERR_INVALID_CHARS : 0)
        : (IsFlagSet(TextOutputFlags::NoBestFitChars) ? WC_NO_BEST_FIT_CHARS : 0);
}

void
TextOutput::InsertBom()
{
    if (m_codeConvertUtf && IsFlagSet(TextOutputFlags::InsertBom))
    {
        WriteChars({ &BomChar, 1 }, nullptr, nullptr);
    }
}

void
TextOutput::FlushFile()
{
    assert(m_mode == TextOutputMode::File);

    size_t cbWritten = 0;
    size_t const cbToWrite = m_bytesPos;
    m_bytesPos = 0;

    while (cbWritten != cbToWrite)
    {
        DWORD cbBatch = cbToWrite - cbWritten > WriteMax
            ? WriteMax
            : static_cast<DWORD>(cbToWrite - cbWritten);
        if (!WriteFile(m_outputHandle, &m_bytes[cbWritten], cbBatch, &cbBatch, nullptr))
        {
            auto lastError = GetLastError();
            throw std::runtime_error("WriteFile error " + std::to_string(lastError));
        }

        assert(cbBatch != 0);
        cbWritten += cbBatch;
    }
}

void
TextOutput::FlushConsole(std::u16string_view pendingChars)
{
    assert(m_mode == TextOutputMode::Console);

    size_t cchWritten = 0;
    size_t const cchToWrite = pendingChars.size();

    while (cchWritten != cchToWrite)
    {
        DWORD cchBatch = cchToWrite - cchWritten > WriteMax
            ? WriteMax
            : static_cast<DWORD>(cchToWrite - cchWritten);

        auto const lastChar = pendingChars[cchWritten + cchBatch - 1];
        if (0xD800 <= lastChar && lastChar < 0xDC00)
        {
            // Don't end batch with a high surrogate.
            cchBatch -= 1;
            if (cchBatch == 0)
            {
                SaveRemainingChars(pendingChars, cchWritten);
                break;
            }
        }

        if (!WriteConsoleW(m_outputHandle, &pendingChars[cchWritten], cchBatch, &cchBatch, nullptr))
        {
            auto lastError = GetLastError();
            throw std::runtime_error("WriteConsoleW error " + std::to_string(lastError));
        }

        assert(cchBatch != 0);
        cchWritten += cchBatch;
    }
}

void
TextOutput::ConvertAndAppendBytes(
    std::u16string_view pendingChars,
    _In_opt_ PCCH pDefaultChar,
    _Inout_opt_ bool* pUsedDefaultChar)
{
    size_t pendingCharsPos = 0;
    LSTATUS status = m_codeConvert.Utf16ToEncoded(
        pendingChars, pendingCharsPos,
        m_bytes, m_bytesPos,
        m_wc2mbFlags,
        m_codeConvertUtf ? nullptr : pDefaultChar,
        m_codeConvertUtf ? nullptr : pUsedDefaultChar);
    if (pendingChars.size() != pendingCharsPos)
    {
        SaveRemainingChars(pendingChars, pendingCharsPos);
    }

    if (status != ERROR_SUCCESS)
    {
        if (status == ERROR_NO_UNICODE_TRANSLATION)
        {
            throw std::range_error("Input is not valid UTF-16LE.");
        }
        else
        {
            throw std::runtime_error("UTF16-to-MBCS conversion error " +
                std::to_string(status) +
                ".");
        }
    }
}

void
TextOutput::SaveRemainingChars(std::u16string_view pendingChars, size_t pendingCharsPos)
{
    assert(pendingCharsPos < pendingChars.size());
    auto const cchRemaining = pendingChars.size() - pendingCharsPos;

    // If we're about to resize, pendingChars better not point into m_chars.
    assert(
        cchRemaining <= m_chars.size() ||
        pendingChars.data() + pendingChars.size() < m_chars.data() ||
        m_chars.data() + m_chars.capacity() < pendingChars.data());

    EnsureSize(m_chars, cchRemaining);
    memmove(m_chars.data(), pendingChars.data() + pendingCharsPos, cchRemaining * sizeof(char16_t));

    m_charsPos = cchRemaining;
}

std::u16string_view
TextOutput::ConsumePendingChars(std::u16string_view newChars)
{
    if (IsFlagSet(TextOutputFlags::ExpandCRLF))
    {
        AppendCharsAndExpandCRLF(newChars);
    }
    else if (m_charsPos != 0)
    {
        AppendChars(newChars);
    }
    else
    {
        // Don't buffer in m_chars.
        return newChars;
    }

    auto const charsPos = m_charsPos;
    m_charsPos = 0;
    return { m_chars.data(), charsPos };
}

void
TextOutput::AppendCharsAndExpandCRLF(std::u16string_view newChars)
{
    auto const pchSrc = newChars.data();
    auto const cchSrc = newChars.size();
    auto cchDest = m_charsPos;
    auto cchDestRequired = cchDest + cchSrc < cchDest
        ? ~(size_t)0 // Force exception from EnsureSize.
        : cchDest + cchSrc;

    EnsureSize(m_chars, cchDestRequired);
    auto pchDest = m_chars.data();

    for (size_t i = 0; i != cchSrc; i++)
    {
        auto const ch = pchSrc[i];
        if (ch != L'\n')
        {
            // Tight loop in the non-LF case.
            pchDest[cchDest++] = ch;
        }
        else
        {
            // Possible resize needed in the LF case.
            cchDestRequired += 1;
            EnsureSize(m_chars, cchDestRequired);
            pchDest = m_chars.data();

            pchDest[cchDest++] = L'\r';
            pchDest[cchDest++] = L'\n';
        }
    }

    m_charsPos = cchDest;
}

void
TextOutput::AppendChars(std::u16string_view newChars)
{
    EnsureSize(m_chars, m_charsPos, newChars.size());
    memcpy(m_chars.data() + m_charsPos, newChars.data(), newChars.size() * sizeof(char16_t));
    m_charsPos += newChars.size();
}

void
TextOutput::OpenHandle(
    TextToolsUniqueHandle outputOwner,
    _In_ HANDLE outputHandle,
    unsigned codePage,
    TextOutputFlags flags)
{
    auto const fileType = GetFileType(outputHandle);
    if (fileType == FILE_TYPE_UNKNOWN)
    {
        auto lastError = GetLastError();
        if (lastError != ERROR_SUCCESS)
        {
            throw std::runtime_error("GetFileType error " + std::to_string(lastError));
        }
    }

    Close();

    // Note: Even if the file ends up Console, we want to validate the codePage parameter.
    SetCodeConvert(codePage);
    m_mode = TextOutputMode::File;
    SetFlags(flags);

    m_outputOwner = std::move(outputOwner);
    m_outputHandle = outputHandle;

    if (IsFlagSet(TextOutputFlags::CheckConsole) && fileType == FILE_TYPE_CHAR)
    {
        DWORD consoleMode;
        if (GetConsoleMode(m_outputHandle, &consoleMode))
        {
            m_codeConvert = CodeConvert(CodePageUtf16LE);
            m_codeConvertUtf = true;
            m_mode = TextOutputMode::Console;
            SetFlags(flags);
            goto Done; // Note: Don't write BOM to console.
        }
    }

    InsertBom();

Done:

    return;
}

TextOutput::~TextOutput()
{
    Flush();
}

TextOutput::TextOutput() noexcept
    : m_bytes()
    , m_chars()
    , m_outputOwner()
    , m_outputHandle()
    , m_codeConvert()
    , m_codeConvertUtf()
    , m_mode()
    , m_flags()
    , m_wc2mbFlags()
    , m_bytesPos()
    , m_charsPos()
{
    return;
}

void
TextOutput::Flush()
{
    if (m_mode == TextOutputMode::File)
    {
        FlushFile();
    }
}

void
TextOutput::Close() noexcept
{
    Flush();
    m_outputOwner.reset();
    m_outputHandle = {};
    m_codeConvert = {};
    m_codeConvertUtf = {};
    m_mode = {};
    m_flags = {};
    m_wc2mbFlags = {};
    m_bytesPos = {};
    m_charsPos = {};
}

TextOutputMode
TextOutput::Mode() const noexcept
{
    return m_mode;
}

void
TextOutput::OpenChars(
    TextOutputFlags flags)
{
    Close();

    m_codeConvert = CodeConvert(CodePageUtf16LE);
    m_codeConvertUtf = true;
    m_mode = TextOutputMode::Chars;
    SetFlags(flags);

    InsertBom();
}

void
TextOutput::OpenBytes(
    unsigned codePage,
    TextOutputFlags flags)
{
    Close();

    SetCodeConvert(codePage);
    m_mode = TextOutputMode::Bytes;
    SetFlags(flags);

    InsertBom();
}

void
TextOutput::OpenBorrowedHandle(
    _In_ HANDLE outputHandle,
    unsigned codePage,
    TextOutputFlags flags)
{
    OpenHandle({}, outputHandle, codePage, flags);
}

LSTATUS
TextOutput::OpenFile(
    _In_ PCWSTR outputFile,
    unsigned codePage,
    TextOutputFlags flags)
{
    LSTATUS status;

    HANDLE const outputHandle = CreateFileW(
        outputFile,
        FILE_WRITE_DATA,
        FILE_SHARE_READ | FILE_SHARE_DELETE,
        nullptr,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN,
        nullptr);
    if (outputHandle == INVALID_HANDLE_VALUE)
    {
        status = GetLastError();
    }
    else
    {
        OpenHandle(TextToolsUniqueHandle(outputHandle), outputHandle, codePage, flags);
        status = ERROR_SUCCESS;
    }

    return status;
}

std::u16string_view
TextOutput::BufferedChars() const
{
    assert(m_mode == TextOutputMode::Chars);
    return { m_chars.data(), m_charsPos };
}

std::string_view
TextOutput::BufferedBytes() const
{
    assert(m_mode == TextOutputMode::Bytes);
    return { m_bytes.data(), m_bytesPos };
}

void
TextOutput::WriteChars(
    std::u16string_view chars,
    _In_opt_ PCCH pDefaultChar,
    _Inout_opt_ bool* pUsedDefaultChar)
{
    switch (m_mode)
    {
    default:
        assert(false);
        break;

    case TextOutputMode::Chars:
        if (IsFlagSet(TextOutputFlags::ExpandCRLF))
        {
            AppendCharsAndExpandCRLF(chars);
        }
        else
        {
            AppendChars(chars);
        }
        break;

    case TextOutputMode::Console:
        FlushConsole(ConsumePendingChars(chars));
        break;

    case TextOutputMode::Bytes:
        ConvertAndAppendBytes(ConsumePendingChars(chars), pDefaultChar, pUsedDefaultChar);
        break;

    case TextOutputMode::File:
        ConvertAndAppendBytes(ConsumePendingChars(chars), pDefaultChar, pUsedDefaultChar);
        if (m_bytesPos >= 16384)
        {
            FlushFile();
        }
        break;
    }

    return;
}
