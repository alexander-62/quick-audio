// SPDX-License-Identifier: GPL-3.0-or-later
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "../src/preferences.h"
#include <cassert>
#include <iostream>
constexpr wchar_t OutputId[]=L"{0.0.0.00000000}.{11111111-2222-3333-4444-555555555555}";
constexpr wchar_t MicId[]=L"{0.0.1.00000000}.{66666666-7777-8888-9999-000000000000}";
int wmain(int argc,wchar_t** argv){
    if(argc==2 && !wcscmp(argv[1],L"--missing-env")){
        assert(SetEnvironmentVariableW(L"LOCALAPPDATA",nullptr));
        assert(qa::SettingsFile().empty());
        assert(!qa::EnsureSettings());
        assert(GetLastError()==ERROR_PATH_NOT_FOUND);
        return 0;
    }
    if(argc==3 && !wcscmp(argv[1],L"--read")){
        std::wstring file=argv[2];assert(SUCCEEDED(qa::CheckSettingsReadable(file)));
        wchar_t alias[256]{};
        GetPrivateProfileStringW(qa::NamesSection(0),OutputId,L"",alias,256,file.c_str());
        assert(!wcscmp(alias,L"Монитор \"Аудио\""));
        assert(GetPrivateProfileIntW(qa::HiddenSection(0),OutputId,0,file.c_str())==1);
        GetPrivateProfileStringW(qa::NamesSection(1),MicId,L"",alias,256,file.c_str());
        assert(!wcscmp(alias,L"Проводной микрофон"));
        assert(GetPrivateProfileIntW(qa::HiddenSection(1),MicId,0,file.c_str())==1);
        assert(SUCCEEDED(qa::SaveDevicePreference(file,6,0,OutputId,L"")));
        assert(SUCCEEDED(qa::SaveDevicePreference(file,5,0,OutputId,L"")));
        return 0;
    }
    assert(argc==1);
    wchar_t temp[32768]{},path[32768]{},executable[32768]{};
    assert(GetTempPathW(32768,temp)>0);assert(GetTempFileNameW(temp,L"qap",0,path)!=0);
    auto handle=CreateFileW(path,GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);assert(handle!=INVALID_HANDLE_VALUE);
    wchar_t bom=0xFEFF;DWORD written=0;assert(WriteFile(handle,&bom,2,&written,nullptr) && written==2);CloseHandle(handle);
    std::wstring file=path;
    assert(SUCCEEDED(qa::SaveDevicePreference(file,6,0,OutputId,L"Монитор \"Аудио\"")));
    assert(SUCCEEDED(qa::SaveDevicePreference(file,4,0,OutputId,L"")));
    assert(SUCCEEDED(qa::SaveDevicePreference(file,6,1,MicId,L"Проводной микрофон")));
    assert(SUCCEEDED(qa::SaveDevicePreference(file,4,1,MicId,L"")));
    assert(GetModuleFileNameW(nullptr,executable,32768)>0);
    auto command=qa::Quote(executable)+L" --read "+qa::Quote(file);
    STARTUPINFOW startup{sizeof(startup)};PROCESS_INFORMATION process{};
    assert(CreateProcessW(executable,command.data(),nullptr,nullptr,FALSE,0,nullptr,nullptr,&startup,&process));
    assert(WaitForSingleObject(process.hProcess,30000)==WAIT_OBJECT_0);
    DWORD code=1;assert(GetExitCodeProcess(process.hProcess,&code));CloseHandle(process.hThread);CloseHandle(process.hProcess);assert(code==0);
    wchar_t alias[256]{};GetPrivateProfileStringW(qa::NamesSection(0),OutputId,L"",alias,256,file.c_str());assert(!alias[0]);
    assert(GetPrivateProfileIntW(qa::HiddenSection(0),OutputId,0,file.c_str())==0);
    GetPrivateProfileStringW(qa::NamesSection(1),MicId,L"",alias,256,file.c_str());assert(!wcscmp(alias,L"Проводной микрофон"));
    assert(GetPrivateProfileIntW(qa::HiddenSection(1),MicId,0,file.c_str())==1);
    assert(FAILED(qa::SaveDevicePreference(L"",6,0,OutputId,L"test")));
    assert(FAILED(qa::SaveDevicePreference(file,6,2,OutputId,L"test")));
    auto lock=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);assert(lock!=INVALID_HANDLE_VALUE);
    assert(FAILED(qa::SaveDevicePreference(file,6,0,OutputId,L"blocked")));
    CloseHandle(lock);assert(DeleteFileW(path));
    assert(FAILED(qa::CheckSettingsReadable(file)));
    SetLastError(ERROR_SUCCESS);assert(FAILED(qa::SettingsError()));
    command=qa::Quote(executable)+L" --missing-env";
    assert(CreateProcessW(executable,command.data(),nullptr,nullptr,FALSE,0,nullptr,nullptr,&startup,&process));
    assert(WaitForSingleObject(process.hProcess,30000)==WAIT_OBJECT_0);
    code=1;assert(GetExitCodeProcess(process.hProcess,&code));CloseHandle(process.hThread);CloseHandle(process.hProcess);assert(code==0);
    std::cout<<"Cross-process device preference persistence passed\n";
}
