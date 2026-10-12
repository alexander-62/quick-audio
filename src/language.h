// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <windows.h>
#include <atomic>
#include <cwchar>
#include <string>
namespace qa {
enum class Language { Automatic=0,Russian=1,English=2 };
inline Language ParseLanguage(const wchar_t* value) {
    if(!wcscmp(value,L"ru"))return Language::Russian;
    if(!wcscmp(value,L"en"))return Language::English;
    return Language::Automatic;
}
inline bool UseRussian(Language mode,LANGID displayLanguage) {
    return mode==Language::Russian || (mode==Language::Automatic && PRIMARYLANGID(displayLanguage)==LANG_RUSSIAN);
}
inline Language ReadLanguage() {
    // Resolve without shell COM on ShellHost's UI thread.
    static const std::wstring file=[](){
        wchar_t path[32768]{};DWORD n=GetEnvironmentVariableW(L"LOCALAPPDATA",path,32768);
        return n && n<32768?std::wstring(path,n)+L"\\QuickAudio\\settings.ini":std::wstring();
    }();
    if(file.empty())return Language::Automatic;
    wchar_t value[16]{};GetPrivateProfileStringW(L"General",L"Language",L"auto",value,16,file.c_str());
    return ParseLanguage(value);
}
inline std::atomic<Language>& LanguageStorage(){static std::atomic<Language> mode{Language::Automatic};return mode;}
inline Language LanguageMode(){return LanguageStorage().load();}
inline bool ApplyLanguage(Language mode){return LanguageStorage().exchange(mode)!=mode;}
inline bool RefreshLanguage(){return ApplyLanguage(ReadLanguage());}
inline const wchar_t* Text(const wchar_t* russian,const wchar_t* english) {
    return UseRussian(LanguageMode(),GetUserDefaultUILanguage())?russian:english;
}
}
