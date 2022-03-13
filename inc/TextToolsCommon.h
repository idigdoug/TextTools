// Copyright (c) Doug Cook.
// Licensed under the MIT License.

#pragma once
#include <memory>

namespace TextToolsImpl
{
    struct CloseHandle_delete
    {
        void operator()(HANDLE h) const noexcept;
    };
}

using TextToolsUniqueHandle = std::unique_ptr<void, TextToolsImpl::CloseHandle_delete>;
