// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "product.h"
#include <cwchar>
namespace qa {
inline HRESULT SettingsError(){DWORD error=GetLastError();return HRESULT_FROM_WIN32(error?error:ERROR_WRITE_FAULT);}
inline HRESULT CheckSettingsReadable(std::wstring const& file){
    if(file.empty())return HRESULT_FROM_WIN32(ERROR_PATH_NOT_FOUND);
    auto handle=CreateFileW(file.c_str(),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(handle==INVALID_HANDLE_VALUE)return SettingsError();CloseHandle(handle);return S_OK;
}
inline const wchar_t* NamesSection(DWORD flow){return flow?L"MicrophoneNames":L"OutputNames";}
inline const wchar_t* HiddenSection(DWORD flow){return flow?L"HiddenMicrophones":L"HiddenOutputs";}
inline HRESULT SaveDevicePreference(std::wstring const& file,DWORD kind,DWORD flow,std::wstring const& id,std::wstring const& alias){
    if(flow>1 || id.empty() || (kind!=4 && kind!=5 && kind!=6))return E_INVALIDARG;
    auto hr=CheckSettingsReadable(file);if(FAILED(hr))return hr;
    std::wstring quoted=L"\""+alias+L"\"";
    const wchar_t* section=kind==6?NamesSection(flow):HiddenSection(flow);
    const wchar_t* value=kind==6?(alias.empty()?nullptr:quoted.c_str()):(kind==4?L"1":nullptr);
    SetLastError(ERROR_SUCCESS);
    if(!WritePrivateProfileStringW(section,id.c_str(),value,file.c_str()))return SettingsError();
    // Flush the profile cache and the file before acknowledging the UI request.
    WritePrivateProfileStringW(nullptr,nullptr,nullptr,file.c_str());
    auto handle=CreateFileW(file.c_str(),GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(handle==INVALID_HANDLE_VALUE)return SettingsError();
    bool flushed=FlushFileBuffers(handle)!=0;hr=flushed?S_OK:SettingsError();CloseHandle(handle);if(FAILED(hr))return hr;
    if(kind==6){
        wchar_t actual[256]{};GetPrivateProfileStringW(section,id.c_str(),L"",actual,256,file.c_str());
        if(alias!=actual)return HRESULT_FROM_WIN32(ERROR_WRITE_FAULT);
    }else if((GetPrivateProfileIntW(section,id.c_str(),0,file.c_str())!=0)!=(kind==4))return HRESULT_FROM_WIN32(ERROR_WRITE_FAULT);
    return S_OK;
}
}
