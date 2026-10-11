#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#undef GetCurrentTime
#include <sddl.h>
#include <string>
#include <cstdio>
#include <mutex>
#include <algorithm>
#include <cstring>
#include "product.h"
namespace qa {
constexpr DWORD Magic=0x51415032, Version=3;
constexpr size_t MaxDevices=64, IdSize=512, NameSize=256;
constexpr CLSID TapClsid{0x05c1af10,0x9e71,0x4125,{0x85,0x12,0xb4,0xf4,0x1e,0x73,0x01,0x00}};
struct Device { wchar_t id[IdSize]; wchar_t name[NameSize],alias[NameSize]; DWORD form,hidden; };
struct State {
    DWORD magic,version,ownerPid,stopping;
    unsigned long long revision,request,completed,generation;
    DWORD syncCalls,flow,kind;
    wchar_t commandId[IdSize];
    wchar_t commandName[NameSize];DWORD autostart;
    DWORD count[2]; Device devices[2][MaxDevices];
    wchar_t defaults[2][3][IdSize];
    wchar_t status[384]; DWORD lastHresult;
};
template<size_t N> void Copy(wchar_t (&dest)[N],std::wstring const& s) { wcsncpy_s(dest,s.c_str(),_TRUNCATE); }
inline std::wstring BaseName() {
    HANDLE token; if(!OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY,&token)) return L"Local\\QuickAudio_v3";
    DWORD size=0; GetTokenInformation(token,TokenUser,nullptr,0,&size);
    std::string storage(size,'\0');
    if(!GetTokenInformation(token,TokenUser,storage.data(),size,&size)) {CloseHandle(token);return L"Local\\QuickAudio_v3";}
    LPWSTR sid=nullptr; ConvertSidToStringSidW(reinterpret_cast<TOKEN_USER*>(storage.data())->User.Sid,&sid);
    std::wstring result=L"Local\\QuickAudio_v3_"+std::wstring(sid?sid:L"unknown");
    if(sid) LocalFree(sid); CloseHandle(token); return result;
}
inline std::wstring ExeDir() { wchar_t p[32768]; GetModuleFileNameW(nullptr,p,32768);std::wstring s(p);return s.substr(0,s.find_last_of(L'\\')+1); }
inline bool Log(std::wstring const& dir,std::wstring const& message) noexcept {
    static std::mutex lock; std::lock_guard<std::mutex> guard(lock);
    SYSTEMTIME st; GetLocalTime(&st); wchar_t prefix[128];
    swprintf_s(prefix,L"%02d:%02d:%02d pid=%lu tid=%lu ",st.wHour,st.wMinute,st.wSecond,GetCurrentProcessId(),GetCurrentThreadId());
    auto w=std::wstring(prefix)+message+L"\r\n";
    int n=WideCharToMultiByte(CP_UTF8,0,w.c_str(),(int)w.size(),nullptr,0,nullptr,nullptr);
    std::string data(n,'\0'); WideCharToMultiByte(CP_UTF8,0,w.c_str(),(int)w.size(),data.data(),n,nullptr,nullptr);
    (void)dir;auto path=DataDir()+L"QuickAudio.log";
    WIN32_FILE_ATTRIBUTE_DATA info{};if(GetFileAttributesExW(path.c_str(),GetFileExInfoStandard,&info) && (info.nFileSizeHigh || info.nFileSizeLow>1024*1024))MoveFileExW(path.c_str(),(DataDir()+L"QuickAudio.previous.log").c_str(),MOVEFILE_REPLACE_EXISTING);
    auto h=CreateFileW(path.c_str(),FILE_APPEND_DATA,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(h!=INVALID_HANDLE_VALUE) {DWORD written;bool ok=WriteFile(h,data.data(),(DWORD)data.size(),&written,nullptr)!=0;CloseHandle(h);return ok;}return false;
}
struct Bus {
    HANDLE mapping=nullptr,mutex=nullptr,command=nullptr,changed=nullptr,stop=nullptr;
    State* state=nullptr;
    ~Bus() {if(state)UnmapViewOfFile(state);for(auto h:{mapping,mutex,command,changed,stop})if(h)CloseHandle(h);}
    bool Open(bool create) {
        auto base=BaseName();
        mutex=CreateMutexW(nullptr,FALSE,(base+L"_lock").c_str());
        command=CreateEventW(nullptr,FALSE,FALSE,(base+L"_command").c_str());
        changed=CreateEventW(nullptr,FALSE,FALSE,(base+L"_changed").c_str());
        stop=CreateEventW(nullptr,TRUE,FALSE,(base+L"_stop").c_str());
        mapping=create?CreateFileMappingW(INVALID_HANDLE_VALUE,nullptr,PAGE_READWRITE,0,sizeof(State),(base+L"_state").c_str()):OpenFileMappingW(FILE_MAP_ALL_ACCESS,FALSE,(base+L"_state").c_str());
        if(mapping)state=static_cast<State*>(MapViewOfFile(mapping,FILE_MAP_ALL_ACCESS,0,0,sizeof(State)));
        return state && mutex && command && changed && stop;
    }
    bool Lock(DWORD ms=1000) {auto r=WaitForSingleObject(mutex,ms);return r==WAIT_OBJECT_0 || r==WAIT_ABANDONED;}
    void Unlock() {ReleaseMutex(mutex);}
    bool Snapshot(State& out) {if(!Lock())return false;out=*state;Unlock();return out.magic==Magic && out.version==Version;}
    bool Send(DWORD kind,DWORD flow,std::wstring const& id,std::wstring const& name=L"",unsigned long long* requestNumber=nullptr) {
        if(!Lock(0))return false;
        bool ok=state->magic==Magic && !state->stopping && state->request==state->completed;
        if(ok){state->kind=kind;state->flow=flow;Copy(state->commandId,id);Copy(state->commandName,name);++state->request;if(requestNumber)*requestNumber=state->request;}
        Unlock(); if(ok)SetEvent(command);return ok;
    }
};
}
