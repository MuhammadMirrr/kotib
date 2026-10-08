; *** Inno Setup 6.5.0+ — oʻzbekcha xabarlar ***
;
; Kotib uchun tarjima qilingan (uzb.mirqobilov.com). Inno Setup rasmiy
; oʻzbekcha tarjima bilan kelmaydi, shuning uchun u shu yerda turadi.
;
; DIQQAT: fayl UTF-8 + BOM bilan saqlanishi SHART. BOM boʻlmasa Inno uni
; tizim ANSI kod sahifasida oʻqiydi va ʻ, ʼ belgilari oddiy apostrofga
; aylanib qoladi.
;
; Oʻrniga nuqta QOʻSHMANG: aslida nuqtasiz boʻlgan xabarlarga Inno uni
; oʻzi qoʻshadi.

[LangOptions]
LanguageName=O<02BB>zbekcha
LanguageID=$0443
LanguageCodePage=0

[Messages]

; *** Sarlavhalar
SetupAppTitle=Oʻrnatish
SetupWindowTitle=%1 — oʻrnatish
UninstallAppTitle=Oʻchirish
UninstallAppFullTitle=%1 ni oʻchirish

; *** Umumiy
InformationTitle=Maʼlumot
ConfirmTitle=Tasdiqlang
ErrorTitle=Xato

; *** SetupLdr
SetupLdrStartupMessage=%1 oʻrnatiladi. Davom etilsinmi?
LdrCannotCreateTemp=Vaqtinchalik fayl yaratilmadi. Oʻrnatish toʻxtatildi
LdrCannotExecTemp=Vaqtinchalik papkadagi fayl ishga tushmadi. Oʻrnatish toʻxtatildi
HelpTextNote=

; *** Ishga tushishdagi xatolar
LastErrorMessage=%1.%n%nXato %2: %3
SetupFileMissing=Oʻrnatish papkasida %1 fayli yoʻq. Muammoni tuzating yoki dasturning yangi nusxasini oling.
SetupFileCorrupt=Oʻrnatuvchi fayllari buzilgan. Dasturning yangi nusxasini oling.
SetupFileCorruptOrWrongVer=Oʻrnatuvchi fayllari buzilgan yoki bu versiyaga mos emas. Muammoni tuzating yoki dasturning yangi nusxasini oling.
InvalidParameter=Buyruq satrida notoʻgʻri parametr berildi:%n%n%1
SetupAlreadyRunning=Oʻrnatuvchi allaqachon ishlab turibdi.
WindowsVersionNotSupported=Bu dastur kompyuteringizdagi Windows versiyasini qoʻllab-quvvatlamaydi.
WindowsServicePackRequired=Bu dastur uchun %1 Service Pack %2 yoki undan yangisi kerak.
NotOnThisPlatform=Bu dastur %1 da ishlamaydi.
OnlyOnThisPlatform=Bu dastur %1 da ishga tushirilishi kerak.
OnlyOnTheseArchitectures=Bu dasturni faqat quyidagi protsessor arxitekturalari uchun moʻljallangan Windows versiyalariga oʻrnatish mumkin:%n%n%1
WinVersionTooLowError=Bu dastur uchun %1 %2 yoki undan yangisi kerak.
WinVersionTooHighError=Bu dasturni %1 %2 yoki undan yangisiga oʻrnatib boʻlmaydi.
AdminPrivilegesRequired=Bu dasturni oʻrnatish uchun administrator sifatida kirgan boʻlishingiz kerak.
PowerUserPrivilegesRequired=Bu dasturni oʻrnatish uchun administrator yoki Power Users guruhi aʼzosi sifatida kirgan boʻlishingiz kerak.
SetupAppRunningError=%1 hozir ishlab turibdi.%n%nUning barcha nusxalarini yoping va davom etish uchun OK ni bosing, chiqish uchun Bekor qilish ni bosing.
UninstallAppRunningError=%1 hozir ishlab turibdi.%n%nUning barcha nusxalarini yoping va davom etish uchun OK ni bosing, chiqish uchun Bekor qilish ni bosing.

