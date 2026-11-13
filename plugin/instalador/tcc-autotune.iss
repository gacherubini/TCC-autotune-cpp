; ============================================================================
;  tcc-autotune.iss — instalador do plugin para Windows (Inno Setup 6)
;
;  O que faz: copia a pasta "TCC Autotune.vst3" inteira para a pasta VST3 do
;  sistema (C:\Program Files\Common Files\VST3), onde todo DAW procura plugins
;  VST3, e registra um desinstalador. É o mesmo destino do instalar_vst3.bat:
;  quem já rodou o .bat não fica com o plugin duplicado — o instalador grava
;  por cima da mesma pasta. Só o VST3 entra; o Standalone fica de fora.
;
;  Como compilar (o workflow .github/workflows/release.yml faz isto sozinho):
;     iscc /DAppVersion=1.0.0 ^
;          /DVst3Dir="..\build\TccAutotune_artefacts\Release\VST3\TCC Autotune.vst3" ^
;          tcc-autotune.iss
;  Saída: Output\TCC-Autotune-Setup-Windows.exe (ao lado deste arquivo).
;
;  As duas variáveis chegam de fora, por /D, para o mesmo .iss servir a
;  qualquer versão e a qualquer pasta de build:
;     AppVersion — a versão (a tag sem o "v"; "0.0.0-pr" fora de tag);
;     Vst3Dir    — a pasta "TCC Autotune.vst3" compilada.
; ============================================================================

#ifndef AppVersion
  #error "Passe a versao com /DAppVersion=1.0.0"
#endif
#ifndef Vst3Dir
  #error "Passe a pasta do .vst3 com /DVst3Dir=..."
#endif

[Setup]
; AppId: nunca mude: é o que liga reinstalação e desinstalação. O Windows
; reconhece o programa instalado por este GUID; trocar o valor faria uma versão
; nova aparecer como outro programa, com dois desinstaladores. As chaves duplas
; "{{" são o escape do Inno Setup para a chave literal do GUID.
AppId={{3A3652D4-FEED-48E3-9055-290BC2A52E5B}
AppName=TCC Autotune
AppVersion={#AppVersion}
AppPublisher=Gabriel Cherubini
AppPublisherURL=https://tcc-autotune.fly.dev/
AppSupportURL=https://github.com/gacherubini/TCC-autotune-cpp
; Gravar em Program Files\Common Files exige administrador.
PrivilegesRequired=admin
; O plugin é só x64; em modo 64 bits {commoncf64} aponta para a pasta certa.
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
; O destino é fixo (a pasta VST3 do sistema): não faz sentido perguntar.
; DefaultDirName é exigido pelo Inno, e é lá que fica o desinstalador.
DefaultDirName={commonpf64}\TCC Autotune
DisableDirPage=yes
DisableProgramGroupPage=yes
OutputBaseFilename=TCC-Autotune-Setup-Windows
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
UninstallDisplayName=TCC Autotune (VST3)

[Languages]
Name: "brazilianportuguese"; MessagesFile: "compiler:Languages\BrazilianPortuguese.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[Files]
; A pasta .vst3 é um "bundle": a DLL fica em Contents\x86_64-win e há também
; Contents\Resources (moduleinfo.json). Por isso vai a pasta inteira, não só a
; DLL. ignoreversion: sempre sobrescreve, mesmo reinstalando a mesma versão.
Source: "{#Vst3Dir}\*"; DestDir: "{commoncf64}\VST3\TCC Autotune.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs

[UninstallDelete]
; Remove a pasta inteira, inclusive arquivos que não vieram do instalador
; (por exemplo, sobras de uma cópia antiga feita pelo instalar_vst3.bat).
Type: filesandordirs; Name: "{commoncf64}\VST3\TCC Autotune.vst3"
