; First is default
!insertmacro MUI_LANGUAGE "Danish"

; Language string
LangString LanguageCode ${LANG_DANISH}   "da"

; Language selection dialog
LangString SelectInstallerLanguage  ${LANG_DANISH} "Vælg venligst sprog til installation"

; installation directory text
LangString DirectoryChooseTitle ${LANG_DANISH} "Installationsmappe" 
LangString DirectoryChooseUpdate ${LANG_DANISH} "Vælg ${APPNAME} mappe til opdatering til version ${VERSION_LONG}.(XXX):"
LangString DirectoryChooseSetup ${LANG_DANISH} "Vælg mappe hvor ${APPNAME} skal installeres:"

; CheckStartupParams message box
LangString CheckStartupParamsMB ${LANG_DANISH} "Kunne ikke finde programmet '$INSTPROG'. Baggrundsopdatering fejlede."

; check windows version
LangString CheckWindowsVersionDP ${LANG_DANISH} "Checker Windows version..."
LangString CheckWindowsVersionMB ${LANG_DANISH} '${APPNAME} supporterer kun Windows XP.$\n$\nForsøg på installation på Windows $R0 kan resultere i nedbrud og datatab.$\n$\n'
LangString CheckWindowsServPackMB ${LANG_DANISH} "It is recomended to run ${APPNAME} on the latest service pack for your operating system.$\nThis will help with performance and stability of the program."
LangString UseLatestServPackDP ${LANG_DANISH} "Please use Windows Update to install the latest Service Pack."

; checkifadministrator function (install)
LangString CheckAdministratorInstDP ${LANG_DANISH} "Checker rettigheder til installation..."
LangString CheckAdministratorInstMB ${LANG_DANISH} 'Det ser ud til at du benytter en konto med begrænsninger.$\nDu skal have "administrator" rettigheder for at installere ${APPNAME}.'

; checkifadministrator function (uninstall)
LangString CheckAdministratorUnInstDP ${LANG_DANISH} "Checker rettigheder til at afinstallere..."
LangString CheckAdministratorUnInstMB ${LANG_DANISH} 'Det ser ud til at du benytter en konto med begrænsninger.$\nDu skal have "administrator" rettigheder for at afinstallere ${APPNAME}.'

; checkifalreadycurrent
LangString CheckIfCurrentMB ${LANG_DANISH} "Det ser ud til at ${APPNAME} ${VERSION_LONG} allerede er installeret.$\n$\nØnsker du at installere igen?"

; checkcpuflags
LangString MissingSSE2 ${LANG_DANISH} "This machine may not have a CPU with SSE2 support, which is required to run ${APPNAME} ${VERSION_LONG}. Do you want to continue?"

; closesecondlife function (install)
LangString CloseSecondLifeInstDP ${LANG_DANISH} "Venter på at ${APPNAME} skal lukke ned..."
LangString CloseSecondLifeInstMB ${LANG_DANISH} "${APPNAME} kan ikke installeres mens programmet kører.$\n$\nAfslut programmet for at fortsætte.$\nVælg ANNULÉR for at afbryde installation."

; closesecondlife function (uninstall)
LangString CloseSecondLifeUnInstDP ${LANG_DANISH} "Venter på at ${APPNAME} skal lukke ned..."
LangString CloseSecondLifeUnInstMB ${LANG_DANISH} "${APPNAME} kan ikke afinstalleres mens programmet kører.$\n$\nAfslut programmet for at fortsætte.$\nVælg ANNULÉR for at afbryde installation."

; CheckNetworkConnection
LangString CheckNetworkConnectionDP ${LANG_DANISH} "Checker netværksforbindelse..."

; removecachefiles
LangString RemoveCacheFilesDP ${LANG_DANISH} "Sletter cache filer i dokument mappen"

; delete program files
LangString DeleteProgramFilesMB ${LANG_DANISH} "Der er stadig filer i ${APPNAME} program mappen.$\n$\nDette er sandsynligvis filer du har oprettet eller flyttet til :$\n$INSTDIR$\n$\nØnsker du at fjerne disse filer?"

; uninstall text
LangString UninstallTextMsg ${LANG_DANISH} "Dette vil afinstallere ${APPNAME} ${VERSION_LONG} fra dit system."
