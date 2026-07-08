; VapManager installer (Inno Setup 6+)
; Build: build_installer.bat

#define MyAppName "VapManager"
#define MyAppVersion "1.0.2"
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
OutputBaseFilename=setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
LicenseFile=..\docs\PRIVACY_POLICY_RU.txt
InfoBeforeFile=..\docs\INSTALL_WELCOME_RU.txt
InfoAfterFile=..\docs\INSTALL_FINISH_RU.txt
PrivilegesRequired=lowest
ArchitecturesInstallIn64BitMode=x64

[Languages]
Name: "russian"; MessagesFile: "compiler:Languages\Russian.isl"

[Messages]
BeveledLabel=Политика конфиденциальности
LicenseLabel=Ознакомьтесь с политикой конфиденциальности. Если вы согласны с условиями, нажмите «Принимаю», чтобы продолжить установку.
LicenseLabel3=Прочитайте следующую политику конфиденциальности.
WelcomeLabel1=Добро пожаловать
FinishedLabel=Завершение установки
ClickFinish=Нажмите «Готово», чтобы выйти из мастера установки.

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
  WizardForm.WelcomeLabel1.Caption := 'Установка VapManager';
  WizardForm.WelcomeLabel2.Caption :=
    'Программа для работы с AGV, задачами обслуживания, уведомлениями и командным чатом.' + #13#10 + #13#10 +
    'Следующий шаг — политика конфиденциальности.' + #13#10 +
    'Затем выберите папку установки.' + #13#10 + #13#10 +
    'После установки запустите VapManager и войдите под своим логином.' + #13#10 +
    'Никаких дополнительных настроек не требуется.';
end;
