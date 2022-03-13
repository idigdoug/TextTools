// Copyright (c) Doug Cook.
// Licensed under the MIT License.

#pragma once

#undef min
#undef max

#ifdef NDEBUG
#define verify(expression) (void)(expression)
#else
#define verify(expression) assert(expression)
#endif

namespace TextToolsImpl
{
    template<class Str>
    void
    EnsureSize(Str& str, size_t minSize)
    {
        if (str.size() < minSize)
        {
            str.reserve(minSize);
            str.resize(str.capacity());
        }
    }

    template<class Str>
    void
    EnsureSize(Str& str, size_t currentPos, size_t appendSize)
    {
        size_t const minSize = currentPos + appendSize < currentPos
            ? ~(size_t)0 // Force exception from reserve(minSize).
            : currentPos + appendSize;
        if (str.size() < minSize)
        {
            str.reserve(minSize);
            str.resize(str.capacity());
        }
    }
}
