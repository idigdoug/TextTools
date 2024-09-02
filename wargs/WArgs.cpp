// Copyright (c) Doug Cook.
// Licensed under the MIT License.

#include "pch.h"
#include "WArgs.h"
#include "WArgsContext.h"
#include "TokenReader.h"

#include <CodePageInfo.h>
#include <CodeConvert.h>
#include <TextInput.h>

static constexpr std::wstring_view ClipboardFilename = L"<clipboard>";
static constexpr std::wstring_view StdInFilename = L"<stdin>";
static constexpr std::wstring_view EchoCommand = L"cmd.exe /c echo";

static constexpr UINT16 MaxCharsLimit = 32767;
static constexpr UINT16 MaxCharsDefault = 8000;
static constexpr INT8 MaxProcsLimit = MAXIMUM_WAIT_OBJECTS;
static constexpr INT8 MaxProcsDefault = 1;

static void
WarnIfNotEmpty(PCWSTR oldValue, PCSTR argName)
{
    if (oldValue != nullptr && oldValue[0] != 0)
    {
        fprintf(stderr, "%hs: warning : '%hs' overriding old value '%ls'.\n",
            AppName, argName, oldValue);
    }
}

static void
WarnIfNotZero(unsigned oldValue, PCSTR argName)
{
    if (oldValue != 0)
    {
        fprintf(stderr, "%hs: warning : '%hs' overriding old value '%u'.\n",
            AppName, argName, oldValue);
    }
}

static void
WarnIfNotNegative(int oldValue, PCSTR argName)
{
    if (oldValue >= 0)
    {
        fprintf(stderr, "%hs: warning : '%hs' overriding old value '%u'.\n",
            AppName, argName, oldValue);
    }
}

static void
EscapeArg(std::wstring& escapedArg, std::wstring_view arg)
{
    escapedArg.clear();
    escapedArg.push_back(L' ');
    if (!arg.empty() &&
        arg.npos == arg.find_first_of(std::wstring_view(L" \"\t\r\n"), 0))
    {
        escapedArg.append(arg);
    }
    else
    {
        size_t cBackslash = 0;

        escapedArg.push_back(L'"');

        for (auto const ch : arg)
        {
            switch (ch)
            {
            default:
                escapedArg.push_back(ch);
                cBackslash = 0;
                break;

            case L'\\':
                escapedArg.push_back(ch);
                cBackslash += 1;
                break;

            case L'"':
                // Before an embedded double-quote, we need to double the prior run of
                // backslashes (if any), then add one more to escape the double-quote.
                escapedArg.append(cBackslash + (size_t)1, L'\\');
                escapedArg.push_back(ch);
                cBackslash = 0;
                break;
            }
        }

        // Before the closing double-quote, we need to double the prior run of
        // backslashes (if any).
        escapedArg.append(cBackslash, L'\\');
        escapedArg.push_back(L'"');
    }
}

static void
Replace(
    std::wstring& newVal,
    std::wstring_view oldVal,
    std::wstring_view find,
    std::wstring_view replace)
{
    newVal.clear();

    size_t prev = 0;
    for (;;)
    {
        auto next = oldVal.find(find, prev);
        if (next == oldVal.npos)
        {
            newVal.append(oldVal, prev);
            break;
        }

        newVal.append(oldVal, prev, next - prev);
        newVal.append(replace);
        prev = next + find.size();
    }
}

static TextInput
OpenInput(std::wstring const& filename, unsigned codePage, TextInputFlags flags)
{
    TextInput input;

    if (filename == ClipboardFilename)
    {
        auto const status = input.OpenClipboard(flags);
        if (status != ERROR_SUCCESS)
        {
            fprintf(stderr, "%hs: warning : Clipboard error %u. Clipboard not read.\n",
                AppName, status);
            input.OpenChars({}, flags);
        }
    }
    else if (filename == StdInFilename)
    {
        input.OpenBorrowedHandle(GetStdHandle(STD_INPUT_HANDLE), codePage, flags);
    }
    else
    {
        auto status = input.OpenFile(filename.c_str(), codePage, flags);
        if (status != ERROR_SUCCESS)
        {
            fprintf(stderr, "%hs: warning : CreateFile error %u opening input file '%ls'.\n",
                AppName, status, filename.c_str());
            input.OpenChars({}, flags);
        }
    }

    return input;
}


