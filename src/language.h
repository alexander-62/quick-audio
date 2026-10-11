// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <windows.h>
namespace qa {
// Use the account's Windows display language, not the formatting locale.
// Cache per module so worker threads and the shell UI use the same language.
inline const wchar_t* Text(const wchar_t* russian,const wchar_t* english) {
    static const bool useRussian=PRIMARYLANGID(GetUserDefaultUILanguage())==LANG_RUSSIAN;
    return useRussian?russian:english;
}
}