; *** Boshlanishdagi savollar
PrivilegesRequiredOverrideTitle=Oʻrnatish rejimini tanlang
PrivilegesRequiredOverrideInstruction=Oʻrnatish rejimini tanlang
PrivilegesRequiredOverrideText1=%1 barcha foydalanuvchilar uchun (administrator huquqi kerak) yoki faqat siz uchun oʻrnatilishi mumkin.
PrivilegesRequiredOverrideText2=%1 faqat siz uchun yoki barcha foydalanuvchilar uchun (administrator huquqi kerak) oʻrnatilishi mumkin.
PrivilegesRequiredOverrideAllUsers=&Barcha foydalanuvchilar uchun
PrivilegesRequiredOverrideAllUsersRecommended=&Barcha foydalanuvchilar uchun (tavsiya etiladi)
PrivilegesRequiredOverrideCurrentUser=&Faqat men uchun
PrivilegesRequiredOverrideCurrentUserRecommended=&Faqat men uchun (tavsiya etiladi)

; *** Turli xatolar
ErrorCreatingDir=«%1» papkasi yaratilmadi
ErrorTooManyFilesInDir=«%1» papkasida fayl yaratilmadi — unda juda koʻp fayl bor

; *** Umumiy xabarlar
ExitSetupTitle=Oʻrnatishdan chiqish
ExitSetupMessage=Oʻrnatish tugamadi. Hozir chiqsangiz, dastur oʻrnatilmaydi.%n%nOʻrnatishni keyinroq qaytadan boshlashingiz mumkin.%n%nChiqilsinmi?
AboutSetupMenuItem=Oʻrnatuvchi &haqida...
AboutSetupTitle=Oʻrnatuvchi haqida
AboutSetupMessage=%1 %2%n%3%n%n%1 sahifasi:%n%4
AboutSetupNote=
TranslatorNote=

; *** Tugmalar
ButtonBack=< &Orqaga
ButtonNext=&Keyingi >
ButtonInstall=&Oʻrnatish
ButtonOK=OK
ButtonCancel=Bekor qilish
ButtonYes=&Ha
ButtonYesToAll=Hammasiga h&a
ButtonNo=&Yoʻq
ButtonNoToAll=Hammasiga yoʻ&q
ButtonFinish=&Tayyor
ButtonBrowse=&Tanlash...
ButtonWizardBrowse=Ta&nlash...
ButtonNewFolder=&Yangi papka

; *** Til tanlash
SelectLanguageTitle=Oʻrnatish tilini tanlang
SelectLanguageLabel=Oʻrnatish davomida ishlatiladigan tilni tanlang.

; *** Umumiy sehrgar matni
ClickNext=Davom etish uchun «Keyingi» ni, chiqish uchun «Bekor qilish» ni bosing.
BeveledLabel=
BrowseDialogTitle=Papkani tanlash
BrowseDialogLabel=Quyidagi roʻyxatdan papkani tanlang va OK ni bosing.
NewFolderName=Yangi papka

; *** «Xush kelibsiz»
WelcomeLabel1=[name] oʻrnatish sehrgariga xush kelibsiz
WelcomeLabel2=Kompyuteringizga [name/ver] oʻrnatiladi.%n%nDavom etishdan oldin boshqa ilovalarni yopish tavsiya etiladi.

; *** Parol
WizardPassword=Parol
PasswordLabel1=Bu oʻrnatish parol bilan himoyalangan.
PasswordLabel3=Parolni kiriting va «Keyingi» ni bosing. Katta-kichik harflar farqlanadi.
PasswordEditLabel=&Parol:
IncorrectPassword=Parol notoʻgʻri. Qayta urinib koʻring.

; *** Litsenziya
WizardLicense=Litsenziya shartnomasi
LicenseLabel=Davom etishdan oldin quyidagi muhim maʼlumotni oʻqing.
LicenseLabel3=Quyidagi litsenziya shartnomasini oʻqing. Oʻrnatishni davom ettirish uchun uning shartlarini qabul qilishingiz kerak.
LicenseAccepted=Shartlarni &qabul qilaman
LicenseNotAccepted=Shartlarni qabul &qilmayman

