// Copyright (c) Doug Cook.
// Licensed under the MIT License.

#pragma once
#include <memory>

#define TEXTTOOLS_VERSION "v1.0.2"
#define TEXTTOOLS_LICENSE "Distributed under the terms of the MIT License.\nWritten by Doug Cook."

#define TEXTTOOLS_VERSION_STR(toolNameString) "\n" \
    toolNameString " (TextUtils) " TEXTTOOLS_VERSION "\n" \
    TEXTTOOLS_LICENSE "\n" \

namespace TextToolsImpl
{
    struct CloseHandle_delete
    {
        void operator()(HANDLE h) const noexcept;
    };
}

using TextToolsUniqueHandle = std::unique_ptr<void, TextToolsImpl::CloseHandle_delete>;
