// Copyright (c) Doug Cook.
// Licensed under the MIT License.

#pragma once
#include "TextToolsCommon.h"
#include "CodeConvert.h"
#include <memory>
#include <string>
#include <string_view>

enum class TextInputFlags : uint8_t
{
    None = 0,
    FoldCRLF = 0x01, // Convert CRLF or CR to LF.
    ConsumeBom = 0x02, // If input starts with BOM, consume BOM and override codepage.
    InvalidMbcsError = 0x04, // Use MB_ERR_INVALID_CHARS in conversion.
    CheckConsole = 0x10, // If input is a console, use ReadConsoleW and override codepage.
    ConsoleCtrlZ = 0x20, // If using ReadConsoleW, Read() returns immediately for Ctrl-Z.
    Default = InvalidMbcsError | CheckConsole | ConsoleCtrlZ
};
DEFINE_ENUM_FLAG_OPERATORS(TextInputFlags);

enum class TextInputMode : uint8_t
{
    None,
    Chars,
    Bytes,
    File,
    Console,
};

class TextInput
{
    std::string m_bytes;
    std::u16string m_chars;

    TextToolsUniqueHandle m_inputOwner;
    HANDLE m_inputHandle;
    CodeConvert m_codeConvert;
    TextInputMode m_mode;
    TextInputFlags m_flags;

    bool m_skipNextCharIfNewline;
    size_t m_bytesPos;
    size_t m_charsPos;

    constexpr bool
    IsFlagSet(TextInputFlags flag) const noexcept;

    void
    SetCodeConvert(unsigned codePage);

    void
    FoldCRLF() noexcept;

    void
    ConsumeBytes(size_t consumedBytes) noexcept;

    /*
    Clears m_chars. Fills m_chars from m_bytes. Calls FoldCRLF().
    Throws for conversion failure.
    */
    void
    Convert();

    void
    ReadBytesFromFile();

    void
    ReadBytesFromFile(DWORD cbMaxToRead);

    void
    ReadCharsFromConsole();

    void
    OpenHandle(
        TextToolsUniqueHandle inputOwner,
        _In_ HANDLE inputHandle,
        unsigned codePage,
        TextInputFlags flags);

public:

    TextInput() noexcept;

    /*
    Closes any existing input.
    */
    void
    Close() noexcept;

    /*
    Gets the current mode of operation.
    */
    TextInputMode
    Mode() const noexcept;

    /*
    Closes any existing input, then copies inputChars to the Chars() buffer.
    */
    void
    OpenChars(
        std::u16string_view inputChars,
        TextInputFlags flags = TextInputFlags::Default);

    /*
    Closes any existing input, then converts inputBytes to UTF16 and stores the
    result in the Chars() buffer.
    */
    void
    OpenBytes(
        std::string_view inputBytes,
        unsigned codePage = CP_ACP,
        TextInputFlags flags = TextInputFlags::Default);

    /*
    Closes any existing input. Sets up to read from the input file. Reads and
    converts an initial chunk of input.
    */
    void
    OpenBorrowedHandle(
        _In_ HANDLE inputHandle,
        unsigned codePage = CP_ACP,
        TextInputFlags flags = TextInputFlags::Default);

    /*
    Opens the specified file. If successful, closes any existing input, sets up
    to read from the input file, and reads and converts an initial chunk of
    input.
    */
    LSTATUS
    OpenFile(
        _In_ PCWSTR inputFile,
        unsigned codePage = CP_ACP,
        TextInputFlags flags = TextInputFlags::Default);

    /*
    Gets text from the clipboard. If successful, closes any existing input and
    copies the clipboard text to the Chars() buffer.
    (Implemented in ClipboardText.cpp.)
    */
    LSTATUS
    OpenClipboard(
        TextInputFlags flags = TextInputFlags::Default);

    /*
    Gets the currently-available UTF-16LE characters.
    */
    std::u16string_view
    Chars() const noexcept;

    /*
    Clears the Chars() buffer, then loads more from the input source.
    If no more input is available (e.g. end-of-file), returns false.
    */
    bool
    ReadNextChars();
};
