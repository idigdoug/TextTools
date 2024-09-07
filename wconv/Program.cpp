// Copyright (c) Doug Cook.
// Licensed under the MIT License.

#include "pch.h"
#include "WConv.h"

#include <ArgParser.h>
#include <TextToolsCommon.h>

static int
Usage()
{
    fputs(R"(
Usage: wconv [-f ENCODING] [[-i] INPUTFILE...] [-t ENCODING] [-o OUTPUTFILE]
or:    wconv -l

Converts text from one encoding to another. Similar to the "iconv" tool.

-i INPUTFILE, --input=...    Input text from file. Default: stdin.
--iClip                      Input text from clipboard.
-f ENCODING, --from-code=... Encoding of input. Use NNNN, cpNNNN, utf8,
                             utf8bom, utf16, utf16bom, utf16be, etc.
                             Default: 1252bom (cp1252 unless UTF BOM present).

-o OUTPUTFILE, --output=...  Output text to file. Default: stdout.
--oClip                      Output text to clipboard.
--oNoWarn                    Don't warn for unconvertible output.
--subst=CHAR                 Substitution for unconvertible output.
                             Default: Encoding-specific, frequently '?'.
-t ENCODING, --to-code=...   Encoding of output. Use NNNN, cpNNNN, utf8,
                             utf8bom, utf16, utf16bom, utf16be, etc.
                             Default: utf8-bom (UTF-8 with BOM).

-r, --replace                Silently replace invalid input with U+FFFD.
                             Default: Report an error for invalid input.
--no-best-fit                Disable the use of best-fit characters.
-s, --silent                 Suppress conversion errors. Same as
                             '--replace --oNoWarn'.
-n NEWLINE, --newline=...    Newline output behavior: CRLF, LF, or PRESERVE.
                             Default: PRESERVE.

If -l or --list is specified, show supported encodings and exit.
If -h or --help is specified, show usage and exit.
If --version is specified, show the version number of wconv and then exit.

ENCODING names ignore case and punctuation (e.g. 'utf-8' is the same as
'UTF8'). They may be formatted as digits (Windows code page identifier), 'CP'
followed by digits, or 'UTF' followed by '8', '16', '32', '16LE', '16BE',
'32LE', or '32BE'. Input encodings may have a 'bom' suffix indicating that if
the input starts with a BOM, the BOM should be consumed and the corresponding
UTF encoding should override the specified encoding. Output UTF encodings may
have a 'BOM' suffix indicating that the resulting output should begin with a
BOM.

Examples:

  Copy input.txt (cp1252 or UTF with BOM) to output.txt (UTF-8 with BOM):
    wconv input.txt -o output.txt

  Copy input.txt (cp437) to output.txt (UTF-16BE), normalizing CR/LF to CRLF:
    wconv -f cp437 input.txt -t utf16be -o output.txt -n CRLF

  Copy input.txt to stdout (UTF-16 if console, UTF-8 with BOM if redirected):
    wconv -f utf8 input.txt

  Copy input.txt (cp1252 or UTF with BOM) to clipboard (UTF-16):
    wconv -i input.txt -oclip

  Copy text from clipboard (UTF-16) to output.txt (UTF-8 with BOM):
    wconv --iclip -o output.txt
)", stdout);

    return 1;
}

static int
Version()
{
    fputs(TEXTTOOLS_VERSION_STR("wconv"), stdout);
    return 1;
}

