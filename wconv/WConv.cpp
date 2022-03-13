// Copyright (c) Doug Cook.
// Licensed under the MIT License.

#include "pch.h" 
#include "WConv.h"

#include <ClipboardText.h>
#include <CodePageInfo.h>
#include <CodeConvert.h>
#include <TextInput.h>
#include <TextOutput.h>

static constexpr std::wstring_view ClipboardFilename = L"<clipboard>";
static constexpr std::wstring_view StdInFilename = L"<stdin>";
static constexpr std::wstring_view StdOutFilename = L"<stdout>";

static constexpr std::wstring_view PreserveStr = L"preserve";
static constexpr std::wstring_view CRLFStr = L"crlf";
static constexpr std::wstring_view LFStr = L"lf";

static void
WarnIfNotEmpty(PCWSTR oldValue, PCSTR argName)
{
    if (oldValue != nullptr && oldValue[0] != 0)
    {
        fprintf(stderr, "%hs: warning : '%hs' overriding old value '%ls'.\n",
            AppName, argName, oldValue);
    }
}

CodePageCategory
WConv::ParseEncoding(
    std::wstring_view value,
    PCSTR argName,
    _Inout_ Encoding* pEncoding)
{
    CodePageArg arg(value);
    if (arg.ParseResult == CodePageCategory::Error)
    {
        fprintf(stderr, "%hs: error : Unrecognized '%hs' encoding '%*ls'\n",
            AppName, argName, (unsigned)value.size(), value.data());
    }
    else if (CodePageInfo cpi(arg.CodePage);
        !CodeConvert::SupportsCodePage(cpi))
    {
        fprintf(stderr, "%hs: error : Unsupported '%hs' encoding '%*ls'."
            " This tool supports UTF-8, UTF-16, UTF-32, and Windows SBCS/DBCS code pages."
            " Use -l for a list of supported encodings.\n",
            AppName, argName, (unsigned)value.size(), value.data());
    }
    else
    {
        if (pEncoding->Specified)
        {
            fprintf(stderr, "%hs: warning : '%hs' overriding old value 'cp%u%hs'.\n",
                AppName, argName,
                pEncoding->CodePage, pEncoding->Bom ? "BOM" : "");
        }

        pEncoding->CodePage = arg.CodePage;
        pEncoding->Bom = arg.BomSuffix;
        pEncoding->Specified = true;
    }

    return arg.ParseResult;
}

PCSTR
WConv::NewlineBehaviorToString(NewlineBehavior newlineBehavior) noexcept
{
    switch (newlineBehavior)
    {
    case NewlineBehavior::None: return "None";
    case NewlineBehavior::Preserve: return "PRESERVE";
    case NewlineBehavior::LF: return "LF";
    case NewlineBehavior::CRLF: return "CRLF";
    default: return nullptr;
    }
}

bool
WConv::SetInputEncoding(std::wstring_view value, PCSTR argName)
{
    auto const category = ParseEncoding(value, argName, &m_inputEncoding);
    return category != CodePageCategory::Error;
}

bool
WConv::SetOutputEncoding(std::wstring_view value, PCSTR argName)
{
    auto const category = ParseEncoding(value, argName, &m_outputEncoding);
    if (category == CodePageCategory::None && m_outputEncoding.Bom)
    {
        fprintf(stderr, "%hs: warning : '%hs' ignoring BOM suffix for non-UTF code page '%*ls'.\n",
            AppName, argName,
            (unsigned)value.size(), value.data());
    }
    return category != CodePageCategory::Error;
}

void
WConv::SetOutputFilename(std::wstring_view value, PCSTR argName)
{
    WarnIfNotEmpty(m_outputFilename.c_str(), argName);
    m_outputFilename = value;
}

void
WConv::SetOutputClipboard(PCSTR argName)
{
    WarnIfNotEmpty(m_outputFilename.c_str(), argName);
    m_outputFilename = ClipboardFilename;
}

void
WConv::SetOutputReplacementChar(char value, PCSTR argName) noexcept
{
    if (m_outputDefault)
    {
        fprintf(stderr, "%hs: warning : '%hs' overriding old value '%hc'.\n",
            AppName, argName, m_outputDefaultChar);
    }

    m_outputDefaultChar = value;
    m_outputDefault = &m_outputDefaultChar;
}

