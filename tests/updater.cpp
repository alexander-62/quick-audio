#include "../src/shared.h"
#include "../src/updater.h"
#include <iostream>
#include <stdexcept>
using namespace qa::updates;
void Check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
template<typename Function>void Reject(Function action,const char* message){bool rejected=false;try{action();}catch(Error const&){rejected=true;}Check(rejected,message);}
std::wstring Fixture(){return LR"({"draft":false,"prerelease":false,"tag_name":"v0.2.0","html_url":"https://github.com/alexander-62/quick-audio/releases/tag/v0.2.0","assets":[{"name":"QuickAudio-Setup-0.2.0-x64.exe","state":"uploaded","browser_download_url":"https://github.com/alexander-62/quick-audio/releases/download/v0.2.0/QuickAudio-Setup-0.2.0-x64.exe","size":1234,"digest":"sha256:ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"}]})";}
int main(int argc,char** argv){
    winrt::init_apartment(winrt::apartment_type::multi_threaded);
    try{
        Check(IsNewer(L"0.10.0",L"0.2.9"),"versions were compared lexically");Check(!IsNewer(L"0.2.0",L"0.2.0"),"equal version marked newer");Check(!IsNewer(L"0.1.1",L"0.2.0"),"downgrade allowed");
        for(auto value:{L"0.2.0-beta",L"0.2.0;calc",L"01.2.3",L"1.2",L"65536.0.0",L"4294967296.0.0",L"-1.0.0"})Reject([&]{ParseVersion(value);},"unsafe version accepted");
        Check(AllowedUrl(ApiUrl,true),"API rejected");Check(AllowedUrl(L"https://release-assets.githubusercontent.com/a?b=c"),"release CDN rejected");
        for(auto url:{L"http://github.com/file",L"https://github.com.evil.test/file",L"https://evil.test/file",L"https://user:pass@github.com/file",L"https://github.com:444/file",L"https://github.com/file#fragment"})Check(!AllowedUrl(url),"unsafe address accepted");
        auto fixture=Fixture();auto release=ParseRelease(fixture);Check(release.version==L"0.2.0" && release.size==1234,"release parse failed");
        auto evil=fixture;evil.replace(evil.find(L"/releases/download/"),19,L"/another/download/");Reject([&]{ParseRelease(evil);},"unrelated download URL accepted");
        auto beta=fixture;beta.replace(beta.find(L"\"prerelease\":false"),18,L"\"prerelease\":true");Reject([&]{ParseRelease(beta);},"prerelease accepted");
        auto missing=fixture;missing.replace(missing.find(L"sha256:"),7,L"sha512:");Reject([&]{ParseRelease(missing);},"missing SHA256 accepted");
        std::vector<BYTE> bytes{'a','b','c'};release.size=bytes.size();VerifyBytes(bytes,release);bytes[0]='x';Reject([&]{VerifyBytes(bytes,release);},"modified installer accepted");bytes.pop_back();Reject([&]{VerifyBytes(bytes,release);},"partial installer accepted");
        std::cout<<"Updater rules: PASS (versions, URLs, release metadata, SHA256, truncation)\n";
        if(argc>1 && std::string(argv[1])=="--live"){
            std::atomic<bool> cancel{false};auto actual=Latest(cancel);auto installer=Download(actual,cancel);installer->Verify(actual);
            std::wcout<<L"Live GitHub check/download/SHA256: PASS ("<<actual.version<<L", "<<actual.size<<L" bytes; not executed)\n";
            std::atomic<bool> cancelled{true};Reject([&]{Download(actual,cancelled);},"cancelled download continued");std::cout<<"Cancelled download: PASS\n";
        }
        return 0;
    }catch(Error const& e){std::wcerr<<L"FAIL: "<<e.message<<L"\n";return 1;}catch(std::exception const& e){std::cerr<<"FAIL: "<<e.what()<<"\n";return 1;}catch(winrt::hresult_error const& e){std::wcerr<<L"FAIL: "<<e.message().c_str()<<L"\n";return 1;}
}
