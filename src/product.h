#pragma once
#include <windows.h>
#include <shlobj.h>
#include <string>
#include "version.h"
#include "language.h"
namespace qa {
inline std::wstring DataDir(){
    // Never invoke shell COM folder resolution on ShellHost's UI thread.
    static std::wstring path=[](){wchar_t value[32768]{};DWORD length=GetEnvironmentVariableW(L"LOCALAPPDATA",value,32768);std::wstring result;
        if(length && length<32768)result.assign(value,length);
        if(result.empty())return std::wstring();result+=L"\\QuickAudio\\";CreateDirectoryW(result.c_str(),nullptr);return result;
    }();return path;
}
inline std::wstring SettingsFile(){auto dir=DataDir();return dir.empty()?std::wstring():dir+L"settings.ini";}
inline bool EnsureSettings(){
    auto file=SettingsFile();if(file.empty()){SetLastError(ERROR_PATH_NOT_FOUND);return false;}auto h=CreateFileW(file.c_str(),GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(h==INVALID_HANDLE_VALUE)return GetLastError()==ERROR_FILE_EXISTS;
    const wchar_t bom=0xFEFF;DWORD written=0;bool ok=WriteFile(h,&bom,2,&written,nullptr)!=0 && written==2;CloseHandle(h);return ok;
}
inline bool SetLanguage(Language mode){
    if(mode!=Language::Automatic && mode!=Language::Russian && mode!=Language::English)return false;
    if(!EnsureSettings())return false;
    bool ok=WritePrivateProfileStringW(L"General",L"Language",mode==Language::Russian?L"ru":mode==Language::English?L"en":L"auto",SettingsFile().c_str())!=0;
    if(ok)RefreshLanguage();return ok;
}
inline std::wstring ExePath(){wchar_t path[32768];DWORD n=GetModuleFileNameW(nullptr,path,32768);return std::wstring(path,n);}
inline std::wstring Quote(std::wstring const& text){return L"\""+text+L"\"";}
inline constexpr wchar_t RunKey[]=L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
inline constexpr wchar_t ApprovedKey[]=L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\StartupApproved\\Run";
inline bool StartupEnabled(){
    wchar_t command[32768]{};DWORD size=sizeof(command);
    if(RegGetValueW(HKEY_CURRENT_USER,RunKey,L"QuickAudio",RRF_RT_REG_SZ,nullptr,command,&size)!=ERROR_SUCCESS || !command[0])return false;
    BYTE approval[12]{};size=sizeof(approval);
    if(RegGetValueW(HKEY_CURRENT_USER,ApprovedKey,L"QuickAudio",RRF_RT_REG_BINARY,nullptr,approval,&size)==ERROR_SUCCESS && size && (approval[0]==3 || approval[0]==7))return false;
    return true;
}
inline LONG SetStartup(bool enabled,std::wstring const& executable){
    HKEY key=nullptr;auto error=RegCreateKeyExW(HKEY_CURRENT_USER,RunKey,0,nullptr,0,KEY_SET_VALUE,nullptr,&key,nullptr);if(error)return error;
    if(enabled){auto command=Quote(executable)+L" --run";error=RegSetValueExW(key,L"QuickAudio",0,REG_SZ,reinterpret_cast<BYTE const*>(command.c_str()),static_cast<DWORD>((command.size()+1)*2));}
    else error=RegDeleteValueW(key,L"QuickAudio");RegCloseKey(key);
    if(error==ERROR_FILE_NOT_FOUND)error=ERROR_SUCCESS;
    if(!error && RegOpenKeyExW(HKEY_CURRENT_USER,ApprovedKey,0,KEY_SET_VALUE,&key)==ERROR_SUCCESS){RegDeleteValueW(key,L"QuickAudio");RegCloseKey(key);}return error;
}
}

