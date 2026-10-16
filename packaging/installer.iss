; ============================================================
;  XJTUNetLogin Inno Setup 脚本
;  由 CI 通过 /DMyAppVersion=x.y.z 传入版本号
; ============================================================

#ifndef MyAppVersion
  #define MyAppVersion "0.0.0"
#endif

#ifndef SourceDir
  #define SourceDir "..\build\bin"
#endif

#ifndef OutputDir
  #define OutputDir "..\dist"
#endif

#define MyAppName       "仙交网络助手"
#define MyAppNameEn     "XJTUNetLogin"
#define MyAppPublisher  "Hz6826"
#define MyAppURL        "https://github.com/Hz6826/XJTUNetLogin"
#define MyAppExeName    "XJTUNetLogin.exe"

[Setup]
; 固定 GUID，升级时才会覆盖安装而不是并存
AppId={{0DC6B9F5-EC59-DFB9-7416-6F7A5EA2AB18}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppVerName={#MyAppName} {#MyAppVersionEn}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}/issues
AppUpdatesURL={#MyAppURL}/releases

DefaultDirName={autopf}\{#MyAppName}
DefaultGroupName={#MyAppName}
DisableProgramGroupPage=yes
DisableDirPage=auto
DisableReadyPage=yes

OutputDir={#OutputDir}
OutputBaseFilename={#MyAppNameEn}-Setup-{#MyAppVersion}

Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
ArchitecturesInstallIn64BitMode=x64compatible
UninstallDisplayIcon={app}\{#MyAppExeName}
SetupIconFile=..\resources\windows\app.ico
LicenseFile=..\LICENSE
MinVersion=10.0

[Languages]
Name: "chinesesimplified"; MessagesFile: "ChineseSimplified.isl"
Name: "english";          MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
; build/bin 整个目录（exe + Qt DLL + 资源）
Source: "{#SourceDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\{#MyAppName}";                          Filename: "{app}\{#MyAppExeName}"
Name: "{group}\{cm:UninstallProgram,{#MyAppName}}";    Filename: "{uninstallexe}"
Name: "{autodesktop}\{#MyAppName}";                    Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#MyAppName}}"; Flags: nowait postinstall skipifsilent

[UninstallDelete]
; 不删用户配置 —— 卸载重装时配置还在 %APPDATA%
