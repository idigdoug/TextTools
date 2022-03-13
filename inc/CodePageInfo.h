// Copyright (c) Doug Cook.
// Licensed under the MIT License.

#pragma once
#include <string>

constexpr unsigned CodePageUtf8 = CP_UTF8;
constexpr unsigned CodePageUtf16LE = 1200;
constexpr unsigned CodePageUtf16BE = 1201;
constexpr unsigned CodePageUtf32LE = 12000;
constexpr unsigned CodePageUtf32BE = 12001;

enum class CodePageCategory : UINT8
{
    None,    // Invalid value.
    Error,   // Error returned by GetCPInfoExA.
    Sbcs,    // Single-byte character set.
    Dbcs,    // Up-to-2-byte character set with lead-byte ranges.
    Complex, // Not SBCS, DBCS, or UTF.
    Utf,
};

struct CodePageArg
{
    unsigned CodePage;
    bool BomSuffix;
    CodePageCategory ParseResult;

    constexpr
    CodePageArg() noexcept
        : CodePage()
        , BomSuffix()
        , ParseResult() {}

    /*
    Parse code page. Expected format is:
    (NNNN|cpNNNN|utf8|utf16[be|le]|utf32[be|le])[bom]
    ParseResult will be Error, Utf, or None (parsed by number, may or may not be valid).
    */
    explicit
    CodePageArg(std::wstring_view arg) noexcept;
};

struct CodePageInfo
    : CPINFOEXW
{
    unsigned UnresolvedCodePage;
    CodePageCategory Category;

    explicit
    CodePageInfo(unsigned codePage) noexcept;

    std::string
    Name() const;
};
