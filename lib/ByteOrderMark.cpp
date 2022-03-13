// Copyright (c) Doug Cook.
// Licensed under the MIT License.

#include "pch.h"
#include "ByteOrderMark.h"
#include <CodePageInfo.h>

ByteOrderMark const ByteOrderMark::Standard[5] = {
    { CodePageUtf8,    3, "\xEF\xBB\xBF" },
    { CodePageUtf32LE, 4, "\xFF\xFE\x00\x00" }, // Must come before UTF16LE.
    { CodePageUtf16LE, 2, "\xFF\xFE" },
    { CodePageUtf32BE, 4, "\x00\x00\xFE\xFF" },
    { CodePageUtf16BE, 2, "\xFE\xFF" },
};

ByteOrderMatch
ByteOrderMark::Match(std::string_view data) const
{
    auto const dataProvided = data.data();
    auto const sizeProvided = data.size();
    UINT8 sizeToCheck = Size <= sizeProvided
        ? Size
        : (UINT8)sizeProvided;
    return 0 != memcmp(Data, dataProvided, sizeToCheck) ? ByteOrderMatch::No
        : sizeToCheck == Size ? ByteOrderMatch::Yes
        : ByteOrderMatch::NeedMoreData;
}