; *** Maʼlumot sahifalari
WizardInfoBefore=Maʼlumot
InfoBeforeLabel=Davom etishdan oldin quyidagi muhim maʼlumotni oʻqing.
InfoBeforeClickLabel=Davom etishga tayyor boʻlsangiz, «Keyingi» ni bosing.
WizardInfoAfter=Maʼlumot
InfoAfterLabel=Davom etishdan oldin quyidagi muhim maʼlumotni oʻqing.
InfoAfterClickLabel=Davom etishga tayyor boʻlsangiz, «Keyingi» ni bosing.

; *** Foydalanuvchi maʼlumoti
WizardUserInfo=Foydalanuvchi maʼlumoti
UserInfoDesc=Maʼlumotlaringizni kiriting.
UserInfoName=&Foydalanuvchi nomi:
UserInfoOrg=&Tashkilot:
UserInfoSerial=&Seriya raqami:
UserInfoNameRequired=Nomni kiritishingiz kerak.

; *** Papka tanlash
WizardSelectDir=Oʻrnatish joyini tanlang
SelectDirDesc=[name] qayerga oʻrnatilsin?
SelectDirLabel3=[name] quyidagi papkaga oʻrnatiladi.
SelectDirBrowseLabel=Davom etish uchun «Keyingi» ni bosing. Boshqa papkani tanlash uchun «Tanlash» ni bosing.
DiskSpaceGBLabel=Kamida [gb] GB boʻsh joy kerak.
DiskSpaceMBLabel=Kamida [mb] MB boʻsh joy kerak.
CannotInstallToNetworkDrive=Tarmoq diskiga oʻrnatib boʻlmaydi.
CannotInstallToUNCPath=UNC manzilga oʻrnatib boʻlmaydi.
InvalidPath=Disk harfi bilan toʻliq yoʻl kiriting, masalan:%n%nC:\APP%n%nyoki UNC manzil:%n%n\\server\papka
InvalidDrive=Tanlangan disk yoki tarmoq papkasi mavjud emas yoki ochilmayapti. Boshqasini tanlang.
DiskSpaceWarningTitle=Diskda joy yetarli emas
DiskSpaceWarning=Oʻrnatish uchun kamida %1 KB boʻsh joy kerak, tanlangan diskda esa %2 KB bor.%n%nBaribir davom etilsinmi?
DirNameTooLong=Papka nomi yoki yoʻli juda uzun.
InvalidDirName=Papka nomi notoʻgʻri.
BadDirName32=Papka nomida quyidagi belgilar boʻlmasligi kerak:%n%n%1
DirExistsTitle=Papka mavjud
DirExists=Papka:%n%n%1%n%nallaqachon bor. Baribir oʻsha papkaga oʻrnatilsinmi?
DirDoesntExistTitle=Papka mavjud emas
DirDoesntExist=Papka:%n%n%1%n%nmavjud emas. U yaratilsinmi?

; *** Tarkibiy qismlar
WizardSelectComponents=Tarkibni tanlang
SelectComponentsDesc=Nimalar oʻrnatilsin?
SelectComponentsLabel2=Oʻrnatmoqchi boʻlgan qismlarni belgilang, keraksizlarini olib tashlang. Tayyor boʻlsangiz «Keyingi» ni bosing.
FullInstallation=Toʻliq oʻrnatish
CompactInstallation=Ixcham oʻrnatish
CustomInstallation=Tanlab oʻrnatish
NoUninstallWarningTitle=Tarkib allaqachon bor
NoUninstallWarning=Quyidagi qismlar kompyuteringizda allaqachon oʻrnatilgan:%n%n%1%n%nUlarning belgisini olib tashlash oʻrnatilganini oʻchirmaydi.%n%nBaribir davom etilsinmi?
ComponentSize1=%1 KB
ComponentSize2=%1 MB
ComponentsDiskSpaceGBLabel=Tanlanganlar uchun kamida [gb] GB joy kerak.
ComponentsDiskSpaceMBLabel=Tanlanganlar uchun kamida [mb] MB joy kerak.

; *** Qoʻshimcha vazifalar
WizardSelectTasks=Qoʻshimcha vazifalarni tanlang
SelectTasksDesc=Yana nima qilinsin?
SelectTasksLabel2=[name] oʻrnatilayotganda bajarilishi kerak boʻlgan qoʻshimcha vazifalarni tanlang va «Keyingi» ni bosing.

