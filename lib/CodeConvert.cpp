// Copyright (c) Doug Cook.
// Licensed under the MIT License.

#include "pch.h"
#include <CodeConvert.h>
#include <CodePageInfo.h>
#include "Utility.h"

#include <assert.h>
#include <stdexcept>

using namespace TextToolsImpl;

// Ensure that cOutputNeeded <= INT_MAX/sizeof(WCHAR) by limiting cBatch to INT_MAX/sizeof(WCHAR).
static constexpr int MultiByteBatchMax = INT_MAX / sizeof(WCHAR);

static constexpr char16_t UnicodeReplacement = 0xFFFD;

namespace
{
    struct UtfConvertResult
    {
        void const* InputPos;
        void const* OutputPos;
        LSTATUS UsedReplacement;
    };

    enum class ByteSwap : UINT8
    {
        None,
        Input,
        Output
    };

    template<ByteSwap Swap>
    struct ByteSwapper
    {
        static UINT16
        Input16(UINT16 n) noexcept
        {
            if constexpr (Swap == ByteSwap::Input)
                return _byteswap_ushort(n);
            else
                return n;
        }

        static UINT32
        Input32(UINT32 n) noexcept
        {
            if constexpr (Swap == ByteSwap::Input)
                return _byteswap_ulong(n);
            else
                return n;
        }

        static UINT16
        Output16(UINT16 n) noexcept
        {
            if constexpr (Swap == ByteSwap::Output)
                return _byteswap_ushort(n);
            else
                return n;
        }

        static UINT32
        Output32(UINT32 n) noexcept
        {
            if constexpr (Swap == ByteSwap::Output)
                return _byteswap_ulong(n);
            else
                return n;
        }
    };
}

// Validates. If appropriate, byte-swaps.
template<ByteSwap Swap>
static UtfConvertResult
Utf16ToUtf16(
    _In_reads_(cInput) char16_t const* const pInput,
    size_t const cInput,
    _Pre_cap_(cInput) char16_t* const pOutput) noexcept
{
    static constexpr ByteSwapper<Swap> swap;
    size_t iInput = 0;
    LSTATUS usedReplacement = ERROR_SUCCESS;

    for (; iInput != cInput; iInput += 1)
    {
        auto const ch0 = swap.Input16(pInput[iInput]);
        if (ch0 < 0xD800 || ch0 >= 0xE000)
        {
            // Not a surrogate.
            pOutput[iInput] = swap.Output16(ch0);
        }
        else if (ch0 >= 0xDC00)
        {
            // Unmatched low surrogate.
            pOutput[iInput] = swap.Output16(UnicodeReplacement);
            usedReplacement = ERROR_NO_UNICODE_TRANSLATION;
        }
        else if (iInput + 1 == cInput)
        {
            // High surrogate at end of input. Don't consume it.
            break;
        }
        else if (
            auto const ch1 = swap.Input16(pInput[iInput + 1]);
            ch1 >= 0xDC00 && ch1 < 0xE000)
        {
            // Surrogate pair.
            pOutput[iInput] = swap.Output16(ch0);
            iInput += 1;
            pOutput[iInput] = swap.Output16(ch1);
        }
        else
        {
            // Unmatched high surrogate.
            pOutput[iInput] = swap.Output16(UnicodeReplacement);
            usedReplacement = ERROR_NO_UNICODE_TRANSLATION;
        }
    }

    return { &pInput[iInput], &pOutput[iInput], usedReplacement };
}

// Validates. If appropriate, byte-swaps.
template<ByteSwap Swap>
static UtfConvertResult
Utf16ToUtf32(
    _In_reads_(cInput) char16_t const* pInput,
    size_t const cInput,
    _Pre_cap_(cInput) char32_t* pOutput) noexcept
{
    static constexpr ByteSwapper<Swap> swap;
    size_t iInput = 0;
    size_t iOutput = 0;
    LSTATUS usedReplacement = ERROR_SUCCESS;

    for (; iInput != cInput; iInput += 1)
    {
        auto const ch0 = swap.Input16(pInput[iInput]);
        if (ch0 < 0xD800 || ch0 >= 0xE000)
        {
            // Not a surrogate.
            pOutput[iOutput++] = swap.Output32(ch0);
        }
        else if (ch0 >= 0xDC00)
        {
            // Unmatched low surrogate.
            pOutput[iOutput++] = swap.Output32(UnicodeReplacement);
            usedReplacement = ERROR_NO_UNICODE_TRANSLATION;
        }
        else if (iInput + 1 == cInput)
        {
            // High surrogate at end of input. Don't consume it.
            break;
        }
        else if (
            auto const ch1 = swap.Input16(pInput[iInput + 1]);
            ch1 >= 0xDC00 && ch1 < 0xE000)
        {
            // Surrogate pair.
            pOutput[iOutput++] = swap.Output32(0x10000 + (((ch0 - 0xD800) << 10) | (ch1 - 0xDC00)));
            iInput += 1;
        }
        else
        {
            // Unmatched high surrogate.
            pOutput[iOutput++] = swap.Output32(UnicodeReplacement);
            usedReplacement = ERROR_NO_UNICODE_TRANSLATION;
        }
    }

    return { &pInput[iInput], &pOutput[iOutput], usedReplacement };
}

