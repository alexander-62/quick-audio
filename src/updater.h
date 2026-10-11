// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "product.h"
#include <winhttp.h>
#include <bcrypt.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Data.Json.h>
#include <array>
#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <vector>

namespace qa::updates {
inline constexpr wchar_t ReleasePage[]=L"https://github.com/alexander-62/quick-audio/releases/latest";
inline constexpr wchar_t ApiUrl[]=L"https://api.github.com/repos/alexander-62/quick-audio/releases/latest";
inline constexpr size_t MaxInstallerBytes=64*1024*1024;
struct Error { std::wstring message; };
inline void Require(bool condition,const wchar_t* message){if(!condition)throw Error{message};}
inline void NetworkCheck(bool ok){if(!ok)throw Error{qa::Text(L"Ошибка подключения (",L"Connection error (")+std::to_wstring(GetLastError())+qa::Text(L"). Проверьте интернет и повторите.",L"). Check your internet connection and try again.")};}

// Only stable MAJOR.MINOR.PATCH tags are accepted. Never compare versions as text.
inline std::array<unsigned,3> ParseVersion(std::wstring const& value){
    std::array<unsigned,3> parts{};size_t position=0;
    for(size_t i=0;i<3;++i){
        size_t begin=position;while(position<value.size() && value[position]>=L'0' && value[position]<=L'9'){
            unsigned next=parts[i]*10+value[position++]-L'0';Require(next<=65535,qa::Text(L"Некорректная версия выпуска.",L"Invalid release version."));parts[i]=next;
        }
        Require(position>begin && (position-begin==1 || value[begin]!=L'0'),qa::Text(L"Некорректная версия выпуска.",L"Invalid release version."));
        if(i<2){Require(position<value.size() && value[position++]==L'.',qa::Text(L"Некорректная версия выпуска.",L"Invalid release version."));}
    }
    Require(position==value.size(),qa::Text(L"Некорректная версия выпуска.",L"Invalid release version."));return parts;
}
inline bool IsNewer(std::wstring const& available,std::wstring const& current){return ParseVersion(available)>ParseVersion(current);}
inline bool AllowedUrl(std::wstring const& url,bool metadata=false){
    if(url.find(L'#')!=std::wstring::npos)return false;
    URL_COMPONENTS parts{sizeof(parts)};parts.dwHostNameLength=parts.dwUrlPathLength=parts.dwExtraInfoLength=parts.dwUserNameLength=parts.dwPasswordLength=static_cast<DWORD>(-1);
    if(!WinHttpCrackUrl(url.c_str(),0,0,&parts) || parts.nScheme!=INTERNET_SCHEME_HTTPS || parts.nPort!=443 || parts.dwUserNameLength || parts.dwPasswordLength)return false;
    std::wstring host(parts.lpszHostName,parts.dwHostNameLength);
    if(metadata)return host==L"api.github.com" && url==ApiUrl;
    return host==L"github.com" || host==L"release-assets.githubusercontent.com" || host==L"objects.githubusercontent.com";
}
struct HttpHandle {
    HINTERNET value=nullptr;explicit HttpHandle(HINTERNET handle):value(handle){NetworkCheck(value!=nullptr);}
    ~HttpHandle(){if(value)WinHttpCloseHandle(value);}HttpHandle(HttpHandle const&)=delete;
};
inline std::vector<BYTE> Get(std::wstring url,size_t limit,std::atomic<bool> const& cancel,std::function<void(size_t)> progress={},bool metadata=false){
    HttpHandle session(WinHttpOpen(L"QuickAudio/" QA_VERSION_W,WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0));
    DWORD protocols=WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2|WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_3;
    NetworkCheck(WinHttpSetOption(session.value,WINHTTP_OPTION_SECURE_PROTOCOLS,&protocols,sizeof(protocols))!=0);
    NetworkCheck(WinHttpSetTimeouts(session.value,10000,10000,10000,15000)!=0);
    auto deadline=std::chrono::steady_clock::now()+std::chrono::minutes(3);
    for(int redirect=0;redirect<5;++redirect){
        Require(!cancel,qa::Text(L"Проверка отменена.",L"Check cancelled."));Require(AllowedUrl(url,metadata),qa::Text(L"Выпуск ссылается на недопустимый адрес.",L"The release links to a disallowed address."));
        URL_COMPONENTS parts{sizeof(parts)};parts.dwHostNameLength=parts.dwUrlPathLength=parts.dwExtraInfoLength=static_cast<DWORD>(-1);
        NetworkCheck(WinHttpCrackUrl(url.c_str(),0,0,&parts)!=0);
        std::wstring host(parts.lpszHostName,parts.dwHostNameLength),path(parts.lpszUrlPath,parts.dwUrlPathLength);
        if(parts.dwExtraInfoLength)path.append(parts.lpszExtraInfo,parts.dwExtraInfoLength);
        HttpHandle connection(WinHttpConnect(session.value,host.c_str(),443,0));
        HttpHandle request(WinHttpOpenRequest(connection.value,L"GET",path.c_str(),nullptr,WINHTTP_NO_REFERER,WINHTTP_DEFAULT_ACCEPT_TYPES,WINHTTP_FLAG_SECURE));
        DWORD disabled=WINHTTP_DISABLE_REDIRECTS|WINHTTP_DISABLE_COOKIES|WINHTTP_DISABLE_AUTHENTICATION;
        NetworkCheck(WinHttpSetOption(request.value,WINHTTP_OPTION_DISABLE_FEATURE,&disabled,sizeof(disabled))!=0);
        auto headers=metadata?L"Accept: application/vnd.github+json\r\nX-GitHub-Api-Version: 2022-11-28\r\nCache-Control: no-cache\r\n":L"Accept: application/octet-stream\r\n";
        NetworkCheck(WinHttpSendRequest(request.value,headers,static_cast<DWORD>(-1),WINHTTP_NO_REQUEST_DATA,0,0,0)!=0);
        NetworkCheck(WinHttpReceiveResponse(request.value,nullptr)!=0);
        DWORD status=0,size=sizeof(status);NetworkCheck(WinHttpQueryHeaders(request.value,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,WINHTTP_HEADER_NAME_BY_INDEX,&status,&size,WINHTTP_NO_HEADER_INDEX)!=0);
        if(status==301 || status==302 || status==303 || status==307 || status==308){
            Require(!metadata,qa::Text(L"GitHub вернул неожиданный адрес проверки.",L"GitHub returned an unexpected check URL."));
            DWORD length=0;WinHttpQueryHeaders(request.value,WINHTTP_QUERY_LOCATION,WINHTTP_HEADER_NAME_BY_INDEX,nullptr,&length,WINHTTP_NO_HEADER_INDEX);
            Require(length>2 && length<32768,qa::Text(L"Некорректное перенаправление загрузки.",L"Invalid download redirect."));
            std::vector<wchar_t> location(length/sizeof(wchar_t)+1);
            NetworkCheck(WinHttpQueryHeaders(request.value,WINHTTP_QUERY_LOCATION,WINHTTP_HEADER_NAME_BY_INDEX,location.data(),&length,WINHTTP_NO_HEADER_INDEX)!=0);url=location.data();continue;
        }
        if(status==403 || status==429)throw Error{qa::Text(L"GitHub ограничил запросы. Повторите проверку позже.",L"GitHub rate limit reached. Check again later.")};
        Require(status==200,qa::Text(L"GitHub не смог предоставить выпуск. Повторите позже.",L"GitHub could not provide the release. Try again later."));
        std::vector<BYTE> bytes;std::array<BYTE,65536> buffer;
        for(;;){
            Require(!cancel,qa::Text(L"Загрузка отменена.",L"Download cancelled."));Require(std::chrono::steady_clock::now()<deadline,qa::Text(L"Загрузка заняла слишком долго. Повторите позже.",L"Download timed out. Try again later."));
            DWORD read=0;NetworkCheck(WinHttpReadData(request.value,buffer.data(),static_cast<DWORD>(buffer.size()),&read)!=0);
            if(!read)break;Require(bytes.size()+read<=limit,qa::Text(L"Размер ответа превышает допустимый.",L"Response exceeds the size limit."));bytes.insert(bytes.end(),buffer.begin(),buffer.begin()+read);if(progress)progress(bytes.size());
        }
        return bytes;
    }
    throw Error{qa::Text(L"Слишком много перенаправлений загрузки.",L"Too many download redirects.")};
}
struct Release {std::wstring version,page,url,digest;size_t size=0;};
inline Release ParseRelease(std::wstring const& json){
    using namespace winrt::Windows::Data::Json;
    auto data=JsonObject::Parse(json);Require(!data.GetNamedBoolean(L"draft") && !data.GetNamedBoolean(L"prerelease"),qa::Text(L"Этот выпуск не является стабильным.",L"This is not a stable release."));
    std::wstring tag(data.GetNamedString(L"tag_name"));Require(tag.size()>1 && tag[0]==L'v',qa::Text(L"Некорректная версия выпуска.",L"Invalid release version."));
    Release result;result.version=tag.substr(1);ParseVersion(result.version);
    result.page=L"https://github.com/alexander-62/quick-audio/releases/tag/"+tag;
    Require(std::wstring(data.GetNamedString(L"html_url"))==result.page,qa::Text(L"Некорректная страница выпуска.",L"Invalid release page."));
    auto name=L"QuickAudio-Setup-"+result.version+L"-x64.exe";
    for(auto const& item:data.GetNamedArray(L"assets")){
        auto asset=item.GetObject();if(std::wstring(asset.GetNamedString(L"name"))!=name)continue;
        Require(result.url.empty(),qa::Text(L"В выпуске несколько установщиков с одинаковым именем.",L"The release has multiple installers with the same name."));
        Require(asset.GetNamedString(L"state")==L"uploaded",qa::Text(L"Установщик ещё не загружен полностью.",L"The installer has not finished uploading."));
        result.url=asset.GetNamedString(L"browser_download_url");
        Require(result.url==L"https://github.com/alexander-62/quick-audio/releases/download/"+tag+L"/"+name,qa::Text(L"Некорректный адрес установщика.",L"Invalid installer URL."));
        double size=asset.GetNamedNumber(L"size");Require(size>=1024 && size<=MaxInstallerBytes && size==static_cast<size_t>(size),qa::Text(L"Некорректный размер установщика.",L"Invalid installer size."));result.size=static_cast<size_t>(size);
        std::wstring digest(asset.GetNamedString(L"digest",L""));Require(digest.size()==71 && digest.substr(0,7)==L"sha256:",qa::Text(L"У выпуска нет контрольной суммы SHA-256.",L"The release has no SHA-256 checksum."));result.digest=digest.substr(7);
        for(auto& c:result.digest){if(c>=L'A' && c<=L'F')c+=L'a'-L'A';Require((c>=L'0' && c<=L'9') || (c>=L'a' && c<=L'f'),qa::Text(L"Некорректная контрольная сумма.",L"Invalid checksum."));}
    }
    Require(!result.url.empty(),qa::Text(L"В выпуске нет установщика для x64.",L"The release has no x64 installer."));return result;
}
inline Release Latest(std::atomic<bool> const& cancel){
    auto bytes=Get(ApiUrl,2*1024*1024,cancel,{},true);Require(!bytes.empty(),qa::Text(L"GitHub вернул пустой ответ.",L"GitHub returned an empty response."));
    int length=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,reinterpret_cast<char*>(bytes.data()),static_cast<int>(bytes.size()),nullptr,0);Require(length>0,qa::Text(L"Некорректный ответ GitHub.",L"Invalid GitHub response."));
    std::wstring json(length,0);MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,reinterpret_cast<char*>(bytes.data()),static_cast<int>(bytes.size()),json.data(),length);return ParseRelease(json);
}
class Sha256 {
    BCRYPT_ALG_HANDLE algorithm=nullptr;BCRYPT_HASH_HANDLE hash=nullptr;
public:
    Sha256(){Require(BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0)>=0,qa::Text(L"Не удалось проверить SHA-256.",L"Could not verify SHA-256."));
        if(BCryptCreateHash(algorithm,&hash,nullptr,0,nullptr,0,0)<0){BCryptCloseAlgorithmProvider(algorithm,0);algorithm=nullptr;throw Error{qa::Text(L"Не удалось проверить SHA-256.",L"Could not verify SHA-256.")};}}
    ~Sha256(){if(hash)BCryptDestroyHash(hash);if(algorithm)BCryptCloseAlgorithmProvider(algorithm,0);}
    Sha256(Sha256 const&)=delete;Sha256& operator=(Sha256 const&)=delete;
    void Add(BYTE const* bytes,ULONG length){Require(BCryptHashData(hash,const_cast<BYTE*>(bytes),length,0)>=0,qa::Text(L"Не удалось проверить SHA-256.",L"Could not verify SHA-256."));}
    std::wstring Finish(){std::array<BYTE,32> digest;Require(BCryptFinishHash(hash,digest.data(),static_cast<ULONG>(digest.size()),0)>=0,qa::Text(L"Не удалось проверить SHA-256.",L"Could not verify SHA-256."));std::wstring result;for(auto byte:digest){result+=L"0123456789abcdef"[byte>>4];result+=L"0123456789abcdef"[byte&15];}return result;}
};
inline void VerifyBytes(std::vector<BYTE> const& bytes,Release const& release){
    Require(bytes.size()==release.size,qa::Text(L"Установщик загрузился не полностью. Повторите загрузку.",L"Installer download is incomplete. Download it again."));Sha256 hash;hash.Add(bytes.data(),static_cast<ULONG>(bytes.size()));Require(hash.Finish()==release.digest,qa::Text(L"Контрольная сумма не совпала. Установщик не будет запущен.",L"Checksum mismatch. The installer will not be launched."));
}
struct Installer {
    std::wstring directory,path;HANDLE file=INVALID_HANDLE_VALUE;bool keep=false;
    ~Installer(){if(file!=INVALID_HANDLE_VALUE)CloseHandle(file);if(!keep && !path.empty())DeleteFileW(path.c_str());if(!keep && !directory.empty())RemoveDirectoryW(directory.c_str());}
    void Verify(Release const& release){
        if(file==INVALID_HANDLE_VALUE)file=CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
        Require(file!=INVALID_HANDLE_VALUE,qa::Text(L"Не удалось открыть установщик.",L"Could not open the installer."));LARGE_INTEGER size{};Require(GetFileSizeEx(file,&size)!=0 && size.QuadPart==release.size,qa::Text(L"Размер установщика изменился.",L"Installer size changed."));
        LARGE_INTEGER start{};Require(SetFilePointerEx(file,start,nullptr,FILE_BEGIN)!=0,qa::Text(L"Не удалось проверить установщик.",L"Could not verify the installer."));
        Sha256 hash;std::array<BYTE,65536> bytes;DWORD read=0;for(;;){Require(ReadFile(file,bytes.data(),static_cast<DWORD>(bytes.size()),&read,nullptr)!=0,qa::Text(L"Не удалось прочитать установщик.",L"Could not read the installer."));if(!read)break;hash.Add(bytes.data(),read);}Require(hash.Finish()==release.digest,qa::Text(L"Контрольная сумма установщика не совпала.",L"Installer checksum mismatch."));
    }
};
inline std::shared_ptr<Installer> Download(Release const& release,std::atomic<bool> const& cancel,std::function<void(size_t)> progress={}){
    auto bytes=Get(release.url,release.size,cancel,progress);VerifyBytes(bytes,release);Require(!cancel,qa::Text(L"Загрузка отменена.",L"Download cancelled."));
    auto result=std::make_shared<Installer>();auto base=DataDir();Require(!base.empty(),qa::Text(L"Недоступна папка обновлений.",L"Updates folder is unavailable."));base+=L"Updates";
    Require(CreateDirectoryW(base.c_str(),nullptr)!=0 || GetLastError()==ERROR_ALREADY_EXISTS,qa::Text(L"Не удалось создать папку обновлений.",L"Could not create the updates folder."));
    GUID guid;Require(SUCCEEDED(CoCreateGuid(&guid)),qa::Text(L"Не удалось создать файл обновления.",L"Could not create the update file."));wchar_t unique[40];StringFromGUID2(guid,unique,40);result->directory=base+L"\\"+unique;
    Require(CreateDirectoryW(result->directory.c_str(),nullptr)!=0,qa::Text(L"Не удалось создать папку загрузки.",L"Could not create the download folder."));result->path=result->directory+L"\\QuickAudio-Setup-"+release.version+L"-x64.exe";
    auto file=CreateFileW(result->path.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);Require(file!=INVALID_HANDLE_VALUE,qa::Text(L"Не удалось сохранить установщик.",L"Could not save the installer."));
    DWORD written=0;bool ok=WriteFile(file,bytes.data(),static_cast<DWORD>(bytes.size()),&written,nullptr)!=0 && written==bytes.size();if(ok)ok=FlushFileBuffers(file)!=0;CloseHandle(file);Require(ok,qa::Text(L"Не удалось сохранить весь установщик.",L"Could not save the complete installer."));result->Verify(release);return result;
}
inline bool Installed(){
    wchar_t directory[32768]{};DWORD bytes=sizeof(directory);
    if(RegGetValueW(HKEY_CURRENT_USER,L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\{5F15B389-4D25-485F-9954-94B63C48626A}_is1",L"InstallLocation",RRF_RT_REG_SZ,nullptr,directory,&bytes)!=ERROR_SUCCESS)return false;
    std::wstring path=directory;while(!path.empty() && (path.back()==L'\\' || path.back()==L'/'))path.pop_back();path+=L"\\QuickAudio.exe";return !_wcsicmp(path.c_str(),ExePath().c_str());
}
}

