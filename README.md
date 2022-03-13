# TextTools
Text processing tools for Windows - wargs (xargs for Windows), wconv (iconv for Windows)

## wargs - xargs for Windows

wargs works like [xargs](https://www.man7.org/linux/man-pages/man1/xargs.1.html), but is
coded using Win32 APIs and includes some Windows-specific options.

- Option to specify the input text encoding. Text is converted to Unicode before
  parsing and constructing the command lines.
- Option to launch commands in the background (don't wait for command to exit).
- Option to read input from clipboard.
- Supports up to 64 concurrent child commands.

## wconv - iconv for Windows

wconv works like [iconv](https://man7.org/linux/man-pages/man1/iconv.1.html), but is
coded using Win32 APIs and includes some Windows-specific options.

wconv is based on TextToolsLib, which supports a streaming interface on top of the
Win32 MultiByteToWideChar and WideCharToMultiByte APIs. As a result, it gains both
advantages and disadvantages of the Win32 APIs. It natively supports all Windows
SBCS and DBCS encodings in addition to UTF-8, UTF-16LE, UTF-16BE, UTF-32LE, and
UTF-32BE. However, it does not support any of the more-complex MBCS encodings.

## TextToolsLib - streaming text encoding/decoding library

- ArgParser.h - simple command-line argument parsing (getopt-style semantics).
- CodeConvert.h - streaming conversion from SBCS/DBCS/UTF to UTF-16LE and from
  UTF-16LE to SBCS/DBCS/UTF. The SBCS, DBCS, and UTF-8 support is based on the
  Win32 MultiByteToWideChar and WideCharToMultiByte APIs. The UTF-16 and UTF-32
  support is hand-coded. Special support for correctly handling multi-byte
  characters that cross buffer boundaries. (Does not support more-complex MBCS
  encodings.)
- CodePageInfo.h - simple class for getting properties for a code page.
- TextInput.h - handles input from a pipe, file, console, or other source.
  Converts the input from a specified encoding to UTF-16LE using CodeConvert.h.
- TextOutput.h - handles output to a pipe, file, console, or other destination.
  Converts the output from UTF-16LE to a specified encoding using
  CodeConvert.h.
