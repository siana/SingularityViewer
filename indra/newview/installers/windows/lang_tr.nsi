; First is default
!insertmacro MUI_LANGUAGE "Turkish"

; Language string
LangString LanguageCode ${LANG_TURKISH}  "tr"

; Language selection dialog
LangString SelectInstallerLanguage  ${LANG_TURKISH} "Lütfen yükleyicinin dilini seçin"

; installation directory text
LangString DirectoryChooseTitle ${LANG_TURKISH} "Yükleme Dizini" 
LangString DirectoryChooseUpdate ${LANG_TURKISH} "${VERSION_LONG}.(XXX) sürümüne güncelleştirme yapmak için ${APPNAME} dizinini seçin:"
LangString DirectoryChooseSetup ${LANG_TURKISH} "${APPNAME}'ın yükleneceği dizini seçin:"

; CheckStartupParams message box
LangString CheckStartupParamsMB ${LANG_TURKISH} "'$INSTPROG' programı bulunamadı. Sessiz güncelleştirme başarılamadı."

; check windows version
LangString CheckWindowsVersionDP ${LANG_TURKISH} "Windows sürümü kontrol ediliyor..."
LangString CheckWindowsVersionMB ${LANG_TURKISH} "${APPNAME} sadece Windows XP'i destekler.$\n$\nWindows $R0 üzerine yüklemeye çalışmak sistem çökmelerine ve veri kaybına neden olabilir.$\n$\n"
LangString CheckWindowsServPackMB ${LANG_TURKISH} "It is recomended to run ${APPNAME} on the latest service pack for your operating system.$\nThis will help with performance and stability of the program."
LangString UseLatestServPackDP ${LANG_TURKISH} "Please use Windows Update to install the latest Service Pack."

; checkifadministrator function (install)
LangString CheckAdministratorInstDP ${LANG_TURKISH} "Yükleme izni kontrol ediliyor..."
LangString CheckAdministratorInstMB ${LANG_TURKISH} "'Sınırlı' bir hesap kullanıyor görünüyorsunuz.$\n${APPNAME}'ı yüklemek için bir 'yönetici' olmalısınız."

; checkifadministrator function (uninstall)
LangString CheckAdministratorUnInstDP ${LANG_TURKISH} "Kaldırma izni kontrol ediliyor..."
LangString CheckAdministratorUnInstMB ${LANG_TURKISH} "'Sınırlı' bir hesap kullanıyor görünüyorsunuz.$\n${APPNAME}'ı kaldırmak için bir 'yönetici' olmalısınız."

; checkifalreadycurrent
LangString CheckIfCurrentMB ${LANG_TURKISH} "${APPNAME} ${VERSION_LONG} zaten yüklü.$\n$\nTekrar yüklemek ister misiniz?"

; checkcpuflags
LangString MissingSSE2 ${LANG_TURKISH} "Bu makinede SSE2 desteğine sahip bir CPU bulunmayabilir, ${APPNAME} ${VERSION_LONG} çalıştırmak için bu gereklidir. Devam etmek istiyor musunuz?"

; closesecondlife function (install)
LangString CloseSecondLifeInstDP ${LANG_TURKISH} "${APPNAME}'ın kapatılması bekleniyor..."
LangString CloseSecondLifeInstMB ${LANG_TURKISH} "${APPNAME} zaten çalışırken kapatılamaz.$\n$\nYaptığınız işi bitirdikten sonra ${APPNAME}'ı kapatmak ve devam etmek için Tamam seçimini yapın.$\nYüklemeyi iptal etmek için İPTAL seçimini yapın."

; closesecondlife function (uninstall)
LangString CloseSecondLifeUnInstDP ${LANG_TURKISH} "${APPNAME}'ın kapatılması bekleniyor..."
LangString CloseSecondLifeUnInstMB ${LANG_TURKISH} "${APPNAME} zaten çalışırken kaldırılamaz.$\n$\nYaptığınız işi bitirdikten sonra ${APPNAME}'ı kapatmak ve devam etmek için Tamam seçimini yapın.$\nİptal etmek için İPTAL seçimini yapın."

; CheckNetworkConnection
LangString CheckNetworkConnectionDP ${LANG_TURKISH} "Ağ bağlantısı kontrol ediliyor..."

; removecachefiles
LangString RemoveCacheFilesDP ${LANG_TURKISH} "Belgeler ve Ayarlar klasöründeki önbellek dosyaları siliniyor"

; delete program files
LangString DeleteProgramFilesMB ${LANG_TURKISH} "${APPNAME} program dizininizde hala dosyalar var.$\n$\nBunlar muhtemelen sizin oluşturduğunuz veya şuraya taşıdığınız dosyalar:$\n$INSTDIR$\n$\nBunları kaldırmak istiyor musunuz?"

; uninstall text
LangString UninstallTextMsg ${LANG_TURKISH} "Bu adımla ${APPNAME} ${VERSION_LONG} sisteminizden kaldırılacaktır."
