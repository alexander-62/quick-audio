#pragma once
#include "shared.h"
#include <tlhelp32.h>
#include <commctrl.h>
#include <memory>
#include <shellapi.h>
#include "updater.h"
#include <thread>
namespace qa {
inline constexpr UINT UpdateChanged=WM_APP+20,OpenUpdates=WM_APP+21;
enum class UpdateStage { Idle,Checking,Available,Downloading,Ready,Launched,Error };
struct UpdateUi {
    std::mutex mutex;std::atomic<bool> cancel{false};HWND window=nullptr;
    UpdateStage stage=UpdateStage::Idle;updates::Release release;
    std::shared_ptr<updates::Installer> installer;std::wstring message=qa::Text(L"Проверка выполняется по нажатию кнопки.",L"Click the button to check for updates.");
    bool installed=updates::Installed();int percent=-1;
    void Notify(){if(!cancel)PostMessageW(window,UpdateChanged,0,0);}
    void Fail(std::wstring const& text){ {std::lock_guard<std::mutex> lock(mutex);stage=UpdateStage::Error;message=text;}Notify();}
};
inline void CheckUpdates(std::shared_ptr<UpdateUi> const& ui){
    {std::lock_guard<std::mutex> lock(ui->mutex);ui->stage=UpdateStage::Checking;ui->message=qa::Text(L"Проверяем GitHub Releases…",L"Checking GitHub Releases…");ui->release={};ui->installer=nullptr;}
    ui->Notify();std::thread([ui]{
        CoInitializeEx(nullptr,COINIT_MULTITHREADED);
        try{
            auto release=updates::Latest(ui->cancel);bool newer=updates::IsNewer(release.version,QA_VERSION_W);
            {std::lock_guard<std::mutex> lock(ui->mutex);ui->release=std::move(release);ui->stage=newer?UpdateStage::Available:UpdateStage::Idle;
                ui->message=newer?qa::Text(L"Доступна версия ",L"Version available: ")+ui->release.version+(ui->installed?qa::Text(L". Можно установить обновление.",L". You can install the update."):qa::Text(L". Для portable скачайте ZIP из выпуска.",L". For portable, download the release ZIP.")):std::wstring(qa::Text(L"Установлена актуальная версия ",L"You have the latest version: "))+QA_VERSION_W+L".";}
            ui->Notify();
        }catch(updates::Error const& e){ui->Fail(e.message);}catch(...){ui->Fail(qa::Text(L"Не удалось прочитать сведения о выпуске. Повторите позже.",L"Could not read release details. Try again later."));}
        CoUninitialize();
    }).detach();
}
inline void DownloadUpdate(std::shared_ptr<UpdateUi> const& ui){
    updates::Release release;
    {std::lock_guard<std::mutex> lock(ui->mutex);release=ui->release;ui->stage=UpdateStage::Downloading;ui->percent=-1;ui->message=qa::Text(L"Загружаем установщик…",L"Downloading installer…");}
    ui->Notify();std::thread([ui,release]{
        CoInitializeEx(nullptr,COINIT_MULTITHREADED);
        try{
            auto installer=updates::Download(release,ui->cancel,[ui,release](size_t size){
                int percent=static_cast<int>(size*100/release.size);bool changed=false;
                {std::lock_guard<std::mutex> lock(ui->mutex);if(percent!=ui->percent){ui->percent=percent;ui->message=qa::Text(L"Загрузка: ",L"Download: ")+std::to_wstring(percent)+qa::Text(L"%. Проверяем SHA-256…",L"%. Verifying SHA-256…");changed=true;}}
                if(changed)ui->Notify();
            });
            {std::lock_guard<std::mutex> lock(ui->mutex);ui->installer=std::move(installer);ui->stage=UpdateStage::Ready;ui->message=qa::Text(L"Установщик проверен. Открываем…",L"Installer verified. Opening…");}ui->Notify();
        }catch(updates::Error const& e){ui->Fail(e.message);}catch(...){ui->Fail(qa::Text(L"Не удалось загрузить обновление. Повторите позже.",L"Could not download the update. Try again later."));}
        CoUninitialize();
    }).detach();
}
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
    UINT dpi=GetDpiForWindow(parent);auto scale=[dpi](int value){return MulDiv(value,dpi,96);};
    auto child=CreateWindowExW(0,klass,title,WS_CHILD|WS_VISIBLE|style,scale(x),scale(y),scale(w),scale(h),parent,reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),GetModuleHandleW(nullptr),nullptr);
    SendMessageW(child,WM_SETFONT,reinterpret_cast<WPARAM>(font),TRUE);return child;
}
inline LRESULT CALLBACK ManagerProc(HWND window,UINT message,WPARAM wp,LPARAM lp){
    static HFONT font=nullptr;static HWND status=nullptr,start=nullptr,stop=nullptr,startup=nullptr,updateStatus=nullptr,updateButton=nullptr;static std::shared_ptr<UpdateUi> update;
    switch(message){
    case WM_CREATE:{
        font=CreateFontW(-MulDiv(16,GetDpiForWindow(window),96),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
        Child(window,L"STATIC",L"QuickAudio " QA_VERSION_W,0,20,16,350,24,0,font);
        Child(window,L"STATIC",qa::Text(L"Выход звука и микрофон прямо в Win + A",L"Audio output and microphone in Win + A"),0,20,48,350,24,0,font);
        status=Child(window,L"STATIC",L"",0,20,82,350,24,0,font);
        start=Child(window,L"BUTTON",qa::Text(L"Включить",L"Enable"),WS_TABSTOP|BS_PUSHBUTTON,20,116,158,32,101,font);
        stop=Child(window,L"BUTTON",qa::Text(L"Выключить",L"Disable"),WS_TABSTOP|BS_PUSHBUTTON,190,116,158,32,102,font);
        startup=Child(window,L"BUTTON",qa::Text(L"Запускать вместе с Windows",L"Start with Windows"),WS_TABSTOP|BS_AUTOCHECKBOX,20,166,340,28,103,font);
        Child(window,L"BUTTON",qa::Text(L"Папка настроек",L"Settings folder"),WS_TABSTOP|BS_PUSHBUTTON,20,208,158,32,104,font);
        Child(window,L"BUTTON",L"GitHub",WS_TABSTOP|BS_PUSHBUTTON,190,208,158,32,105,font);
        Child(window,L"STATIC",qa::Text(L"Обновления",L"Updates"),0,20,258,328,24,0,font);
        updateStatus=Child(window,L"STATIC",qa::Text(L"Проверка выполняется по нажатию кнопки.",L"Click the button to check for updates."),0,20,288,328,46,0,font);
        updateButton=Child(window,L"BUTTON",qa::Text(L"Проверить обновления",L"Check for updates"),WS_TABSTOP|BS_PUSHBUTTON,20,342,328,34,106,font);
        Child(window,L"BUTTON",qa::Text(L"Что нового в последнем выпуске",L"Latest release notes"),WS_TABSTOP|BS_PUSHBUTTON,20,386,328,30,107,font);
        update=std::make_shared<UpdateUi>();update->window=window;
        SetTimer(window,1,1000,nullptr);SendMessageW(window,WM_TIMER,1,0);return 0;}
    case OpenUpdates:{SetFocus(updateButton);UpdateStage stage;{std::lock_guard<std::mutex> lock(update->mutex);stage=update->stage;}if(stage==UpdateStage::Idle || stage==UpdateStage::Error)CheckUpdates(update);return 0;}
    case UpdateChanged:{
        std::shared_ptr<updates::Installer> installer;updates::Release release;
        {std::lock_guard<std::mutex> lock(update->mutex);
            SetWindowTextW(updateStatus,update->message.c_str());
            bool available=!update->release.version.empty() && updates::IsNewer(update->release.version,QA_VERSION_W);
            std::wstring button=available?(update->installed?qa::Text(L"Установить ",L"Install "):qa::Text(L"Открыть выпуск ",L"Open release "))+update->release.version:qa::Text(L"Проверить обновления",L"Check for updates");
            SetWindowTextW(updateButton,button.c_str());bool busy=update->stage==UpdateStage::Checking || update->stage==UpdateStage::Downloading || update->stage==UpdateStage::Ready || update->stage==UpdateStage::Launched;EnableWindow(updateButton,!busy);
            if(update->stage==UpdateStage::Ready){installer=update->installer;release=update->release;update->stage=UpdateStage::Launched;}
        }
        if(installer)try{
            installer->Verify(release);SHELLEXECUTEINFOW execute{sizeof(execute)};execute.fMask=SEE_MASK_NOCLOSEPROCESS|SEE_MASK_FLAG_NO_UI;execute.hwnd=window;execute.lpVerb=L"open";execute.lpFile=installer->path.c_str();execute.nShow=SW_SHOWNORMAL;
            updates::Require(ShellExecuteExW(&execute)!=0,qa::Text(L"Не удалось открыть установщик. Повторите загрузку.",L"Could not open the installer. Download it again."));installer->keep=true;
            {std::lock_guard<std::mutex> lock(update->mutex);update->message=qa::Text(L"Установщик открыт. Завершите обновление в нём.",L"Installer opened. Complete the update there.");}update->Notify();
            if(execute.hProcess)std::thread([ui=update,process=execute.hProcess]{
                while(!ui->cancel && WaitForSingleObject(process,250)==WAIT_TIMEOUT){}
                CloseHandle(process);if(!ui->cancel){ {std::lock_guard<std::mutex> lock(ui->mutex);ui->stage=UpdateStage::Idle;ui->release={};ui->message=qa::Text(L"Установщик закрыт. Можно повторить проверку.",L"Installer closed. You can check again.");}ui->Notify();}
            }).detach();
        }catch(updates::Error const& e){update->Fail(e.message);}catch(...){update->Fail(qa::Text(L"Не удалось запустить обновление. Повторите позже.",L"Could not start the update. Try again later."));}
        return 0;}
    case WM_TIMER:{bool running=AgentRunning();SetWindowTextW(status,running?qa::Text(L"Панель включена — откройте Win + A",L"Panel enabled — open Win + A"):qa::Text(L"Панель выключена",L"Panel disabled"));EnableWindow(start,!running);EnableWindow(stop,running);SendMessageW(startup,BM_SETCHECK,StartupEnabled()?BST_CHECKED:BST_UNCHECKED,0);return 0;}
    case WM_COMMAND:{auto id=LOWORD(wp);bool ok=true;
        if(id==101)ok=LaunchAgent();else if(id==102)ok=StopAgent()==0;
        else if(id==103)ok=SetStartup(SendMessageW(startup,BM_GETCHECK,0,0)==BST_CHECKED,ExePath())==ERROR_SUCCESS;
        else if(id==104)ShellExecuteW(window,L"open",DataDir().c_str(),nullptr,nullptr,SW_SHOWNORMAL);
        else if(id==105)ShellExecuteW(window,L"open",L"https://github.com/alexander-62/quick-audio",nullptr,nullptr,SW_SHOWNORMAL);
        else if(id==106){
            UpdateStage stage;updates::Release release;bool installed;
            {std::lock_guard<std::mutex> lock(update->mutex);stage=update->stage;release=update->release;installed=update->installed;}
            if(stage==UpdateStage::Idle || stage==UpdateStage::Error || stage==UpdateStage::Available){
                if(!release.version.empty() && updates::IsNewer(release.version,QA_VERSION_W)){
                    if(installed)DownloadUpdate(update);else ShellExecuteW(window,L"open",release.page.c_str(),nullptr,nullptr,SW_SHOWNORMAL);
                }else CheckUpdates(update);
            }
        }
        else if(id==107)ShellExecuteW(window,L"open",updates::ReleasePage,nullptr,nullptr,SW_SHOWNORMAL);
        if(!ok)MessageBoxW(window,qa::Text(L"Не удалось применить настройку. Проверьте журнал в папке настроек.",L"Could not apply the setting. Check the log in the settings folder."),L"QuickAudio",MB_OK|MB_ICONERROR);
        SendMessageW(window,WM_TIMER,1,0);return 0;}
    case WM_DESTROY:if(update)update->cancel=true;KillTimer(window,1);if(font)DeleteObject(font);PostQuitMessage(0);return 0;
    }return DefWindowProcW(window,message,wp,lp);
}
inline int ShowManager(HINSTANCE instance,bool checkUpdates=false){
    const wchar_t* name=L"QuickAudio.Manager";auto existing=FindWindowW(name,nullptr);if(existing){ShowWindow(existing,SW_RESTORE);SetForegroundWindow(existing);if(checkUpdates)PostMessageW(existing,OpenUpdates,0,0);return 0;}
    INITCOMMONCONTROLSEX common{sizeof(common),ICC_STANDARD_CLASSES};InitCommonControlsEx(&common);
    WNDCLASSW klass{};klass.hInstance=instance;klass.lpszClassName=name;klass.lpfnWndProc=ManagerProc;klass.hCursor=LoadCursorW(nullptr,MAKEINTRESOURCEW(32512));klass.hIcon=LoadIconW(instance,MAKEINTRESOURCEW(1));klass.hbrBackground=reinterpret_cast<HBRUSH>(COLOR_WINDOW+1);RegisterClassW(&klass);
    UINT dpi=GetDpiForSystem();RECT rect{0,0,MulDiv(374,dpi,96),MulDiv(438,dpi,96)};AdjustWindowRectExForDpi(&rect,WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX,FALSE,0,dpi);
    auto window=CreateWindowExW(0,name,qa::Text(L"QuickAudio — настройки",L"QuickAudio — Settings"),WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX,CW_USEDEFAULT,CW_USEDEFAULT,rect.right-rect.left,rect.bottom-rect.top,nullptr,nullptr,instance,nullptr);if(!window)return 1;
    ShowWindow(window,SW_SHOWNORMAL);if(checkUpdates)PostMessageW(window,OpenUpdates,0,0);MSG message;while(GetMessageW(&message,nullptr,0,0)>0){if(!IsDialogMessageW(window,&message)){TranslateMessage(&message);DispatchMessageW(&message);}}return 0;
}
}

