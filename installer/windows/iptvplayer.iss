; Windows installer (Inno Setup 6). Built by CI:
;   iscc /DAppVersion=0.7.0 /DSourceDir=dist\IPTVPlayer /DOutputDir=dist installer\windows\iptvplayer.iss
; SourceDir is the folder made by windeployqt.

#ifndef AppVersion
  #define AppVersion "0.0.0"
#endif
#ifndef SourceDir
  #define SourceDir "..\..\dist\IPTVPlayer"
#endif
#ifndef OutputDir
  #define OutputDir "..\..\dist"
#endif

[Setup]
; Never change AppId: Windows uses it to find the installed version to update.
AppId={{7C1B5E0A-4F3D-4C59-9A63-2E7B1D8F4A21}
AppName=IPTV Player
AppVersion={#AppVersion}
AppVerName=IPTV Player {#AppVersion}
AppPublisher=IPTV Player
AppPublisherURL=https://github.com/sadikBr/IPTV-Player
DefaultDirName={autopf}\IPTV Player
DefaultGroupName=IPTV Player
DisableProgramGroupPage=yes
; Installs for the current user without admin rights; admins can choose all users.
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputDir={#OutputDir}
OutputBaseFilename=IPTVPlayer-setup-x64
SetupIconFile=..\..\assets\icon\app.ico
UninstallDisplayIcon={app}\IPTVPlayer.exe
UninstallDisplayName=IPTV Player
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
CloseApplications=yes

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"

[Files]
Source: "{#SourceDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[InstallDelete]
; Old Qt files from a previous version
Type: filesandordirs; Name: "{app}\qml"

[Icons]
Name: "{autoprograms}\IPTV Player"; Filename: "{app}\IPTVPlayer.exe"
Name: "{autodesktop}\IPTV Player"; Filename: "{app}\IPTVPlayer.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\IPTVPlayer.exe"; Description: "{cm:LaunchProgram,IPTV Player}"; Flags: nowait postinstall skipifsilent