; *** Start menyusi papkasi
WizardSelectProgramGroup=Start menyusidagi papkani tanlang
SelectStartMenuFolderDesc=Yorliqlar qayerga qoʻyilsin?
SelectStartMenuFolderLabel3=Yorliqlar Start menyusining quyidagi papkasiga qoʻyiladi.
SelectStartMenuFolderBrowseLabel=Davom etish uchun «Keyingi» ni bosing. Boshqa papkani tanlash uchun «Tanlash» ni bosing.
MustEnterGroupName=Papka nomini kiritishingiz kerak.
GroupNameTooLong=Papka nomi yoki yoʻli juda uzun.
InvalidGroupName=Papka nomi notoʻgʻri.
BadGroupName=Papka nomida quyidagi belgilar boʻlmasligi kerak:%n%n%1
NoProgramGroupCheck2=Start menyusida papka &yaratilmasin

; *** «Oʻrnatishga tayyor»
WizardReady=Oʻrnatishga tayyor
ReadyLabel1=[name] kompyuteringizga oʻrnatilishga tayyor.
ReadyLabel2a=Oʻrnatish uchun «Oʻrnatish» ni bosing. Sozlamalarni koʻrish yoki oʻzgartirish uchun «Orqaga» ni bosing.
ReadyLabel2b=Oʻrnatish uchun «Oʻrnatish» ni bosing.
ReadyMemoUserInfo=Foydalanuvchi maʼlumoti:
ReadyMemoDir=Oʻrnatish joyi:
ReadyMemoType=Oʻrnatish turi:
ReadyMemoComponents=Tanlangan tarkib:
ReadyMemoGroup=Start menyusidagi papka:
ReadyMemoTasks=Qoʻshimcha vazifalar:

; *** Yuklab olish
DownloadingLabel2=Fayllar yuklab olinmoqda...
ButtonStopDownload=&Toʻxtatish
StopDownload=Yuklab olish toʻxtatilsinmi?
ErrorDownloadAborted=Yuklab olish toʻxtatildi
ErrorDownloadFailed=Yuklab olinmadi: %1 %2
ErrorDownloadSizeFailed=Hajmni aniqlab boʻlmadi: %1 %2
ErrorProgress=Notoʻgʻri jarayon: %2 dan %1
ErrorFileSize=Notoʻgʻri fayl hajmi: kutilgani %1, topilgani %2

; *** Arxivdan chiqarish
ExtractingLabel=Fayllar chiqarilmoqda...
ButtonStopExtraction=&Toʻxtatish
StopExtraction=Chiqarish toʻxtatilsinmi?
ErrorExtractionAborted=Chiqarish toʻxtatildi
ErrorExtractionFailed=Chiqarib boʻlmadi: %1

; *** Arxiv xatolari
ArchiveIncorrectPassword=Parol notoʻgʻri
ArchiveIsCorrupted=Arxiv buzilgan
ArchiveUnsupportedFormat=Arxiv formati qoʻllab-quvvatlanmaydi

; *** Tayyorgarlik
WizardPreparing=Oʻrnatishga tayyorgarlik
PreparingDesc=[name] kompyuteringizga oʻrnatishga tayyorlanmoqda.
PreviousInstallNotCompleted=Oldingi dasturni oʻrnatish yoki oʻchirish tugallanmagan. Uni yakunlash uchun kompyuterni qayta yuklashingiz kerak.%n%nQayta yuklagandan soʻng [name] oʻrnatishni qaytadan boshlang.
CannotContinue=Oʻrnatishni davom ettirib boʻlmaydi. Chiqish uchun «Bekor qilish» ni bosing.
ApplicationsFound=Quyidagi ilovalar yangilanishi kerak boʻlgan fayllardan foydalanmoqda. Ularni oʻrnatuvchi oʻzi yopishiga ruxsat berish tavsiya etiladi.
ApplicationsFound2=Quyidagi ilovalar yangilanishi kerak boʻlgan fayllardan foydalanmoqda. Ularni oʻrnatuvchi oʻzi yopishiga ruxsat berish tavsiya etiladi. Oʻrnatish tugagach, ular qayta ishga tushiriladi.
CloseApplications=Ilovalar &avtomatik yopilsin
DontCloseApplications=Ilovalar &yopilmasin
ErrorCloseApplications=Hamma ilovani avtomatik yopib boʻlmadi. Davom etishdan oldin yangilanadigan fayllardan foydalanayotgan ilovalarni oʻzingiz yopishingiz tavsiya etiladi.
PrepareToInstallNeedsRestart=Kompyuterni qayta yuklash kerak. Qayta yuklagandan soʻng [name] oʻrnatishni qaytadan boshlang.%n%nHozir qayta yuklansinmi?

