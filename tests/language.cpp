// SPDX-License-Identifier: GPL-3.0-or-later
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "../src/product.h"
#include <cassert>
#include <iostream>
int main(){
    // Use an isolated profile; never change the real user's preferences.
    wchar_t temp[32768]{},folder[32768]{},original[32768]{};
    DWORD n=GetEnvironmentVariableW(L"LOCALAPPDATA",original,32768);
    assert(GetTempPathW(32768,temp)>0);
    assert(GetTempFileNameW(temp,L"qal",0,folder)!=0);
    assert(DeleteFileW(folder));assert(CreateDirectoryW(folder,nullptr));
    assert(SetEnvironmentVariableW(L"LOCALAPPDATA",folder));
    using qa::Language;
    assert(qa::LanguageMode()==Language::Automatic);
    assert(qa::ParseLanguage(L"ru")==Language::Russian);
    assert(qa::ParseLanguage(L"en")==Language::English);
    assert(qa::ParseLanguage(L"auto")==Language::Automatic);
    assert(qa::ParseLanguage(L"invalid")==Language::Automatic);
    assert(qa::UseRussian(Language::Automatic,0x0419));
    assert(qa::UseRussian(Language::Automatic,0x0819));
    assert(!qa::UseRussian(Language::Automatic,0x0409));
    assert(!qa::UseRussian(Language::Automatic,0x0407));
    assert(qa::UseRussian(Language::Russian,0x0409));
    assert(!qa::UseRussian(Language::English,0x0419));
    assert(qa::EnsureSettings());
    assert(WritePrivateProfileStringW(L"OutputNames",L"device",L"Мой звук",qa::SettingsFile().c_str()));
    for(auto mode:{Language::Russian,Language::English,Language::Automatic}){
        assert(qa::SetLanguage(mode));
        assert(qa::LanguageMode()==mode && qa::ReadLanguage()==mode);
        assert(!qa::RefreshLanguage());
        const wchar_t* expected=qa::UseRussian(mode,GetUserDefaultUILanguage())?L"Русский":L"English";
        assert(!wcscmp(qa::Text(L"Русский",L"English"),expected));
    }
    assert(!qa::SetLanguage(static_cast<Language>(99)));
    assert(qa::ReadLanguage()==Language::Automatic);
    // A write by another process is picked up on the next refresh.
    assert(WritePrivateProfileStringW(L"General",L"Language",L"en",qa::SettingsFile().c_str()));
    assert(qa::RefreshLanguage());assert(qa::LanguageMode()==Language::English);
    assert(WritePrivateProfileStringW(L"General",L"Language",L"invalid",qa::SettingsFile().c_str()));
    assert(qa::RefreshLanguage());assert(qa::LanguageMode()==Language::Automatic);
    wchar_t alias[64]{};
    GetPrivateProfileStringW(L"OutputNames",L"device",L"",alias,64,qa::SettingsFile().c_str());
    assert(!wcscmp(alias,L"Мой звук"));
    assert(DeleteFileW(qa::SettingsFile().c_str()));
    auto data=qa::DataDir();data.pop_back();assert(RemoveDirectoryW(data.c_str()));
    assert(RemoveDirectoryW(folder));
    assert(SetEnvironmentVariableW(L"LOCALAPPDATA",n?original:nullptr));
    std::cout<<"Language selection and persistence passed\n";
}
