// Copyright (c) Doug Cook.
// Licensed under the MIT License.

#pragma once
#include <string_view>

/*
Parser for argument lists that follow getopt-style rules.

Typical usage:

    ArgParser ap("MyApp", argc, argv);
    while (ap.MoveNextArg())
    {
        if (ap.BeginDashDashArg())
        {
            if (ap.CurrentArgIsEmpty()) // i.e. just "--"
            {
                ...
            }
            // Need 2 characters to distinguish "sandbox" from "separate".
            else if (ap.CurrentArgNameMatches(2, L"sandbox"))
            {
                argSandbox = true;
            }
            // Need 2 characters to distinguish "sandbox" from "separate".
            else if (ap.CurrentArgNameMatches(2, "separate))
            {
                if (!ap.GetLongArgVal(argSeparate, true)) { error... }
            }
            else
            {
                ap.PrintLongArgError();
            }
        }
        else if (ap.BeginDashOrSlashArg())
        {
            if (ap.CurrentArgIsEmpty()) // i.e. just "-" or "/"
            {
                ...
            }
            else while (ap.MoveNextArgChar())
            {
                switch (ap.CurrentArgChar())
                {
                case 'a':
                    argA = true;
                    break;
                case 'b':
                    if (!ap.ReadShortArgVal(argB, true)) { error... }
                    break;
                default:
                    ap.PrintShortArgError();
                    break;
                }
            }
        }
        else
        {
            fileArgs.push_back(ap.CurrentArg());
        }
    }
*/
class ArgParser
{
    PCSTR const m_appName;
    _Field_size_(m_argCount) PWSTR const* const m_args;
    unsigned const m_argCount;
    unsigned m_currentArgIndex;
    PCWSTR m_currentArgPos;
    bool m_argError;

private:

    bool
    ReadArgValImpl(
        std::wstring_view& val,
        bool emptyOk,
        _In_opt_z_ PCWSTR shortArgVal,
        WCHAR argChar,
        bool space) noexcept;

    bool
    ReadArgValImpl(
        unsigned& val,
        bool zeroOk,
        int radix,
        _In_opt_z_ PCWSTR shortArgVal,
        WCHAR argChar,
        bool space) noexcept;

public:

    /*
    Sets AppName=appName, Args=argv, ArgCount=argc,
    CurrentArg=argv[0], CurrentArgPos="", ArgError=false.
    */
    ArgParser(
        _In_z_ PCSTR appName, // Used in error messages.
        unsigned argc,
        _In_count_(argc) PWSTR argv[]) noexcept;

    // Returns AppName.
    _Ret_z_ PCSTR
    AppName() const noexcept;

    // Returns Args[index].
    _Ret_z_ PCWSTR
    Arg(unsigned index) const noexcept;

    // Returns ArgCount.
    unsigned
    ArgCount() const noexcept;

    // Returns CurrentArgIndex.
    unsigned
    CurrentArgIndex() const noexcept;

    // Returns Args[CurrentArgIndex].
    _Ret_z_ PCWSTR
    CurrentArg() const noexcept;

    // Returns CurrentArgPos.
    _Ret_z_ PCWSTR
    CurrentArgPos() const noexcept;

    // Returns CurrentArgPos[0].
    WCHAR
    CurrentArgChar() const noexcept;

    // Returns CurrentArgPos[1] == 0.
    bool
    CurrentArgIsEmpty() const noexcept;

    // Returns string from CurrentArgPos[1] to first of ':', '=', '\0'.
    std::wstring_view
    CurrentArgName() const noexcept;

    // Returs true if CurrentArgName is at least minMatchLength characters and
    // is a prefix of expectedArgName.
    bool
    CurrentArgNameMatches(unsigned minMatchLength, PCWSTR expectedName) const noexcept;

    // Returns ArgError.
    bool
    ArgError() const noexcept;

    // Sets ArgError = value.
    void
    SetArgError(bool value = true) noexcept;

    // Sets ArgError = ArgError || !argOk.
    void
    SetArgErrorIfFalse(bool argOk) noexcept;

    // Sets ArgError=true and prints
    // "AppName: error : Unrecognized argument '-CurrentArgChar'".
    void
    PrintShortArgError() noexcept;

    // Sets ArgError=true and prints
    // "AppName: error : Unrecognized argument 'CurrentArg'".
    void
    PrintLongArgError() noexcept;

