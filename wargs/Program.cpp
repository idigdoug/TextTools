// Copyright (c) Doug Cook.
// Licensed under the MIT License.

#include "pch.h"
#include "WArgs.h"

#include <ArgParser.h>
#include <TextToolsCommon.h>

static int
Usage()
{
    fputs(R"(
Usage: wargs [OPTIONS...] COMMAND [PARAMS...]

Repeatedly invokes "COMMAND PARAMS... ARGS..." with batches of ARGS... read
from input. Similar to the Unix "xargs" tool.

COMMAND is the first argument that does not start with "-" or "/".
If no COMMAND is specified, the default command is echo (cmd.exe /c echo).

-0, --null                   Same as "--delimiter=\0".
-a FILE, --arg-file=FILE     Read input from FILE instead of stdin.
-b, --background             Do not wait for command to exit.
-c, --iClip                  Read input from clipboard instead of stdin.
-d CHAR, --delimiter=CHAR    Use CHAR instead of whitespace to split up input
                             into arguments. Disables processing of "-E",
                             quotes, and backslashes during input. CHAR is
                             parsed as a C wchar_t literal, e.g. "-d$",
                             "-d\t", "-d\x0A" are all accepted.
-E EOFSTR, --eof=EOFSTR      Stop if any input argument equals EOFSTR.
-f ENCODING, --from-code=... Encoding of input. Use NNN, cpNNN, utf8,
                             utf8bom, utf16, utf16bom, utf16be, etc.
                             Default: cp0bom (CP_ACP unless BOM present).
-I REPLSTR, --replace=...    Replace instances of REPLSTR in PARAMS... with
                             line read from input. Splits input at newlines.
-L MAXLINES, --max-lines=... Limits each batch to MAXLINES lines of input.
-n MAXARGS, --max-args=...   Limits each batch to MAXARGS arguments.
-o, --open-tty               Start COMMAND with stdin = console (CONIN$).
-P MAXPROCS, --max-procs=... Start up to MAXPROCS batches in parallel.
-p, --interactive            Prompts for Y from console (CONIN$) before each
                             batch.
--process-slot-var=VAR       Set environment variable VAR to the parallelism
                             ID. Useful when MAXPROCS > 1.
-r, --no-run-if-empty        Disable the standard behavior of running COMMAND
                             once if there are no ARGS.
-s MAXCHARS, --max-chars=... Limits each batch's command length to MAXCHARS.
--show-limits                Output the limits of this implementation before
                             running any commands.
-t, --verbose                Output command line to stderr before each batch.
-x, --exit                   Exit instead of skipping the argument if the
                             argument would force the command line to exceed
                             MAXCHARS.
-h, -?, --help               Show this usage information and then exit.
--version                    Show the version number of wargs and then exit.

ENCODING names ignore case and punctuation (e.g. 'utf-8' is the same as
'UTF8'). They may be formatted as digits (Windows code page identifier), 'CP'
followed by digits, or 'UTF' followed by '8', '16', '32', '16LE', '16BE',
'32LE', or '32BE'. Input encodings may have a 'BOM' suffix indicating that if
the input starts with a BOM, the BOM should be consumed and the corresponding
UTF encoding should override the specified encoding.
)", stdout);

    return 1;
}

static int
Version()
{
    fputs(TEXTTOOLS_VERSION_STR("wargs"), stdout);
    return 1;
}

