#pragma once
#include "shared.h"
#include <tlhelp32.h>
#include <commctrl.h>
#include <memory>
#include <shellapi.h>
namespace qa {
inline int StopAgent(){
    Bus bus;if(!bus.Open(false))return GetLastError()==ERROR_FILE_NOT_FOUND?0:1;
    auto state=std::make_unique<State>();if(!bus.Snapshot(*state))return 1;
    HANDLE process=state->ownerPid?OpenProcess(SYNCHRONIZE,FALSE,state->ownerPid):nullptr;
    bool signalled=SetEvent(bus.stop)!=0;DWORD waited=process?WaitForSingleObject(process,5000):WAIT_OBJECT_0;if(process)CloseHandle(process);
    return signalled && waited==WAIT_OBJECT_0?0:1;
}
inline bool LaunchAgent(){
    std::wstring command=Quote(ExePath())+L" --run";STARTUPINFOW start{sizeof(start)};start.dwFlags=STARTF_USESHOWWINDOW;start.wShowWindow=SW_HIDE;PROCESS_INFORMATION process{};
    bool ok=CreateProcessW(ExePath().c_str(),command.data(),nullptr,nullptr,FALSE,0,nullptr,nullptr,&start,&process)!=0;
    if(ok){CloseHandle(process.hThread);CloseHandle(process.hProcess);}return ok;
}
inline int PrepareUpdate(){
    if(auto window=FindWindowW(L"QuickAudio.Manager",nullptr)){
        DWORD pid=0;GetWindowThreadProcessId(window,&pid);auto owner=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION|SYNCHRONIZE,FALSE,pid);
        if(owner){wchar_t path[32768];DWORD length=32768;
            if(QueryFullProcessImageNameW(owner,0,path,&length) && !_wcsicmp(path,ExePath().c_str())){PostMessageW(window,WM_CLOSE,0,0);if(WaitForSingleObject(owner,5000)!=WAIT_OBJECT_0){CloseHandle(owner);return 1;}}
            CloseHandle(owner);
        }
    }
    if(StopAgent())return 1;Sleep(750);
    // A diagnostic DLL remains pinned in ShellHost. Release only shell hosts
    // in this session that have a QuickAudio DLL loaded from this installation.
    DWORD session=0;ProcessIdToSessionId(GetCurrentProcessId(),&session);
    auto executable=ExePath();auto directory=executable.substr(0,executable.find_last_of(L'\\')+1);
    auto snapshot=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);if(snapshot==INVALID_HANDLE_VALUE)return 1;
    PROCESSENTRY32W process{sizeof(process)};int result=0;
    if(Process32FirstW(snapshot,&process))do{
        DWORD other=0;if(_wcsicmp(process.szExeFile,L"ShellHost.exe") || !ProcessIdToSessionId(process.th32ProcessID,&other) || session!=other)continue;
        bool ours=false;auto modules=CreateToolhelp32Snapshot(TH32CS_SNAPMODULE|TH32CS_SNAPMODULE32,process.th32ProcessID);
        if(modules==INVALID_HANDLE_VALUE)continue;MODULEENTRY32W entry{sizeof(entry)};
        if(Module32FirstW(modules,&entry))do{
            std::wstring path(entry.szExePath);auto slash=path.find_last_of(L'\\');
            if(slash!=std::wstring::npos && !_wcsicmp(path.substr(0,slash+1).c_str(),directory.c_str()) && path.substr(slash+1).find(L"QuickAudio.Hook.")==0)ours=true;
        }while(Module32NextW(modules,&entry));CloseHandle(modules);
        if(ours){auto host=OpenProcess(PROCESS_TERMINATE|SYNCHRONIZE,FALSE,process.th32ProcessID);
            if(!host)result=1;else{if(!TerminateProcess(host,0) || WaitForSingleObject(host,5000)!=WAIT_OBJECT_0)result=1;CloseHandle(host);}}
    }while(Process32NextW(snapshot,&process));CloseHandle(snapshot);return result;
}
inline bool AgentRunning(){Bus bus;if(!bus.Open(false))return false;auto state=std::make_unique<State>();if(!bus.Snapshot(*state) || state->stopping || !state->ownerPid)return false;auto process=OpenProcess(SYNCHRONIZE,FALSE,state->ownerPid);if(!process)return false;bool active=WaitForSingleObject(process,0)==WAIT_TIMEOUT;CloseHandle(process);return active;}
inline HWND Child(HWND parent,const wchar_t* klass,const wchar_t* title,DWORD style,int x,int y,int w,int h,int id,HFONT font){
    auto child=CreateWindowExW(0,klass,title,WS_CHILD|WS_VISIBLE|style,x,y,w,h,parent,reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),GetModuleHandleW(nullptr),nullptr);
    SendMessageW(child,WM_SETFONT,reinterpret_cast<WPARAM>(font),TRUE);return child;
}
inline LRESULT CALLBACK ManagerProc(HWND window,UINT message,WPARAM wp,LPARAM lp){
    static HFONT font=nullptr;static HWND status=nullptr,start=nullptr,stop=nullptr,startup=nullptr;
    switch(message){
    case WM_CREATE:{
        font=CreateFontW(-16,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
        Child(window,L"STATIC",L"QuickAudio " QA_VERSION_W,0,20,16,350,24,0,font);
        Child(window,L"STATIC",L"Выход звука и микрофон прямо в Win + A",0,20,48,350,24,0,font);
        status=Child(window,L"STATIC",L"",0,20,82,350,24,0,font);
        start=Child(window,L"BUTTON",L"Включить",WS_TABSTOP|BS_PUSHBUTTON,20,116,158,32,101,font);
        stop=Child(window,L"BUTTON",L"Выключить",WS_TABSTOP|BS_PUSHBUTTON,190,116,158,32,102,font);
        startup=Child(window,L"BUTTON",L"Запускать вместе с Windows",WS_TABSTOP|BS_AUTOCHECKBOX,20,166,340,28,103,font);
        Child(window,L"BUTTON",L"Папка настроек",WS_TABSTOP|BS_PUSHBUTTON,20,208,158,32,104,font);
        Child(window,L"BUTTON",L"GitHub",WS_TABSTOP|BS_PUSHBUTTON,190,208,158,32,105,font);
        SetTimer(window,1,1000,nullptr);SendMessageW(window,WM_TIMER,1,0);return 0;}
    case WM_TIMER:{bool running=AgentRunning();SetWindowTextW(status,running?L"Панель включена — откройте Win + A":L"Панель выключена");EnableWindow(start,!running);EnableWindow(stop,running);SendMessageW(startup,BM_SETCHECK,StartupEnabled()?BST_CHECKED:BST_UNCHECKED,0);return 0;}
    case WM_COMMAND:{auto id=LOWORD(wp);bool ok=true;
        if(id==101)ok=LaunchAgent();else if(id==102)ok=StopAgent()==0;
        else if(id==103)ok=SetStartup(SendMessageW(startup,BM_GETCHECK,0,0)==BST_CHECKED,ExePath())==ERROR_SUCCESS;
        else if(id==104)ShellExecuteW(window,L"open",DataDir().c_str(),nullptr,nullptr,SW_SHOWNORMAL);
        else if(id==105)ShellExecuteW(window,L"open",L"https://github.com/alexander-62/quick-audio",nullptr,nullptr,SW_SHOWNORMAL);
        if(!ok)MessageBoxW(window,L"Не удалось применить настройку. Проверьте журнал в папке настроек.",L"QuickAudio",MB_OK|MB_ICONERROR);
        SendMessageW(window,WM_TIMER,1,0);return 0;}
    case WM_DESTROY:KillTimer(window,1);if(font)DeleteObject(font);PostQuitMessage(0);return 0;
    }return DefWindowProcW(window,message,wp,lp);
}
inline int ShowManager(HINSTANCE instance){
    const wchar_t* name=L"QuickAudio.Manager";auto existing=FindWindowW(name,nullptr);if(existing){ShowWindow(existing,SW_RESTORE);SetForegroundWindow(existing);return 0;}
    INITCOMMONCONTROLSEX common{sizeof(common),ICC_STANDARD_CLASSES};InitCommonControlsEx(&common);
    WNDCLASSW klass{};klass.hInstance=instance;klass.lpszClassName=name;klass.lpfnWndProc=ManagerProc;klass.hCursor=LoadCursorW(nullptr,MAKEINTRESOURCEW(32512));klass.hIcon=LoadIconW(instance,MAKEINTRESOURCEW(1));klass.hbrBackground=reinterpret_cast<HBRUSH>(COLOR_WINDOW+1);RegisterClassW(&klass);
    RECT rect{0,0,374,262};AdjustWindowRectEx(&rect,WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX,FALSE,0);
    auto window=CreateWindowExW(0,name,L"QuickAudio — настройки",WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX,CW_USEDEFAULT,CW_USEDEFAULT,rect.right-rect.left,rect.bottom-rect.top,nullptr,nullptr,instance,nullptr);if(!window)return 1;
    ShowWindow(window,SW_SHOWNORMAL);MSG message;while(GetMessageW(&message,nullptr,0,0)>0){if(!IsDialogMessageW(window,&message)){TranslateMessage(&message);DispatchMessageW(&message);}}return 0;
}
}
