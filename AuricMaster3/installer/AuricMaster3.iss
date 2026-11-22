; Auric Master 3 - Windows installer (Inno Setup 6)
; Installs the VST3 plugin into the standard folder C:\Program Files\Common Files\VST3

#define AppVersion "3.0.0"

[Setup]
AppId={{0BB54849-2DFB-4201-B534-E6BFB9ADADB1}
AppName=Auric Master 3
AppVersion={#AppVersion}
AppPublisher=AuricAudio
DefaultDirName={commoncf64}\VST3
DisableDirPage=yes
DisableProgramGroupPage=yes
UninstallFilesDir={autopf}\Auric Master 3
UninstallDisplayName=Auric Master 3 (VST3)
OutputDir=..\..\installer-out
OutputBaseFilename=AuricMaster3-Setup-Windows
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
Compression=lzma2
SolidCompression=yes
WizardStyle=modern

[Languages]
Name: "german"; MessagesFile: "compiler:Languages\German.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[Files]
Source: "..\..\build-am3\AuricMaster3_artefacts\Release\VST3\Auric Master 3.vst3\*"; DestDir: "{commoncf64}\VST3\Auric Master 3.vst3"; Flags: recursesubdirs createallsubdirs ignoreversion

[Messages]
german.FinishedLabel=Auric Master 3 wurde installiert.%n%nFL Studio: Options > Manage plugins > Find installed plugins.
english.FinishedLabel=Auric Master 3 has been installed.%n%nFL Studio: Options > Manage plugins > Find installed plugins.