// Validates. If appropriate, byte-swaps.
template<ByteSwap Swap>
static UtfConvertResult
Utf32ToUtf16(
    _In_reads_(cInput) char32_t const* pInput,
    size_t const cInput,
    _Pre_cap_(cInput * 2) char16_t* pOutput) noexcept
{
    static constexpr ByteSwapper<Swap> swap;
    size_t iInput = 0;
    size_t iOutput = 0;
    LSTATUS usedReplacement = ERROR_SUCCESS;

    for (; iInput != cInput; iInput += 1)
    {
        auto const ch = swap.Input32(pInput[iInput]);
        if (ch <= 0xFFFF)
        {
            // Note: Not checking for surrogates, which would be errors if we were more strict.
            pOutput[iOutput++] = swap.Output16(static_cast<char16_t>(ch));
        }
        else if (ch <= 0x10FFFF)
        {
            auto val = ch - 0x10000;
            pOutput[iOutput++] = swap.Output16(static_cast<char16_t>(val >> 10) + 0xD800);
            pOutput[iOutput++] = swap.Output16(static_cast<char16_t>(val & 0x3FF) + 0xDC00);
        }
        else
        {
            pOutput[iOutput++] = swap.Output16(UnicodeReplacement);
            usedReplacement = ERROR_NO_UNICODE_TRANSLATION;
        }
    }

    return { &pInput[iInput], &pOutput[iOutput], usedReplacement };
}

unsigned
CodeConvert::ResolveCodePage(unsigned codePage) noexcept
{
    CPINFOEXW info;
    return GetCPInfoExW(codePage, 0, &info)
        ? info.CodePage
        : codePage;
}

bool
CodeConvert::SupportsCategory(CodePageCategory category) noexcept
{
    return
        category == CodePageCategory::Sbcs ||
        category == CodePageCategory::Dbcs ||
        category == CodePageCategory::Utf;
}

bool
CodeConvert::SupportsCodePage(CodePageInfo const& info) noexcept
{
    return SupportsCategory(info.Category);
}

bool
CodeConvert::SupportsCodePage(unsigned codePage) noexcept
{
    return SupportsCategory(CodePageInfo(codePage).Category);
}

CodeConvert::CodeConvert(CodePageInfo const& info) noexcept
    : m_codePage(info.CodePage) {}

CodePageCategory
CodeConvert::ThrowIfNotSupported() const
{
    CodePageInfo info(m_codePage);

    if (info.Category == CodePageCategory::Error)
    {
        throw std::runtime_error("GetCPInfo returned error for code page " + info.Name() + '.');
    }
    else if (!CodeConvert::SupportsCodePage(info))
    {
        throw std::runtime_error(
            "Code page " + info.Name() +
            " is not a supported code page. This library supports"
            " Windows SBCS and DBCS code pages,"
            " UTF-8 (65001),"
            " UTF-16LE (1200),"
            " UTF-16BE (1201),"
            " UTF-32LE (12000),"
            " and UTF-32BE (12001).");
    }

    return info.Category;
}

