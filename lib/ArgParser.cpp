// Copyright (c) Doug Cook.
// Licensed under the MIT License.

#include "pch.h"
#include <ArgParser.h>
#include <assert.h>

static bool
IsLongArgNameEnd(wchar_t ch) noexcept
{
    return
        ch == L'\0' ||
        ch == L':' ||
        ch == L'=';
}

static size_t
GetLongArgNameLength(PCWSTR szLongArg) noexcept
{
    size_t i;
    for (i = 0; !IsLongArgNameEnd(szLongArg[i]); i += 1) {}
    return i;
}

ArgParser::ArgParser(
    _In_z_ PCSTR appName,
    unsigned argc,
    _In_count_(argc) PWSTR argv[]) noexcept
    : m_appName(appName)
    , m_args(argv)
    , m_argCount(argc)
    , m_currentArgIndex(0) // Before-begin
    , m_currentArgPos(L"")
    , m_argError() {}

_Ret_z_ PCSTR
ArgParser::AppName() const noexcept
{
    return m_appName;
}

_Ret_z_ PCWSTR
ArgParser::Arg(unsigned index) const noexcept
{
    assert(index < m_argCount);
    return m_args[index];
}

unsigned
ArgParser::ArgCount() const noexcept
{
    return m_argCount;
}

unsigned
ArgParser::CurrentArgIndex() const noexcept
{
    return m_currentArgIndex;
}

_Ret_z_ PCWSTR
ArgParser::CurrentArg() const noexcept
{
    return m_args[m_currentArgIndex];
}

_Ret_z_ PCWSTR
ArgParser::CurrentArgPos() const noexcept
{
    return m_currentArgPos;
}

bool
ArgParser::CurrentArgIsEmpty() const noexcept
{
    assert(m_currentArgPos[0] != 0);
    return m_currentArgPos[1] == 0;
}

WCHAR
ArgParser::CurrentArgChar() const noexcept
{
    return m_currentArgPos[0];
}

std::wstring_view
ArgParser::CurrentArgName() const noexcept
{
    assert(m_currentArgPos[0] != 0);
    auto pLongArg = &m_currentArgPos[1];
    return std::wstring_view(pLongArg, GetLongArgNameLength(pLongArg));
}

bool
ArgParser::ArgError() const noexcept
{
    return m_argError;
}

void
ArgParser::SetArgError(bool value) noexcept
{
    m_argError = value;
}

void
ArgParser::SetArgErrorIfFalse(bool argOk) noexcept
{
    m_argError = m_argError || !argOk;
}

void
ArgParser::PrintShortArgError() noexcept
{
    m_argError = true;
    fprintf(stderr, "%hs: error : Unrecognized short argument '%lc' in '%ls'\n",
        m_appName, CurrentArgChar(), CurrentArg());
}

void
ArgParser::PrintLongArgError() noexcept
{
    m_argError = true;
    fprintf(stderr, "%hs: error : Unrecognized long argument '%ls'\n",
        m_appName, CurrentArg());
}

bool
ArgParser::MoveNextArg() noexcept
{
    assert(m_currentArgIndex < m_argCount);
    m_currentArgIndex += 1;
    m_currentArgPos = L"";
    return m_currentArgIndex < m_argCount;
}

bool
ArgParser::BeginDashDashArg() noexcept
{
    auto const current = CurrentArg();
    if (current[0] == L'-' && current[1] == L'-')
    {
        m_currentArgPos = &current[1]; // Before-begin
        return true;
    }

    return false;
}

bool
ArgParser::BeginDashOrSlashArg() noexcept
{
    auto const current = CurrentArg();
    if (current[0] == L'-' || current[0] == L'/')
    {
        m_currentArgPos = &current[0]; // Before-begin
        return true;
    }

    return false;
}

bool
ArgParser::BeginDashArg() noexcept
{
    auto const current = CurrentArg();
    if (current[0] == L'-')
    {
        m_currentArgPos = &current[0]; // Before-begin
        return true;
    }

    return false;
}

bool
ArgParser::BeginSlashArg() noexcept
{
    auto const current = CurrentArg();
    if (current[0] == L'/')
    {
        m_currentArgPos = &current[0]; // Before-begin
        return true;
    }

    return false;
}

bool
ArgParser::MoveNextArgChar() noexcept
{
    assert(m_currentArgPos[0] != 0);
    m_currentArgPos += 1;
    return m_currentArgPos[0] != 0;
}

_Ret_z_ PCWSTR
ArgParser::ReadArgCharsVal() noexcept
{
    assert(m_currentArgPos[0] != 0);
    PCWSTR const shortArgVal = m_currentArgPos + 1;

    // Consume rest of current arg.
    m_currentArgPos = L" ";

    return shortArgVal;
}

_Ret_opt_z_ PCWSTR
ArgParser::ReadNextArgVal() noexcept
{
    assert(m_currentArgPos[0] != 0);

    PCWSTR shortArgVal;
    if (m_currentArgIndex + 1 < m_argCount)
    {
        m_currentArgIndex += 1;
        shortArgVal = m_args[m_currentArgIndex];
    }
    else
    {
        shortArgVal = nullptr;
    }

    return shortArgVal;
}

_Ret_opt_z_ PCWSTR
ArgParser::GetLongArgVal() const noexcept
{
    assert(m_currentArgPos[0] != 0);
    auto pLongArg = &m_currentArgPos[1];
    auto pNameEnd = pLongArg + GetLongArgNameLength(pLongArg);
    return pNameEnd[0] ? pNameEnd + 1 : nullptr;
}
bool
ArgParser::ReadArgCharsVal(std::wstring_view& val, bool emptyOk) noexcept
{
    auto const argChar = CurrentArgChar();
    auto const shortArgVal = ReadArgCharsVal();
    if (!emptyOk && shortArgVal[0] == 0)
    {
        m_argError = true;
        fprintf(stderr, "%hs: error : Expected VALUE for '-%lcVALUE'\n",
            m_appName, argChar);
        return false;
    }

    val = shortArgVal;
    return true;
}

