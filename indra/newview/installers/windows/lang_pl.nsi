; First is default
!insertmacro MUI_LANGUAGE "Polish"

; Language string
LangString LanguageCode ${LANG_POLISH}   "pl"

; Language selection dialog
LangString SelectInstallerLanguage  ${LANG_POLISH} "Proszę wybrać język instalatora"

; installation directory text
LangString DirectoryChooseTitle ${LANG_POLISH} "Katalog instalacji" 
LangString DirectoryChooseUpdate ${LANG_POLISH} "Wybierz katalog instalacji ${APPNAME} w celu aktualizacji wersji ${VERSION_LONG}.(XXX):"
LangString DirectoryChooseSetup ${LANG_POLISH} "Wybierz katalog instalacji ${APPNAME} w:"

; CheckStartupParams message box
LangString CheckStartupParamsMB ${LANG_POLISH} "Nie można odnaleźć programu '$INSTPROG'. Cicha aktualizacja zakończyła się niepowodzeniem."

; check windows version
LangString CheckWindowsVersionDP ${LANG_POLISH} "Sprawdzanie wersji Windows..."
LangString CheckWindowsVersionMB ${LANG_POLISH} '${APPNAME} obsługuje tylko Windows XP.$\n$\nPróba zainstalowania na Windows $R0 może spowodować awarie i utratę danych.$\n$\n'
LangString CheckWindowsServPackMB ${LANG_POLISH} "Zalecane jest uruchamianie ${APPNAME} z najnowszym dostępnym Service Packiem zainstalowanym w systemie.$\nPomaga on w podniesieniu wydajności i stabilności programu."
LangString UseLatestServPackDP ${LANG_POLISH} "Użyj usługi Windows Update, aby zainstalować najnowszy Service Pack."

; checkifadministrator function (install)
LangString CheckAdministratorInstDP ${LANG_POLISH} "Sprawdzanie zezwolenia na instalację..."
LangString CheckAdministratorInstMB ${LANG_POLISH} 'Używasz "ograniczonego" konta.$\nMusisz być zalogowany jako "administrator" aby zainstalować ${APPNAME}.'

; checkifadministrator function (uninstall)
LangString CheckAdministratorUnInstDP ${LANG_POLISH} "Sprawdzanie zezwolenia na odinstalowanie..."
LangString CheckAdministratorUnInstMB ${LANG_POLISH} 'Używasz "ograniczonego" konta.$\nMusisz być być zalogowany jako "administrator" aby zainstalować ${APPNAME}.'

; checkifalreadycurrent
LangString CheckIfCurrentMB ${LANG_POLISH} "${APPNAME} ${VERSION_LONG} jest już zainstalowane.$\n$\nCzy chcesz zainstalować ${APPNAME} ponownie?"

; checkcpuflags
LangString MissingSSE2 ${LANG_POLISH} "Ten komputer może nie mieć procesora z obsługą SSE2, który jest wymagany aby uruchomić ${APPNAME} w wersji ${VERSION_LONG}. Chcesz kontynuować?"

; closesecondlife function (install)
LangString CloseSecondLifeInstDP ${LANG_POLISH} "Oczekiwanie na zamknięcie ${APPNAME}..."
LangString CloseSecondLifeInstMB ${LANG_POLISH} "${APPNAME} nie może zostać zainstalowane, ponieważ jest już włączone.$\n$\nZakończ swoje działania i wybierz OK aby zamknąć ${APPNAME} i kontynuować.$\nWybierz CANCEL aby anulować instalację."

; closesecondlife function (uninstall)
LangString CloseSecondLifeUnInstDP ${LANG_POLISH} "Oczekiwanie na zamknięcie ${APPNAME}..."
LangString CloseSecondLifeUnInstMB ${LANG_POLISH} "${APPNAME} nie może zostać zainstalowane, ponieważ jest już włączone.$\n$\nZakończ swoje działania i wybierz OK aby zamknąć ${APPNAME} i kontynuować.$\nWybierz CANCEL aby anulować."

; CheckNetworkConnection
LangString CheckNetworkConnectionDP ${LANG_POLISH} "Sprawdzanie połączenia sieciowego..."

; removecachefiles
LangString RemoveCacheFilesDP ${LANG_POLISH} "Kasowanie plików pamięci podręcznej (cache) w folderze Documents and Settings"

; delete program files
LangString DeleteProgramFilesMB ${LANG_POLISH} "Nadal istnieją pliki w katalogu instalacyjnym ${APPNAME}.$\n$\nMożliwe, że są to pliki, które stworzyłeś/stworzyłaś lub przeniosłeś/przeniosłaś do:$\n$INSTDIR$\n$\nCzy chcesz je usunąć?"

; uninstall text
LangString UninstallTextMsg ${LANG_POLISH} "To spowoduje odinstalowanie ${APPNAME} ${VERSION_LONG} z Twojego systemu."