int __cdecl
wmain(int argc, _In_count_(argc) PWSTR argv[])
{
    int returnCode;

    try
    {
        WConv wconv;
        bool showHelp = false;
        bool showVersion = false;
        bool showList = false;

        ArgParser ap(AppName, argc, argv);
        while (ap.MoveNextArg())
        {
            std::wstring_view val;
            if (ap.BeginDashDashArg())
            {
                if (ap.CurrentArgNameMatches(1, L"from-code"))
                {
                    if (ap.GetLongArgVal(val, false))
                    {
                        ap.SetArgErrorIfFalse(wconv.SetInputEncoding(val, "--from-code"));
                    }
                }
                else if (ap.CurrentArgNameMatches(1, L"help"))
                {
                    showHelp = true;
                }
                else if (ap.CurrentArgNameMatches(2, L"iclipboard"))
                {
                    wconv.AddInputClipboard();
                }
                else if (ap.CurrentArgNameMatches(2, L"inputfile"))
                {
                    if (ap.GetLongArgVal(val, false))
                    {
                        wconv.AddInputFilename(val);
                    }
                }
                else if (ap.CurrentArgNameMatches(1, L"list"))
                {
                    showList = true;
                }
                else if (ap.CurrentArgNameMatches(2, L"newline"))
                {
                    if (ap.GetLongArgVal(val, false))
                    {
                        ap.SetArgErrorIfFalse(wconv.SetNewline(val, "--newline"));
                    }
                }
                else if (ap.CurrentArgNameMatches(2, L"no-best-fit"))
                {
                    wconv.SetNoBestFit();
                }
                else if (ap.CurrentArgNameMatches(2, L"oclipboard"))
                {
                    wconv.SetOutputClipboard("--oclip");
                }
                else if (ap.CurrentArgNameMatches(2, L"onowarning"))
                {
                    wconv.SetOutputNoDefaultCharUsedWarning();
                }
                else if (ap.CurrentArgNameMatches(2, L"outputfile"))
                {
                    if (ap.GetLongArgVal(val, false))
                    {
                        wconv.SetOutputFilename(val, "--output");
                    }
                }
                else if (ap.CurrentArgNameMatches(1, L"replace"))
                {
                    wconv.SetReplace();
                }
                else if (ap.CurrentArgNameMatches(2, L"silent"))
                {
                    wconv.SetSilent();
                }
                else if (ap.CurrentArgNameMatches(2, L"substitution"))
                {
                    if (ap.GetLongArgVal(val, false))
                    {
                        if (val.size() != 1 || val[0] > 255)
                        {
                            fprintf(stderr, "%hs: error : '%ls' requires one ASCII character for value.\n",
                                AppName, ap.CurrentArg());
                            ap.SetArgError();
                        }
                        else
                        {
                            wconv.SetOutputReplacementChar(static_cast<char>(val[0]), "--subst");
                        }
                    }
                }
                else if (ap.CurrentArgNameMatches(1, L"to-code"))
                {
                    if (ap.GetLongArgVal(val, false))
                    {
                        ap.SetArgErrorIfFalse(wconv.SetOutputEncoding(val, "--to-code"));
                    }
                }
                else if (ap.CurrentArgNameMatches(1, L"version"))
                {
                    showVersion = true;
                }
                else
                {
                    ap.PrintLongArgError();
                }
            }
            else if (ap.BeginDashOrSlashArg())
            {
                while (ap.MoveNextArgChar())
                {
                    switch (ap.CurrentArgChar())
                    {
                    case 'f':
                        if (ap.ReadShortArgVal(val, false))
                        {
                            ap.SetArgErrorIfFalse(wconv.SetInputEncoding(val, "-f"));
                        }
                        break;
                    case 'h':
                    case '?':
                        showHelp = true;
                        break;
                    case 'i':
                        if (ap.ReadShortArgVal(val, true))
                        {
                            wconv.AddInputFilename(val);
                        }
                        break;
                    case 'l':
                        showList = true;
                        break;
                    case 'n':
                        if (ap.ReadShortArgVal(val, false))
                        {
                            ap.SetArgErrorIfFalse(wconv.SetNewline(val, "-n"));
                        }
                        break;
                    case 'o':
                        if (ap.ReadShortArgVal(val, false))
                        {
                            wconv.SetOutputFilename(val, "-o");
                        }
                        break;
                    case 'r':
                        wconv.SetReplace();
                        break;
                    case 's':
                        wconv.SetSilent();
                        break;
                    case 't':
                        if (ap.ReadShortArgVal(val, false))
                        {
                            ap.SetArgErrorIfFalse(wconv.SetOutputEncoding(val, "-t"));
                        }
                        break;
                    case 'c':
                    default:
                        ap.PrintShortArgError();
                        break;
                    }
                }
            }
            else
            {
                wconv.AddInputFilename(ap.CurrentArg());
            }
        }

        ap.SetArgErrorIfFalse(wconv.FinalizeParameters());

        if (showHelp)
        {
            returnCode = Usage();
        }
        else if (showVersion)
        {
            returnCode = Version();
        }
        else if (ap.ArgError())
        {
            fprintf(stderr, "%hs: error : Invalid command-line. Use '%hs --help' for more information.\n",
                AppName, AppName);
            returnCode = 1;
        }
        else if (showList)
        {
            returnCode = wconv.PrintSupportedEncodings();
        }
        else
        {
            returnCode = wconv.Run();
        }
    }
    catch (std::exception const& ex)
    {
        fprintf(stderr, "%hs: fatal error : %hs\n",
            AppName, ex.what());
        returnCode = 1;
    }

    return returnCode;
}