int __cdecl
wmain(int argc, _In_count_(argc) PWSTR argv[])
{
    int exitCode;

    try
    {
        WArgs wargs;
        bool showHelp = false;
        bool showVersion = false;
        exitCode = 0;

        ArgParser ap(AppName, argc, argv);
        while (ap.MoveNextArg())
        {
            std::wstring_view val;
            unsigned uval;
            if (ap.BeginDashDashArg())
            {
                if (ap.CurrentArgNameMatches(1, L"arg-file"))
                {
                    if (ap.GetLongArgVal(val, false))
                    {
                        wargs.SetInputFilename(val, "--arg-file");
                    }
                }
                else if (ap.CurrentArgNameMatches(1, L"background"))
                {
                    wargs.SetBackground("--background");
                }
                else if (ap.CurrentArgNameMatches(1, L"delimiter"))
                {
                    if (ap.GetLongArgVal(val, false))
                    {
                        ap.SetArgErrorIfFalse(wargs.SetDelimiter(val, "--delimiter"));
                    }
                }
                else if (ap.CurrentArgNameMatches(2, L"eof"))
                {
                    if (ap.GetLongArgVal(val, true))
                    {
                        wargs.SetEofStr(val, "--eof");
                    }
                }
                else if (ap.CurrentArgNameMatches(2, L"exit"))
                {
                    wargs.SetExitIfSizeExceeded();
                }
                else if (ap.CurrentArgNameMatches(1, L"from-code"))
                {
                    if (ap.GetLongArgVal(val, false))
                    {
                        ap.SetArgErrorIfFalse(wargs.SetInputEncoding(val, "--from-code"));
                    }
                }
                else if (ap.CurrentArgNameMatches(1, L"help"))
                {
                    showHelp = true;
                }
                else if (ap.CurrentArgNameMatches(2, L"iclipboard"))
                {
                    wargs.SetInputClipboard("--iclip");
                }
                else if (ap.CurrentArgNameMatches(2, L"interactive"))
                {
                    wargs.SetInteractive();
                }
                else if (ap.CurrentArgNameMatches(5, L"max-args"))
                {
                    if (ap.GetLongArgVal(uval, false, 10))
                    {
                        wargs.SetMaxArgs(uval, "--max-args");
                    }
                }
                else if (ap.CurrentArgNameMatches(5, L"max-chars"))
                {
                    if (ap.GetLongArgVal(uval, false, 10))
                    {
                        wargs.SetMaxChars(uval, "--max-chars");
                    }
                }
                else if (ap.CurrentArgNameMatches(5, L"max-lines"))
                {
                    if (ap.GetLongArgVal(uval, false, 10))
                    {
                        wargs.SetMaxLines(uval, "--max-lines");
                    }
                }
                else if (ap.CurrentArgNameMatches(5, L"max-procs"))
                {
                    if (ap.GetLongArgVal(uval, true, 10))
                    {
                        wargs.SetMaxProcs(uval, "--max-procs");
                    }
                }
                else if (ap.CurrentArgNameMatches(2, L"no-run-if-empty"))
                {
                    wargs.SetNoRunIfEmpty();
                }
                else if (ap.CurrentArgNameMatches(2, L"null"))
                {
                    ap.SetArgErrorIfFalse(wargs.SetDelimiter(L"\\0", "--null"));
                }
                else if (ap.CurrentArgNameMatches(1, L"open-tty"))
                {
                    wargs.SetOpenTty();
                }
                else if (ap.CurrentArgNameMatches(1, L"process-slot-var"))
                {
                    if (ap.GetLongArgVal(val, false))
                    {
                        ap.SetArgErrorIfFalse(wargs.SetProcessSlotVar(val, "--process-slot-var"));
                    }
                }
                else if (ap.CurrentArgNameMatches(1, L"replace"))
                {
                    if (ap.GetLongArgVal(val, true))
                    {
                        wargs.SetReplaceStr(val, "--replace");
                    }
                }
                else if (ap.CurrentArgNameMatches(1, L"show-limits"))
                {
                    wargs.SetShowLimits();
                }
                else if (ap.CurrentArgNameMatches(4, L"verbose"))
                {
                    wargs.SetVerbose();
                }
                else if (ap.CurrentArgNameMatches(4, L"version"))
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
                    case '?':
                    case 'h':
                        showHelp = true;
                        break;
                    case '0':
                        val = L"\\0";
                        ap.SetArgErrorIfFalse(wargs.SetDelimiter(val, "-0"));
                        break;
                    case 'a':
                        if (ap.ReadShortArgVal(val, false))
                        {
                            wargs.SetInputFilename(val, "-a");
                        }
                        break;
                    case 'b':
                        wargs.SetBackground("-b");
                        break;
                    case 'c':
                        wargs.SetInputClipboard("-c");
                        break;
                    case 'd':
                        if (ap.ReadShortArgVal(val, false))
                        {
                            ap.SetArgErrorIfFalse(wargs.SetDelimiter(val, "-d"));
                        }
                        break;
                    case 'E':
                        if (ap.ReadShortArgVal(val, true))
                        {
                            wargs.SetEofStr(val, "-E");
                        }
                        break;
                    case 'f':
                        if (ap.ReadShortArgVal(val, false))
                        {
                            ap.SetArgErrorIfFalse(wargs.SetInputEncoding(val, "-f"));
                        }
                        break;
                    case 'I':
                        if (ap.ReadShortArgVal(val, true))
                        {
                            wargs.SetReplaceStr(val, "-I");
                        }
                        break;
                    case 'L':
                        if (ap.ReadShortArgVal(uval, false, 10))
                        {
                            wargs.SetMaxLines(uval, "-L");
                        }
                        break;
                    case 'n':
                        if (ap.ReadShortArgVal(uval, false, 10))
                        {
                            wargs.SetMaxArgs(uval, "-n");
                        }
                        break;
                    case 'o':
                        wargs.SetOpenTty();
                        break;
                    case 'P':
                        if (ap.ReadShortArgVal(uval, true, 10))
                        {
                            wargs.SetMaxProcs(uval, "-P");
                        }
                        break;
                    case 'p':
                        wargs.SetInteractive();
                        break;
                    case 'r':
                        wargs.SetNoRunIfEmpty();
                        break;
                    case 's':
                        if (ap.ReadShortArgVal(uval, false, 10))
                        {
                            wargs.SetMaxChars(uval, "-s");
                        }
                        break;
                    case 't':
                        wargs.SetVerbose();
                        break;
                    case 'x':
                        wargs.SetExitIfSizeExceeded();
                        break;
                    default:
                        ap.PrintShortArgError();
                        break;
                    }
                }
            }
            else
            {
                ap.SetArgErrorIfFalse(wargs.SetCommandAndInitialArgs(
                    &argv[ap.CurrentArgIndex()],
                    ap.ArgCount() - ap.CurrentArgIndex()));
                break;
            }
        }

        ap.SetArgErrorIfFalse(wargs.FinalizeParameters());

        if (showHelp)
        {
            exitCode = Usage();
        }
        else if (showVersion)
        {
            exitCode = Version();
        }
        else if (ap.ArgError())
        {
            fprintf(stderr, "%hs: error : Invalid command-line. Use '%hs --help' for more information.\n",
                AppName, AppName);
            exitCode = 1;
        }
        else
        {
            exitCode = wargs.Run();
        }
    }
    catch (std::exception const& ex)
    {
        fprintf(stderr, "%hs: fatal error : %hs\n",
            AppName, ex.what());
        exitCode = 1;
    }

    return exitCode;
}
