; Inno Setup script for ClipKeeper (Qt build).
;
; Per-user install (PrivilegesRequired=lowest): no admin prompt, installs to
; %LOCALAPPDATA%\Programs\ClipKeeperQt. Adapted from the Python app's installer.iss
; (which is stack-agnostic). A DISTINCT AppId + install dir from the Python build,
; so it installs safely alongside it for testing. For the true drop-in, build with
; -DCLIPKEEPER_DROP_IN=ON and point DefaultDirName/AppId at the Python identity.

#define AppName "ClipKeeper"
#define AppVersion "1.0.0"
#define AppExe "ClipKeeper.exe"

[Setup]
AppId={{C7F3A2E9-1B4D-4A6E-9C2F-3E8D1A0B7C64}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher={#AppName}
DefaultDirName={localappdata}\Programs\ClipKeeperQt
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
OutputDir=dist
OutputBaseFilename=ClipKeeper-Setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
SetupIconFile=assets\clipkeeper.ico
UninstallDisplayIcon={app}\{#AppExe}
; An older copy may be running in the tray and locking its files.
CloseApplications=yes
RestartApplications=no

[Tasks]
Name: "startup"; Description: "Start {#AppName} automatically when I sign in"; GroupDescription: "Startup:"
Name: "desktopicon"; Description: "Create a desktop shortcut"; Flags: unchecked

[Files]
; Everything windeployqt staged (exe + Qt DLLs + plugins + VC runtime).
Source: "dist\ClipKeeper\*"; DestDir: "{app}"; Flags: recursesubdirs createallsubdirs ignoreversion

[Icons]
Name: "{autoprograms}\{#AppName}"; Filename: "{app}\{#AppExe}"
Name: "{autodesktop}\{#AppName}"; Filename: "{app}\{#AppExe}"; Tasks: desktopicon

[Run]
; --enable-autostart just writes the per-user Run key and exits; it does not
; launch the app, so it can't collide with the "launch now" step below.
Filename: "{app}\{#AppExe}"; Parameters: "--enable-autostart"; Tasks: startup; Flags: runhidden
Filename: "{app}\{#AppExe}"; Description: "Launch {#AppName} now"; Flags: nowait postinstall skipifsilent

[UninstallRun]
; Tidy the Run key so we don't leave a dangling startup entry behind.
Filename: "{app}\{#AppExe}"; Parameters: "--disable-autostart"; Flags: runhidden; RunOnceId: "DisableAutostart"
