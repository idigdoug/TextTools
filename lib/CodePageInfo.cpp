// Copyright (c) Doug Cook.
// Licensed under the MIT License.

#include "pch.h"
#include <CodePageInfo.h>
#include <wchar.h>

#define WCSCPY(dest, srclit) memcpy(dest, L"" srclit, sizeof(L"" srclit))

// Get the next alphanumeric char (lowercased), or 0 if EOS.
static wchar_t
NextArgChar(std::wstring_view arg, size_t& iArg) noexcept
{
    for (;;)
    {
        auto ch = iArg < arg.size() ? arg[iArg] : L'\0';
        if (ch == 0)
        {
            return ch;
        }

        iArg += 1;

        if (L'0' <= ch && ch <= L'9')
        {
            return ch;
        }

        ch |= 32; // Make uppercase.
        if (L'a' <= ch && ch <= 'z')
        {
            return ch;
        }
    }
}

enum class BomSuffix
{
    NoSuffix,
    Yes,
    NoMatch,
};

static BomSuffix
CheckBomSuffix(std::wstring_view arg, size_t iArg) noexcept
{
    BomSuffix result;
    if (auto ch = NextArgChar(arg, iArg);
        ch == 0)
    {
        result = BomSuffix::NoSuffix;
    }
    else if (ch == L'b' &&
        NextArgChar(arg, iArg) == L'o' &&
        NextArgChar(arg, iArg) == L'm' &&
        NextArgChar(arg, iArg) == 0)
    {
        result = BomSuffix::Yes;
    }
    else
    {
        result = BomSuffix::NoMatch;
    }

    return result;
}

CodePageArg::CodePageArg(std::wstring_view arg) noexcept
    : CodePage()
    , BomSuffix()
    , ParseResult(CodePageCategory::None)
{
    size_t iArg;

    struct UtfEncoding {
        PCWSTR Name;
        UINT16 CodePage;
    };

    static UtfEncoding const UtfEncodings[] = {
        { L"utf8",     CodePageUtf8 },
        { L"utf16",    CodePageUtf16LE },
        { L"utf16le",  CodePageUtf16LE },
        { L"utf16be",  CodePageUtf16BE },
        { L"utf32",    CodePageUtf32LE },
        { L"utf32le",  CodePageUtf32LE },
        { L"utf32be",  CodePageUtf32BE },
    };

    for (auto& enc : UtfEncodings)
    {
        iArg = 0;
        for (size_t iEnc = 0;; iEnc += 1)
        {
            auto encChar = enc.Name[iEnc];
            if (encChar == 0)
            {
                auto const suffix = CheckBomSuffix(arg, iArg);
                if (suffix == BomSuffix::NoMatch)
                {
                    break;
                }
                else
                {
                    CodePage = enc.CodePage;
                    BomSuffix = suffix == BomSuffix::Yes;
                    ParseResult = CodePageCategory::Utf;
                    goto Done;
                }
            }

            auto argChar = NextArgChar(arg, iArg);
            if (encChar != argChar)
            {
                break;
            }
        }
    }

    iArg = 0;

    // Skip leading "cp" if present.
    if (NextArgChar(arg, iArg) != L'c' ||
        NextArgChar(arg, iArg) != L'p')
    {
        iArg = 0;
    }

    PWSTR numEnd;
    errno = 0;
    CodePage = wcstoul(arg.data() + iArg, &numEnd, 10);
    if (errno != 0 || arg.data() + iArg == numEnd)
    {
        ParseResult = CodePageCategory::Error;
        goto Done;
    }

    for (auto& enc : UtfEncodings)
    {
        if (enc.CodePage == CodePage)
        {
            ParseResult = CodePageCategory::Utf;
            break;
        }
    }

    iArg = numEnd - arg.data();

    if (auto const suffix = CheckBomSuffix(arg, iArg);
        suffix == BomSuffix::NoMatch)
    {
        ParseResult = CodePageCategory::Error;
    }
    else
    {
        BomSuffix = suffix == BomSuffix::Yes;
    }

Done:

    return;
}

static _Success_(return) bool
InitUtfCodePage(unsigned codePage, _Out_ CPINFOEXW* pInfo) noexcept
{
    bool utf;

    switch (codePage)
    {
    default:
        utf = false;
        goto Done;
    case CodePageUtf8:
        WCSCPY(pInfo->CodePageName, L"65001 (UTF-8)");
        break;
    case CodePageUtf16LE:
        WCSCPY(pInfo->CodePageName, L"1200 (UTF-16LE)");
        break;
    case CodePageUtf16BE:
        WCSCPY(pInfo->CodePageName, L"1201 (UTF-16BE)");
        break;
    case CodePageUtf32LE:
        WCSCPY(pInfo->CodePageName, L"12000 (UTF-32LE)");
        break;
    case CodePageUtf32BE:
        WCSCPY(pInfo->CodePageName, L"12001 (UTF-32BE)");
        break;
    }

    pInfo->MaxCharSize = 4;
    pInfo->DefaultChar[0] = '?';
    pInfo->LeadByte[0] = 0;
    pInfo->UnicodeDefaultChar = 0xfffd;
    pInfo->CodePage = codePage;
    utf = true;

Done:

    return utf;
}

CodePageInfo::CodePageInfo(unsigned codePage) noexcept
    : CPINFOEXW()
    , UnresolvedCodePage(codePage)
{
    if (InitUtfCodePage(codePage, this))
    {
        Category = CodePageCategory::Utf;
    }
    else if (!GetCPInfoExW(codePage, 0, this))
    {
        CodePage = codePage;
        _snwprintf_s(CodePageName, _TRUNCATE, L"%u (Unknown)", codePage);
        Category = CodePageCategory::Error;
    }
    else if (MaxCharSize == 1)
    {
        Category = CodePageCategory::Sbcs;
    }
    else if (MaxCharSize == 2 && LeadByte[0] != 0)
    {
        Category = CodePageCategory::Dbcs;
    }
    else if (InitUtfCodePage(CodePage, this))
    {
        Category = CodePageCategory::Utf;
    }
    else
    {
        Category = CodePageCategory::Complex;
    }
}

std::string
CodePageInfo::Name() const
{
    std::string name;

    size_t cch = wcsnlen(CodePageName, ARRAYSIZE(CodePageName));
    name.resize(cch);
    for (size_t i = 0; i != cch; i++)
    {
        name[i] = (char)CodePageName[i];
    }

    return name;
}
