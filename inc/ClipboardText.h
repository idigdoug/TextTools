// Copyright (c) Doug Cook.
// Licensed under the MIT License.

#pragma once 

LSTATUS
ClipboardTextGet(std::wstring& value);

LSTATUS
ClipboardTextSet(std::wstring_view value);
