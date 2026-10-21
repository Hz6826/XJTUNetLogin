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

; 已确认：QStandardPaths::AppDataLocation = %APPDATA%\Hz6826\XJTUNetLogin
#define AppDataSubDir   "Hz6826\XJTUNetLogin"

[Setup]
; 固定 GUID，升级时才会覆盖安装而不是并存
AppId={{0DC6B9F5-EC59-DFB9-7416-6F7A5EA2AB18}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppVerName={#MyAppName} {#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}/issues
AppUpdatesURL={#MyAppURL}/releases

DefaultDirName={autopf}\{#MyAppNameEn}
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
; 程序只提供 64 位构建，32 位系统直接拒绝安装，避免装上了却无法运行
ArchitecturesAllowed=x64compatible
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
Source: "{#SourceDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\{#MyAppName}";                          Filename: "{app}\{#MyAppExeName}"
Name: "{group}\{cm:UninstallProgram,{#MyAppName}}";    Filename: "{uninstallexe}"
Name: "{autodesktop}\{#MyAppName}";                    Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#MyAppName}}"; Flags: nowait postinstall skipifsilent

[UninstallDelete]
; 不删用户配置 —— 卸载重装时配置还在 %APPDATA%
; 配置删除交给 [Code] 里带确认框的逻辑

[Code]
procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
var
  DataDir, CfgFile, LogDir: String;
begin
  if CurUninstallStep <> usUninstall then
    Exit;

  // ---- 1) 静默清理启动项（程序写入 HKCU\Run，升级不会到这里） ----
  RegDeleteValue(HKCU,
    'Software\Microsoft\Windows\CurrentVersion\Run',
    'XJTUNetLogin');
  RegDeleteValue(HKCU,
    'Software\Microsoft\Windows\CurrentVersion\Explorer\StartupApproved\Run',
    'XJTUNetLogin');

  // ---- 2) 询问是否删除配置与日志 ----
  DataDir := ExpandConstant('{userappdata}\{#AppDataSubDir}');
  CfgFile := DataDir + '\config.json';
  LogDir  := DataDir + '\logs';

  if (not FileExists(CfgFile)) and (not DirExists(LogDir)) then
    Exit;

  // MB_DEFBUTTON2 → "否"为默认按钮，防手滑
  if MsgBox(
       '是否同时删除应用数据与日志？' + #13#10#13#10 +
       '  ' + CfgFile + #13#10 +
       '  ' + LogDir  + #13#10#13#10 +
       '选择"否"将保留这些文件，不影响卸载。' + #13#10 +
       '（若程序仍在运行，日志文件可能被占用而无法删除，请先退出程序再卸载。）',
       mbConfirmation, MB_YESNO or MB_DEFBUTTON2) = IDYES then
  begin
    DeleteFile(CfgFile);
    DelTree(LogDir, True, True, True);   // 含子目录 / 删只读 / 静默失败
    RemoveDir(DataDir);                  // 目录空了顺手删掉，删不掉也无所谓
  end;
end;