    // Sets CurrentArgIndex += 1, CurrentArgPos = "".
    // Returns true if CurrentArgIndex < ArgCount.
    bool
    MoveNextArg() noexcept;

    // If CurrentArg starts with "--", sets CurrentArgPos to before-begin and
    // returns true.
    bool
    BeginDashDashArg() noexcept;

    // If CurrentArg starts with "-" or "/", sets CurrentArgPos to before-begin
    // and returns true.
    bool
    BeginDashOrSlashArg() noexcept;

    // If CurrentArg starts with "-", sets CurrentArgPos to before-begin and
    // returns true.
    bool
    BeginDashArg() noexcept;

    // If CurrentArg starts with "/", sets CurrentArgPos to before-begin and
    // returns true.
    bool
    BeginSlashArg() noexcept;

    // Sets CurrentArgPos += 1. Returns true if CurrentArgPos < End.
    bool
    MoveNextArgChar() noexcept;

    // Consumes and returns the remainder of the current short arg.
    // Example: "-ab123", if current ArgChar is 'b', consumes and returns "123".
    _Ret_z_ PCWSTR
    ReadArgCharsVal() noexcept;

    // Consumes and returns the value of next arg, null if none.
    // Example: "-abc 123", if current ArgChar is 'b', consumes and returns "123".
    _Ret_opt_z_ PCWSTR
    ReadNextArgVal() noexcept;

    // Consumes and returns the value of the current short arg, null if none.
    // This selects between behavior of ReadArgCharsVal() and ReadNextArgVal(),
    // depending on whether we're at the end of the current short arg.
    // Example: "-abc 123", if current ArgChar is 'b', consumes and returns "c".
    // Example: "-abc 123", if current ArgChar is 'c', consumes and returns "123".
    _Ret_opt_z_ PCWSTR
    ReadShortArgVal() noexcept;

    // Returns the value of the current long arg, null if none.
    _Ret_opt_z_ PCWSTR
    GetLongArgVal() const noexcept;

    // Consumes and returns the remainder of the current short arg. Prints error
    // message if none.
    //
    // Example: "-ab123", if current ArgChar is 'b', consumes and returns "123".
    bool
    ReadArgCharsVal(std::wstring_view& val, bool emptyOk) noexcept;

    // Consumes and returns the value of next arg. Prints error
    // message and returns false if none.
    //
    // Example: "-abc 123", if current ArgChar is 'b', consumes and returns "123".
    bool
    ReadNextArgVal(std::wstring_view& val, bool emptyOk) noexcept;

    // Consumes and returns the value of the current short arg, error if invalid.
    // This selects between behavior of ReadArgCharsVal() and ReadNextArgVal(),
    // depending on whether we're at the end of the current short arg.
    // Example: "-abc 123", if current ArgChar is 'b', consumes and returns "c".
    // Example: "-abc 123", if current ArgChar is 'c', consumes and returns "123".
    bool
    ReadShortArgVal(std::wstring_view& val, bool emptyOk) noexcept;

    // Returns the value of the current long arg. Prints error message if none.
    bool
    GetLongArgVal(std::wstring_view& val, bool emptyOk) noexcept;

    // Consumes and returns the remainder of the current short arg. Prints error
    // message if none.
    //
    // Example: "-ab123", if current ArgChar is 'b', consumes and returns "123".
    bool
    ReadArgCharsVal(unsigned& val, bool zeroOk, int radix = 0) noexcept;

    // Consumes and returns the value of next arg. Prints error message
    // if none or invalid.
    //
    // Example: "-abc 123", if current ArgChar is 'b', consumes and returns "123".
    bool
    ReadNextArgVal(unsigned& val, bool zeroOk, int radix = 0) noexcept;

    // Consumes and returns the value of the current short arg, error if invalid.
    // This selects between behavior of ReadArgCharsVal() and ReadNextArgVal(),
    // depending on whether we're at the end of the current short arg.
    // Example: "-abc 123", if current ArgChar is 'b', consumes and returns "c".
    // Example: "-abc 123", if current ArgChar is 'c', consumes and returns "123".
    bool
    ReadShortArgVal(unsigned& val, bool zeroOk, int radix = 0) noexcept;

    // Returns the value of the current long arg. Prints error message if none
    // or invalid.
    bool
    GetLongArgVal(unsigned& val, bool zeroOk, int radix = 0) noexcept;
};
