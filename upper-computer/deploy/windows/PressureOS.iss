#define MyAppName "PressureOS"
#ifndef MyAppVersion
  #define MyAppVersion "1.0.10"
#endif
#ifndef SourceDir
  #define SourceDir "D:\PressureOS-package-1.0.10-final\PressureOS_Portable_1.0.10_x64"
#endif
#ifndef OutputDir
  #define OutputDir "D:\PressureOS-package-1.0.10-final\installer"
#endif

[Setup]
AppId={{85A64EB8-C58C-4217-9526-7B7704F1C3EC}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppVerName={#MyAppName} {#MyAppVersion}
AppPublisher=PressureOS 项目组
AppCopyright=Copyright (C) 2026 PressureOS 项目组
DefaultDirName={localappdata}\Programs\PressureOS
DefaultGroupName=PressureOS
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputDir={#OutputDir}
OutputBaseFilename=PressureOS_Setup_{#MyAppVersion}_x64
SetupIconFile=PressureOS.ico
UninstallDisplayIcon={app}\PressureOS.exe
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
DisableWelcomePage=no
CloseApplications=yes
RestartApplications=no
DirExistsWarning=no
UsePreviousAppDir=yes
InfoBeforeFile={#SourceDir}\使用说明.txt
VersionInfoVersion={#MyAppVersion}.0
VersionInfoCompany=PressureOS 项目组
VersionInfoDescription=PressureOS 智能精密压力测量系统安装程序
VersionInfoProductName=PressureOS
VersionInfoProductVersion={#MyAppVersion}
VersionInfoCopyright=Copyright (C) 2026 PressureOS 项目组

[Languages]
Name: "chinesesimp"; MessagesFile: "ChineseSimplified.isl"

[Tasks]
Name: "desktopicon"; Description: "创建桌面快捷方式"; GroupDescription: "附加快捷方式："

[Files]
Source: "{#SourceDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{autoprograms}\PressureOS"; Filename: "{app}\PressureOS.exe"; WorkingDir: "{app}"
Name: "{autodesktop}\PressureOS"; Filename: "{app}\PressureOS.exe"; WorkingDir: "{app}"; Tasks: desktopicon

[Run]
Filename: "{app}\PressureOS.exe"; Description: "启动 PressureOS"; WorkingDir: "{app}"; Flags: nowait postinstall skipifsilent
