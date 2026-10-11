// Runs against an active agent. Restores each modified preference before exit.
#include "../src/shared.h"
#include <iostream>
#include <memory>
#include <stdexcept>
qa::Bus bus;
auto Snapshot(){auto value=std::make_unique<qa::State>();if(!bus.Snapshot(*value))throw std::runtime_error("snapshot failed");return value;}
auto Command(DWORD kind,DWORD flow,std::wstring const& id=L"",std::wstring const& name=L""){
    if(!bus.Send(kind,flow,id,name))throw std::runtime_error("send failed");auto pending=Snapshot();auto number=pending->request;
    for(int i=0;i<50;++i){Sleep(100);pending=Snapshot();if(pending->completed==number){if(pending->lastHresult)throw std::runtime_error("backend rejected request");return pending;}}
    throw std::runtime_error("request timeout");
}
int main(){
    if(!bus.Open(false)){std::cout<<"Start QuickAudio first.\n";return 1;}
    auto before=Snapshot();DWORD flow=0;while(flow<2 && !before->count[flow])++flow;if(flow==2)return 2;
    auto device=before->devices[flow][0];std::wstring original(device.alias);bool hidden=device.hidden!=0;bool startup=qa::StartupEnabled();
    try{
        auto renamed=Command(6,flow,device.id,L"Тест \"Аудио\" Ж");bool found=false;
        for(DWORD i=0;i<renamed->count[flow];++i)if(!wcscmp(renamed->devices[flow][i].id,device.id))found=!wcscmp(renamed->devices[flow][i].alias,L"Тест \"Аудио\" Ж");
        if(!found)throw std::runtime_error("unicode/quoted alias roundtrip failed");
        Command(6,flow,device.id,original);std::cout<<"Alias roundtrip: PASS\n";
        auto visibility=Command(hidden?5:4,flow,device.id);found=false;
        for(DWORD i=0;i<visibility->count[flow];++i)if(!wcscmp(visibility->devices[flow][i].id,device.id))found=(visibility->devices[flow][i].hidden!=0)!=hidden;
        if(!found)throw std::runtime_error("visibility failed");Command(hidden?4:5,flow,device.id);std::cout<<"Visibility restore: PASS\n";
        Command(7,startup?0:1);if(qa::StartupEnabled()==startup)throw std::runtime_error("startup toggle failed");Command(7,startup?1:0);std::cout<<"Startup toggle/restore: PASS\n";
        auto after=Snapshot();if(memcmp(before->defaults,after->defaults,sizeof(after->defaults)))throw std::runtime_error("audio defaults changed");std::cout<<"Audio defaults unchanged: PASS\n";return 0;
    }catch(std::exception const& e){
        try{Command(6,flow,device.id,original);Command(hidden?4:5,flow,device.id);Command(7,startup?1:0);}catch(...){}
        std::cout<<"FAIL: "<<e.what()<<"\n";return 3;
    }
}
