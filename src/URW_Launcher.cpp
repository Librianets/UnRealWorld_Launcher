// GUI launcher0.26. Shared multilingual bootstrap and presentation-only hook.
// English launches the original game directly. No game/save/binary patches.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define UNICODE
#define _UNICODE
#include <windows.h>
#include <commctrl.h>
#include <string>
#include <vector>
#include <algorithm>
#include <cwchar>
#include "SteamInstall.h"
#include "SteamBootstrapHash.h"
#pragma comment(lib,"comctl32.lib")
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='x86' publicKeyToken='6595b64144ccf1df' language='*'\"")

enum {Eng=1001,Rus=1002,Fin=1003,Console0=1100,Console1=1101,Console2=1102,Logging=1110,Status=1200,UILanguage=1300,
    UpdateCheck=1600,UpdateEdition,UpdateTitle,UpdateLabel,
    SteamInstallButton=1500,SteamRepair,SteamRestore,SteamLanguage,SteamApply,SteamTitle,SteamStatus,
    LaunchTitle=1400,EngNote,RusNote,FinNote,ConsoleTitle,LoggingTitle,LoggingHelp,UILanguageTitle};
enum class Notice {Loaded,Saved,ReadError,EncodingError,SaveError,GameMissing,RuntimeMissing,CatalogMissing,StartError,StartedEng,StartedRus,StartedFin,Exited,Closed,SteamMessage};
static const wchar_t* WindowClass=L"URWLauncherWindow";
struct App {
    HWND window=nullptr;HFONT font=nullptr;HANDLE process=nullptr,updater=nullptr;
    std::wstring root,ini;int dpi=96,console=0,ui=1;bool logging=false;std::string language="ru";
    Notice notice=Notice::Loaded;std::wstring detail;
    ~App(){if(updater)CloseHandle(updater);if(process)CloseHandle(process);if(font)DeleteObject(font);}
    int Scale(int value)const{return MulDiv(value,dpi,96);}
};
static App app;
static std::wstring Error(DWORD code=GetLastError()) {
    wchar_t* buffer=nullptr;
    FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_IGNORE_INSERTS,nullptr,code,0,(wchar_t*)&buffer,0,nullptr);
    std::wstring value=buffer?buffer:app.ui==0?L"Unknown error":app.ui==2?L"Tuntematon virhe":L"Неизвестная ошибка";if(buffer)LocalFree(buffer);
    while(!value.empty()&&(value.back()==L'\r'||value.back()==L'\n'))value.pop_back();
    return value+L" ("+std::to_wstring(code)+L")";
}
static bool Exists(const std::wstring& path) {
    DWORD value=GetFileAttributesW(path.c_str());return value!=INVALID_FILE_ATTRIBUTES&&!(value&FILE_ATTRIBUTE_DIRECTORY);
}
static std::wstring Directory(const std::wstring& path){size_t pos=path.find_last_of(L"\\/");return pos==std::wstring::npos?L".":path.substr(0,pos);}
static std::wstring GamePath() {
    wchar_t value[32768];GetPrivateProfileStringW(L"Hook",L"GameExe",L"game\\urw.exe",value,32768,app.ini.c_str());
    std::wstring path=value;
    if(path.find(L':')==std::wstring::npos&&path.rfind(L"\\\\",0)!=0)path=app.root+L"\\"+path;
    return path;
}
static std::wstring TranslationPath(bool finnish) {
    wchar_t value[32768];GetPrivateProfileStringW(L"Languages",finnish?L"FinnishFolder":L"RussianFolder",finnish?L"fin_translate":L"rus_translate",value,32768,app.ini.c_str());
    std::wstring path=value;
    if(path.find(L':')==std::wstring::npos&&path.rfind(L"\\\\",0)!=0)path=app.root+L"\\"+path;
    return path;
}
static bool ReadBytes(const std::wstring& path,std::string& bytes,bool allowMissing=false) {
    HANDLE file=CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file==INVALID_HANDLE_VALUE)return allowMissing&&GetLastError()==ERROR_FILE_NOT_FOUND;
    LARGE_INTEGER size={};bool okay=GetFileSizeEx(file,&size)&&size.QuadPart<=1024*1024;
    if(okay){bytes.resize((size_t)size.QuadPart);DWORD got=0;okay=bytes.empty()||(ReadFile(file,&bytes[0],(DWORD)bytes.size(),&got,nullptr)&&got==bytes.size());}
    else SetLastError(ERROR_FILE_TOO_LARGE);
    DWORD error=GetLastError();CloseHandle(file);SetLastError(error);return okay;
}
static std::string Trim(std::string text) {
    auto first=text.find_first_not_of(" \t\r");if(first==std::string::npos)return {};
    return text.substr(first,text.find_last_not_of(" \t\r")-first+1);
}
static std::string Lower(std::string text){for(char& c:text)if(c>='A'&&c<='Z')c+=32;return text;}
static const wchar_t* T(const wchar_t* en,const wchar_t* ru,const wchar_t* fi){return app.ui==0?en:app.ui==2?fi:ru;}
static const char* UILanguageCode(){return app.ui==0?"en":app.ui==2?"fi":"ru";}
static std::string EditedIni(const std::string& original,int console,bool logging) {
    // Preserve unrelated settings, comments, UTF8 bytes and each original line ending.
    // Update all duplicates; use atomic byte writes rather than Windows INI rewriting.
    const bool utf16=original.size()>=2&&((unsigned char)original[0]==255||(unsigned char)original[0]==254);
    if(utf16){SetLastError(ERROR_NO_UNICODE_TRANSLATION);return {};}
    std::string eol=original.empty()||original.find("\r\n")!=std::string::npos?"\r\n":"\n";
    struct Setting {const char* section;const char* key;std::string value;bool written=false;};
    std::vector<Setting> values={{"hook","ConsoleMode",std::to_string(console)},{"hook","LoggingEnabled",logging?"1":"0"},
        {"hook","Language",app.language},{"launcher","UILanguage",UILanguageCode()}};
    std::string section,result;bool foundHook=false,foundLauncher=false;
    auto append=[&](const std::string& value){if(!result.empty()&&result.back()!='\n')result+=eol;result+=value+eol;};
    auto missing=[&](const std::string& which){for(auto& setting:values)if(which==setting.section&&!setting.written){append(std::string(setting.key)+"="+setting.value);setting.written=true;}};
    size_t start=0;
    while(start<original.size()) {
        size_t end=original.find('\n',start);size_t next=end==std::string::npos?original.size():end+1;
        std::string raw=original.substr(start,next-start),content=raw;
        if(!content.empty()&&content.back()=='\n')content.pop_back();
        if(!content.empty()&&content.back()=='\r')content.pop_back();
        std::string text=Trim(content);
        if(start==0&&text.compare(0,3,"\xEF\xBB\xBF")==0)text=Trim(text.substr(3));
        if(!text.empty()&&text[0]=='[') {
            missing(section);auto close=text.find(']');section=close==std::string::npos?"":Lower(Trim(text.substr(1,close-1)));
            foundHook=foundHook||section=="hook";foundLauncher=foundLauncher||section=="launcher";
        } else if(!text.empty()&&text[0]!=';'&&text[0]!='#') {
            size_t equal=content.find('=');
            if(equal!=std::string::npos) {
                std::string key=Lower(Trim(content.substr(0,equal)));
                for(auto& setting:values)if(section==setting.section&&key==Lower(setting.key)) {
                    size_t value=equal+1;while(value<content.size()&&(content[value]==' '||content[value]=='\t'))++value;
                    size_t last=value;while(last<content.size()&&content[last]!=';'&&content[last]!='#')++last;
                    size_t tail=last;while(tail>value&&(content[tail-1]==' '||content[tail-1]=='\t'))--tail;
                    content=content.substr(0,value)+setting.value+content.substr(tail);
                    std::string ending=raw.size()>=2&&raw.substr(raw.size()-2)=="\r\n"?"\r\n":!raw.empty()&&raw.back()=='\n'?"\n":"";
                    raw=content+ending;setting.written=true;break;
                }
            }
        }
        result+=raw;start=next;
    }
    missing(section);
    if(!foundHook){append("[Hook]");missing("hook");}
    if(!foundLauncher){append("[Launcher]");missing("launcher");}
    return result;
}
static void RefreshStatus() {
    const wchar_t* text=L"";
    switch(app.notice) {
    case Notice::SteamMessage:text=L"";break;
    case Notice::Loaded:text=T(L"Settings loaded from URWTrace.ini.",L"Настройки загружены из URWTrace.ini.",L"Asetukset ladattu tiedostosta URWTrace.ini.");break;
    case Notice::Saved:text=T(L"Settings saved. They apply to the next launch.",L"Настройки сохранены. Применяются при следующем запуске.",L"Asetukset tallennettu. Ne tulevat voimaan seuraavalla käynnistyskerralla.");break;
    case Notice::ReadError:text=T(L"Could not read settings: ",L"Не удалось прочитать настройки: ",L"Asetusten lukeminen epäonnistui: ");break;
    case Notice::EncodingError:text=T(L"Could not save settings. The INI must use UTF-8/ASCII.",L"Не удалось сохранить настройки. Требуется INI в UTF-8/ASCII.",L"Tallennus epäonnistui. INI-tiedoston on oltava UTF-8/ASCII-muodossa.");break;
    case Notice::SaveError:text=T(L"Could not save settings: ",L"Не удалось сохранить настройки: ",L"Asetusten tallennus epäonnistui: ");break;
    case Notice::GameMissing:text=T(L"urw.exe was not found. Check GameExe in URWTrace.ini.",L"Не найден urw.exe. Проверьте GameExe в URWTrace.ini.",L"urw.exe ei löytynyt. Tarkista URWTrace.ini-tiedoston GameExe-asetus.");break;
    case Notice::RuntimeMissing:text=T(L"URWTrace.exe and URWTraceHook.dll must be beside the launcher.",L"Рядом с лаунчером нужны URWTrace.exe и URWTraceHook.dll.",L"URWTrace.exe ja URWTraceHook.dll on sijoitettava käynnistimen viereen.");break;
    case Notice::CatalogMissing:text=T(L"Translation files are missing: ",L"Не найдены файлы перевода: ",L"Käännöstiedostoja puuttuu: ");break;
    case Notice::StartError:text=T(L"Could not start the game: ",L"Не удалось запустить игру: ",L"Pelin käynnistäminen epäonnistui: ");break;
    case Notice::StartedEng:text=T(L"English game started.",L"Запущена английская версия.",L"Peli käynnistetty englanniksi.");break;
    case Notice::StartedRus:text=T(L"Russian translation started.",L"Запущена русская версия.",L"Peli käynnistetty venäjäksi.");break;
    case Notice::StartedFin:text=T(L"Finnish translation started.",L"Запущен финский перевод.",L"Suomenkielinen käännös käynnistetty.");break;
    case Notice::Exited:text=T(L"Program exited with code ",L"Программа завершилась с кодом ",L"Ohjelma sulkeutui. Lopetuskoodi: ");break;
    case Notice::Closed:text=T(L"Game closed. You can launch it again.",L"Игра закрыта. Можно запустить снова.",L"Peli suljettu. Voit käynnistää sen uudelleen.");break;
    }
    std::wstring value=std::wstring(text)+app.detail;SetDlgItemTextW(app.window,Status,value.c_str());
}
static void StatusText(Notice notice,const std::wstring& detail=L""){app.notice=notice;app.detail=detail;RefreshStatus();}
static void SyncSteam();
static void SyncControls() {
    CheckRadioButton(app.window,Console0,Console2,Console0+app.console);
    CheckDlgButton(app.window,Logging,app.logging?BST_CHECKED:BST_UNCHECKED);
    SetWindowTextW(app.window,T(L"UnReal World — launcher",L"UnReal World — запуск",L"UnReal World — käynnistin"));
    SetDlgItemTextW(app.window,LaunchTitle,T(L"Start game",L"Запуск игры",L"Käynnistä peli"));
    SetDlgItemTextW(app.window,Eng,T(L"Start ENG",L"Запустить ENG",L"Käynnistä ENG"));
    SetDlgItemTextW(app.window,Rus,T(L"Start RUS",L"Запустить RUS",L"Käynnistä RUS"));
    SetDlgItemTextW(app.window,Fin,T(L"Start FIN",L"Запустить FIN",L"Käynnistä FIN"));
    SetDlgItemTextW(app.window,EngNote,T(L"Original game",L"Оригинальная игра",L"Alkuperäinen peli"));
    SetDlgItemTextW(app.window,RusNote,T(L"Russian translation",L"Русский перевод",L"Venäjänkielinen käännös"));
    SetDlgItemTextW(app.window,FinNote,T(L"Finnish translation",L"Финский перевод",L"Suomenkielinen käännös"));
    SetDlgItemTextW(app.window,ConsoleTitle,T(L"Console — ConsoleMode",L"Консоль — ConsoleMode",L"Konsoli — ConsoleMode"));
    SetDlgItemTextW(app.window,Console0,T(L"0 — Hide console",L"0 — Не показывать консоль",L"0 — Piilota konsoli"));
    SetDlgItemTextW(app.window,Console1,T(L"1 — Text and hook markers",L"1 — Текст и маркеры перехвата",L"1 — Teksti ja sieppausmerkinnät"));
    SetDlgItemTextW(app.window,Console2,T(L"2 — All events, detailed diagnostics",L"2 — Все события, подробная диагностика",L"2 — Kaikki tapahtumat ja tarkka diagnostiikka"));
    SetDlgItemTextW(app.window,LoggingTitle,T(L"Log — LoggingEnabled",L"Журнал — LoggingEnabled",L"Loki — LoggingEnabled"));
    std::wstring label=T(L"Write log — LoggingEnabled = ",L"Записывать журнал — LoggingEnabled = ",L"Kirjoita loki — LoggingEnabled = ");label+=app.logging?L"1":L"0";SetDlgItemTextW(app.window,Logging,label.c_str());
    SetDlgItemTextW(app.window,LoggingHelp,T(L"0 — No log files.\r\n1 — Save logs in the logs folder for bug reports.",L"0 — Журнал не ведётся.\r\n1 — Запись в папку logs для отчёта об ошибках.",L"0 — Lokitiedostoja ei luoda.\r\n1 — Tallenna lokit logs-kansioon virheraportteja varten."));
    SetDlgItemTextW(app.window,UILanguageTitle,T(L"Launcher language",L"Язык лаунчера",L"Käynnistimen kieli"));
    SendDlgItemMessageW(app.window,UILanguage,CB_SETCURSEL,app.ui,0);RefreshStatus();
    SendDlgItemMessageW(app.window,SteamLanguage,CB_SETCURSEL,app.language=="en"?0:app.language=="fi"?2:1,0);
    SyncSteam();
}
static std::wstring IniChoice(const wchar_t* section,const wchar_t* key,const wchar_t* fallback) {
    wchar_t value[128]={};GetPrivateProfileStringW(section,key,fallback,value,128,app.ini.c_str());
    std::wstring choice=value;auto end=choice.find_first_of(L";#");if(end!=std::wstring::npos)choice.resize(end);
    auto first=choice.find_first_not_of(L" \t");if(first==std::wstring::npos)return fallback;
    return choice.substr(first,choice.find_last_not_of(L" \t")-first+1);
}
static void LoadSettings() {
    app.console=(std::max)(0,(std::min)(2,(int)GetPrivateProfileIntW(L"Hook",L"ConsoleMode",0,app.ini.c_str())));
    app.logging=GetPrivateProfileIntW(L"Hook",L"LoggingEnabled",0,app.ini.c_str())!=0;
    std::wstring language=IniChoice(L"Hook",L"Language",L"ru");app.language=_wcsicmp(language.c_str(),L"fi")==0?"fi":_wcsicmp(language.c_str(),L"en")==0?"en":"ru";
    language=IniChoice(L"Launcher",L"UILanguage",L"ru");app.ui=_wcsicmp(language.c_str(),L"en")==0?0:_wcsicmp(language.c_str(),L"fi")==0?2:1;
    SyncControls();
}
static bool SaveSettings() {
    std::string before;if(!ReadBytes(app.ini,before,true)){StatusText(Notice::ReadError,Error());return false;}
    auto updated=EditedIni(before,app.console,app.logging);
    if(updated.empty()){StatusText(Notice::EncodingError);return false;}
    if(updated==before){StatusText(Notice::Saved);return true;}
    std::wstring temp=app.ini+L".launcher_"+std::to_wstring(GetCurrentProcessId())+L".tmp";
    HANDLE file=CreateFileW(temp.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file==INVALID_HANDLE_VALUE){StatusText(Notice::SaveError,Error());return false;}
    DWORD written=0;bool okay=WriteFile(file,updated.data(),(DWORD)updated.size(),&written,nullptr)&&written==updated.size()&&FlushFileBuffers(file);
    DWORD code=GetLastError();CloseHandle(file);
    std::string current;
    if(okay&&(!ReadBytes(app.ini,current,true)||current!=before)){okay=false;code=ERROR_RETRY;}
    if(okay&&!MoveFileExW(temp.c_str(),app.ini.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)){okay=false;code=GetLastError();}
    if(!okay){DeleteFileW(temp.c_str());StatusText(Notice::SaveError,Error(code));return false;}
    WritePrivateProfileStringW(nullptr,nullptr,nullptr,app.ini.c_str());
    StatusText(Notice::Saved);return true;
}
static void CheckUpdate() {
    if(app.process||app.updater)return;
    if(SteamInstall::Running(app.root)){StatusText(Notice::SteamMessage,T(L"Close the game first.",L"Сначала закройте игру.",L"Sulje peli ensin."));return;}
    auto script=app.root+L"\\URWUpdater.ps1";
    if(!Exists(script)){StatusText(Notice::SteamMessage,T(L"Extract the complete translation package first.",L"Сначала распакуйте полный комплект перевода.",L"Pura ensin koko käännöspaketti."));return;}
    if(!SaveSettings())return;
    wchar_t system[32768];if(!GetSystemDirectoryW(system,32768))return;
    std::wstring powershell=std::wstring(system)+L"\\WindowsPowerShell\\v1.0\\powershell.exe";
    int edition=(int)SendDlgItemMessageW(app.window,UpdateEdition,CB_GETCURSEL,0,0);
    std::wstring cmd=L"\""+powershell+L"\" -NoProfile -ExecutionPolicy Bypass -File \""+script+L"\" -Root \""+app.root+L"\" -Edition "+(edition==1?L"lite":L"standard")+L" -CurrentVersion 0.26 -ParentId "+std::to_wstring(GetCurrentProcessId())+L" -UILanguage "+(app.ui==0?L"en":app.ui==2?L"fi":L"ru");
    STARTUPINFOW si={sizeof(si)};PROCESS_INFORMATION pi={};
    if(!CreateProcessW(powershell.c_str(),cmd.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,app.root.c_str(),&si,&pi)){StatusText(Notice::StartError,Error());return;}
    CloseHandle(pi.hThread);app.updater=pi.hProcess;SyncSteam();
}
static void SyncSteam() {
    if(app.updater&&WaitForSingleObject(app.updater,0)==WAIT_OBJECT_0){CloseHandle(app.updater);app.updater=nullptr;}
    bool managed=SteamInstall::Managed(app.root),idle=!app.process&&!app.updater;
    auto state=SteamInstall::Read(app.root);
    auto installedHash=SteamInstall::Hash(app.root+L"\\urw.exe");
    bool installed=SteamInstall::Own(state,installedHash);
    auto masterHash=installed?SteamInstall::Hash(app.root+L"\\URWTrace.exe"):std::string{};
    bool incomplete=installed&&masterHash!=kSteamBootstrapHash;
    bool outdated=installed&&!incomplete&&installedHash!=masterHash;
    for(int id:{Eng,Rus,Fin})EnableWindow(GetDlgItem(app.window,id),idle&&!managed);
    EnableWindow(GetDlgItem(app.window,SteamInstallButton),idle&&!managed);
    EnableWindow(GetDlgItem(app.window,SteamRepair),idle&&managed);
    EnableWindow(GetDlgItem(app.window,SteamRestore),idle&&managed);
    EnableWindow(GetDlgItem(app.window,SteamApply),idle);
    EnableWindow(GetDlgItem(app.window,UpdateCheck),idle);
    EnableWindow(GetDlgItem(app.window,UpdateEdition),idle);
    bool full=Exists(app.root+L"\\rus_translate\\catalog.html");
    SetDlgItemTextW(app.window,UpdateTitle,full?T(L"Updates — installed: Standard",L"Обновления — установлена обычная редакция",L"Päivitykset — asennettu: tavallinen"):T(L"Updates — installed: Lite",L"Обновления — установлена Lite",L"Päivitykset — asennettu: Lite"));
    SetDlgItemTextW(app.window,UpdateCheck,T(L"Check for updates",L"Проверить обновления",L"Tarkista päivitykset"));
    SetDlgItemTextW(app.window,UpdateLabel,T(L"Update edition:",L"Редакция для обновления:",L"Päivityspaketti:"));
    auto combo=GetDlgItem(app.window,UpdateEdition);
    int selected=(int)SendMessageW(combo,CB_GETCURSEL,0,0);if(selected<0)selected=full?0:1;
    // Only recreate captions when the UI language changes; keep user's choice.
    if((int)GetWindowLongPtrW(combo,GWLP_USERDATA)!=app.ui+1){
      SendMessageW(combo,CB_RESETCONTENT,0,0);
      SendMessageW(combo,CB_ADDSTRING,0,(LPARAM)T(L"Standard",L"Обычная",L"Tavallinen"));
      SendMessageW(combo,CB_ADDSTRING,0,(LPARAM)T(L"Lite",L"Облегчённая (Lite)",L"Lite"));
      SendMessageW(combo,CB_SETCURSEL,selected,0);SetWindowLongPtrW(combo,GWLP_USERDATA,app.ui+1);
    }
    SetDlgItemTextW(app.window,SteamTitle,T(L"Launch from Steam",L"Запуск из Steam",L"Käynnistys Steamista"));
    SetDlgItemTextW(app.window,SteamInstallButton,T(L"Enable",L"Включить",L"Ota käyttöön"));
    SetDlgItemTextW(app.window,SteamRepair,outdated?
      T(L"Update Steam loader",L"Обновить загрузчик Steam",L"Päivitä Steam-käynnistin"):
      T(L"Repair after update",L"Восстановить после обновления",L"Korjaa päivityksen jälkeen"));
    SetDlgItemTextW(app.window,SteamRestore,T(L"Restore original",L"Вернуть оригинальный запуск",L"Palauta alkuperäinen"));
    SetDlgItemTextW(app.window,SteamApply,T(L"Change game language",L"Сменить язык игры",L"Vaihda pelin kieli"));
    SetDlgItemTextW(app.window,SteamStatus,managed?(incomplete?
      T(L"Translation package is incomplete or mixed. Extract the complete matching package.",L"Файлы перевода отсутствуют или относятся к разным версиям. Распакуйте полный комплект.",L"Käännöspaketti on puutteellinen tai versiot eivät täsmää. Pura koko oikea paketti."):
      outdated?
      T(L"Translation updated. Click Update Steam loader before launching in Steam.",L"Перевод обновлён. Нажмите «Обновить загрузчик Steam» перед запуском из Steam.",L"Käännös on päivitetty. Paina Päivitä Steam-käynnistin ennen käynnistystä Steamista."):
      installed?
      T(L"Enabled. Launch in Steam; change the language here.",L"Включено. Запускайте из Steam; язык меняется здесь.",L"Käytössä. Käynnistä Steamista; vaihda kieli täällä."):
      T(L"Needs repair or rollback. Direct launch is disabled.",L"Требуется восстановление или откат. Прямой запуск отключён.",L"Korjaa tai palauta. Suora käynnistys on estetty.")):
      T(L"Disabled. Select a game language before enabling.",L"Выключено. Перед включением выберите язык игры.",L"Ei käytössä. Valitse pelin kieli ennen käyttöönottoa."));
}
static void SteamAction(int id) {
    if(app.process)return;
    if(id==SteamApply){
        SteamInstall::Lock lock(app.root);
        if(!lock.okay()||SteamInstall::Running(app.root)){StatusText(Notice::SteamMessage,T(L"Close the game first.",L"Сначала закройте игру.",L"Sulje peli ensin."));return;}
        int selected=(int)SendDlgItemMessageW(app.window,SteamLanguage,CB_GETCURSEL,0,0);
        std::string previous=app.language;app.language=selected==0?"en":selected==2?"fi":"ru";
        if(!SaveSettings())app.language=previous;
        SyncSteam();return;
    }
    if(id==SteamInstallButton){
        // Public installation: all translation files are beside the real game.
        if(_wcsicmp(GamePath().c_str(),(app.root+L"\\urw.exe").c_str())!=0){
            StatusText(Notice::SteamMessage,T(L"Install the translation beside urw.exe; GameExe must be urw.exe.",L"Положите перевод рядом с urw.exe; GameExe должен быть urw.exe.",L"Asenna käännös urw.exe-tiedoston viereen; GameExe=urw.exe."));return;}
    }
    if(id!=SteamRestore){
        if(SteamInstall::Hash(app.root+L"\\URWTrace.exe")!=kSteamBootstrapHash||!Exists(app.root+L"\\URWTraceHook.dll")){
            StatusText(Notice::SteamMessage,T(L"Copy the complete matching translation package first.",L"Сначала распакуйте полный комплект одной версии перевода.",L"Pura ensin koko saman version käännöspaketti."));return;}
        if(app.language!="en"){
            auto folder=TranslationPath(app.language=="fi");
            if(!Exists(folder+L"\\catalog.json")||!Exists(folder+L"\\URW.FNT")){StatusText(Notice::CatalogMissing,folder);return;}
        }
    }
    auto result=id==SteamRestore?SteamInstall::Restore(app.root):SteamInstall::Install(app.root);
    const wchar_t* message=L"";
    using R=SteamInstall::Result;
    switch(result){
    case R::Okay:message=id==SteamRestore?T(L"Original launch restored.",L"Оригинальный запуск восстановлен.",L"Alkuperäinen käynnistys palautettu."):T(L"Ready. Press Play in Steam.",L"Готово. Нажмите «Играть» в Steam.",L"Valmis. Paina Steamissa Pelaa.");break;
    case R::Busy:message=T(L"Close the game and other launchers first.",L"Сначала закройте игру и другие загрузчики.",L"Sulje peli ja muut käynnistimet ensin.");break;
    case R::Unsupported:message=T(L"This game EXE is not supported. Install a compatible translation update; the game was not replaced.",L"Этот EXE ещё не поддерживается. Нужна совместимая версия перевода; игра не заменена.",L"Pelin EXE-versiota ei tueta. Asenna yhteensopiva käännös; peliä ei korvattu.");break;
    case R::Collision:message=T(L"An existing original backup differs. Files were preserved; check urw-original.exe.",L"Обнаружена другая копия оригинала. Файлы сохранены; проверьте urw-original.exe.",L"Alkuperäisen varmuuskopio eroaa. Tiedostot säilytettiin; tarkista urw-original.exe.");break;
    case R::InvalidState:message=T(L"Installation record is damaged. Files were not changed.",L"Повреждена запись установки. Файлы не изменены.",L"Asennustieto on vioittunut. Tiedostoja ei muutettu.");break;
    case R::Missing:message=T(L"Required executable is missing or unreadable.",L"Не найден или недоступен нужный EXE.",L"Tarvittava EXE puuttuu tai sitä ei voi lukea.");break;
    default:message=T(L"Could not finish. Close the game, check file permissions, then repair or restore.",L"Не удалось завершить. Закройте игру, проверьте доступ к файлам, выполните восстановление или откат.",L"Toiminto epäonnistui. Sulje peli, tarkista käyttöoikeudet ja korjaa tai palauta.");break;
    }
    StatusText(Notice::SteamMessage,message);SyncSteam();
}
static void Start(int id) {
    if(app.process||app.updater||SteamInstall::Managed(app.root))return;
    const bool translated=id!=Eng;const std::wstring game=GamePath(),program=translated?app.root+L"\\URWTrace.exe":game;
    if(!Exists(game)){StatusText(Notice::GameMissing);return;}
    if(!Exists(program)||(translated&&!Exists(app.root+L"\\URWTraceHook.dll"))){StatusText(Notice::RuntimeMissing);return;}
    const std::wstring language=id==Fin?L"fi":L"ru",folder=TranslationPath(id==Fin);
    if(translated) {
        if(!Exists(folder+L"\\catalog.json")||!Exists(folder+L"\\URW.FNT")){StatusText(Notice::CatalogMissing,folder+L"\\catalog.json / URW.FNT");return;}
        std::string previous=app.language;app.language=id==Fin?"fi":"ru";
        if(!SaveSettings()){app.language=previous;return;}
    }
    std::wstring command=L"\""+program+L"\""+(translated?L" --language "+language:L"");STARTUPINFOW si={};si.cb=sizeof(si);PROCESS_INFORMATION pi={};
    // English bypasses translation. Explicit language also prevents a second launcher changing this launch's language.
    std::wstring directory=translated?app.root:Directory(game);
    if(!CreateProcessW(program.c_str(),&command[0],nullptr,nullptr,FALSE,0,nullptr,directory.c_str(),&si,&pi)){StatusText(Notice::StartError,Error());return;}
    CloseHandle(pi.hThread);app.process=pi.hProcess;
    for(int button:{Eng,Rus,Fin})EnableWindow(GetDlgItem(app.window,button),FALSE);
    StatusText(id==Fin?Notice::StartedFin:id==Rus?Notice::StartedRus:Notice::StartedEng);SetTimer(app.window,1,300,nullptr);
}
static HWND Control(const wchar_t* cls,const wchar_t* text,DWORD style,int x,int y,int w,int h,int id) {
    HWND control=CreateWindowExW(0,cls,text,WS_CHILD|WS_VISIBLE|style,app.Scale(x),app.Scale(y),app.Scale(w),app.Scale(h),app.window,(HMENU)(INT_PTR)id,GetModuleHandleW(nullptr),nullptr);
    SendMessageW(control,WM_SETFONT,(WPARAM)app.font,TRUE);return control;
}
static LRESULT CALLBACK WindowProc(HWND window,UINT message,WPARAM wParam,LPARAM lParam) {
    switch(message) {
    case WM_CREATE:
        app.window=window;
        Control(L"STATIC",L"",0,20,16,158,22,LaunchTitle);
        Control(L"BUTTON",L"",WS_TABSTOP|BS_PUSHBUTTON,20,46,158,44,Eng);
        Control(L"STATIC",L"",0,20,95,158,28,EngNote);
        Control(L"BUTTON",L"",WS_TABSTOP|BS_PUSHBUTTON,20,132,158,44,Rus);
        Control(L"STATIC",L"",0,20,181,158,28,RusNote);
        Control(L"BUTTON",L"",WS_TABSTOP|BS_PUSHBUTTON,20,218,158,44,Fin);
        Control(L"STATIC",L"",0,20,267,158,28,FinNote);
        Control(L"BUTTON",L"",BS_GROUPBOX,198,12,462,128,ConsoleTitle);
        Control(L"BUTTON",L"",WS_GROUP|WS_TABSTOP|BS_AUTORADIOBUTTON,212,40,434,24,Console0);
        Control(L"BUTTON",L"",WS_TABSTOP|BS_AUTORADIOBUTTON,212,70,434,24,Console1);
        Control(L"BUTTON",L"",WS_TABSTOP|BS_AUTORADIOBUTTON,212,100,434,24,Console2);
        Control(L"BUTTON",L"",BS_GROUPBOX,198,152,462,116,LoggingTitle);
        Control(L"BUTTON",L"",WS_TABSTOP|BS_AUTOCHECKBOX,212,180,434,24,Logging);
        Control(L"STATIC",L"",0,212,212,434,44,LoggingHelp);
        Control(L"STATIC",L"",0,198,285,230,24,UILanguageTitle);
        Control(L"COMBOBOX",L"",WS_TABSTOP|WS_VSCROLL|CBS_DROPDOWNLIST,432,280,228,130,UILanguage);
        for(const wchar_t* language:{L"English",L"Русский",L"Suomi"})SendDlgItemMessageW(window,UILanguage,CB_ADDSTRING,0,(LPARAM)language);
        Control(L"BUTTON",L"",BS_GROUPBOX,20,322,640,164,SteamTitle);
        Control(L"COMBOBOX",L"",WS_TABSTOP|WS_VSCROLL|CBS_DROPDOWNLIST,34,350,170,130,SteamLanguage);
        for(const wchar_t* language:{L"English",L"Русский",L"Suomi"})SendDlgItemMessageW(window,SteamLanguage,CB_ADDSTRING,0,(LPARAM)language);
        Control(L"BUTTON",L"",WS_TABSTOP|BS_PUSHBUTTON,216,348,250,28,SteamApply);
        Control(L"BUTTON",L"",WS_TABSTOP|BS_PUSHBUTTON,34,390,104,34,SteamInstallButton);
        Control(L"BUTTON",L"",WS_TABSTOP|BS_PUSHBUTTON,146,390,260,34,SteamRepair);
        Control(L"BUTTON",L"",WS_TABSTOP|BS_PUSHBUTTON,414,390,232,34,SteamRestore);
        Control(L"STATIC",L"",SS_LEFT,34,436,610,42,SteamStatus);
        Control(L"BUTTON",L"",BS_GROUPBOX,20,492,640,92,UpdateTitle);
        Control(L"STATIC",L"",0,34,514,230,22,UpdateLabel);
        Control(L"COMBOBOX",L"",WS_TABSTOP|WS_VSCROLL|CBS_DROPDOWNLIST,34,540,206,130,UpdateEdition);
        Control(L"BUTTON",L"",WS_TABSTOP|BS_PUSHBUTTON,260,538,386,30,UpdateCheck);
        Control(L"STATIC",L"",SS_LEFT,20,596,640,70,Status);
        LoadSettings();SetTimer(window,2,2000,nullptr);return 0;
    case WM_COMMAND:
        if(LOWORD(wParam)==UILanguage&&HIWORD(wParam)==CBN_SELCHANGE){
            int selected=(int)SendDlgItemMessageW(window,UILanguage,CB_GETCURSEL,0,0);
            if(selected>=0&&selected<=2){app.ui=selected;SyncControls();if(!SaveSettings())LoadSettings();}return 0;
        }
        if(HIWORD(wParam)==BN_CLICKED) {
            int id=LOWORD(wParam);
            if(id==UpdateCheck){CheckUpdate();return 0;}
            if(id==SteamInstallButton||id==SteamRepair||id==SteamRestore||id==SteamApply){SteamAction(id);return 0;}
            if(id==Eng||id==Rus||id==Fin){Start(id);return 0;}
            if((id>=Console0&&id<=Console2)||id==Logging){
                if(id==Logging)app.logging=IsDlgButtonChecked(window,Logging)==BST_CHECKED;else app.console=id-Console0;
                if(!SaveSettings())LoadSettings();else SyncControls();return 0;
            }
        }break;
    case WM_TIMER:
        if(wParam==2){SyncSteam();return 0;}
        if(app.process&&WaitForSingleObject(app.process,0)==WAIT_OBJECT_0){
            DWORD code=0;GetExitCodeProcess(app.process,&code);CloseHandle(app.process);app.process=nullptr;KillTimer(window,1);
            SyncSteam();
            StatusText(code?Notice::Exited:Notice::Closed,code?std::to_wstring(code)+L".":L"");
        }return 0;
    case WM_DESTROY:
        // Closing the launcher never terminates a running game/bootstrap.
        KillTimer(window,1);PostQuitMessage(0);return 0;
    }
    return DefWindowProcW(window,message,wParam,lParam);
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,LPWSTR,int show) {
    SetProcessDPIAware();INITCOMMONCONTROLSEX common={sizeof(common),ICC_STANDARD_CLASSES};InitCommonControlsEx(&common);
    wchar_t path[32768];DWORD count=GetModuleFileNameW(nullptr,path,32768);if(!count||count>=32768)return 1;
    app.root=Directory(path);app.ini=app.root+L"\\URWTrace.ini";
    HDC dc=GetDC(nullptr);app.dpi=GetDeviceCaps(dc,LOGPIXELSY);ReleaseDC(nullptr,dc);
    app.font=CreateFontW(-MulDiv(10,app.dpi,72),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
    WNDCLASSEXW cls={sizeof(cls)};cls.lpfnWndProc=WindowProc;cls.hInstance=instance;cls.lpszClassName=WindowClass;cls.hCursor=LoadCursorW(nullptr,IDC_ARROW);cls.hIcon=LoadIconW(nullptr,IDI_APPLICATION);cls.hbrBackground=(HBRUSH)(COLOR_BTNFACE+1);
    if(!RegisterClassExW(&cls))return 2;
    DWORD style=WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX;RECT size={0,0,app.Scale(680),app.Scale(680)};AdjustWindowRectEx(&size,style,FALSE,0);
    HWND window=CreateWindowExW(0,WindowClass,L"UnReal World — запуск",style,CW_USEDEFAULT,CW_USEDEFAULT,size.right-size.left,size.bottom-size.top,nullptr,nullptr,instance,nullptr);
    if(!window)return 3;ShowWindow(window,show);UpdateWindow(window);
    MSG message;while(GetMessageW(&message,nullptr,0,0)>0){if(!IsDialogMessageW(window,&message)){TranslateMessage(&message);DispatchMessageW(&message);}}
    return (int)message.wParam;
}