bool
WConv::SetNewline(std::wstring_view value, PCSTR argName)
{
    assert(!value.empty());

    NewlineBehavior newlineBehavior;
    if (value.size() <= PreserveStr.size() &&
        CSTR_EQUAL == CompareStringOrdinal(
            PreserveStr.data(), (unsigned)value.size(),
            value.data(), (unsigned)value.size(), TRUE))
    {
        newlineBehavior = NewlineBehavior::Preserve;
    }
    else if (value.size() <= CRLFStr.size() &&
        CSTR_EQUAL == CompareStringOrdinal(
            CRLFStr.data(), (unsigned)value.size(),
            value.data(), (unsigned)value.size(), TRUE))
    {
        newlineBehavior = NewlineBehavior::CRLF;
    }
    else if (value.size() <= LFStr.size() &&
        CSTR_EQUAL == CompareStringOrdinal(
            LFStr.data(), (unsigned)value.size(),
            value.data(), (unsigned)value.size(), TRUE))
    {
        newlineBehavior = NewlineBehavior::LF;
    }
    else
    {
        fprintf(stderr, "%hs: error : Invalid %hs=\"%*ls\", expected CRLF, LF, or PRESERVE.\n",
            AppName, argName, (unsigned)value.size(), value.data());
        return false;
    }

    if (m_newlineBehavior != NewlineBehavior::None)
    {
        fprintf(stderr, "%hs: warning : '%hs' overriding old value '%hs'.\n",
            AppName, argName, NewlineBehaviorToString(m_newlineBehavior));
    }

    m_newlineBehavior = newlineBehavior;
    return true;
}

void
WConv::AddInputFilename(std::wstring_view value)
{
    m_inputFilenames.emplace_back(value);
}

void
WConv::AddInputClipboard()
{
    m_inputFilenames.emplace_back(ClipboardFilename);
}

void
WConv::SetOutputNoDefaultCharUsedWarning() noexcept
{
    m_outputNoDefaultCharUsedWarning = true;
}

void
WConv::SetReplace() noexcept
{
    m_replace = true;
}

void
WConv::SetNoBestFit() noexcept
{
    m_noBestFit = true;
}

void
WConv::SetSilent() noexcept
{
    m_replace = true;
    m_outputNoDefaultCharUsedWarning = true;
}

static BOOL CALLBACK
EnumCodePagesProc(PWSTR name)
{
    PWSTR nameEnd;
    errno = 0;
    unsigned cp = wcstoul(name, &nameEnd, 0);
    if (errno == 0 && nameEnd[0] == 0 && cp != 0)
    {
        CodePageInfo cpi(cp);
        if (cpi.Category != CodePageCategory::Utf &&
            CodeConvert::SupportsCategory(cpi.Category))
        {
            fprintf(stdout, "%ls\n", cpi.CodePageName);
        }
    }
    return true;
}

int
WConv::PrintSupportedEncodings()
{
    int returnCode;

    if (!EnumSystemCodePagesW(&EnumCodePagesProc, CP_INSTALLED))
    {
        fprintf(stderr, "%hs: error : EnumSystemCodePagesW error %u\n",
            AppName, GetLastError());
        returnCode = 1;
    }
    else
    {
        fprintf(stdout, "%-5u (UTF-16LE)\n", CodePageUtf16LE);
        fprintf(stdout, "%-5u (UTF-16BE)\n", CodePageUtf16BE);
        fprintf(stdout, "%-5u (UTF-32LE)\n", CodePageUtf32LE);
        fprintf(stdout, "%-5u (UTF-32BE)\n", CodePageUtf32BE);
        fprintf(stdout, "%-5u (UTF-8)\n", CodePageUtf8);
        returnCode = 0;
    }

    return returnCode;
}

bool
WConv::FinalizeParameters()
{
    if (!m_inputEncoding.Specified)
    {
        m_inputEncoding.CodePage = 1252;
        m_inputEncoding.Bom = true;
    }

    if (!m_outputEncoding.Specified)
    {
        m_outputEncoding.CodePage = CodePageUtf8;
        m_outputEncoding.Bom = true;
    }

    if (m_newlineBehavior == NewlineBehavior::None)
    {
        m_newlineBehavior = NewlineBehavior::Preserve;
    }

    if (m_outputFilename.empty())
    {
        m_outputFilename = StdOutFilename;
    }

    if (m_inputFilenames.empty())
    {
        m_inputFilenames.emplace_back(StdInFilename);
    }
    else
    {
        for (auto& filename : m_inputFilenames)
        {
            if (filename.empty())
            {
                filename = StdInFilename;
            }
        }
    }

    return true;
}