; *** Oʻrnatilmoqda
WizardInstalling=Oʻrnatilmoqda
InstallingLabel=[name] kompyuteringizga oʻrnatilmoqda, biroz kuting.

; *** Tugadi
FinishedHeadingLabel=[name] oʻrnatish sehrgari tugadi
FinishedLabelNoIcons=[name] kompyuteringizga oʻrnatildi.
FinishedLabel=[name] kompyuteringizga oʻrnatildi. Ilovani yorliqlar orqali ishga tushirish mumkin.
ClickFinish=Chiqish uchun «Tayyor» ni bosing.
FinishedRestartLabel=[name] oʻrnatishni yakunlash uchun kompyuterni qayta yuklash kerak. Hozir qayta yuklansinmi?
FinishedRestartMessage=[name] oʻrnatishni yakunlash uchun kompyuterni qayta yuklash kerak.%n%nHozir qayta yuklansinmi?
ShowReadmeCheck=Ha, README faylini koʻrmoqchiman
YesRadio=&Ha, hozir qayta yuklansin
NoRadio=&Yoʻq, keyinroq oʻzim qayta yuklayman
RunEntryExec=%1 ishga tushirilsin
RunEntryShellExec=%1 koʻrilsin

; *** Keyingi disk
ChangeDiskTitle=Keyingi disk kerak
SelectDiskLabel2=%1-diskni qoʻying va OK ni bosing.%n%nBu diskdagi fayllar quyida koʻrsatilganidan boshqa papkada boʻlsa, toʻgʻri yoʻlni kiriting yoki «Tanlash» ni bosing.
PathLabel=&Yoʻl:
FileNotInDir2=«%1» fayli «%2» ichidan topilmadi. Toʻgʻri diskni qoʻying yoki boshqa papkani tanlang.
SelectDirectoryLabel=Keyingi diskning joyini koʻrsating.

; *** Oʻrnatish bosqichi
SetupAborted=Oʻrnatish tugallanmadi.%n%nMuammoni tuzatib, oʻrnatishni qaytadan boshlang.
AbortRetryIgnoreSelectAction=Nima qilinsin?
AbortRetryIgnoreRetry=&Qayta urinib koʻrilsin
AbortRetryIgnoreIgnore=Xatoga &eʼtibor berilmasin
AbortRetryIgnoreCancel=Oʻrnatish bekor qilinsin
RetryCancelSelectAction=Nima qilinsin?
RetryCancelRetry=&Qayta urinib koʻrilsin
RetryCancelCancel=Bekor qilinsin

; *** Holat xabarlari
StatusClosingApplications=Ilovalar yopilmoqda...
StatusCreateDirs=Papkalar yaratilmoqda...
StatusExtractFiles=Fayllar chiqarilmoqda...
StatusDownloadFiles=Fayllar yuklab olinmoqda...
StatusCreateIcons=Yorliqlar yaratilmoqda...
StatusCreateIniEntries=INI yozuvlari yaratilmoqda...
StatusCreateRegistryEntries=Registr yozuvlari yaratilmoqda...
StatusRegisterFiles=Fayllar roʻyxatdan oʻtkazilmoqda...
StatusSavingUninstall=Oʻchirish maʼlumoti saqlanmoqda...
StatusRunProgram=Oʻrnatish yakunlanmoqda...
StatusRestartingApplications=Ilovalar qayta ishga tushirilmoqda...
StatusRollback=Oʻzgarishlar qaytarilmoqda...

