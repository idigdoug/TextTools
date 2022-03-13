// Copyright (c) Doug Cook.
// Licensed under the MIT License.

#pragma once
#include <TextInput.h>

class TokenReader
{
    TextInput m_input;
    std::u16string_view m_chars;
    size_t m_charsUsed;
    unsigned m_lineCount;
    unsigned m_tokenCount;
    bool m_controlZ;
    wchar_t const m_delimiter;

    int
    CharPeek();

    int
    CharPeekRefill();

    void
    InitInputChars() noexcept;

    void
    CharConsume() noexcept;

    /*
    Returns false for EOF, true otherwise.
    */
    bool
    SkipLeadingWhitespace();

    /*
    Returns false for EOF, true otherwise.
    */
    bool
    AppendQuoted(std::wstring& value, wchar_t delimiter);

    bool
    AppendEscaped(std::wstring& value);

    template<bool SplitOnBlank>
    bool
    ReadEscaped(std::wstring& value);

public:

    TokenReader(TextInput&& input, wchar_t delimiter);

    void
    ResetCounts() noexcept;

    unsigned
    LineCount() const noexcept;

    unsigned
    TokenCount() const noexcept;

    /*
    Split at delimiter. No unescaping.
    */
    bool
    ReadDelimited(std::wstring& value);

    /*
    Split on unquoted blank, unescaped blank, or unescaped newline.
    */
    bool
    ReadEscapedToken(std::wstring& value);

    /*
    Trim leading blank. Split on unescaped newline.
    */
    bool
    ReadEscapedLine(std::wstring& value);
};