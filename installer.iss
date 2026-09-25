[Setup]
AppId={{D91DC516-9175-4D87-9AB2-D42315583581}
AppName=Magic Home Controller
#ifndef AppVersion
  #define AppVersion 1.1
#endif
AppVersion={#AppVersion}
AppPublisher=Magic Home Controller contributors
DefaultDirName={autopf}\Magic Home Controller
DefaultGroupName=Magic Home Controller
DisableProgramGroupPage=yes
OutputDir=installer-output
OutputBaseFilename=Magic-Home-Controller-Setup-{#AppVersion}-x64
SetupIconFile=icon.ico
UninstallDisplayIcon={app}\Magic-Home-Controller.exe
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
PrivilegesRequiredOverridesAllowed=dialog
LicenseFile=LICENSE
CloseApplications=yes
RestartApplications=no

[Languages]
Name: "russian"; MessagesFile: "compiler:Languages\Russian.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[Files]
Source: "dist-cpp\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{autoprograms}\Magic Home Controller"; Filename: "{app}\Magic-Home-Controller.exe"
Name: "{autodesktop}\Magic Home Controller"; Filename: "{app}\Magic-Home-Controller.exe"; Tasks: desktopicon

[Tasks]
Name: "desktopicon"; Description: "Создать ярлык на рабочем столе"; GroupDescription: "Дополнительные ярлыки:"
Name: "autostart"; Description: "Запускать вместе с Windows"; GroupDescription: "Автозапуск:"

[Code]
function IsAdminInstallModeSelected: Boolean;
begin
  Result := IsAdminInstallMode;
end;

[Registry]
Root: HKCU; Subkey: "Software\Microsoft\Windows\CurrentVersion\Run"; ValueType: string; ValueName: "Magic Home Controller"; ValueData: """{app}\Magic-Home-Controller.exe"" --minimized"; Tasks: autostart; Check: not IsAdminInstallModeSelected; Flags: uninsdeletevalue
Root: HKLM; Subkey: "Software\Microsoft\Windows\CurrentVersion\Run"; ValueType: string; ValueName: "Magic Home Controller"; ValueData: """{app}\Magic-Home-Controller.exe"" --minimized"; Tasks: autostart; Check: IsAdminInstallModeSelected; Flags: uninsdeletevalue

[Run]
Filename: "{app}\Magic-Home-Controller.exe"; Description: "Запустить Magic Home Controller"; Flags: nowait postinstall skipifsilent