LSTATUS
CodeConvert::EncodedToUtf16(
    std::string_view encodedInput,
    size_t& encodedInputPos,
    std::u16string& utf16Output,
    size_t& utf16OutputPos,
    unsigned mb2wcFlags) const
{
    LSTATUS status;

    auto const pbInputBegin = reinterpret_cast<UINT8 const*>(encodedInput.data());
    auto const pbInputEnd = pbInputBegin + encodedInput.size();
    auto pbInput = pbInputBegin + encodedInputPos;
    auto iOutput = utf16OutputPos;

    assert(pbInputBegin <= pbInput);
    assert(pbInput <= pbInputEnd);
    assert(iOutput <= utf16Output.size());
    if (pbInput < pbInputBegin ||
        pbInputEnd < pbInput ||
        utf16Output.size() < iOutput)
    {
        status = ERROR_INVALID_PARAMETER;
        goto Done;
    }

    switch (m_codePage | 1u) // Combine BE and LE cases
    {
    case CodePageUtf16BE: // Includes CodePageUtf16LE
    {
        size_t const cInput = (pbInputEnd - pbInput) / sizeof(char16_t);
        auto const pInput = reinterpret_cast<char16_t const*>(pbInput);

        // We need one char16 of output for each char16 of input.
        EnsureSize(utf16Output, iOutput, cInput);
        auto const pOutput = utf16Output.data() + iOutput;

        auto const result = m_codePage == CodePageUtf16BE
            ? Utf16ToUtf16<ByteSwap::Input>(pInput, cInput, pOutput)
            : Utf16ToUtf16<ByteSwap::None>(pInput, cInput, pOutput);
        encodedInputPos = static_cast<UINT8 const*>(result.InputPos) - pbInputBegin;
        utf16OutputPos = static_cast<char16_t const*>(result.OutputPos) - utf16Output.data();
        status = mb2wcFlags & MB_ERR_INVALID_CHARS ? result.UsedReplacement : ERROR_SUCCESS;
        break;
    }

    case CodePageUtf32BE: // Includes CodePageUtf32LE
    {
        size_t const cInput = (pbInputEnd - pbInput) / sizeof(char32_t);
        auto const pInput = reinterpret_cast<char32_t const*>(pbInput);

        // We need up to two char16s of output for each char32 of input.
        EnsureSize(utf16Output, iOutput, cInput * 2);
        auto const pOutput = utf16Output.data() + iOutput;

        auto const result = m_codePage == CodePageUtf32BE
            ? Utf32ToUtf16<ByteSwap::Input>(pInput, cInput, pOutput)
            : Utf32ToUtf16<ByteSwap::None>(pInput, cInput, pOutput);
        encodedInputPos = static_cast<UINT8 const*>(result.InputPos) - pbInputBegin;
        utf16OutputPos = static_cast<char16_t const*>(result.OutputPos) - utf16Output.data();
        status = mb2wcFlags & MB_ERR_INVALID_CHARS ? result.UsedReplacement : ERROR_SUCCESS;
        break;
    }

    default: // Includes CodePageUtf8

        // Split into batches no larger than MultiByteBatchMax.
        for (int cbInput; pbInput < pbInputEnd; pbInput += cbInput)
        {
            // Estimate that we need one char16 of output for each byte of input.
            EnsureSize(utf16Output, iOutput, pbInputEnd - pbInput);

            int const cbInputMax = pbInputEnd - pbInput < MultiByteBatchMax
                ? (int)(pbInputEnd - pbInput)
                : MultiByteBatchMax;

            // Trim off an incomplete character sequence.
            if (m_codePage != CodePageUtf8)
            {
                // Assume DBCS. If not DBCS, IsDBCSLeadByteEx returns false and we don't trim anything.
                // Go backwards until we find a byte that can't possibly be a lead byte.
                if (!IsDBCSLeadByteEx(m_codePage, pbInput[cbInputMax - 1]))
                {
                    // Does not end with a lead byte. No trim needed.
                    cbInput = cbInputMax;
                }
                else
                {
                    int i = cbInputMax - 1;
                    while (i != 0 && IsDBCSLeadByteEx(m_codePage, pbInput[i - 1]))
                    {
                        i -= 1;
                    }

                    // If we had to go back an odd number of bytes, the last byte is a lead byte and must be trimmed.
                    cbInput = cbInputMax - ((cbInputMax - i) & 1);
                    if (cbInput == 0)
                    {
                        break;
                    }
                }
            }
            else if ((pbInput[cbInputMax - 1] & 0x80) == 0)
            {
                // UTF-8, ends with an ASCII byte. No trim needed.
                cbInput = cbInputMax;
            }
            else
            {
                // UTF-8, ends with a non-ASCII byte.
                // Go backwards up to 4 bytes to find the lead byte.
                int const cbInputMin = cbInputMax >= 4
                    ? cbInputMax - 4
                    : 0;
                for (cbInput = cbInputMax; cbInput != cbInputMin; cbInput -= 1)
                {
                    ULONG firstZeroPos;
                    UINT8 const pattern = ~pbInput[cbInput - 1]
                        | 1u; // Make sure pattern isn't 0.
                    (void)_BitScanReverse(&firstZeroPos, pattern);
                    int const trailBytes = 6 - firstZeroPos;

                    if (trailBytes == 0)
                    {
                        // It's a trail byte. Keep looking for lead byte.
                    }
                    else if (trailBytes > cbInputMax - cbInput)
                    {
                        // Lead byte. Sequence does not complete before end. Trim it.
                        cbInput -= 1;
                        break;
                    }
                    else
                    {
                        // Either the sequence completes before end or it's invalid. Don't trim anything.
                        cbInput = cbInputMax;
                        break;
                    }
                }

                if (cbInput == 0)
                {
                    break;
                }
            }

            // May need to retry the MB2WC after resizing utf16Output.
            for (;;)
            {
                int const cOutput = utf16Output.size() - iOutput < INT_MAX
                    ? (int)(utf16Output.size() - iOutput)
                    : INT_MAX;

                assert(cbInput > 0);
                assert(cOutput > 0);
                int const cOutputWritten = MultiByteToWideChar(
                    m_codePage,
                    mb2wcFlags,
                    reinterpret_cast<PCCH>(pbInput),
                    cbInput,
                    reinterpret_cast<PWCH>(utf16Output.data() + iOutput),
                    cOutput);
                if (cOutputWritten > 0)
                {
                    // Successful batch.
                    iOutput += cOutputWritten;
                    assert(iOutput <= utf16Output.size());
                    break;
                }

                auto const lastError = GetLastError();
                assert(lastError != ERROR_SUCCESS);
                if (lastError != ERROR_INSUFFICIENT_BUFFER)
                {
                    status = lastError;
                    goto Done;
                }

                // Estimate was too small. Enlarge buffer and try again.
                int const cOutputNeeded = MultiByteToWideChar(
                    m_codePage,
                    mb2wcFlags,
                    reinterpret_cast<PCCH>(pbInput),
                    cbInput,
                    nullptr,
                    0);
                if (cOutputNeeded <= 0)
                {
                    status = GetLastError();
                    assert(status != ERROR_SUCCESS);
                    goto Done;
                }

                assert(cOutput < cOutputNeeded);
                EnsureSize(utf16Output, iOutput, cOutputNeeded);
            }
        }

        encodedInputPos = pbInput - pbInputBegin;
        utf16OutputPos = iOutput;
        status = ERROR_SUCCESS;
        break;
    }

Done:

    assert(encodedInputPos <= encodedInput.size());
    assert(utf16OutputPos <= utf16Output.size());
    return status;
}

