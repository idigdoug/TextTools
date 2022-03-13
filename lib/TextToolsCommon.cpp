// Copyright (c) Doug Cook.
// Licensed under the MIT License.

#include "pch.h"
#include <TextToolsCommon.h>
#include "Utility.h"

void
TextToolsImpl::CloseHandle_delete::operator()(HANDLE h) const noexcept
{
    verify(CloseHandle(h));
}
