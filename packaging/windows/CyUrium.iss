; SPDX-FileCopyrightText: 2026 Cyberalien
; SPDX-License-Identifier: GPL-3.0-only
#ifndef PackageDir
  #error PackageDir is required
#endif
#ifndef OutputPath
  #error OutputPath is required
#endif
#ifndef AppVersion
  #error AppVersion is required
#endif

[Setup]
AppId=CyUrium
AppName=CyUrium
AppVersion={#AppVersion}
AppVerName=CyUrium {#AppVersion} Alpha
AppPublisher=Cyberalien
AppPublisherURL=https://github.com/MrMybal/CyUrium
AppSupportURL=https://github.com/MrMybal/CyUrium/issues
AppUpdatesURL=https://github.com/MrMybal/CyUrium/releases
DefaultDirName={localappdata}\Programs\CyUrium
DefaultGroupName=CyUrium
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0.17763
UninstallDisplayIcon={app}\CyUrium.exe
SetupIconFile={#PackageDir}\assets\branding\urium.ico
LicenseFile={#PackageDir}\LICENSE
OutputDir={#OutputPath}
OutputBaseFilename=CyUrium-{#AppVersion}-windows-x64-setup
WizardStyle=modern dynamic
Compression=lzma2/fast
SolidCompression=yes
CloseApplications=yes
RestartApplications=no
SetupLogging=yes
UninstallLogMode=append

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"
Name: "french"; MessagesFile: "compiler:Languages\French.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
Source: "{#PackageDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\CyUrium"; Filename: "{app}\CyUrium.exe"; WorkingDir: "{app}"
Name: "{group}\{cm:UninstallProgram,CyUrium}"; Filename: "{uninstallexe}"
Name: "{userdesktop}\CyUrium"; Filename: "{app}\CyUrium.exe"; WorkingDir: "{app}"; Tasks: desktopicon

[Run]
Filename: "{app}\CyUrium.exe"; Description: "{cm:LaunchProgram,CyUrium}"; Flags: nowait postinstall skipifsilent
