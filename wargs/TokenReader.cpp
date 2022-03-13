// Copyright (c) Doug Cook.
// Licensed under the MIT License.

#include "pch.h"
#include "TokenReader.h"

int
TokenReader::CharPeek()
{
    return m_charsUsed < m_chars.size()
        ? m_chars[m_charsUsed]
        : CharPeekRefill();
}

int
TokenReader::CharPeekRefill()
{
    assert(m_charsUsed == m_chars.size());

    int returnChar;
    if (m_controlZ)
    {
        m_chars = {};
        m_charsUsed = 0;
        returnChar = -1;
    }
    else
    {
        m_input.ReadNextChars();
        InitInputChars();
        returnChar = m_chars.empty()
            ? -1
            : m_chars[m_charsUsed];
    }

    return returnChar;
}

void
TokenReader::InitInputChars() noexcept
{
    m_chars = m_input.Chars();
    m_charsUsed = 0;

    if (m_input.Mode() == TextInputMode::Console &&
        m_chars.ends_with(L'\x1A')) // Control-Z
    {
        m_chars.remove_suffix(1);
        m_controlZ = true;
    }
}

void
TokenReader::CharConsume() noexcept
{
    assert(m_charsUsed < m_chars.size());
    m_charsUsed += 1;
}

bool
TokenReader::SkipLeadingWhitespace()
{
    for (;;)
    {
        int peek = CharPeek();
        switch (peek)
        {
        case -1:
            return false;

        case L' ': // IsBlank
        case L'\t':
        case L'\n':
            CharConsume();
            continue;

        default:
            return true;
        }
    }
}

bool
TokenReader::AppendQuoted(std::wstring& value, wchar_t delimiter)
{
    bool token = false;

    for (;;)
    {
        int peek = CharPeek();
        if (peek < 0)
        {
            break;
        }
        CharConsume();

        token = true;

        if (peek == delimiter)
        {
            break;
        }

        value.push_back(static_cast<wchar_t>(peek));
    }

    return token;
}

bool
TokenReader::AppendEscaped(std::wstring& value)
{
    bool token;

    int peek = CharPeek();
    if (peek < 0)
    {
        token = false;
    }
    else
    {
        CharConsume();
        value.push_back(static_cast<wchar_t>(peek));
        token = true;
    }

    return token;
}

TokenReader::TokenReader(TextInput&& input, wchar_t delimiter)
    : m_input(std::move(input))
    , m_chars()
    , m_charsUsed()
    , m_lineCount()
    , m_tokenCount()
    , m_controlZ()
    , m_delimiter(delimiter)
{
    InitInputChars();
}

void
TokenReader::ResetCounts() noexcept
{
    m_lineCount = 0;
    m_tokenCount = 0;
}

unsigned
TokenReader::LineCount() const noexcept
{
    return m_lineCount;
}

unsigned
TokenReader::TokenCount() const noexcept
{
    return m_tokenCount;
}

bool
TokenReader::ReadDelimited(std::wstring& value)
{
    bool token = false;
    value.clear();

    for (;;)
    {
        auto const peek = CharPeek();
        if (peek < 0)
        {
            break;
        }

        CharConsume();
        token = true;

        auto const ch = static_cast<wchar_t>(peek);
        if (ch == m_delimiter)
        {
            break;
        }

        value.push_back(ch);
    }

    m_lineCount += token;
    m_tokenCount += token;
    return token;
}

template<bool SplitOnBlank>
bool
TokenReader::ReadEscaped(std::wstring& value)
{
    bool token = false;
    value.clear();

    if (!SkipLeadingWhitespace())
    {
        goto Done;
    }

    for (;;)
    {
        auto const peek = CharPeek();
        switch (peek)
        {
        case -1:
            goto Done;

        case L' ': // IsBlank
        case L'\t':
            assert(token);
            if constexpr (SplitOnBlank)
            {
                // Don't consume.
                goto Done;
            }
            else
            {
                CharConsume();
                value.push_back(static_cast<wchar_t>(peek));
                token = true;
                break;
            }

        case L'\n':
            assert(token);
            CharConsume();
            m_lineCount += 1;
            goto Done;

        case L'\\':
            CharConsume();
            if (!AppendEscaped(value))
            {
                goto Done;
            }
            token = true;
            break;

        case L'"':
        case L'\'':
            CharConsume();
            if (!AppendQuoted(value, static_cast<wchar_t>(peek)))
            {
                goto Done;
            }
            token = true;
            break;

        default:
            CharConsume();
            value.push_back(static_cast<wchar_t>(peek));
            token = true;
            break;
        }
    }

Done:

    m_tokenCount += token;
    return token;
}

bool
TokenReader::ReadEscapedToken(std::wstring& value)
{
    return ReadEscaped<true>(value);
}

bool
TokenReader::ReadEscapedLine(std::wstring& value)
{
    return ReadEscaped<false>(value);
}