CodePageCategory
WArgs::ParseEncoding(
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

bool
WArgs::SetCommandAndInitialArgs(PCWSTR const pArgs[], unsigned cArgs)
{
    bool ok = true;

    if (cArgs == 0)
    {
        m_command.clear();
        m_initialArgs.clear();
    }
    else
    {
        m_command = pArgs[0];
        m_initialArgs.resize(cArgs - 1);
        for (unsigned i = 1; i != cArgs; i++)
        {
            m_initialArgs[i - 1] = pArgs[i];
        }

        // Remove any leading/trailing pairs of double-quotes.
        while (m_command.size() > 1 && m_command.front() == L'"' && m_command.back() == L'"')
        {
            m_command.pop_back();
            m_command.erase(0, 1);
        }

        if (m_command.empty() ||
            m_command.size() > MAX_PATH ||
            m_command.npos != m_command.find(L'"'))
        {
            fprintf(stderr, "%hs: error : Invalid command \"%ls\". "
                "Command may not be empty, "
                "may not be longer than MAX_PATH, "
                "and may not contain '\"' characters.\n",
                AppName, m_command.c_str());
            ok = false;
        }
        else if (m_command.npos != m_command.find_first_of(std::wstring_view(L" \t"), 0))
        {
            m_command.insert(m_command.begin(), L'\"');
            m_command.push_back(L'\"');
        }
    }

    return ok;
}

bool
WArgs::SetInputEncoding(std::wstring_view value, PCSTR argName)
{
    auto const category = ParseEncoding(value, argName, &m_inputEncoding);
    return category != CodePageCategory::Error;
}

void
WArgs::SetInputFilename(std::wstring_view value, PCSTR argName)
{
    WarnIfNotEmpty(m_inputFilename.c_str(), argName);
    m_inputFilename = value;
}

void
WArgs::SetInputClipboard(PCSTR argName)
{
    WarnIfNotEmpty(m_inputFilename.c_str(), argName);
    m_inputFilename = ClipboardFilename;
}

void
WArgs::SetEofStr(std::wstring_view value, PCSTR argName)
{
    WarnIfNotEmpty(m_eofStr.c_str(), argName);
    m_eofStr = value;
}

void
WArgs::SetReplaceStr(std::wstring_view value, PCSTR argName)
{
    assert(!value.empty());

    WarnIfNotEmpty(m_replaceStr.c_str(), argName);

    m_replaceStr = value;

    if (m_maxArgs > 1)
    {
        fprintf(stderr, "%hs: warning : '%hs' overriding -n (max-args)\n",
            AppName, argName);
        m_maxArgs = 0;
    }

    if (m_maxLines > 1)
    {
        fprintf(stderr, "%hs: warning : '%hs' overriding -L (max-lines)\n",
            AppName, argName);
        m_maxLines = 0;
    }
}

void
WArgs::SetMaxLines(unsigned value, PCSTR argName)
{
    WarnIfNotZero(m_maxLines, argName);
    m_maxLines = value;

    if (!m_replaceStr.empty())
    {
        fprintf(stderr, "%hs: warning : '%hs' overriding -I (replace)\n",
            AppName, argName);
        m_replaceStr = {};
    }

    if (m_maxArgs > 1)
    {
        fprintf(stderr, "%hs: warning : '%hs' overriding -n (max-args)\n",
            AppName, argName);
        m_maxArgs = 0;
    }
}

void
WArgs::SetMaxArgs(unsigned value, PCSTR argName)
{
    WarnIfNotZero(m_maxArgs, argName);
    m_maxArgs = value;

    if (!m_replaceStr.empty())
    {
        fprintf(stderr, "%hs: warning : '%hs' overriding -I (replace)\n",
            AppName, argName);
        m_replaceStr = {};
    }

    if (m_maxLines > 1)
    {
        fprintf(stderr, "%hs: warning : '%hs' overriding -L (max-lines)\n",
            AppName, argName);
        m_maxLines = 0;
    }
}

void
WArgs::SetMaxChars(unsigned value, PCSTR argName)
{
    WarnIfNotZero(m_maxChars, argName);
    m_maxChars = value;
}

bool
WArgs::SetProcessSlotVar(std::wstring_view value, PCSTR argName)
{
    if (value.size() > 80)
    {
        fprintf(stderr, "%hs: error : Invalid %hs=\"%*ls\" - variable name too long.\n",
            AppName, argName, (unsigned)value.size(), value.data());
        return false;
    }

    for (auto ch : value)
    {
        if (ch < 32 || ch == '=')
        {
            fprintf(stderr, "%hs: error : Invalid %hs=\"%*ls\" - invalid char found.\n",
                AppName, argName, (unsigned)value.size(), value.data());
            return false;
        }
    }

    WarnIfNotEmpty(m_processSlotVar.c_str(), argName);
    m_processSlotVar = value;

    if (m_background)
    {
        fprintf(stderr, "%hs: warning : '%hs' overriding -b (background)\n",
            AppName, argName);
        m_background = false;
    }

    return true;
}

void
WArgs::SetMaxProcs(unsigned value, PCSTR argName)
{
    WarnIfNotNegative(m_maxProcs, argName);
    m_maxProcs = value <= MaxProcsLimit ? (INT8)value : MaxProcsLimit;

    if (m_background)
    {
        fprintf(stderr, "%hs: warning : '%hs' overriding -b (background)\n",
            AppName, argName);
        m_background = false;
    }
}

void
WArgs::SetBackground(PCSTR argName)
{
    m_background = true;

    if (m_maxProcs >= 0)
    {
        fprintf(stderr, "%hs: warning : '%hs' overriding -P (max-procs)\n",
            AppName, argName);
        m_maxProcs = -1;
    }

    if (!m_processSlotVar.empty())
    {
        fprintf(stderr, "%hs: warning : '%hs' overriding --process-slot-var\n",
            AppName, argName);
        m_processSlotVar = {};
    }
}

bool
WArgs::SetDelimiter(std::wstring_view value, PCSTR argName) noexcept
{
    WarnIfNotNegative(m_delimiter, argName);
    if (value.size() == 1)
    {
        m_delimiter = value[0];
    }
    else if (value[0] != L'\\')
    {
        goto Error;
    }
    else if (value.size() == 2)
    {
        switch (value[1])
        {
        default:
            goto Error;
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
            m_delimiter = value[1] - '0';
            break;
        case 'a':
            m_delimiter = '\a';
            break;
        case 'b':
            m_delimiter = '\b';
            break;
        case 'f':
            m_delimiter = '\f';
            break;
        case 'n':
            m_delimiter = '\n';
            break;
        case 'r':
            m_delimiter = '\r';
            break;
        case 't':
            m_delimiter = '\t';
            break;
        case 'v':
            m_delimiter = '\v';
            break;
        case '?':
            m_delimiter = '\?';
            break;
        case '\\':
            m_delimiter = '\\';
            break;
        case '\'':
            m_delimiter = '\'';
            break;
        case '\"':
            m_delimiter = '\"';
            break;
        }
    }
    else
    {
        switch (value[1])
        {
        default:
            goto Error;
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
            m_delimiter = 0;
            for (size_t i = 1; i != value.size(); i += 1)
            {
                if (i > 3 || value[i] < '0' || value[i] > '7')
                {
                    goto Error;
                }

                m_delimiter = m_delimiter * 8 + value[i] - '0';
            }
            break;
        case 'x':
        case 'u': // Permissive.
        case 'U': // Permissive.
            m_delimiter = 0;
            for (size_t i = 2; i != value.size(); i += 1)
            {
                m_delimiter *= 16;
                if (value[i] >= '0' && value[i] <= '9')
                {
                    m_delimiter += value[i] - '0';
                }
                else if (
                    unsigned ch = value[i] | 32;
                    ch >= 'a' && ch <= 'f')
                {
                    m_delimiter += ch - 'a' + 10;
                }
                else
                {
                    goto Error;
                }

                if (m_delimiter > WCHAR_MAX)
                {
                    goto Error;
                }
            }
            break;
        }
    }

    return true;

Error:

    fprintf(stderr, "%hs: error : Invalid %hs=\"%*ls\" "
        "- Expected a single character or a C-style wchar_t backslash escape sequence.\n",
        AppName, argName, (unsigned)value.size(), value.data());
    return false;
}

void
WArgs::SetInteractive()
{
    m_interactive = true;
}

void
WArgs::SetNoRunIfEmpty()
{
    m_noRunIfEmpty = true;
}

void
WArgs::SetOpenTty()
{
    m_openTty = true;
}

void
WArgs::SetShowLimits() noexcept
{
    m_showLimits = true;
}

void
WArgs::SetVerbose()
{
    m_verbose = true;
}

void
WArgs::SetExitIfSizeExceeded()
{
    m_exitIfSizeExceeded = true;
}

bool
WArgs::FinalizeParameters()
{
    bool ok = true;

    if (m_command.empty())
    {
        m_command = EchoCommand;
    }

    if (m_inputFilename.empty())
    {
        m_inputFilename = StdInFilename;
    }

    if (!m_replaceStr.empty() || m_maxLines != 0)
    {
        m_exitIfSizeExceeded = true;
    }

    if (m_maxChars == 0)
    {
        m_maxChars = MaxCharsDefault;
    }
    else if (m_maxChars > MaxCharsLimit)
    {
        m_maxChars = MaxCharsLimit;
    }

    if (m_background)
    {
        m_maxProcs = 0;
    }
    else if (m_maxProcs < 0)
    {
        m_maxProcs = MaxProcsDefault;
    }
    else if (m_maxProcs == 0 || m_maxProcs > MaxProcsLimit)
    {
        m_maxProcs = MaxProcsLimit;
    }

    if (m_delimiter >= 0 && !m_eofStr.empty())
    {
        fprintf(stderr, "%hs: warning : '-d' (delimiter) overriding -E (eof)\n",
            AppName);
        m_eofStr = {};
    }

    if (!m_inputEncoding.Specified)
    {
        m_inputEncoding.CodePage = CP_ACP;
        m_inputEncoding.Bom = true;
    }

    if (m_interactive)
    {
        m_verbose = true;
    }

    return ok;
}

unsigned
WArgs::Run() const
{
#ifndef NDEBUG
    fprintf(stderr, "DEBUG: %hs", AppName);
    if (m_background ||
        m_interactive ||
        m_noRunIfEmpty ||
        m_openTty ||
        m_verbose ||
        m_exitIfSizeExceeded)
    {
        fprintf(stderr, " -%hs%hs%hs%hs%hs%hs",
            m_background ? "b" : "",
            m_interactive ? "p" : "",
            m_noRunIfEmpty ? "r" : "",
            m_openTty ? "o" : "",
            m_verbose ? "t" : "",
            m_exitIfSizeExceeded ? "x" : "");
    }
    if (m_showLimits) { fprintf(stderr, " --show-limits"); }
    if (m_inputEncoding.Specified) fprintf(stderr, " -f cp%u%hs", m_inputEncoding.CodePage, m_inputEncoding.Bom ? "BOM" : "");
    if (!m_inputFilename.empty()) { fprintf(stderr, " -a\"%ls\"", m_inputFilename.c_str()); }
    if (!m_eofStr.empty()) { fprintf(stderr, " -E\"%ls\"", m_eofStr.c_str()); }
    if (!m_processSlotVar.empty()) { fprintf(stderr, " --process-slot-var=%ls", m_processSlotVar.c_str()); }
    if (!m_replaceStr.empty()) { fprintf(stderr, " -I%ls", m_replaceStr.c_str()); }
    if (m_maxLines) { fprintf(stderr, " -L%u", m_maxLines); }
    if (m_maxArgs) { fprintf(stderr, " -n%u", m_maxArgs); }
    if (m_maxChars) { fprintf(stderr, " -s%u", m_maxChars); }
    if (m_maxProcs >= 0) { fprintf(stderr, " -P%u", m_maxProcs); }
    if (m_delimiter >= 0) { fprintf(stderr, " -d\\x%02X", m_delimiter); }
    fprintf(stderr, " %ls", m_command.c_str());
    for (auto arg : m_initialArgs)
    {
        std::wstring escapedArg;
        EscapeArg(escapedArg, arg);
        fprintf(stderr, "%ls", escapedArg.c_str());
    }
    fprintf(stderr, "\n");
#endif // NDEBUG

    if (m_showLimits)
    {
        fprintf(stderr, "%hs: info : -s (max-chars) limit=%u, default=%u, actual=%u.\n",
            AppName, MaxCharsLimit, MaxCharsDefault, m_maxChars);
        fprintf(stderr, "%hs: info : -P (max-procs) limit=%u, default=%u, actual=%u.\n",
            AppName, MaxProcsLimit, MaxProcsDefault, m_maxProcs);
    }

    Context context(*this, m_inputFilename != StdInFilename);
    std::wstring commandLine = m_command;

    if (m_replaceStr.empty())
    {
        std::wstring escapedArg;
        for (auto const& arg : m_initialArgs)
        {
            EscapeArg(escapedArg, arg);
            commandLine += escapedArg;
        }
    }

    auto const commandLineInitialSize = commandLine.size();
    if (commandLineInitialSize >= m_maxChars)
    {
        fprintf(stderr, "%hs: error : Initial command line (length=%Iu) is too long (max-chars=%u).\n",
            AppName, commandLineInitialSize, m_maxChars);
        context.AccumulateExitCode(ExitCodeFatalCommandCannotRun);
    }
    else
    {
        bool const inputCheckBom = m_inputEncoding.Specified
            ? m_inputEncoding.Bom
            : ClipboardFilename != m_inputFilename;
        TextInputFlags const inputFlags =
            TextInputFlags::FoldCRLF |
            (inputCheckBom ? TextInputFlags::ConsumeBom : TextInputFlags::None) |
            TextInputFlags::InvalidMbcsError |
            TextInputFlags::CheckConsole |
            TextInputFlags::ConsoleCtrlZ;
        TokenReader reader(
            OpenInput(m_inputFilename, m_inputEncoding.CodePage, inputFlags),
            static_cast<wchar_t>(m_delimiter));

        std::wstring token;
        std::wstring replacedArg;
        std::wstring escapedArg;
        bool runWithNoArgs = !m_noRunIfEmpty && m_replaceStr.empty();

        while (!context.ExitCodeIsFatal())
        {
            bool const tokenRead =
                m_delimiter >= 0 ? reader.ReadDelimited(token)
                : m_replaceStr.empty() ? reader.ReadEscapedToken(token)
                : reader.ReadEscapedLine(token);
            if (!tokenRead ||
                (!m_eofStr.empty() && m_eofStr == token))
            {
                if (runWithNoArgs || commandLine.size() != commandLineInitialSize)
                {
                    context.StartProcess(commandLine.data());
                }

                break;
            }

            if (m_replaceStr.empty())
            {
                // Normal mode (not -I)

                EscapeArg(escapedArg, token);

                if (m_maxChars - commandLineInitialSize <= escapedArg.size())
                {
                    fprintf(stderr, "%hs: %hs : Token (length=%Iu) is too long to fit on command line (max-chars=%u).\n",
                        AppName, m_exitIfSizeExceeded ? "error" : "warning",
                        escapedArg.size(), m_maxChars);
                    if (m_exitIfSizeExceeded)
                    {
                        context.AccumulateExitCode(ExitCodeFatalCommandCannotRun);
                        break;
                    }
                    else
                    {
                        continue;
                    }
                }

                bool const tokenFits = m_maxChars > commandLine.size() + escapedArg.size();

                if (tokenFits)
                {
                    commandLine += escapedArg;
                    assert(commandLine.size() <= m_maxChars);
                }

                if (!tokenFits ||
                    (0 != m_maxArgs && m_maxArgs <= reader.TokenCount()) ||
                    (0 != m_maxLines && m_maxLines <= reader.LineCount()))
                {
                    context.StartProcess(commandLine.data());
                    commandLine.erase(commandLineInitialSize);
                    reader.ResetCounts();
                    runWithNoArgs = false;
                }

                if (!tokenFits)
                {
                    commandLine += escapedArg;
                    assert(commandLine.size() <= m_maxChars);
                }
            }
            else
            {
                // Replace mode (-I)

                for (auto const& arg : m_initialArgs)
                {
                    Replace(replacedArg, arg, m_replaceStr, token);
                    EscapeArg(escapedArg, replacedArg);
                    commandLine += escapedArg;
                }

                if (commandLine.size() >= m_maxChars)
                {
                    fprintf(stderr, "%hs: error : Command line (length=%Iu) is too long (max-chars=%u).\n",
                        AppName, commandLine.size(), m_maxChars);
                    context.AccumulateExitCode(ExitCodeFatalCommandCannotRun);
                    break;
                }

                context.StartProcess(commandLine.data());
                commandLine.erase(commandLineInitialSize);
            }
        }
    }

    context.WaitForAllProcessesToExit();
    return context.UnsignedExitCode();
}