; *** Turli xatolar
ErrorInternal2=Ichki xato: %1
ErrorFunctionFailedNoCode=%1 bajarilmadi
ErrorFunctionFailed=%1 bajarilmadi; kod %2
ErrorFunctionFailedWithMessage=%1 bajarilmadi; kod %2.%n%3
ErrorExecutingProgram=Faylni ishga tushirib boʻlmadi:%n%1

; *** Registr xatolari
ErrorRegOpenKey=Registr kalitini ochishda xato:%n%1\%2
ErrorRegCreateKey=Registr kalitini yaratishda xato:%n%1\%2
ErrorRegWriteKey=Registr kalitiga yozishda xato:%n%1\%2

; *** INI xatolari
ErrorIniEntry=«%1» faylida INI yozuvini yaratishda xato.

; *** Fayl nusxalash xatolari
FileAbortRetryIgnoreSkipNotRecommended=Bu fayl &oʻtkazib yuborilsin (tavsiya etilmaydi)
FileAbortRetryIgnoreIgnoreNotRecommended=Xatoga &eʼtibor berilmasin (tavsiya etilmaydi)
SourceIsCorrupted=Manba fayl buzilgan
SourceDoesntExist=«%1» manba fayli mavjud emas
SourceVerificationFailed=Manba faylni tekshirib boʻlmadi: %1
VerificationSignatureDoesntExist=«%1» imzo fayli mavjud emas
VerificationSignatureInvalid=«%1» imzo fayli notoʻgʻri
VerificationKeyNotFound=«%1» imzo fayli nomaʼlum kalitdan foydalanadi
VerificationFileNameIncorrect=Fayl nomi notoʻgʻri
VerificationFileTagIncorrect=Fayl tegi notoʻgʻri
VerificationFileSizeIncorrect=Fayl hajmi notoʻgʻri
VerificationFileHashIncorrect=Fayl xesh-qiymati notoʻgʻri
ExistingFileReadOnly2=Mavjud faylni almashtirib boʻlmadi — u «faqat oʻqish uchun» deb belgilangan.
ExistingFileReadOnlyRetry=«Faqat oʻqish» belgisi &olib tashlansin va qayta urinilsin
ExistingFileReadOnlyKeepExisting=Mavjud fayl &qoldirilsin
ErrorReadingExistingDest=Mavjud faylni oʻqishda xato yuz berdi:
FileExistsSelectAction=Nima qilinsin?
FileExists2=Fayl allaqachon mavjud.
FileExistsOverwriteExisting=Mavjud fayl &ustiga yozilsin
FileExistsKeepExisting=Mavjud fayl &qoldirilsin
FileExistsOverwriteOrKeepAll=Keyingi toʻqnashuvlarda ham &shunday qilinsin
ExistingFileNewerSelectAction=Nima qilinsin?
ExistingFileNewer2=Mavjud fayl oʻrnatilayotganidan yangiroq.
ExistingFileNewerOverwriteExisting=Mavjud fayl &ustiga yozilsin
ExistingFileNewerKeepExisting=Mavjud fayl &qoldirilsin (tavsiya etiladi)
ExistingFileNewerOverwriteOrKeepAll=Keyingi toʻqnashuvlarda ham &shunday qilinsin
ErrorChangingAttr=Mavjud faylning xossalarini oʻzgartirishda xato yuz berdi:
ErrorCreatingTemp=Nishon papkada fayl yaratishda xato yuz berdi:
ErrorReadingSource=Manba faylni oʻqishda xato yuz berdi:
ErrorCopying=Faylni nusxalashda xato yuz berdi:
ErrorDownloading=Faylni yuklab olishda xato yuz berdi:
ErrorExtracting=Arxivdan chiqarishda xato yuz berdi:
ErrorReplacingExistingFile=Mavjud faylni almashtirishda xato yuz berdi:
ErrorRestartReplace=RestartReplace bajarilmadi:
ErrorRenamingTemp=Nishon papkadagi faylni qayta nomlashda xato yuz berdi:
ErrorRegisterServer=DLL/OCX roʻyxatdan oʻtmadi: %1
ErrorRegSvr32Failed=RegSvr32 %1 kodi bilan tugadi
ErrorRegisterTypeLib=Tip kutubxonasi roʻyxatdan oʻtmadi: %1

