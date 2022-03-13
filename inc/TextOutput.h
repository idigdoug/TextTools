// Copyright (c) Doug Cook.
// Licensed under the MIT License.

#pragma once
#include "TextToolsCommon.h"
#include "CodeConvert.h"
#include <memory>
#include <string>
#include <string_view>

enum class TextOutputFlags : uint8_t
{
    None = 0,
    ExpandCRLF = 0x01, // Convert LF to CRLF.
    InsertBom = 0x02, // If encoding is UTF, insert a BOM at start of output.
    InvalidUtf16Error = 0x04, // Use WC_ERR_INVALID_CHARS in conversion (affects UTF output only).
    NoBestFitChars = 0x08, // Use WC_NO_BEST_FIT_CHARS (affects non-UTF output only).
    CheckConsole = 0x10, // If output is a console, use WriteConsoleW and override codepage.
    Default = InvalidUtf16Error | NoBestFitChars | CheckConsole
};
DEFINE_ENUM_FLAG_OPERATORS(TextOutputFlags);

enum class TextOutputMode : uint8_t
{
    None,
    Chars,
    Bytes,
    File,
    Console,
};

class TextOutput
{
    std::string m_bytes;
    std::u16string m_chars;

    TextToolsUniqueHandle m_outputOwner;
    HANDLE m_outputHandle;
    CodeConvert m_codeConvert;
    bool m_codeConvertUtf;
    TextOutputMode m_mode;
    TextOutputFlags m_flags;
    unsigned m_wc2mbFlags;

    size_t m_bytesPos;
    size_t m_charsPos;

    constexpr bool
    IsFlagSet(TextOutputFlags flag) const noexcept;

    void
    SetCodeConvert(unsigned codePage);

    // Sets m_flags, then updates m_wc2mbFlags.
    void
    SetFlags(TextOutputFlags flags) noexcept;

    // Inserts BOM (or not) based on m_codeConvertUtf and m_flags.
    void
    InsertBom();

    void
    FlushFile();

    void
    FlushConsole(std::u16string_view pendingChars);

    /*
    Converts pendingChars to bytes, appends result to m_bytes.
    Calls SaveRemainingChars if needed (e.g. trailing high surrogate).
    */
    void
    ConvertAndAppendBytes(
        std::u16string_view pendingChars,
        _In_opt_ PCCH pDefaultChar,
        _Inout_opt_ bool* pUsedDefaultChar);

    void
    SaveRemainingChars(std::u16string_view pendingChars, size_t pendingCharsPos);

    /*
    Appends newChars to pending chars, then expands CRLF,
    then returns the result, clearing pending chars to empty.
    */
    std::u16string_view
    ConsumePendingChars(std::u16string_view newChars);

    void
    AppendCharsAndExpandCRLF(std::u16string_view newChars);

    void
    AppendChars(std::u16string_view newChars);

    void
    OpenHandle(
        TextToolsUniqueHandle outputOwner,
        _In_ HANDLE outputHandle,
        unsigned codePage,
        TextOutputFlags flags);

public:

    /*
    Flushes and closes any existing output.
    */
    ~TextOutput();

    TextOutput() noexcept;

    /*
    Writes any buffered bytes to file/console (as appropriate for Mode).
    */
    void
    Flush();

    /*
    Flushes and closes any existing output.
    */
    void
    Close() noexcept;

    /*
    Gets the current mode of operation.
    */
    TextOutputMode
    Mode() const noexcept;

    /*
    Flushes and closes any existing output, then opens with Mode = Chars
    (buffer chars in-memory).
    */
    void
    OpenChars(
        TextOutputFlags flags = TextOutputFlags::Default);

    /*
    Flushes and closes any existing output, then opens with Mode = Bytes
    (convert and buffer bytes in-memory).
    */
    void
    OpenBytes(
        unsigned codePage = CP_ACP,
        TextOutputFlags flags = TextOutputFlags::Default);

    /*
    Flushes and closes any existing output, then opens with Mode = File/Console
    (convert and write bytes to file/console).
    */
    void
    OpenBorrowedHandle(
        _In_ HANDLE outputHandle,
        unsigned codePage = CP_ACP,
        TextOutputFlags flags = TextOutputFlags::Default);

    /*
    Opens the specified file. If successful, flushes and closes any existing
    output, then opens with Mode = File/Console (convert and write bytes to
    file/console).
    */
    LSTATUS
    OpenFile(
        _In_ PCWSTR outputFile,
        unsigned codePage = CP_ACP,
        TextOutputFlags flags = TextOutputFlags::Default);

    /*
    Gets buffered in-memory chars. Valid only if Mode == Chars.
    */
    std::u16string_view
    BufferedChars() const;

    /*
    Gets buffered in-memory bytes. Valid only if Mode == Bytes.
    */
    std::string_view
    BufferedBytes() const;

    /*
    Appends chars to output.
    If the pUsedDefaultChar != null and the default char gets used, sets
    *pUsedDefaultChar = true. Otherwise leaves pUsedDefaultChar at the prior value.
    */
    void
    WriteChars(
        std::u16string_view chars,
        _In_opt_ PCCH pDefaultChar = nullptr,
        _Inout_opt_ bool* pUsedDefaultChar = nullptr);
};