LSTATUS
CodeConvert::Utf16ToEncoded(
    std::u16string_view utf16Input,
    size_t& utf16InputPos,
    std::string& encodedOutput,
    size_t& encodedOutputPos,
    unsigned wc2mbFlags,
    _In_opt_ PCCH pDefaultChar,
    _Inout_opt_ bool* pUsedDefaultChar) const
{
    LSTATUS status;

    auto const pInputBegin = utf16Input.data();
    auto const pInputEnd = pInputBegin + utf16Input.size();
    auto pInput = pInputBegin + utf16InputPos;
    size_t iOutput = encodedOutputPos;
    BOOL globalUsedDefaultChar = false;
    BOOL localUsedDefaultChar = false;
    BOOL* const pLocalUsedDefaultChar = pUsedDefaultChar ? &localUsedDefaultChar : nullptr;

    assert(pInputBegin <= pInput);
    assert(pInput <= pInputEnd);
    assert(iOutput <= encodedOutput.size());
    if (pInput < pInputBegin ||
        pInputEnd < pInput ||
        encodedOutput.size() < iOutput)
    {
        status = ERROR_INVALID_PARAMETER;
        goto Done;
    }

    switch (m_codePage | 1u) // Combine BE and LE cases
    {
    case CodePageUtf16BE: // Includes CodePageUtf16LE

        if (pDefaultChar || pUsedDefaultChar)
        {
            status = ERROR_INVALID_PARAMETER;
        }
        else
        {
            size_t const cInput = pInputEnd - pInput;

            // We need one char16 of output for each char16 of input.
            EnsureSize(encodedOutput, iOutput, cInput * sizeof(char16_t));
            auto const pOutput = reinterpret_cast<char16_t*>(encodedOutput.data() + iOutput);

            auto const result = m_codePage == CodePageUtf16BE
                ? Utf16ToUtf16<ByteSwap::Output>(pInput, cInput, pOutput)
                : Utf16ToUtf16<ByteSwap::None>(pInput, cInput, pOutput);
            utf16InputPos = static_cast<char16_t const*>(result.InputPos) - pInputBegin;
            encodedOutputPos = static_cast<char const*>(result.OutputPos) - encodedOutput.data();
            status = wc2mbFlags & WC_ERR_INVALID_CHARS ? result.UsedReplacement : ERROR_SUCCESS;
        }
        break;

    case CodePageUtf32BE: // Includes CodePageUtf32LE

        if (pDefaultChar || pUsedDefaultChar)
        {
            status = ERROR_INVALID_PARAMETER;
        }
        else
        {
            size_t const cInput = pInputEnd - pInput;

            // We need up to one char32 of output for each char16 of input.
            EnsureSize(encodedOutput, iOutput, cInput * sizeof(char32_t));
            auto const pOutput = reinterpret_cast<char32_t*>(encodedOutput.data() + iOutput);

            auto const result = m_codePage == CodePageUtf32BE
                ? Utf16ToUtf32<ByteSwap::Output>(pInput, cInput, pOutput)
                : Utf16ToUtf32<ByteSwap::None>(pInput, cInput, pOutput);
            utf16InputPos = static_cast<char16_t const*>(result.InputPos) - pInputBegin;
            encodedOutputPos = static_cast<char const*>(result.OutputPos) - encodedOutput.data();
            status = wc2mbFlags & WC_ERR_INVALID_CHARS ? result.UsedReplacement : ERROR_SUCCESS;
        }
        break;

    default: // Includes CodePageUtf8

        // Split into batches no larger than MultiByteBatchMax.
        for (int cInput; pInput < pInputEnd; pInput += cInput)
        {
            int const cInputMax = pInputEnd - pInput < MultiByteBatchMax
                ? (int)(pInputEnd - pInput)
                : MultiByteBatchMax;
            if (pInput[cInputMax - 1] < 0xD800 || pInput[cInputMax - 1] > 0xDBFF)
            {
                cInput = cInputMax;
            }
            else
            {
                // Don't end with a high surrogate.
                cInput = cInputMax - 1;
                if (cInput == 0)
                {
                    break;
                }
            }

            // Estimate that we need two bytes of output for each char16 of input.
            EnsureSize(encodedOutput, iOutput, (pInputEnd - pInput) * 2);

            // May need to retry the WC2MB after resizing encodedOutput.
            for (;;)
            {
                int const cOutput = encodedOutput.size() - iOutput < INT_MAX
                    ? (int)(encodedOutput.size() - iOutput)
                    : INT_MAX;

                assert(cInput > 0);
                assert(cOutput > 0);
                localUsedDefaultChar = false;
                int const cOutputWritten = WideCharToMultiByte(
                    m_codePage,
                    wc2mbFlags,
                    reinterpret_cast<PCWCH>(pInput),
                    cInput,
                    encodedOutput.data() + iOutput,
                    cOutput,
                    pDefaultChar,
                    pLocalUsedDefaultChar);
                if (cOutputWritten > 0)
                {
                    // Successful batch.
                    iOutput += cOutputWritten;
                    globalUsedDefaultChar |= localUsedDefaultChar;
                    assert(iOutput <= encodedOutput.size());
                    break;
                }

                auto const lastError = GetLastError();
                assert(lastError != ERROR_SUCCESS);
                if (lastError != ERROR_INSUFFICIENT_BUFFER)
                {
                    status = lastError;
                    goto Done;
                }

                // Estimate was too small. Enlarge buffer and try again.
                int const cOutputNeeded = WideCharToMultiByte(
                    m_codePage,
                    wc2mbFlags,
                    reinterpret_cast<PCWCH>(pInput),
                    cInput,
                    nullptr,
                    0,
                    pDefaultChar,
                    nullptr);
                if (cOutputNeeded <= 0)
                {
                    status = GetLastError();
                    assert(status != ERROR_SUCCESS);
                    goto Done;
                }

                assert(cOutput < cOutputNeeded);
                EnsureSize(encodedOutput, iOutput, cOutputNeeded);
            }
        }

        utf16InputPos = pInput - pInputBegin;
        encodedOutputPos = iOutput;
        if (globalUsedDefaultChar)
        {
            *pUsedDefaultChar = true;
        }
        status = ERROR_SUCCESS;
        break;
    }

Done:

    assert(utf16InputPos <= utf16Input.size());
    assert(encodedOutputPos <= encodedOutput.size());

    return status;
}
