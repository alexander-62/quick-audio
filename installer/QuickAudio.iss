#define AppVersion "0.2.6"
[Setup]
AppId={{5F15B389-4D25-485F-9954-94B63C48626A}
AppName=QuickAudio
AppVersion={#AppVersion}
AppPublisher=alexander-62
AppPublisherURL=https://github.com/alexander-62/quick-audio
AppSupportURL=https://github.com/alexander-62/quick-audio/issues
AppUpdatesURL=https://github.com/alexander-62/quick-audio/releases
DefaultDirName={localappdata}\Programs\QuickAudio
DefaultGroupName=QuickAudio
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
ArchitecturesAllowed=x64os
ArchitecturesInstallIn64BitMode=x64os
MinVersion=10.0.26100
OutputDir=..\dist
OutputBaseFilename=QuickAudio-Setup-{#AppVersion}-x64
SetupIconFile=..\src\app.ico
UninstallDisplayIcon={app}\QuickAudio.exe
LicenseFile=..\LICENSE
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
CloseApplications=no
RestartApplications=no
Uninstallable=yes
VersionInfoVersion={#AppVersion}.0

[Languages]
Name: "russian"; MessagesFile: "compiler:Languages\Russian.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[CustomMessages]
russian.Autostart=Запускать QuickAudio вместе с Windows
english.Autostart=Start QuickAudio with Windows
russian.Manager=Открыть управление QuickAudio
english.Manager=Open QuickAudio controls
russian.ReleaseFailed=Не удалось закрыть QuickAudio. Закройте окно управления и повторите попытку.
english.ReleaseFailed=Could not stop QuickAudio. Close its control window and try again.

[Tasks]
Name: "autostart"; Description: "{cm:Autostart}"; Flags: unchecked

[Files]
Source: "..\build\QuickAudio.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\build\QuickAudio.Hook.{#AppVersion}.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\LICENSE"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\THIRD_PARTY_NOTICES.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\README.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\README.ru.md"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\QuickAudio"; Filename: "{app}\QuickAudio.exe"; Parameters: "--settings"
Name: "{group}\Uninstall QuickAudio"; Filename: "{uninstallexe}"

[UninstallDelete]
Type: files; Name: "{app}\QuickAudio.Hook.log"
Type: files; Name: "{app}\QuickAudio.Hook.previous.log"

[Run]
Filename: "{app}\QuickAudio.exe"; Parameters: "--autostart"; Flags: runhidden; Tasks: autostart
Filename: "{app}\QuickAudio.exe"; Parameters: "--remove-autostart"; Flags: runhidden; Check: not WizardIsTaskSelected('autostart')
Filename: "{app}\QuickAudio.exe"; Parameters: "--run"; Flags: nowait runhidden
Filename: "{app}\QuickAudio.exe"; Parameters: "--settings"; Description: "{cm:Manager}"; Flags: nowait postinstall skipifsilent unchecked

[Code]
procedure InitializeWizard;
var Command: String; Approved: AnsiString;
begin
  if RegQueryStringValue(HKCU, 'Software\Microsoft\Windows\CurrentVersion\Run', 'QuickAudio', Command) then
    if not RegQueryBinaryValue(HKCU, 'Software\Microsoft\Windows\CurrentVersion\Explorer\StartupApproved\Run', 'QuickAudio', Approved) or
      (Length(Approved) = 0) or ((Ord(Approved[1]) <> 3) and (Ord(Approved[1]) <> 7)) then
      WizardSelectTasks('autostart');
end;

function PrepareToInstall(var NeedsRestart: Boolean): String;
var Code: Integer; ProgramPath: String;
begin
  Result := '';
  ProgramPath := ExpandConstant('{app}\QuickAudio.exe');
  if FileExists(ProgramPath) then
    if not Exec(ProgramPath, '--prepare-update', '', SW_HIDE, ewWaitUntilTerminated, Code) or (Code <> 0) then
      Result := CustomMessage('ReleaseFailed');
end;

procedure CurUninstallStepChanged(Step: TUninstallStep);
var Code: Integer; ProgramPath: String;
begin
  if Step = usUninstall then begin
    ProgramPath := ExpandConstant('{app}\QuickAudio.exe');
    if FileExists(ProgramPath) then begin
      if not Exec(ProgramPath, '--prepare-update', '', SW_HIDE, ewWaitUntilTerminated, Code) or (Code <> 0) then
        RaiseException(CustomMessage('ReleaseFailed'));
      Exec(ProgramPath, '--remove-autostart', '', SW_HIDE, ewWaitUntilTerminated, Code);
    end;
  end;
end;
