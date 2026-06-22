; VapManager installer (Inno Setup 6+)
; Build: build_installer.bat

#define MyAppName "VapManager"
#define MyAppVersion "1.0.0"
#define MyAppPublisher "VapManager"
#define MyAppExeName "VapManager.exe"
#define MyDistDir "..\dist\VapManager"

[Setup]
AppId={{A7E4B2C1-9F3D-4E8A-B6C5-1D2E3F4A5B6C}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
DefaultDirName={autopf}\{#MyAppName}
DefaultGroupName={#MyAppName}
DisableProgramGroupPage=yes
OutputDir=..\dist
OutputBaseFilename=VapManager-Setup-{#MyAppVersion}
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
LicenseFile=..\docs\PRIVACY_POLICY_RU.txt
InfoBeforeFile=..\docs\INSTALL_WELCOME_RU.txt
PrivilegesRequired=lowest
ArchitecturesInstallIn64BitMode=x64

[Languages]
Name: "russian"; MessagesFile: "compiler:Languages\Russian.isl"

[Tasks]
Name: "desktopicon"; Description: "Создать значок на рабочем столе"; GroupDescription: "Дополнительно:"; Flags: unchecked

[Files]
Source: "{#MyDistDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "Запустить {#MyAppName}"; Flags: nowait postinstall skipifsilent

[Code]
procedure InitializeWizard;
begin
  WizardForm.WelcomeLabel2.Caption :=
    'Мастер установит VapManager на ваш компьютер.' + #13#10 + #13#10 +
    'На следующем шаге ознакомьтесь с политикой конфиденциальности.' + #13#10 +
    'Затем выберите папку установки.' + #13#10 + #13#10 +
    'Для работы нужен доступ к серверу PostgreSQL (настройки в config.ini).';
end;
