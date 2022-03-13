// Copyright (c) Doug Cook.
// Licensed under the MIT License.

#pragma once
#include <string_view>

enum class ByteOrderMatch
{
    No, // Is not a match.
    Yes, // Is a match.
    NeedMoreData, // Read more data and try again.
};

struct ByteOrderMark
{
    UINT16 CodePage;
    UINT8 Size;
    char const* Data;

    static ByteOrderMark const Standard[5]; // UTF8, UTF32LE, UTF16LE, UTF32BE, UTF16BE.

    ByteOrderMatch
    Match(std::string_view data) const;
};