int
WConv::Run() const
{
#ifndef NDEBUG
    fprintf(stderr, "DEBUG: %hs", AppName);
    if (m_replace) fprintf(stderr, " -r");
    if (m_noBestFit) fprintf(stderr, " --no-best-fit");
    if (m_outputNoDefaultCharUsedWarning) fprintf(stderr, " --oNoWarn");
    if (m_outputDefault) fprintf(stderr, " --subst=\"%hc\"", m_outputDefaultChar);
    fprintf(stderr, " -n %hs", NewlineBehaviorToString(m_newlineBehavior));

    if (m_inputEncoding.Specified) fprintf(stderr, " -f cp%u%hs", m_inputEncoding.CodePage, m_inputEncoding.Bom ? "BOM" : "");
    for (auto& inputFilename : m_inputFilenames)
    {
        fprintf(stderr, " \"%ls\"", inputFilename.c_str());
    }

    if (m_outputEncoding.Specified) fprintf(stderr, " -t cp%u%hs", m_outputEncoding.CodePage, m_outputEncoding.Bom ? "BOM" : "");
    fprintf(stderr, " -o \"%ls\"", m_outputFilename.c_str());
    fprintf(stderr, "\n");
#endif // NDEBUG

    int returnCode = 0;
    bool usedDefaultChar = false;
    bool* const pUsedDefaultChar = m_outputNoDefaultCharUsedWarning ? nullptr : &usedDefaultChar;

    TextOutput output;
    bool const outputClipboard = ClipboardFilename == m_outputFilename;
    bool const outputInsertBom = m_outputEncoding.Specified
        ? m_outputEncoding.Bom
        : !outputClipboard; // Add BOM to clipboard only if explicit '-t'.
    TextOutputFlags const outputFlags =
        (NewlineBehavior::CRLF == m_newlineBehavior ? TextOutputFlags::ExpandCRLF : TextOutputFlags::None) |
        (outputInsertBom ? TextOutputFlags::InsertBom : TextOutputFlags::None) |
        (m_replace ? TextOutputFlags::None : TextOutputFlags::InvalidUtf16Error) |
        (m_noBestFit ? TextOutputFlags::NoBestFitChars : TextOutputFlags::None) |
        TextOutputFlags::CheckConsole;
    if (outputClipboard)
    {
        output.OpenChars(outputFlags);
    }
    else if (m_outputFilename == StdOutFilename)
    {
        output.OpenBorrowedHandle(GetStdHandle(STD_OUTPUT_HANDLE), m_outputEncoding.CodePage, outputFlags);
    }
    else
    {
        output.OpenFile(m_outputFilename.c_str(), m_outputEncoding.CodePage, outputFlags);
    }

    TextInput input;
    for (auto const& inputFilename : m_inputFilenames)
    {
        bool const inputClipboard = ClipboardFilename == inputFilename;
        bool const inputCheckBom = m_inputEncoding.Specified
            ? m_inputEncoding.Bom
            : !inputClipboard; // Eat BOM from clipboard only if explicit '-b'.
        TextInputFlags const inputFlags =
            (NewlineBehavior::Preserve != m_newlineBehavior ? TextInputFlags::FoldCRLF : TextInputFlags::None) |
            (inputCheckBom ? TextInputFlags::ConsumeBom : TextInputFlags::None) |
            (m_replace ? TextInputFlags::None : TextInputFlags::InvalidMbcsError) |
            TextInputFlags::CheckConsole |
            TextInputFlags::ConsoleCtrlZ;
        if (inputClipboard)
        {
            auto const status = input.OpenClipboard(inputFlags);
            if (status != ERROR_SUCCESS)
            {
                fprintf(stderr, "%hs: warning : clipboard error %u. Clipboard not read.\n",
                    AppName, status);
                input.OpenChars({}, inputFlags);
            }
        }
        else if (StdInFilename == inputFilename)
        {
            input.OpenBorrowedHandle(GetStdHandle(STD_INPUT_HANDLE), m_inputEncoding.CodePage, inputFlags);
        }
        else
        {
            auto status = input.OpenFile(inputFilename.c_str(), m_inputEncoding.CodePage, inputFlags);
            if (status != ERROR_SUCCESS)
            {
                fprintf(stderr, "%hs: warning : CreateFile error %u opening input file '%ls'. Skipping.\n",
                    AppName, status, inputFilename.c_str());
                continue;
            }
        }

        try
        {
            do
            {
                auto inputChars = input.Chars();
                if (input.Mode() == TextInputMode::Console &&
                    inputChars.ends_with(L'\x1A')) // Control-Z
                {
                    inputChars.remove_suffix(1);
                    output.WriteChars(inputChars, m_outputDefault, pUsedDefaultChar);
                    break;
                }

                output.WriteChars(inputChars, m_outputDefault, pUsedDefaultChar);
            } while (input.ReadNextChars());
        }
        catch (std::range_error const& ex)
        {
            fprintf(stderr, "%ls: error : %hs\n",
                inputFilename.c_str(), ex.what());
            returnCode = 1;
        }
    }

    if (usedDefaultChar)
    {
        fprintf(stderr, "%hs: warning : Some input could not be converted to the output encoding.\n",
            AppName);
    }

    if (outputClipboard)
    {
        auto const clipChars = output.BufferedChars();
        static_assert(sizeof(char16_t) == sizeof(wchar_t));
        auto const status = ClipboardTextSet({ (wchar_t const*)clipChars.data(), clipChars.size() });
        if (status != ERROR_SUCCESS)
        {
            fprintf(stderr, "%hs: error : clipboard error %u. Clipboard not updated.\n",
                AppName, status);
            returnCode = 1;
        }
    }

    return returnCode;
}
