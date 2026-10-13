// SPDX-License-Identifier: GPL-3.0-or-later
// XAML Diagnostics connection pattern: m417z/UWPSpy and Notification Center Styler.
#include "shared.h"
#include <initguid.h>
#include <mmdeviceapi.h>
#include <functiondiscoverykeys_devpkey.h>
#include <propvarutil.h>
#include <xamlom.h>
#include <tlhelp32.h>
#include <winrt/base.h>
#include <thread>
#include <vector>
#include <atomic>
#include <set>
#include <iostream>
#include <chrono>
#include "manager.h"
#include "preferences.h"
using namespace qa;
// Undocumented Windows PolicyConfig: only SetDefaultEndpoint is used.
// Its ten preceding methods preserve the verified COM vtable slot.
struct __declspec(uuid("f8679f50-850a-41cf-9c72-430f290290c8")) Policy: IUnknown {
    virtual HRESULT STDMETHODCALLTYPE Reserved0()=0; virtual HRESULT STDMETHODCALLTYPE Reserved1()=0;
    virtual HRESULT STDMETHODCALLTYPE Reserved2()=0; virtual HRESULT STDMETHODCALLTYPE Reserved3()=0;
    virtual HRESULT STDMETHODCALLTYPE Reserved4()=0; virtual HRESULT STDMETHODCALLTYPE Reserved5()=0;
    virtual HRESULT STDMETHODCALLTYPE Reserved6()=0; virtual HRESULT STDMETHODCALLTYPE Reserved7()=0;
    virtual HRESULT STDMETHODCALLTYPE Reserved8()=0; virtual HRESULT STDMETHODCALLTYPE Reserved9()=0;
    virtual HRESULT STDMETHODCALLTYPE SetDefaultEndpoint(PCWSTR,ERole)=0;
};
constexpr CLSID PolicyClsid{0x870af99c,0x171d,0x4f9e,{0xaf,0x0d,0xe6,0x3d,0xf4,0x0c,0x2b,0xc9}};
std::wstring HresultText(HRESULT hr) {wchar_t b[32];swprintf_s(b,L"0x%08X",(unsigned)hr);return b;}
void Print(std::wstring const& w) {int n=WideCharToMultiByte(CP_UTF8,0,w.c_str(),(int)w.size(),nullptr,0,nullptr,nullptr);std::string s(n,0);WideCharToMultiByte(CP_UTF8,0,w.c_str(),(int)w.size(),s.data(),n,nullptr,nullptr);DWORD written;WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),s.data(),n,&written,nullptr);}
struct Notifications: winrt::implements<Notifications,IMMNotificationClient> {
    HANDLE event; explicit Notifications(HANDLE h):event(h){}
    HRESULT STDMETHODCALLTYPE OnDeviceStateChanged(LPCWSTR,DWORD) noexcept override {SetEvent(event);return S_OK;}
    HRESULT STDMETHODCALLTYPE OnDeviceAdded(LPCWSTR) noexcept override {SetEvent(event);return S_OK;}
    HRESULT STDMETHODCALLTYPE OnDeviceRemoved(LPCWSTR) noexcept override {SetEvent(event);return S_OK;}
    HRESULT STDMETHODCALLTYPE OnDefaultDeviceChanged(EDataFlow,ERole,LPCWSTR) noexcept override {SetEvent(event);return S_OK;}
    HRESULT STDMETHODCALLTYPE OnPropertyValueChanged(LPCWSTR,const PROPERTYKEY) noexcept override {SetEvent(event);return S_OK;}
};
struct Audio {
    winrt::com_ptr<IMMDeviceEnumerator> enumerator;
    winrt::com_ptr<Notifications> notification;
    HANDLE event;bool settingsUnavailable=false;
    explicit Audio(HANDLE e):event(e){}
    ~Audio(){Reset();}
    void Reset(){if(enumerator && notification)enumerator->UnregisterEndpointNotificationCallback(notification.get());notification=nullptr;enumerator=nullptr;}
    HRESULT Ensure() {
        if(enumerator)return S_OK;
        auto hr=CoCreateInstance(__uuidof(MMDeviceEnumerator),nullptr,CLSCTX_ALL,__uuidof(IMMDeviceEnumerator),enumerator.put_void());
        if(SUCCEEDED(hr) && event){notification=winrt::make_self<Notifications>(event);hr=enumerator->RegisterEndpointNotificationCallback(notification.get());}
        if(FAILED(hr))Reset();return hr;
    }
    std::wstring Default(DWORD flow,ERole role) {
        winrt::com_ptr<IMMDevice> d; LPWSTR id=nullptr;
        if(!enumerator || FAILED(enumerator->GetDefaultAudioEndpoint((EDataFlow)flow,role,d.put())) || FAILED(d->GetId(&id)))return L"";
        std::wstring result(id);CoTaskMemFree(id);return result;
    }
    HRESULT Read(State& s) {
        settingsUnavailable=false;auto hr=CheckSettingsReadable(SettingsFile());if(FAILED(hr)){settingsUnavailable=true;return hr;}
        hr=Ensure();if(FAILED(hr))return hr;
        memset(s.devices,0,sizeof(s.devices));memset(s.defaults,0,sizeof(s.defaults));s.count[0]=s.count[1]=0;
        for(DWORD f=0;f<2;++f){
            winrt::com_ptr<IMMDeviceCollection> collection;hr=enumerator->EnumAudioEndpoints((EDataFlow)f,DEVICE_STATE_ACTIVE,collection.put());
            if(FAILED(hr)){Reset();return hr;}
            UINT count=0;collection->GetCount(&count);std::vector<Device> devices;
            for(UINT i=0;i<count;++i){
                winrt::com_ptr<IMMDevice> d;winrt::com_ptr<IPropertyStore> props;LPWSTR id=nullptr;
                if(FAILED(collection->Item(i,d.put())) || FAILED(d->GetId(&id)))continue;
                Device item{};Copy(item.id,id);CoTaskMemFree(id);item.form=UnknownFormFactor;
                if(SUCCEEDED(d->OpenPropertyStore(STGM_READ,props.put()))){
                    PROPVARIANT v{};
                    if(SUCCEEDED(props->GetValue(PKEY_Device_FriendlyName,&v)) && v.vt==VT_LPWSTR)Copy(item.name,v.pwszVal);PropVariantClear(&v);
                    if(SUCCEEDED(props->GetValue(PKEY_AudioEndpoint_FormFactor,&v)) && v.vt==VT_UI4)item.form=v.ulVal;PropVariantClear(&v);
                }
                if(!item.name[0])Copy(item.name,qa::Text(L"Аудиоустройство",L"Audio device"));
                item.hidden=GetPrivateProfileIntW(HiddenSection(f),item.id,0,SettingsFile().c_str())?1:0;
                GetPrivateProfileStringW(NamesSection(f),item.id,L"",item.alias,NameSize,SettingsFile().c_str());
                devices.push_back(item);
            }
            std::sort(devices.begin(),devices.end(),[](auto const&a,auto const&b){int n=_wcsicmp(a.name,b.name);return n?n<0:wcscmp(a.id,b.id)<0;});
            s.count[f]=(DWORD)std::min(devices.size(),MaxDevices);std::copy_n(devices.begin(),s.count[f],s.devices[f]);
            for(int r=0;r<3;++r)Copy(s.defaults[f][r],Default(f,(ERole)r));
        }return S_OK;
    }
    HRESULT Switch(DWORD flow,std::wstring const& id,bool sync) {
        auto hr=Ensure();if(FAILED(hr))return hr;
        winrt::com_ptr<IMMDevice> d;hr=enumerator->GetDevice(id.c_str(),d.put());if(FAILED(hr))return hr;
        DWORD state=0;d->GetState(&state);if(state!=DEVICE_STATE_ACTIVE)return HRESULT_FROM_WIN32(ERROR_DEVICE_NOT_AVAILABLE);
        winrt::com_ptr<IMMEndpoint> endpoint;hr=d->QueryInterface(__uuidof(IMMEndpoint),endpoint.put_void());if(FAILED(hr))return hr;
        EDataFlow realFlow;endpoint->GetDataFlow(&realFlow);if(realFlow!=(EDataFlow)flow)return E_INVALIDARG;
        winrt::com_ptr<Policy> policy;hr=CoCreateInstance(PolicyClsid,nullptr,CLSCTX_ALL,__uuidof(Policy),policy.put_void());if(FAILED(hr))return hr;
        std::wstring previous[3];for(int r=0;r<3;++r)previous[r]=Default(flow,(ERole)r);
        int roles=sync?3:2;int changed=0;
        for(int r=0;r<roles;++r){hr=policy->SetDefaultEndpoint(id.c_str(),(ERole)r);if(FAILED(hr))break;++changed;}
        if(SUCCEEDED(hr)){for(int r=0;r<roles;++r)if(Default(flow,(ERole)r)!=id){hr=E_FAIL;break;}}
        if(FAILED(hr))for(int r=0;r<changed;++r)if(!previous[r].empty())policy->SetDefaultEndpoint(previous[r].c_str(),(ERole)r);
        return hr;
    }
};
HRESULT WriteDiagnostics(State const& state){
    auto file=SettingsFile();auto hr=CheckSettingsReadable(file);if(FAILED(hr))return hr;
    SYSTEMTIME time;GetSystemTime(&time);wchar_t stamp[64];
    swprintf_s(stamp,L"%04d-%02d-%02d %02d:%02d:%02d UTC",time.wYear,time.wMonth,time.wDay,time.wHour,time.wMinute,time.wSecond);
    std::wstring text=L"QuickAudio diagnostics\nVersion: " QA_VERSION_W L"\nExecutable: "+ExePath()+L"\nSettings file: "+file+L"\nGenerated: "+stamp+L"\nPID: "+std::to_wstring(GetCurrentProcessId())+L"\n";
    WIN32_FILE_ATTRIBUTE_DATA info{};
    if(GetFileAttributesExW(file.c_str(),GetFileExInfoStandard,&info))text+=L"Settings size: "+std::to_wstring((static_cast<unsigned long long>(info.nFileSizeHigh)<<32)|info.nFileSizeLow)+L" bytes\n";
    text+=L"\nActive Windows endpoints and preferences read by this agent:\n";
    for(DWORD flow=0;flow<2;++flow)for(DWORD i=0;i<state.count[flow];++i){
        auto const& d=state.devices[flow][i];
        wchar_t stored[256]{};GetPrivateProfileStringW(NamesSection(flow),d.id,L"",stored,256,file.c_str());
        bool hidden=GetPrivateProfileIntW(HiddenSection(flow),d.id,0,file.c_str())!=0;
        text+=(flow?L"MICROPHONE":L"OUTPUT")+std::wstring(L"\n  id: ")+d.id+L"\n  Windows name: "+d.name+L"\n  panel alias: "+d.alias+L"\n  saved alias: "+stored+L"\n  panel hidden: "+std::to_wstring(d.hidden)+L"\n  saved hidden: "+std::to_wstring(hidden)+L"\n";
    }
    text+=L"\nSaved endpoint IDs absent from the active list (may be disconnected or changed):\n";
    for(DWORD flow=0;flow<2;++flow)for(auto section:{NamesSection(flow),HiddenSection(flow)}){
        std::vector<wchar_t> keys(65536);
        DWORD n=GetPrivateProfileStringW(section,nullptr,L"",keys.data(),static_cast<DWORD>(keys.size()),file.c_str());
        if(n==keys.size()-2)text+=L"  Warning: key list truncated\n";
        for(size_t offset=0;offset<n;){
            std::wstring key(keys.data()+offset);offset+=key.size()+1;
            bool active=false;for(DWORD i=0;i<state.count[flow];++i)if(key==state.devices[flow][i].id)active=true;
            if(!active){wchar_t value[256]{};GetPrivateProfileStringW(section,key.c_str(),L"",value,256,file.c_str());text+=L"  "+std::wstring(section)+L" "+key+L" = "+value+L"\n";}
        }
    }
    int size=WideCharToMultiByte(CP_UTF8,0,text.c_str(),static_cast<int>(text.size()),nullptr,0,nullptr,nullptr);
    if(size<=0)return SettingsError();
    std::string bytes(size,0);WideCharToMultiByte(CP_UTF8,0,text.c_str(),static_cast<int>(text.size()),bytes.data(),size,nullptr,nullptr);
    auto path=DataDir()+L"QuickAudio-diagnostics.txt";auto temp=path+L".tmp";
    auto handle=CreateFileW(temp.c_str(),GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);if(handle==INVALID_HANDLE_VALUE)return SettingsError();
    DWORD written=0;bool ok=WriteFile(handle,bytes.data(),static_cast<DWORD>(bytes.size()),&written,nullptr)!=0 && written==bytes.size();
    hr=ok?S_OK:SettingsError();if(SUCCEEDED(hr) && !FlushFileBuffers(handle))hr=SettingsError();CloseHandle(handle);
    if(FAILED(hr)){DeleteFileW(temp.c_str());return hr;}
    if(!MoveFileExW(temp.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)){hr=SettingsError();DeleteFileW(temp.c_str());return hr;}
    return S_OK;
}
void Publish(Bus& bus,State const& audio) {
    if(!bus.Lock())return;
    auto const& old=*bus.state;
    bool different=old.count[0]!=audio.count[0] || old.count[1]!=audio.count[1] || memcmp(old.devices,audio.devices,sizeof(old.devices)) || memcmp(old.defaults,audio.defaults,sizeof(old.defaults)) || wcscmp(old.status,audio.status) || old.completed!=audio.completed || old.syncCalls!=audio.syncCalls || old.autostart!=audio.autostart || old.language!=audio.language || old.lastHresult!=audio.lastHresult;
    bus.state->count[0]=audio.count[0];bus.state->count[1]=audio.count[1];
    memcpy(bus.state->devices,audio.devices,sizeof(audio.devices));memcpy(bus.state->defaults,audio.defaults,sizeof(audio.defaults));
    Copy(bus.state->status,audio.status);bus.state->lastHresult=audio.lastHresult;bus.state->completed=audio.completed;bus.state->syncCalls=audio.syncCalls;bus.state->autostart=audio.autostart;bus.state->language=audio.language;
    if(different)++bus.state->revision;bus.Unlock();if(different)SetEvent(bus.changed);
}
HRESULT Inject(DWORD pid,std::wstring const& dll) {
    auto lib=LoadLibraryExW(L"Windows.UI.Xaml.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);if(!lib)return HRESULT_FROM_WIN32(GetLastError());
    auto fn=reinterpret_cast<decltype(&InitializeXamlDiagnosticsEx)>(GetProcAddress(lib,"InitializeXamlDiagnosticsEx"));HRESULT hr=E_FAIL;
    if(fn)for(int i=1;i<=128;++i){wchar_t name[128];swprintf_s(name,L"VisualDiagConnection%d",i);hr=fn(name,pid,L"",dll.c_str(),TapClsid,nullptr);if(hr!=HRESULT_FROM_WIN32(ERROR_NOT_FOUND))break;}
    FreeLibrary(lib);return hr;
}
void Supervisor(Bus& bus,std::wstring dir) {
    CoInitializeEx(nullptr,COINIT_MULTITHREADED);DWORD session=0;ProcessIdToSessionId(GetCurrentProcessId(),&session);
    std::set<unsigned long long> seen;
    while(WaitForSingleObject(bus.stop,1000)==WAIT_TIMEOUT){
        auto snap=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);PROCESSENTRY32W p{sizeof(p)};
        if(snap==INVALID_HANDLE_VALUE)continue;
        if(Process32FirstW(snap,&p))do{
            if(_wcsicmp(p.szExeFile,L"ShellHost.exe"))continue;
            DWORD other=0;if(!ProcessIdToSessionId(p.th32ProcessID,&other) || other!=session)continue;
            auto handle=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,p.th32ProcessID);if(!handle)continue;
            FILETIME created{},exit{},kernel{},user{};auto timed=GetProcessTimes(handle,&created,&exit,&kernel,&user);CloseHandle(handle);if(!timed)continue;
            unsigned long long key=((unsigned long long)created.dwHighDateTime<<32)|created.dwLowDateTime;
            if(seen.count(key))continue;
            auto hr=Inject(p.th32ProcessID,dir+QA_HOOK_FILE);
            Log(dir,L"ATTACH target="+std::to_wstring(p.th32ProcessID)+L" hr="+HresultText(hr));
            if(SUCCEEDED(hr))seen.insert(key);
        }while(Process32NextW(snap,&p));CloseHandle(snap);
    }CoUninitialize();
}
int Run() {
    if(!EnsureSettings())return 4;
    auto dir=ExeDir();auto singleton=CreateMutexW(nullptr,FALSE,(BaseName()+L"_agent").c_str());
    if(GetLastError()==ERROR_ALREADY_EXISTS){CloseHandle(singleton);return 0;}
    Bus bus;if(!bus.Open(true)){CloseHandle(singleton);return 2;}
    ResetEvent(bus.stop);ResetEvent(bus.command);ResetEvent(bus.changed);
    if(!bus.Lock())return 3;auto generation=bus.state->magic==Magic?bus.state->generation+1:1;
    memset(bus.state,0,sizeof(State));bus.state->magic=Magic;bus.state->version=Version;bus.state->ownerPid=GetCurrentProcessId();bus.state->generation=generation;
    bus.state->syncCalls=GetPrivateProfileIntW(L"Audio",L"SyncCalls",1,SettingsFile().c_str());bus.Unlock();
    Log(dir,L"AGENT_START version=" QA_VERSION_W);Log(dir,L"SETTINGS_PATH file="+SettingsFile()+L" executable="+ExePath());
    HANDLE refresh=CreateEventW(nullptr,FALSE,TRUE,nullptr);
    std::thread supervisor([&]{Supervisor(bus,dir);});
    {
        Audio audio(refresh);bool initialReport=true;std::wstring lastCatalog;auto currentStorage=std::make_unique<State>();auto& current=*currentStorage;bus.Snapshot(current);
        while(WaitForSingleObject(bus.stop,0)!=WAIT_OBJECT_0){
            auto requestStorage=std::make_unique<State>();auto& request=*requestStorage;if(!bus.Snapshot(request)){Sleep(100);continue;}
            if(request.request>current.completed){
                HRESULT hr=S_OK;current.status[0]=0;
                if(request.kind==1){hr=audio.Switch(request.flow,request.commandId,current.syncCalls!=0);Log(dir,L"SWITCH flow="+std::to_wstring(request.flow)+L" id="+request.commandId+L" sync="+std::to_wstring(current.syncCalls)+L" hr="+HresultText(hr));}
                else if(request.kind==2){
                    if(WritePrivateProfileStringW(L"Audio",L"SyncCalls",request.flow?L"1":L"0",SettingsFile().c_str()))current.syncCalls=request.flow?1:0;else hr=SettingsError();
                }
                else if(request.kind==7){hr=HRESULT_FROM_WIN32(SetStartup(request.flow!=0,ExePath()));}
                else if(request.kind==9){hr=request.flow<=2 && SetLanguage(static_cast<Language>(request.flow))?S_OK:E_FAIL;}
                else if(request.kind==10){hr=audio.Read(current);if(SUCCEEDED(hr))hr=WriteDiagnostics(current);if(SUCCEEDED(hr))hr=reinterpret_cast<INT_PTR>(ShellExecuteW(nullptr,L"open",DataDir().c_str(),nullptr,nullptr,SW_SHOWNORMAL))>32?S_OK:E_FAIL;}
                else if(request.kind==8){hr=reinterpret_cast<INT_PTR>(ShellExecuteW(nullptr,L"open",ExePath().c_str(),L"--updates",nullptr,SW_SHOWNORMAL))>32?S_OK:E_FAIL;}
                else if(request.kind==4 || request.kind==5 || request.kind==6){
                    bool known=false;if(request.flow<2)for(DWORD i=0;i<current.count[request.flow];++i)if(!wcscmp(current.devices[request.flow][i].id,request.commandId))known=true;
                    if(!known)hr=E_INVALIDARG;
                    else {
                        std::wstring name(request.commandName);while(!name.empty() && iswspace(name.front()))name.erase(name.begin());while(!name.empty() && iswspace(name.back()))name.pop_back();
                        bool valid=true;for(auto ch:name)if(ch<32 || ch==127)valid=false;
                        if(request.kind==6 && !valid)hr=E_INVALIDARG;
                        else hr=SaveDevicePreference(SettingsFile(),request.kind,request.flow,request.commandId,name);
                    }
                    Log(dir,(request.kind==6?L"RENAME":L"VISIBILITY")+std::wstring(L" flow=")+std::to_wstring(request.flow)+L" hidden="+std::to_wstring(request.kind==4)+L" id="+request.commandId+L" hr="+HresultText(hr)+L" settings="+SettingsFile());
                }
                current.completed=request.request;current.lastHresult=hr;
                if(FAILED(hr))Copy(current.status,request.kind==1?qa::Text(L"Не удалось переключить устройство. Проверьте подключение и попробуйте снова.",L"Could not switch devices. Check the connection and try again."):qa::Text(L"Не удалось сохранить настройку. Попробуйте снова.",L"Could not save the setting. Try again."));
            }
            RefreshLanguage();current.language=static_cast<DWORD>(LanguageMode());auto hr=audio.Read(current);
            current.autostart=StartupEnabled()?1:0;
            if(FAILED(hr)){current.lastHresult=hr;current.count[0]=current.count[1]=0;Copy(current.status,audio.settingsUnavailable?qa::Text(L"Не удалось прочитать файл настроек. Настройки не сброшены.",L"Could not read the settings file. Preferences have not been reset."):qa::Text(L"Аудиослужба недоступна. Повторяем подключение…",L"Audio service unavailable. Reconnecting…"));}
            else if(wcscmp(current.status,L"Аудиослужба недоступна. Повторяем подключение…")==0 || wcscmp(current.status,L"Audio service unavailable. Reconnecting…")==0 || wcscmp(current.status,L"Не удалось прочитать файл настроек. Настройки не сброшены.")==0 || wcscmp(current.status,L"Could not read the settings file. Preferences have not been reset.")==0){current.status[0]=0;current.lastHresult=0;}
            if(SUCCEEDED(hr)){
                std::wstring catalog;
                for(DWORD flow=0;flow<2;++flow)for(DWORD i=0;i<current.count[flow];++i){auto const& d=current.devices[flow][i];catalog+=std::to_wstring(flow)+L"|"+d.id+L"|"+d.name+L"|"+d.alias+L"|"+std::to_wstring(d.hidden)+L"\n";}
                if(catalog!=lastCatalog || initialReport){
                    Log(dir,L"DEVICE_SNAPSHOT settings="+SettingsFile()+L"\n"+catalog);lastCatalog=catalog;
                    if(initialReport){auto report=WriteDiagnostics(current);Log(dir,L"DIAGNOSTICS hr="+HresultText(report));initialReport=false;}
                }
            }
            Publish(bus,current);
            HANDLE events[]{bus.stop,bus.command,refresh};auto reason=WaitForMultipleObjects(3,events,FALSE,5000);
            if(reason==WAIT_OBJECT_0+2){if(WaitForSingleObject(bus.stop,150)==WAIT_OBJECT_0)break;ResetEvent(refresh);}
        }
    }
    if(bus.Lock()){bus.state->stopping=1;++bus.state->revision;bus.Unlock();}SetEvent(bus.changed);SetEvent(bus.stop);
    supervisor.join();CloseHandle(refresh);CloseHandle(singleton);Log(dir,L"AGENT_STOP");return 0;
}
int Inspect(bool roundtrip) {
    CoInitializeEx(nullptr,COINIT_MULTITHREADED);Audio audio(nullptr);State state{};auto hr=audio.Read(state);
    if(FAILED(hr)){Print(L"ENUM_ERROR "+HresultText(hr)+L"\n");return 1;}
    for(int f=0;f<2;++f){Print(f?L"MICROPHONES\n":L"OUTPUTS\n");for(DWORD i=0;i<state.count[f];++i)Print(std::to_wstring(i)+L"\t"+state.devices[f][i].name+L"\t"+state.devices[f][i].id+L"\n");for(int r=0;r<3;++r)Print(L"DEFAULT "+std::to_wstring(r)+L"\t"+state.defaults[f][r]+L"\n");}
    winrt::com_ptr<Policy> policy;hr=CoCreateInstance(PolicyClsid,nullptr,CLSCTX_ALL,__uuidof(Policy),policy.put_void());Print(L"POLICY_INTERFACE "+HresultText(hr)+L"\n");
    if(roundtrip && SUCCEEDED(hr)){
        for(int f=0;f<2;++f)for(int r=0;r<3;++r)if(state.defaults[f][r][0]){
            auto h=policy->SetDefaultEndpoint(state.defaults[f][r],(ERole)r);
            bool match=audio.Default(f,(ERole)r)==state.defaults[f][r];Print(L"CURRENT_DEVICE_CHECK flow="+std::to_wstring(f)+L" role="+std::to_wstring(r)+L" hr="+HresultText(h)+L" confirmed="+(match?L"yes":L"no")+L"\n");if(FAILED(h)||!match)hr=E_FAIL;
        }
    }return SUCCEEDED(hr)?0:1;
}
int wmain(int argc,wchar_t** argv) {
    std::wstring action=argc>1?argv[1]:L"--settings";
    if(action==L"--version"){Print(QA_VERSION_W L"\n");return 0;}
    if(action==L"--settings" || action==L"--updates")return ShowManager(GetModuleHandleW(nullptr),action==L"--updates");
    if(action==L"--prepare-update")return PrepareUpdate();
    if(action==L"--list" || action==L"--check"){auto result=Inspect(action==L"--check");CoUninitialize();return result;}
    if(action==L"--stop")return StopAgent();
    if(action==L"--autostart" || action==L"--remove-autostart"){
        return SetStartup(action==L"--autostart",ExePath())==ERROR_SUCCESS?0:1;
    }
    if(action!=L"--run")return 2;CoInitializeEx(nullptr,COINIT_MULTITHREADED);auto result=Run();CoUninitialize();return result;
}
