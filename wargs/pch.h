// Copyright (c) Doug Cook.
// Licensed under the MIT License.

#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <assert.h>
#include <stdio.h>

#include <memory>
#include <string>
#include <string_view>
#include <stdexcept>
#include <vector>

PCSTR constexpr AppName = "wargs";