; *** Oʻchirish roʻyxatidagi belgilar
UninstallDisplayNameMark=%1 (%2)
UninstallDisplayNameMarks=%1 (%2, %3)
UninstallDisplayNameMark32Bit=32-bit
UninstallDisplayNameMark64Bit=64-bit
UninstallDisplayNameMarkAllUsers=Barcha foydalanuvchilar
UninstallDisplayNameMarkCurrentUser=Joriy foydalanuvchi

; *** Oʻrnatishdan keyingi xatolar
ErrorOpeningReadme=README faylini ochishda xato yuz berdi.
ErrorRestartingComputer=Kompyuterni qayta yuklab boʻlmadi. Buni oʻzingiz bajaring.

; *** Oʻchirish
UninstallNotFound=«%1» fayli mavjud emas. Oʻchirib boʻlmaydi.
UninstallOpenError=«%1» faylini ochib boʻlmadi. Oʻchirib boʻlmaydi
UninstallUnsupportedVer=«%1» oʻchirish jurnali bu versiya tanimaydigan formatda. Oʻchirib boʻlmaydi
UninstallUnknownEntry=Oʻchirish jurnalida nomaʼlum yozuv uchradi (%1)
ConfirmUninstall=%1 va uning barcha qismlari butunlay oʻchirilsinmi?
UninstallOnlyOnWin64=Buni faqat 64-bitli Windows'da oʻchirish mumkin.
OnlyAdminCanUninstall=Buni faqat administrator huquqiga ega foydalanuvchi oʻchira oladi.
UninstallStatusLabel=%1 kompyuteringizdan oʻchirilmoqda, biroz kuting.
UninstalledAll=%1 kompyuteringizdan muvaffaqiyatli oʻchirildi.
UninstalledMost=%1 oʻchirildi.%n%nBaʼzi elementlar oʻchirilmadi — ularni qoʻlda oʻchirish mumkin.
UninstalledAndNeedsRestart=%1 oʻchirilishini yakunlash uchun kompyuterni qayta yuklash kerak.%n%nHozir qayta yuklansinmi?
UninstallDataCorrupted=«%1» fayli buzilgan. Oʻchirib boʻlmaydi

; *** Oʻchirish bosqichi
ConfirmDeleteSharedFileTitle=Umumiy fayl oʻchirilsinmi?
ConfirmDeleteSharedFile2=Tizim quyidagi umumiy fayldan boshqa hech qaysi dastur foydalanmayapti deb hisoblaydi. U oʻchirilsinmi?%n%nAgar biror dastur hamon undan foydalanayotgan boʻlsa, oʻchirilgach u dastur notoʻgʻri ishlashi mumkin. Ishonchingiz komil boʻlmasa «Yoʻq» ni tanlang. Faylni qoldirish hech qanday zarar keltirmaydi.
SharedFileNameLabel=Fayl nomi:
SharedFileLocationLabel=Joyi:
WizardUninstalling=Oʻchirish holati
StatusUninstalling=%1 oʻchirilmoqda...

; *** Oʻchirishni bloklash sabablari
ShutdownBlockReasonInstallingApp=%1 oʻrnatilmoqda.
ShutdownBlockReasonUninstallingApp=%1 oʻchirilmoqda.

[CustomMessages]

NameAndVersion=%1 %2
AdditionalIcons=Qoʻshimcha yorliqlar:
CreateDesktopIcon=&Ish stolida yorliq yaratilsin
CreateQuickLaunchIcon=&Tez ishga tushirish panelida yorliq yaratilsin
ProgramOnTheWeb=%1 — internetda
UninstallProgram=%1 ni oʻchirish
LaunchProgram=%1 ni ishga tushirish
AssocFileExtension=%1 ni %2 fayl kengaytmasi bilan &bogʻlansin
AssocingFileExtension=%1 %2 kengaytmasi bilan bogʻlanmoqda...
AutoStartProgramGroupDescription=Avtomatik ishga tushish:
AutoStartProgram=%1 avtomatik ishga tushsin
AddonHostProgramNotFound=%1 siz tanlagan papkadan topilmadi.%n%nBaribir davom etilsinmi?