bool
ArgParser::ReadNextArgVal(std::wstring_view& val, bool emptyOk) noexcept
{
    auto const argChar = CurrentArgChar();
    auto const shortArgVal = ReadNextArgVal();
    if (!shortArgVal ||
        (!emptyOk && shortArgVal[0] == 0))
    {
        m_argError = true;
        fprintf(stderr, "%hs: error : Expected VALUE for '-%lc VALUE'\n",
            m_appName, argChar);
        return false;
    }

    val = shortArgVal;
    return true;
}

bool
ArgParser::GetLongArgVal(std::wstring_view& val, bool emptyOk) noexcept
{
    auto const longArgVal = GetLongArgVal();
    if (!emptyOk && (longArgVal == nullptr || longArgVal[0] == 0))
    {
        m_argError = true;
        fprintf(stderr, "%hs: error : Expected VALUE for '%ls=VALUE'\n",
            m_appName, CurrentArg());
        return false;
    }

    val = longArgVal ? std::wstring_view(longArgVal) : std::wstring_view();
    return true;
}

bool
ArgParser::ReadArgCharsVal(unsigned& val, bool zeroOk, int radix) noexcept
{
    auto const argChar = CurrentArgChar();
    auto const shortArgVal = ReadArgCharsVal();
    if (shortArgVal[0] == 0)
    {
        m_argError = true;
        fprintf(stderr, "%hs: error : Expected uint32 VALUE for '-%lcVALUE'\n",
            m_appName, argChar);
        return false;
    }

    wchar_t* end;
    errno = 0;
    unsigned parsedVal = wcstoul(shortArgVal, &end, radix);
    if (errno != 0)
    {
        m_argError = true;
        fprintf(stderr, "%hs: error : Range error parsing uint32 '-%lc%ls'\n",
            m_appName, argChar, shortArgVal);
        return false;
    }
    else if (end[0])
    {
        m_argError = true;
        fprintf(stderr, "%hs: error : Trailing characters following uint32 '-%lc%ls'\n",
            m_appName, argChar, shortArgVal);
        return false;
    }
    else if (!zeroOk && parsedVal == 0)
    {
        m_argError = true;
        fprintf(stderr, "%hs: error : Expected nonzero uint32 value for '-%lc%ls'\n",
            m_appName, argChar, shortArgVal);
        return false;
    }

    val = parsedVal;
    return true;
}

bool
ArgParser::ReadNextArgVal(unsigned& val, bool zeroOk, int radix) noexcept
{
    auto const argChar = CurrentArgChar();
    auto const shortArgVal = ReadNextArgVal();
    if (!shortArgVal)
    {
        m_argError = true;
        fprintf(stderr, "%hs: error : Expected uint32 VALUE for '-%lc VALUE'\n",
            m_appName, argChar);
        return false;
    }

    wchar_t* end;
    errno = 0;
    unsigned parsedVal = wcstoul(shortArgVal, &end, radix);
    if (errno != 0)
    {
        m_argError = true;
        fprintf(stderr, "%hs: error : Range error parsing uint32 '-%lc %ls'\n",
            m_appName, argChar, shortArgVal);
        return false;
    }
    else if (end[0])
    {
        m_argError = true;
        fprintf(stderr, "%hs: error : Trailing characters following uint32 '-%lc %ls'\n",
            m_appName, argChar, shortArgVal);
        return false;
    }
    else if (!zeroOk && parsedVal == 0)
    {
        m_argError = true;
        fprintf(stderr, "%hs: error : Expected nonzero uint32 value for '-%lc %ls'\n",
            m_appName, argChar, shortArgVal);
        return false;
    }

    val = parsedVal;
    return true;
}

bool
ArgParser::GetLongArgVal(unsigned& val, bool zeroOk, int radix) noexcept
{
    auto const longArgVal = GetLongArgVal();
    if (!longArgVal)
    {
        m_argError = true;
        fprintf(stderr, "%hs: error : Expected uint32 value for '%ls=value'\n",
            m_appName, CurrentArg());
        return false;
    }

    wchar_t* end;
    errno = 0;
    unsigned parsedVal = wcstoul(longArgVal, &end, radix);
    if (errno != 0)
    {
        m_argError = true;
        fprintf(stderr, "%hs: error : Range error parsing uint32 '%ls'\n",
            m_appName, CurrentArg());
        return false;
    }
    else if (end[0])
    {
        m_argError = true;
        fprintf(stderr, "%hs: error : Trailing characters following uint32 '%ls'\n",
            m_appName, CurrentArg());
        return false;
    }
    else if (!zeroOk && parsedVal == 0)
    {
        m_argError = true;
        fprintf(stderr, "%hs: error : Expected nonzero uint32 value for '%ls'\n",
            m_appName, CurrentArg());
        return false;
    }

    val = parsedVal;
    return true;
}

bool
ArgParser::CurrentArgNameMatches(
    unsigned minMatchLength,
    PCWSTR expectedName) const noexcept
{
    assert(m_currentArgPos[0] != 0);
    bool matches;
    auto const pCurrentArg = &m_currentArgPos[1];
    for (unsigned i = 0;; i += 1)
    {
        if (IsLongArgNameEnd(pCurrentArg[i]))
        {
            // Reached end of arg without any mismatch.
            matches = i >= minMatchLength; // Matched enough?
            break;
        }
        else if (expectedName[i] != pCurrentArg[i])
        {
            matches = false;
            break;
        }
    }

    return matches;
}
