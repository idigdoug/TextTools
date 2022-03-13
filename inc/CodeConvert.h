// Copyright (c) Doug Cook.
// Licensed under the MIT License.

#pragma once
#include <string_view>
#include <string>

enum class CodePageCategory : UINT8;
struct CodePageInfo;

/*
Streaming conversion between encoded character data and UTF-16.
Supports UTF-8, UTF-16, UTF-32, and SBCS/DBCS Windows code pages.
Best-effort support for more complex Windows code pages.
*/
class CodeConvert
{
    unsigned m_codePage;

    // Uses GetCPInfoExW to resolve CP_MACCP or CP_THREAD_ACP into a normal
    // code page.
    static unsigned
    ResolveCodePage(unsigned codePage) noexcept;

public:

    /*
    Returns true if the specified code page category is likely to work well
    with this class. This will return true for Sbcs, Dbcs, or Utf. It will
    return false for other categories because incorrect conversions will occur
    when a chunk of input ends in the middle of a character byte sequence.
    */
    static bool
    SupportsCategory(CodePageCategory category) noexcept;

    /*
    Returns true if the specified code page is likely to work well with this
    class. This will return true for SBCS, DBCS, or UTF code pages. It will
    return false for other categories because incorrect conversions will occur
    when a chunk of input ends in the middle of a character byte sequence.
    */
    static bool
    SupportsCodePage(unsigned codePage) noexcept;

    /*
    Returns true if the specified code page is likely to work well with this
    class. This will return true for code pages where info.Category is Sbcs,
    Dbcs, or Utf. It will return false for other categories because incorrect
    conversions will occur when a chunk of input ends in the middle of a
    character byte sequence.
    */
    static bool
    SupportsCodePage(CodePageInfo const& info) noexcept;

    /*
    Initializes a CodeConvert that uses UTF-8.
    */
    constexpr
    CodeConvert() noexcept
        : m_codePage(CP_UTF8) {}

    /*
    Initializes a CodeConvert that uses the encoding given by the specified Windows
    Code Page identifier. If codePage is a special value (CP_ACP, CP_OEMCP, CP_MACCP,
    or CP_THREAD_ACP), resolves the provided special value into the normal Windows
    Code Page identifier corresponding to the special value. Use the CodePageUtfXXX
    constants for UTF-8, UTF-16 and UTF-32 encodings.
    */
    explicit constexpr
    CodeConvert(unsigned codePage) noexcept
        : m_codePage(
            codePage > CP_THREAD_ACP ? codePage
            : codePage == CP_ACP ? GetACP()
            : codePage == CP_OEMCP ? GetOEMCP()
            : ResolveCodePage(codePage)) {}

    /*
    Initializes a CodeConvert that uses the encoding given by info.CodePage.
    */
    explicit
    CodeConvert(CodePageInfo const& info) noexcept;

    /*
    Returns the encoding used by this CodeConvert, specified as a Windows Code Page
    identifier. If this object was constructed using a special code page value (CP_ACP,
    CP_OEMCP, CP_MACCP, or CP_THREAD_ACP), this returns the resolved value instead of
    the special value.
    */
    constexpr unsigned
    CodePage() const noexcept
    {
        return m_codePage;
    }

    /*
    If !SupportsCodePage(CodePage()), throws std::runtime_error.
    Otherwise, returns code page category.
    */
    CodePageCategory
    ThrowIfNotSupported() const;

    /*
    Converts a chunk of encoded input to UTF-16 output and appends it to utf16Output.
    - Requires: encodedInputPos <= encodedInput.size().
    - Requires: utf16OutputPos <= utf16Output.size().
    - Conversion starts at encodedInput[encodedInputPos].
    - Conversion consumes as much of encodedInput as possible. It may stop before the
      end if encodedInput ends in the middle of a character, e.g. on a DBCS lead byte.
    - encodedInputPos will be updated to reflect the position of the next unconsumed
      input, or will be set to encodedInput.size() if all input was consumed.
    - Output is written starting at utf16Output[utf16OutputPos].
    - utf16Output will be resized as necessary to store the output.
    - utf16OutputPos will be updated to reflect the used output size.
    - May throw in case of out-of-memory (when utf16Output is resized).
    - Returns ERROR_SUCCESS or any error returned by MultiByteToWideChar.
    */
    LSTATUS
    EncodedToUtf16(
        std::string_view encodedInput,
        size_t& encodedInputPos, // <= encodedInput.size()
        std::u16string& utf16Output,
        size_t& utf16OutputPos, // <= utf16Output.size()
        unsigned mb2wcFlags = 0) const;

    /*
    Converts a chunk of UTF-16 input to encoded output and appends it to encodedOutput.
    - Requires: utf16InputPos <= utf16Input.size().
    - Requires: encodedOutputPos <= encodedOutput.size().
    - Requires: Flags are valid for the encoding.
    - Requires: If encoding is UTF, pDefaultChar and pUsedDefaultChar must be null.
    - Conversion starts at utf16Input[utf16InputPos].
    - Conversion consumes as much of utf16Input as possible. It will stop before the end
      if utf16Input ends with a high surrogate.
    - utf16InputPos will be updated to reflect the position of the next unconsumed input,
      or will be set to utf16Input.size() if all input was consumed.
    - Output is written starting at encodedOutput[encodedOutputPos].
    - encodedOutput will be resized as necessary to store the output.
    - encodedOutputPos will be updated to reflect the used output size.
    - May throw in case of out-of-memory (when encodedOutput is resized).
    - If pUsedDefaultChar != null and default char is used, sets *pUsedDefaultChar = true.
    - Returns ERROR_SUCCESS or any error returned by WideCharToMultiByte.
    */
    LSTATUS
    Utf16ToEncoded(
        std::u16string_view utf16Input,
        size_t& utf16InputPos, // <= utf16Input.size()
        std::string& encodedOutput,
        size_t& encodedOutputPos, // <= encodedOutput.size()
        unsigned wc2mbFlags = 0,
        _In_opt_ PCCH pDefaultChar = nullptr,
        _Inout_opt_ bool* pUsedDefaultChar = nullptr) const;
};
