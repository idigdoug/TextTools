// Copyright (c) Doug Cook.
// Licensed under the MIT License.

#pragma once

enum class CodePageCategory : UINT8;

class WConv
{
    enum class NewlineBehavior : UCHAR
    {
        None,
        Preserve,
        LF,
        CRLF,
    };

    struct Encoding
    {
        unsigned CodePage;
        bool Bom;
        bool Specified;
    };

    Encoding m_inputEncoding = {}; // -f, --from-code
    Encoding m_outputEncoding = {};  // -t, --to-code
    bool m_replace = false;
    bool m_noBestFit = false;
    NewlineBehavior m_newlineBehavior = {};
    bool m_outputNoDefaultCharUsedWarning = false;
    char m_outputDefaultChar = 0;
    PCCH m_outputDefault = nullptr;

    std::wstring m_outputFilename;
    std::vector<std::wstring> m_inputFilenames;

private:

    static PCSTR
    NewlineBehaviorToString(NewlineBehavior newlineBehavior) noexcept;

    static [[nodiscard]] CodePageCategory
    ParseEncoding(std::wstring_view value, PCSTR argName, _Inout_ Encoding* pEncoding);

public:

    [[nodiscard]] bool
    SetInputEncoding(std::wstring_view value, PCSTR argName);

    [[nodiscard]] bool
    SetOutputEncoding(std::wstring_view value, PCSTR argName);

    void
    SetOutputFilename(std::wstring_view value, PCSTR argName);

    void
    SetOutputClipboard(PCSTR argName);

    void
    SetOutputReplacementChar(char value, PCSTR argName) noexcept;

    [[nodiscard]] bool
    SetNewline(std::wstring_view value, PCSTR argName);

    void
    AddInputFilename(std::wstring_view value);

    void
    AddInputClipboard();

    void
    SetOutputNoDefaultCharUsedWarning() noexcept;

    void
    SetReplace() noexcept;

    void
    SetNoBestFit() noexcept;

    void
    SetSilent() noexcept;

    [[nodiscard]] static int
    PrintSupportedEncodings();

    [[nodiscard]] bool
    FinalizeParameters();

    [[nodiscard]] int
    Run() const;
};
