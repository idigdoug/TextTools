// Copyright (c) Doug Cook.
// Licensed under the MIT License.

#pragma once

enum class CodePageCategory : UINT8;

class WArgs
{
    class Context;

    enum ExitCode : int
    {
        ExitCodeSuccess = 0,
        ExitCodeOtherError = 1,
        ExitCodeFatalOtherError = -1,
        ExitCodeCommandError = 123,
        ExitCodeFatalCommandError = -124,
        ExitCodeCommandKilled = 125,
        ExitCodeFatalCommandCannotRun = -126,
        ExitCodeFatalCommandNotFound = -127,
    };

    struct Encoding
    {
        unsigned CodePage;
        bool Bom;
        bool Specified;
    };

    std::wstring m_command;
    std::vector<std::wstring> m_initialArgs;
    std::wstring m_inputFilename; // -a, --arg-file
    std::wstring m_eofStr; // -E, --eof
    std::wstring m_processSlotVar; // --process-slot-var
    std::wstring m_replaceStr; // -I, --replace
    Encoding m_inputEncoding = {}; // -f, --from-code
    unsigned m_maxLines = 0; // -L, --max-lines
    unsigned m_maxArgs = 0; // -n, --max-args
    unsigned m_maxChars = 0; // -s, --max-chars
    int m_delimiter = -1; // -d, --delimiter
    INT8 m_maxProcs = -1; // -P, --max-procs
    bool m_background = 0; // -b, --background
    bool m_interactive = 0; // -p, --interactive
    bool m_noRunIfEmpty = 0; // -r, --no-run-if-empty
    bool m_openTty = 0; // -o, --open-tty
    bool m_verbose = 0; // -t, --verbose
    bool m_exitIfSizeExceeded = 0; // -x, --exit
    bool m_showLimits = 0; // --show-limits
    bool m_noQuoteArgs = 0; // -Q, --no-quote-args

private:

    static [[nodiscard]] CodePageCategory
    ParseEncoding(std::wstring_view value, PCSTR argName, _Inout_ Encoding* pEncoding);

    void
    EscapeArg(std::wstring& escapedArg, std::wstring_view arg) const;

public:

    [[nodiscard]] bool
    SetCommandAndInitialArgs(PCWSTR const pArgs[], unsigned cArgs);

    [[nodiscard]] bool
    SetInputEncoding(std::wstring_view value, PCSTR argName);

    void
    SetInputFilename(std::wstring_view value, PCSTR argName);

    void
    SetInputClipboard(PCSTR argName);

    void
    SetEofStr(std::wstring_view value, PCSTR argName);

    [[nodiscard]] bool
    SetProcessSlotVar(std::wstring_view value, PCSTR argName);

    void
    SetReplaceStr(std::wstring_view value, PCSTR argName);

    void
    SetMaxLines(unsigned value, PCSTR argName);

    void
    SetMaxArgs(unsigned value, PCSTR argName);

    void
    SetOpenTty();

    void
    SetMaxProcs(unsigned value, PCSTR argName);

    void
    SetBackground(PCSTR argName);

    void
    SetInteractive();

    void
    SetNoRunIfEmpty();

    void
    SetNoQuoteArgs();

    void
    SetMaxChars(unsigned value, PCSTR argName);

    void
    SetVerbose();

    void
    SetExitIfSizeExceeded();

    void
    SetShowLimits() noexcept;

    [[nodiscard]] bool
    SetDelimiter(std::wstring_view value, PCSTR argName) noexcept;

    [[nodiscard]] bool
    FinalizeParameters();

    /*
    Returns:
    0 = Success.
    123 = One or more commands exited with error status other than 255 (non-fatal).
    124 = A command exited with error status 255 (fatal).
    126 = A command cannot be run (fatal).
    127 = A command is not found (fatal).
    */
    [[nodiscard]] unsigned
    Run() const;
};
