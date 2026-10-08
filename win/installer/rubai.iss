; Kotib — Windows oʻrnatuvchisi (Inno Setup 6).
;
; Bitta skriptdan uch xil fayl chiqadi:
;
;   Kotib-<v>-win-setup.exe        nutq modeli ICHIDA (`/DBundleModel=…`) —
;                                  birinchi oʻrnatish, saytdagi fayl
;   Kotib-<v>-win-yangilash.exe    MODELSIZ (`/DYangilash`) — avto-yangilanish
;                                  paketi (~30–45 MB), model allaqachon diskda
;   Kotib-<v>-win-onlayn-setup.exe modelsiz, modelni oʻzi yuklaydi — faqat
;                                  ishlab chiqish uchun
;
; x64 va ARM64 binarlari ikkalasi ham ichida (`Check: IsArm64`). Tarjima
; modeli ilovaning oʻzida, kerak boʻlganda yuklab olinadi.
;
; PER-USER (1.2, S7): administrator huquqi soʻralmaydi, ilova
; `%LOCALAPPDATA%\Programs\Kotib` ga, nutq modeli `%LOCALAPPDATA%\Kotib\models`
; ga tushadi. Shu sababli avto-yangilanish UAC oynasisiz, jim oʻtadi (VS Code
; user setup, Slack, Telegram — hammasi shu yoʻl bilan). 1.1.0 gacha
; oʻrnatma per-machine edi (`C:\Program Files\Kotib`) — uni koʻchirish
; pastdagi «1.1.0 DAN KOʻCHISH» boʻlimida.
;
; Build: `win/tools/ornatuvchi-yasa.sh` (iscc faqat Windows'da — VM ichida).

#define AppName        "Kotib"
; Versiya bu yerda YOZILMAYDI — yagona manba repo ildizidagi `VERSION`.
; Uni yigʻuvchi skript beradi: `iscc /DAppVersion=<versiya> …`
; (`win/tools/ornatuvchi-yasa.sh`, `win/build.ps1`). Busiz yigʻilsa oʻrnatuvchi
; notoʻgʻri versiya bilan chiqardi — shuning uchun toʻxtatamiz.
#ifndef AppVersion
  #error AppVersion berilmagan: iscc /DAppVersion=<VERSION faylidagi qiymat> (win/tools/ornatuvchi-yasa.sh shuni qiladi)
#endif
#if defined(BundleModel) && defined(Yangilash)
  #error BundleModel va Yangilash birga boʻlmaydi: yangilanish paketi modelsiz
#endif
#define AppPublisher   "Muhammad Mirkabilov"
#define AppExe         "Kotib.exe"
#define AppUrl         "https://github.com/MuhammadMirrr/kotib"
#define ModelUrl       "https://cdn.mirqobilov.com/v1.0/ggml-rubaistt.bin"
#define ModelSha256    "1b02df434902015e1464611a7748927e42fcb55c49791c85239cf713c8edc1a3"
#define ModelSize      823369796
; Ilovaning yagona nusxa mutex'i (`win/ui/app.h` → kMutexName).
#define AppMutex       "Global\KotibSingleInstance"
; 1.1.0 gacha per-machine oʻrnatmaning (va 1.2 dagi per-user oʻrnatmaning
; ham) AppId'si. Bitta ID: «Ilovalar» roʻyxatida bitta Kotib turadi.
#define AppIdStr       "{9C4D7B02-5E18-4A73-8F26-1D3B6E9A0C55}"

; Tayyorlangan binar papkalari (`win/tools/ornatuvchi-yasa.sh` beradi).
#ifndef SrcX64
  #define SrcX64 "..\build-x64"
#endif
#ifndef SrcArm64
  #define SrcArm64 "..\build-arm64"
#endif

[Setup]
; AppId 1.1.0 dagi bilan bir xil. Per-user rejimda Inno oʻz yozuvini HKCU
; ga qoʻyadi, eski per-machine yozuv (HKLM) esa koʻchishda oʻchiriladi —
; natijada «Ilovalar» da bitta yozuv qoladi.
AppId={{9C4D7B02-5E18-4A73-8F26-1D3B6E9A0C55}
AppName={#AppName}
AppVersion={#AppVersion}
; Busiz Windows "Ilovalar" ro'yxatida "... version 1.0.0" deb ko'rsatadi.
AppVerName={#AppName} {#AppVersion}
AppPublisher={#AppPublisher}
AppPublisherURL={#AppUrl}
AppSupportURL={#AppUrl}
; Foydalanish shartlari va maxfiylik siyosati — foydalanuvchi «Rozi boʻlaman»
; bosmaguncha oʻrnatish davom etmaydi. Anonim statistika uchun ochiq rozilik
; shu yerda olinadi (toggle yoʻq, shuning uchun bu eʼlon SHART).
LicenseFile=shartlar.txt
; Per-user rejimda {autopf} = %LOCALAPPDATA%\Programs.
DefaultDirName={autopf}\Kotib
; Papka tanlanmaydi: avto-yangilanish va koʻchish shu yoʻlga tayanadi.
; `UsePreviousAppDir=no` — eski per-machine yozuvdagi `C:\Program Files\Kotib`
; qayta tanlanib, yozib boʻlmaydigan joyga oʻrnatishga urinmasin.
DisableDirPage=yes
UsePreviousAppDir=no
DefaultGroupName={#AppName}
DisableProgramGroupPage=yes
UninstallDisplayIcon={app}\{#AppExe}
OutputDir=..\..\dist
#if defined(BundleModel)
OutputBaseFilename=Kotib-{#AppVersion}-win-setup
#elif defined(Yangilash)
OutputBaseFilename=Kotib-{#AppVersion}-win-yangilash
#else
; Modelsiz, oʻzi yuklaydigan variant faqat ishlab chiqish uchun —
; foydalanuvchiga tarqatiladigani modelli, yagona fayl.
OutputBaseFilename=Kotib-{#AppVersion}-win-onlayn-setup
#endif
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern

; 64-bitli ilova; 32-bitli Windows'da oʻrnatilmaydi.
; `x64compatible` ARM64 Windows'ni ham qamraydi — u x64 ni emulyatsiya
; qila oladi. Qaysi binar qoʻyilishini esa [Files] dagi `IsArm64` hal
; qiladi, shuning uchun ARM mashinada TABIIY nusxa oʻrnatiladi.
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible

; Windows 10 1809 (build 17763) dan pastda ishlamaydi — WASAPI va DWM
; funksiyalari yetishmaydi.
MinVersion=10.0.17763

; Per-user: administrator huquqi soʻralmaydi (S7). Faqat 1.1.0 dagi
; per-machine nusxani olib tashlash uchun BIR MARTA UAC soʻraladi.
PrivilegesRequired=lowest

; Ishlab turgan Kotib'ni Restart Manager yopadi; qayta ochishni oʻzimiz
; qilamiz (CurStepChanged → ssDone), chunki RM uni qaytarmaydi.
CloseApplications=yes
RestartApplications=no

[Languages]
; Inno Setup rasmiy oʻzbekcha tarjima bilan kelmaydi — u yonimizda turadi
; (`Uzbek.isl`, UTF-8 + BOM). Ilgari bu yerda `compiler:Default.isl` edi va
; butun sehrgar ingliz tilida chiqardi.
Name: "uz"; MessagesFile: "Uzbek.isl"

[CustomMessages]
; `CreateDesktopIcon` va `AutoStartProgram` `Uzbek.isl` dan keladi.
uz.LaunchApp={#AppName} ni ishga tushirish
uz.DownloadingModel=Til modeli yuklab olinmoqda (785 MB)...
uz.ModelFailed=Til modelini yuklab olib boʻlmadi (3 marta urinildi).%n%nBu odatda internet ulanishi uzilganini bildiradi — 785 MB fayl uchun barqaror ulanish kerak.%n%nQOʻLDA YUKLAB OLISH (tavsiya etiladi):%n%n1. Quyidagi havolani brauzerda oching yoki yuklab olish menejeriga bering:%n   {#ModelUrl}%n%n2. Yuklangan «ggml-rubaistt.bin» faylini shu oʻrnatuvchi (setup.exe) turgan papkaga qoʻying%n%n3. Oʻrnatuvchini qaytadan ishga tushiring — u faylni oʻzi topadi va internetsiz oʻrnatadi%n%nTexnik xato: %1
uz.ModelCopyFailed=Til modeli yangi joyga koʻchirilmadi. Diskda joy yetarlimi?%n%n%1
uz.FileBusy=Yangilanish oʻrnatilmadi: «%1» fayli band. Kotib oʻzgartirilmadi.
uz.OldUninstallFailed=Kotib endi faqat sizning profilingizga oʻrnatildi.%n%nOldingi versiya («C:\Program Files\Kotib») olib tashlanmadi — administrator ruxsati berilmadi. Uni keyinroq «Sozlamalar → Ilovalar» dan oʻchirib qoʻyishingiz mumkin; yangi Kotib bunga bogʻliq emas.
uz.UninstallModelsSavol=Til modellari saqlab qolinsinmi?
uz.UninstallModels=Nutq modeli va yuklab olingan tarjima modeli ~4 GB joy egallaydi. Saqlab qolsangiz, Kotibni qayta oʻrnatganda ularni qayta yuklash shart boʻlmaydi.
uz.UninstallModelsHa=Ha, saqlansin
uz.UninstallModelsYoq=Yoʻq, oʻchirilsin

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "Qoʻshimcha:"

; Avtostart bu yerda EMAS — ilovaning oʻzi birinchi ishga tushganda yoqadi
; (`win/ui/autostart.cpp`). Koʻchishda esa mavjud yozuv yangi yoʻlga
; oʻtkaziladi (CurStepChanged → ssPostInstall).

[Files]
; IKKALA ARXITEKTURA BITTA OʻRNATUVCHIDA.
;
; Windows'da macOS'dagi «universal binary» yoʻq, shuning uchun ikkala
; toʻplam ham ichkariga solinadi va oʻrnatish paytida MOSI tanlanadi.
; Narxi kichik — ARM64 toʻplami siqilganda ~10 MB, model esa 785 MB —
; foydasi katta: foydalanuvchi qaysi protsessori borligini bilishi shart
; emas, yagona fayl bor.
;
; Manba papkalari TAYYORLANGAN boʻladi (`win/tools/ornatuvchi-yasa.sh`):
; ular ichida faqat tarqatiladigan fayllar turadi — `Kotib.exe`, DLL'lar va
; VAD modeli. Shu sababli nomma-nom sanash kerak emas va MSVC/llvm-mingw
; farqi (`whisper.dll` yoki `libwhisper.dll`, `ggml-cpu-*.dll` yoki
; `ggml-cpu.dll`) oʻz-oʻzidan hal boʻladi.
Source: "{#SrcX64}\*";   DestDir: "{app}"; Flags: ignoreversion; Check: not IsArm64
Source: "{#SrcArm64}\*"; DestDir: "{app}"; Flags: ignoreversion; Check: IsArm64

Source: "..\..\LICENSE";                   DestDir: "{app}"; DestName: "LICENSE.txt"; Flags: ignoreversion

; NUTQ MODELI OʻRNATUVCHI ICHIDA
;
; macOS'dagi `.pkg` bilan bir xil qoida: bitta fayl yuklab olinadi va
; ilova darhol ishlaydi. Oʻzbekistonda oʻrnatish paytida 785 MB yuklash
; koʻpincha uzilib qolardi.
;
; Joyi — `%LOCALAPPDATA%\Kotib\models`, ilova papkasida EMAS: yangilanish
; paketi ilova papkasini almashtiradi, model esa unga tegmasdan qoladi.
; Allaqachon toʻgʻri hajmda turgan boʻlsa (qayta oʻrnatish, koʻchish) —
; 785 MB qayta yozilmaydi.
;
; `nocompression`: model allaqachon kvantlangan (q8_0) va siqilmaydi —
; LZMA2/max unga soatlab CPU sarflab, hajmni deyarli qisqartirmaydi.
#ifdef BundleModel
Source: "{#BundleModel}"; DestDir: "{localappdata}\Kotib\models"; \
    DestName: "ggml-rubaistt.bin"; Flags: ignoreversion nocompression uninsneveruninstall; \
    Check: not ModelAlreadyInstalled
#endif

[Icons]
Name: "{group}\{#AppName}";                Filename: "{app}\{#AppExe}"
Name: "{group}\{#AppName} ni oʻchirish";   Filename: "{uninstallexe}"
Name: "{autodesktop}\{#AppName}";          Filename: "{app}\{#AppExe}"; Tasks: desktopicon

[Run]
; Oynali oʻrnatish oxiridagi «ishga tushirish» katagi. Kotib avval ochiq
; boʻlgan boʻlsa (yoki avto-yangilanish) uni CurStepChanged oʻzi qayta
; ochadi — katak koʻrsatilmaydi, aks holda ikkinchi nusxa Sozlamalarni ochardi.
Filename: "{app}\{#AppExe}"; Description: "{cm:LaunchApp}"; \
    Flags: nowait postinstall skipifsilent; \
    Check: AppWasNotRunning

[UninstallDelete]
; Avto-yangilanish yuklagan oʻrnatuvchilar va ularning loglari.
Type: filesandordirs; Name: "{localappdata}\Kotib\yangilanish"

[Code]
//
// MODEL QAYERDAN OLINADI (faqat `onlayn` variantda; `setup` da model ichida,
// `yangilash` da esa umuman tegilmaydi)
//
// Model 785 MB. GitHub'dan yuklab olish O'zbekistonda ishonchsiz — provayder,
// proksi yoki antivirus TLS ulanishini uzib qo'yishi mumkin (WinHTTP 12152:
// "Server javobi buzilgan"). Shuning uchun manbalar ketma-ket tekshiriladi:
//
//   1. Allaqachon o'rnatilgan  — %LOCALAPPDATA%\Kotib\models
//   2. 1.1.0 per-machine nusxa — C:\Program Files\Kotib\models (koʻchish)
//   3. setup.exe YONIDA        — foydalanuvchi modelni brauzer, yuklab olish
//                                menejeri, Telegram yoki fleshka orqali olib,
//                                setup.exe yoniga qo'yishi mumkin
//   4. Internetdan yuklash     — 3 marta urinib ko'riladi
//
// 1.1.0 DAN KOʻCHISH
//
// 1.1.0 gacha Kotib per-machine edi (HKLM yozuvi, `C:\Program Files\Kotib`).
// Per-user oʻrnatuvchi u yerga yoza olmaydi, shuning uchun:
//   a) model eski papkadan yangi joyga NUSXALANADI (eski uninstaller uni
//      oʻchiradi — nusxa undan OLDIN);
//   b) eski Kotib yopiladi va eski uninstaller BIR MARTA UAC bilan jim
//      ishga tushiriladi (rad etilsa ham yangi Kotib ishlaydi, faqat xabar);
//   c) HKCU Run dagi avtostart yozuvi yangi yoʻlga oʻtkaziladi;
//   d) Kotib ochiq boʻlgan boʻlsa — yangisi ochiladi.
// Hammasi ssPostInstall da, yangi fayllar joyida turganda: biror qadam
// yiqilsa ham foydalanuvchi ishlaydigan Kotib bilan qoladi.

var
  DownloadPage: TDownloadWizardPage;
  ModelSource: String;      // ko'chirish uchun manba; bo'sh = ko'chirish shart emas
  AppWasRunning: Boolean;   // o'rnatishdan oldin ilova ochiq edimi
  KotibYangilash: Boolean;  // `/KOTIBYANGILASH` — avto-yangilovchi ishga tushirdi
  ModellarniOchir: Boolean; // uninstall: til modellari ham oʻchirilsinmi
  KotibOchildi: Boolean;    // ssDone da yangi Kotib ishga tushirildi

// ---- Fayl band emasligini tekshirish (faqat avto-yangilanishda)
//
// Inno yiqilgan oʻrnatishni orqaga qaytarganda ALMASHTIRILGAN fayllarni
// tiklamaydi (faqat yangi qoʻshilganlarini oʻchiradi). Masalan `ggml.dll`
// band boʻlsa, undan oldingi `ggml-base.dll`, `ggml-cpu*.dll` allaqachon yangi —
// ilova aralash DLL'lar bilan ishga tushmay qolardi (VM'da sinab koʻrilgan).
// Shuning uchun HECH NARSAGA tegmasdan oldin har bir faylni yozish+oʻchirish
// huquqi bilan, boʻlishmasdan ochib koʻramiz.

const
  GENERIC_WRITE = $40000000;
  DELETE_HUQUQI = $00010000;
  OPEN_EXISTING = 3;
  NOTOGRI_DASTAK = -1;

// Setup jarayoni 32-bit — HANDLE 32 bitga sigʻadi.
function CreateFileW(Nom: String; Huquq, Ulashish, Xavfsizlik, Yaratish, Bayroq,
                     Andoza: Cardinal): Longint;
  external 'CreateFileW@kernel32.dll stdcall';
function CloseHandle(Dastak: Longint): Boolean;
  external 'CloseHandle@kernel32.dll stdcall';

// {app} dagi birinchi band fayl nomi, hammasi boʻsh boʻlsa — boʻsh satr.
function BandFayl(): String;
var
  Qidiruv: TFindRec;
  Papka: String;
  Dastak: Longint;
begin
  Result := '';
  Papka := ExpandConstant('{app}');
  if not FindFirst(Papka + '\*', Qidiruv) then
    Exit;
  try
    repeat
      if (Qidiruv.Attributes and FILE_ATTRIBUTE_DIRECTORY) <> 0 then
        Continue;
      Dastak := CreateFileW(Papka + '\' + Qidiruv.Name, GENERIC_WRITE or DELETE_HUQUQI,
                            0, 0, OPEN_EXISTING, 0, 0);
      if Dastak = NOTOGRI_DASTAK then
      begin
        Result := Qidiruv.Name;
        Exit;
      end;
      CloseHandle(Dastak);
    until not FindNext(Qidiruv);
  finally
    FindClose(Qidiruv);
  end;
end;

// ---- Umumiy yordamchilar

function KotibOchiqmi(): Boolean;
begin
  Result := CheckForMutexes('{#AppMutex}');
end;

// Kotib yopilishini `Soniya` gacha kutadi. Yopildi — True.
function KotibYopilishiniKut(Soniya: Integer): Boolean;
var
  I: Integer;
begin
  for I := 1 to Soniya * 4 do
  begin
    if not KotibOchiqmi() then
    begin
      Result := True;
      Exit;
    end;
    Sleep(250);
  end;
  Result := not KotibOchiqmi();
end;

// Ochiq Kotib'ni yopadi: avval yumshoq (`taskkill` WM_CLOSE yuboradi), 5 s
// ichida yopilmasa — majburan. Restart Manager faqat {app} dagi fayllarni
// ushlab turgan jarayonni yopadi; 1.1.0 nusxasi esa boshqa papkada.
procedure KotibniYop();
var
  ResultCode: Integer;
begin
  if not KotibOchiqmi() then
    Exit;
  Exec(ExpandConstant('{sys}\taskkill.exe'), '/im {#AppExe}', '', SW_HIDE,
       ewWaitUntilTerminated, ResultCode);
  if KotibYopilishiniKut(5) then
    Exit;
  Exec(ExpandConstant('{sys}\taskkill.exe'), '/f /im {#AppExe}', '', SW_HIDE,
       ewWaitUntilTerminated, ResultCode);
  KotibYopilishiniKut(5);
end;

function ParametrBormi(const Nom: String): Boolean;
var
  I: Integer;
begin
  Result := False;
  for I := 1 to ParamCount do
    if CompareText(ParamStr(I), Nom) = 0 then
    begin
      Result := True;
      Exit;
    end;
end;

function YangiModelYoli(): String;
begin
  Result := ExpandConstant('{localappdata}\Kotib\models\ggml-rubaistt.bin');
end;

// Fayl bor va hajmi to'g'rimi (chala yuklangan fayl o'tib ketmasligi uchun).
function ModelFileValid(const Path: String): Boolean;
var
  Size: Int64;
begin
  Result := False;
  if not FileExists(Path) then
    Exit;
  if not FileSize64(Path, Size) then
    Exit;
  Result := (Size = {#ModelSize});
end;

// [Files] Check: ham shu — model joyida boʻlsa qayta yozilmaydi.
function ModelAlreadyInstalled(): Boolean;
begin
  Result := ModelFileValid(YangiModelYoli());
end;

// ---- 1.1.0 per-machine nusxa

// Uninstall yozuvining kaliti — AppId bitta, shuning uchun eski per-machine
// (HKLM, 64-bit koʻrinish: 1.1.0 ham 64-bit rejimda) va yangi per-user (HKCU)
// yozuvlar bir xil yoʻlda.
function UninstallKalit(): String;
begin
  Result := 'Software\Microsoft\Windows\CurrentVersion\Uninstall\{#AppIdStr}_is1';
end;

// Eski nusxaning papkasi (oxirida «\»), yoʻq boʻlsa boʻsh satr.
function EskiPapka(): String;
begin
  Result := '';
  if not RegQueryStringValue(HKLM64, UninstallKalit(), 'InstallLocation', Result) then
    Result := ''
  else if Result <> '' then
    Result := AddBackslash(Result);
end;

function EskiUninstaller(): String;
begin
  Result := '';
  if RegQueryStringValue(HKLM64, UninstallKalit(), 'UninstallString', Result) then
    Result := RemoveQuotes(Result);
  if (Result <> '') and not FileExists(Result) then
    Result := '';
end;

// Eski per-machine nusxani olib tashlaydi (BIR MARTA UAC). Jim rejimda ham
// UAC oynasi chiqadi — bu faqat 1.1.0 dan oʻtishda, bir marta.
// Muvaffaqiyatsiz (rad etildi) — False.
function EskiNusxaniOlibTashla(): Boolean;
var
  Cmd: String;
  ResultCode: Integer;
begin
  Result := True;
  Cmd := EskiUninstaller();
  if Cmd = '' then
    Exit;
  Log('1.1.0 per-machine nusxa topildi: ' + Cmd);
  KotibniYop();
  // `runas` — administrator huquqi bilan. Rad etilsa ShellExec False qaytaradi.
  if not ShellExec('runas', Cmd, '/VERYSILENT /SUPPRESSMSGBOXES /NORESTART', '',
                   SW_HIDE, ewWaitUntilTerminated, ResultCode) then
  begin
    Log('eski uninstaller ishga tushmadi: ' + SysErrorMessage(ResultCode));
    Result := False;
    Exit;
  end;
  Log('eski uninstaller tugadi, kod ' + IntToStr(ResultCode));
  Result := (EskiUninstaller() = '');
end;

// Juda eski nomdagi oʻrnatmalar ("RubaiSTT" 0.x, "Audio-Matnga" 1.0). Ular
// ham per-machine edi; HKCU dagilari esa huquqsiz oʻchiriladi.
procedure JudaEskilarniOlibTashla();
var
  Idlar: array[0..1] of String;
  Kalit, Cmd: String;
  ResultCode, I: Integer;
  Mashina: Boolean;
begin
  Idlar[0] := '{7F3A9C21-4E56-4B8D-9A17-2C6E5D8B1A40}_is1';
  Idlar[1] := '{2D8B5E14-7A93-4C61-B0F2-8E5D3A6C9147}_is1';
  for I := 0 to 1 do
  begin
    Kalit := 'Software\Microsoft\Windows\CurrentVersion\Uninstall\' + Idlar[I];
    Cmd := '';
    Mashina := RegQueryStringValue(HKLM64, Kalit, 'UninstallString', Cmd);
    if not Mashina then
      if not RegQueryStringValue(HKCU, Kalit, 'UninstallString', Cmd) then
        Continue;
    Cmd := RemoveQuotes(Cmd);
    if not FileExists(Cmd) then
      Continue;
    if Mashina then
      ShellExec('runas', Cmd, '/VERYSILENT /SUPPRESSMSGBOXES /NORESTART', '',
                SW_HIDE, ewWaitUntilTerminated, ResultCode)
    else
      Exec(Cmd, '/VERYSILENT /SUPPRESSMSGBOXES /NORESTART', '',
           SW_HIDE, ewWaitUntilTerminated, ResultCode);
  end;
end;

// HKCU Run dagi `Kotib` yozuvi boʻlsa — yangi yoʻlga. Yozuv yoʻq boʻlsa
// (foydalanuvchi avtostartni oʻchirgan) — tegmaymiz.
procedure AvtostartniKochir();
var
  RunKalit, Qiymat: String;
begin
  RunKalit := 'Software\Microsoft\Windows\CurrentVersion\Run';
  if not RegQueryStringValue(HKCU, RunKalit, '{#AppName}', Qiymat) then
    Exit;
  RegWriteStringValue(HKCU, RunKalit, '{#AppName}',
                      '"' + ExpandConstant('{app}\{#AppExe}') + '"');
end;

// ---- Sehrgar

function InitializeSetup(): Boolean;
begin
  Result := True;
  KotibYangilash := ParametrBormi('/KOTIBYANGILASH');
  AppWasRunning := KotibOchiqmi() or
                   CheckForMutexes('Global\AudioMatngaSingleInstance') or
                   CheckForMutexes('Global\RubaiSTTDictationSingleInstance');
  // Avto-yangilovchi oʻrnatuvchini ishga tushirib, oʻzi yopiladi — uning
  // chiqishini kutamiz, aks holda Restart Manager uni majburan yopardi.
  // Har holda yangilanishdan keyin Kotib qayta ochiladi.
  if KotibYangilash then
  begin
    AppWasRunning := True;
    if not KotibYopilishiniKut(30) then
      KotibniYop();
  end;
end;

// [Run] bo'limi uchun: ilova avval ishlamayotgan bo'lsagina "ishga tushirish"
// katagi ko'rsatiladi.
function AppWasNotRunning(): Boolean;
begin
  Result := not AppWasRunning;
end;

function OnDownloadProgress(const Url, FileName: String; const Progress, ProgressMax: Int64): Boolean;
begin
  if ProgressMax <> 0 then
    DownloadPage.SetProgress(Progress, ProgressMax);
  Result := True;
end;

// setup.exe yonidagi model.
function LocalModelPath(): String;
begin
  Result := ExpandConstant('{src}\ggml-rubaistt.bin');
end;

// Internetdan yuklash, N marta urinib. Muvaffaqiyatda True.
//
// Uzilish ko'pincha vaqtinchalik bo'ladi, shuning uchun bitta xatodan keyin
// darhol taslim bo'lmaymiz.
function TryDownload(Attempts: Integer; var ErrText: String): Boolean;
var
  I: Integer;
begin
  Result := False;
  ErrText := '';
  for I := 1 to Attempts do
  begin
    try
      if WizardSilent() then
        DownloadTemporaryFile('{#ModelUrl}', 'ggml-rubaistt.bin', '{#ModelSha256}', nil)
      else
      begin
        DownloadPage.Clear;
        DownloadPage.Add('{#ModelUrl}', 'ggml-rubaistt.bin', '{#ModelSha256}');
        DownloadPage.Download;
      end;
      Result := True;
      Exit;
    except
      ErrText := GetExceptionMessage;
    end;
  end;
end;

// Modelni tayyorlaydi. Bo'sh satr = muvaffaqiyat, aks holda xato matni.
// `ModelSource` boʻsh boʻlmasa — ssPostInstall uni yangi joyga nusxalaydi.
function AcquireModel(): String;
var
  ErrText, Eski: String;
begin
  Result := '';
  ModelSource := '';

  if ModelAlreadyInstalled() then
    Exit;

  // 1.1.0 dan koʻchish: model eski papkada — 785 MB qayta yuklanmaydi,
  // ssPostInstall uni yangi joyga nusxalaydi. `setup` variantida esa model
  // ichida va [Files] uni oʻzi joylaydi — eski nusxaga tayanmaymiz.
  Eski := EskiPapka();
#ifndef BundleModel
  if (Eski <> '') and ModelFileValid(Eski + 'models\ggml-rubaistt.bin') then
  begin
    ModelSource := Eski + 'models\ggml-rubaistt.bin';
    Exit;
  end;
#endif

#if defined(BundleModel) || defined(Yangilash)
  // `setup`: model ichida, [Files] uni joylaydi. `yangilash`: model 1.2 dan
  // beri diskda; u yoʻq boʻlsa ilova oʻzi «Model fayli topilmadi» deydi —
  // 785 MB ni jim yangilanish ichida yuklamaymiz.
  Exit;
#endif

  // setup.exe yonida
  if ModelFileValid(LocalModelPath()) then
  begin
    ModelSource := LocalModelPath();
    Exit;
  end;

  // Internet
  if not WizardSilent() then
    DownloadPage.Show;
  try
    if TryDownload(3, ErrText) then
      ModelSource := ExpandConstant('{tmp}\ggml-rubaistt.bin')
    else
      Result := FmtMessage(ExpandConstant('{cm:ModelFailed}'), [ErrText]);
  finally
    if not WizardSilent() then
      DownloadPage.Hide;
  end;
end;

procedure InitializeWizard;
begin
  DownloadPage := CreateDownloadPage(
    SetupMessage(msgWizardPreparing),
    ExpandConstant('{cm:DownloadingModel}'),
    @OnDownloadProgress);
end;

// Oynali (odatiy) o'rnatish.
function NextButtonClick(CurPageID: Integer): Boolean;
var
  ErrText: String;
begin
  Result := True;
  if CurPageID <> wpReady then
    Exit;

  ErrText := AcquireModel();
  if ErrText <> '' then
  begin
    SuppressibleMsgBox(ErrText, mbCriticalError, MB_OK, IDOK);
    Result := False;      // "Ready" sahifasida qolamiz, qayta urinish mumkin
  end;
end;

// Jimgina o'rnatish (/SILENT, /VERYSILENT).
//
// MUHIM: NextButtonClick sehrgar hodisasi — jim rejimda sehrgar ishlamaydi
// va u UMUMAN chaqirilmaydi. PrepareToInstall esa ikkala rejimda ham
// chaqiriladi.
function PrepareToInstall(var NeedsRestart: Boolean): String;
var
  Band: String;
begin
  Result := '';
  // Faqat avto-yangilanishda: Kotib InitializeSetup da yopilgan. Oynali
  // oʻrnatishda esa ochiq Kotib'ni Inno'ning oʻzi yopishni taklif qiladi —
  // bu tekshiruv undan oldin turib, uni toʻsib qoʻyardi.
  if KotibYangilash and DirExists(ExpandConstant('{app}')) then
  begin
    Band := BandFayl();
    if Band <> '' then
    begin
      Log('fayl band, yangilanish boshlanmadi: ' + Band);
      Result := FmtMessage(ExpandConstant('{cm:FileBusy}'), [Band]);
      Exit;
    end;
  end;
  if WizardSilent() then
    Result := AcquireModel();
end;

procedure CurStepChanged(CurStep: TSetupStep);
var
  Dst: String;
  ResultCode: Integer;
begin
  if CurStep = ssPostInstall then
  begin
    // a) Model yangi joyga (manba boʻsh — allaqachon joyida yoki ichida).
    if ModelSource <> '' then
    begin
      Dst := YangiModelYoli();
      ForceDirectories(ExtractFileDir(Dst));
      if not CopyFile(ModelSource, Dst, False) then
        SuppressibleMsgBox(FmtMessage(ExpandConstant('{cm:ModelCopyFailed}'), [Dst]),
                           mbCriticalError, MB_OK, IDOK);
    end;

    // b) 1.1.0 per-machine nusxa — model nusxalangandan KEYIN.
    if EskiUninstaller() <> '' then
    begin
      if EskiNusxaniOlibTashla() then
        // Eski HKLM yozuvi turganda Inno oʻz yozuviga «(Joriy foydalanuvchi)»
        // qoʻshib qoʻyadi (ikkita bir xil nom boʻlmasin deb). Eskisi endi
        // yoʻq — «Ilovalar» da oddiy «Kotib <v>» qolsin.
        RegWriteStringValue(HKCU, UninstallKalit(), 'DisplayName', '{#AppName} {#AppVersion}')
      else
        SuppressibleMsgBox(ExpandConstant('{cm:OldUninstallFailed}'),
                           mbInformation, MB_OK, IDOK);
    end;
    JudaEskilarniOlibTashla();

    // c) Avtostart yangi yoʻlga.
    AvtostartniKochir();
    Exit;
  end;

  // d) Kotib ochiq edi yoki avto-yangilanish — yangisini ochamiz.
  if (CurStep = ssDone) and AppWasRunning then
    KotibOchildi := Exec(ExpandConstant('{app}\{#AppExe}'), '', '', SW_SHOWNORMAL,
                         ewNoWait, ResultCode);
end;

// Avto-yangilanish YIQILDI (fayl band, disk toʻla, bekor qilindi) — eski
// Kotib diskda turibdi, uni qayta ochamiz: foydalanuvchi ilovasiz qolmasin.
// Yangilovchi keyingi urinishni oʻzi rejalashtiradi (`ornatish.log` da sabab).
// `KotibOchildi` — ssDone dagi ishga tushirish mutex yaratib ulgurmagan
// boʻlishi mumkin; ikkinchi nusxa Sozlamalar oynasini ochib yuborardi.
procedure DeinitializeSetup();
var
  ResultCode: Integer;
begin
  if not KotibYangilash or KotibOchildi or KotibOchiqmi() then
    Exit;
  try
    if Exec(ExpandConstant('{app}\{#AppExe}'), '', '', SW_SHOWNORMAL, ewNoWait, ResultCode) then
      Log('yangilanish oʻrnatilmadi — eski Kotib qayta ochildi');
  except
    Log('eski Kotib qayta ochilmadi: ' + GetExceptionMessage);
  end;
end;

// ---- Uninstall (E8)

function InitializeUninstall(): Boolean;
begin
  Result := True;
  ModellarniOchir := False;
  // Ochiq Kotib .exe va DLL'larni qulflaydi — fayllar qolib ketmasin.
  KotibniYop();
end;

// «Til modellari saqlab qolinsinmi?» — Inno'ning «Haqiqatan oʻchirasizmi?»
// tasdigʻidan KEYIN (usUninstall), oʻzbekcha tugmalar bilan. Savol ataylab
// «saqlansinmi» shaklida: TaskDialog'da standart tugma doim BIRINCHISI
// (`MB_DEFBUTTON2` ni rad etadi — «Invalid Buttons»), demak Enter yoki
// shoshib bosish modellarni SAQLAYDI. Jim rejimda ham saqlanadi.
function ModellarniOchirishniSora(): Boolean;
var
  Tugmalar: TArrayOfString;
begin
  Result := False;
  if UninstallSilent() then
    Exit;
  // Diqqat: satr `[` bilan boshlansa Inno uni boʻlim sarlavhasi deb oʻqiydi.
  Tugmalar := [ExpandConstant('{cm:UninstallModelsHa}'), ExpandConstant('{cm:UninstallModelsYoq}')];
  Result := TaskDialogMsgBox(ExpandConstant('{cm:UninstallModelsSavol}'),
                             ExpandConstant('{cm:UninstallModels}'),
                             mbConfirmation, MB_YESNO, Tugmalar, 0) = IDNO;
end;

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
var
  RunKalit, Qiymat: String;
begin
  if CurUninstallStep = usUninstall then
  begin
    ModellarniOchir := ModellarniOchirishniSora();
    if ModellarniOchir then
      Log('til modellari ham oʻchiriladi')
    else
      Log('til modellari saqlanadi');
    // Avtostart yozuvi shu nusxaga koʻrsatsa — oʻchiriladi. Boshqa
    // (tirik) nusxaga koʻrsatsa — tegmaymiz.
    RunKalit := 'Software\Microsoft\Windows\CurrentVersion\Run';
    if RegQueryStringValue(HKCU, RunKalit, '{#AppName}', Qiymat) then
      if CompareText(RemoveQuotes(Qiymat), ExpandConstant('{app}\{#AppExe}')) = 0 then
        RegDeleteValue(HKCU, RunKalit, '{#AppName}');
  end;

  if (CurUninstallStep = usPostUninstall) and ModellarniOchir then
  begin
    DelTree(ExpandConstant('{localappdata}\Kotib\models'), True, True, True);
    DelTree(ExpandConstant('{localappdata}\Kotib\tarjima-model-33b'), True, True, True);
    // Chala qolgan tarjima modeli yuklamasi (`tarjima_yuklovchi.cpp`).
    DelTree(ExpandConstant('{localappdata}\Kotib\tarjima-model-33b.yangi'), True, True, True);
    DeleteFile(ExpandConstant('{localappdata}\Kotib\tarjima-model.tar.gz'));
  end;
end;
