// Regression: the panel client must follow backend state even when its local
// settings path contains a different language. Requires a running test agent.
#include "../src/shared.h"
#include <memory>
#include <iostream>
#include <stdexcept>
qa::Bus languageBus;
auto LanguageSnapshot(){auto state=std::make_unique<qa::State>();if(!languageBus.Snapshot(*state))throw std::runtime_error("snapshot failed");return state;}
auto LanguageCommand(DWORD language){
    auto before=LanguageSnapshot();unsigned long long sequence=0;
    if(!languageBus.Send(9,language,L"",L"",&sequence))throw std::runtime_error("send failed");
    for(int i=0;i<60;++i){Sleep(100);auto state=LanguageSnapshot();if(state->completed>=sequence){
        if(state->lastHresult || state->language!=language || state->revision<=before->revision)throw std::runtime_error("language was not published");return state;
    }}throw std::runtime_error("language timeout");
}
int main(){
    if(!languageBus.Open(false)){std::cerr<<"Start the installed QuickAudio agent first.\n";return 1;}
    auto original=LanguageSnapshot();wchar_t temp[32768]{},folder[32768]{},environment[32768]{};
    DWORD length=GetEnvironmentVariableW(L"LOCALAPPDATA",environment,32768);
    if(!GetTempPathW(32768,temp) || !GetTempFileNameW(temp,L"qli",0,folder))return 2;
    if(!DeleteFileW(folder) || !CreateDirectoryW(folder,nullptr) || !SetEnvironmentVariableW(L"LOCALAPPDATA",folder))return 2;
    int result=0;
    try{
        if(!qa::SetLanguage(qa::Language::Russian))throw std::runtime_error("isolated settings failed");
        for(auto mode:{qa::Language::English,qa::Language::Russian,qa::Language::English,qa::Language::Automatic}){
            auto state=LanguageCommand(static_cast<DWORD>(mode));
            if(qa::ReadLanguage()!=qa::Language::Russian)throw std::runtime_error("client profile unexpectedly changed");
            qa::ApplyLanguage(static_cast<qa::Language>(state->language));
            auto expected=qa::UseRussian(mode,GetUserDefaultUILanguage())?L"Устройства вывода":L"Output devices";
            if(wcscmp(qa::Text(L"Устройства вывода",L"Output devices"),expected))throw std::runtime_error("panel text ignored backend language");
            if(memcmp(original->defaults,state->defaults,sizeof(state->defaults)))throw std::runtime_error("audio defaults changed");
            for(DWORD flow=0;flow<2;++flow)for(DWORD i=0;i<original->count[flow];++i){auto& device=original->devices[flow][i];
                for(DWORD j=0;j<state->count[flow];++j)if(!wcscmp(device.id,state->devices[flow][j].id))
                    if(wcscmp(device.alias,state->devices[flow][j].alias) || device.hidden!=state->devices[flow][j].hidden)throw std::runtime_error("device preferences changed");
            }
        }
        std::cout<<"Language IPC with different client settings: PASS\nAudio defaults and device preferences unchanged: PASS\n";
    }catch(std::exception const& e){std::cerr<<"FAIL: "<<e.what()<<"\n";result=3;}
    try{LanguageCommand(original->language);std::cout<<"Original language restored: PASS\n";}catch(...){std::cerr<<"Original language restoration failed\n";result=4;}
    DeleteFileW(qa::SettingsFile().c_str());auto data=qa::DataDir();if(!data.empty())data.pop_back();RemoveDirectoryW(data.c_str());RemoveDirectoryW(folder);
    SetEnvironmentVariableW(L"LOCALAPPDATA",length?environment:nullptr);return result;
}
