; First is default
!insertmacro MUI_LANGUAGE "Russian"

; Language string
LangString LanguageCode ${LANG_RUSSIAN}  "ru"

; Language selection dialog
LangString SelectInstallerLanguage  ${LANG_RUSSIAN} "Выберите язык программы установки"

; installation directory text
LangString DirectoryChooseTitle ${LANG_RUSSIAN} "Каталог установки" 
LangString DirectoryChooseUpdate ${LANG_RUSSIAN} "Выберите каталог ${APPNAME} для обновления до версии ${VERSION_LONG}.(XXX):"
LangString DirectoryChooseSetup ${LANG_RUSSIAN} "Выберите каталог для установки ${APPNAME}:"

; CheckStartupParams message box
LangString CheckStartupParamsMB ${LANG_RUSSIAN} "Не удалось найти программу «$INSTPROG». Автоматическое обновление не выполнено."

; check windows version
LangString CheckWindowsVersionDP ${LANG_RUSSIAN} "Проверка версии Windows..."
LangString CheckWindowsVersionMB ${LANG_RUSSIAN} '${APPNAME} может работать только в Windows XP.$\n$\nПопытка установки в Windows $R0 может привести к сбою и потере данных.$\n$\n'
LangString CheckWindowsServPackMB ${LANG_RUSSIAN} "It is recomended to run ${APPNAME} on the latest service pack for your operating system.$\nThis will help with performance and stability of the program."
LangString UseLatestServPackDP ${LANG_RUSSIAN} "Please use Windows Update to install the latest Service Pack."

; checkifadministrator function (install)
LangString CheckAdministratorInstDP ${LANG_RUSSIAN} "Проверка разрешений на установку..."
LangString CheckAdministratorInstMB ${LANG_RUSSIAN} 'Вероятно, у вас ограниченный аккаунт.$\nДля установки ${APPNAME} необходимы права администратора.'

; checkifadministrator function (uninstall)
LangString CheckAdministratorUnInstDP ${LANG_RUSSIAN} "Проверка разрешений на удаление..."
LangString CheckAdministratorUnInstMB ${LANG_RUSSIAN} 'Вероятно, у вас ограниченный аккаунт.$\nДля удаления ${APPNAME} необходимы права администратора.'

; checkifalreadycurrent
LangString CheckIfCurrentMB ${LANG_RUSSIAN} "Вероятно, версия ${APPNAME} ${VERSION_LONG} уже установлена.$\n$\nУстановить ее снова?"

; checkcpuflags
LangString MissingSSE2 ${LANG_RUSSIAN} "Возможно, на этом компьютере нет ЦП с поддержкой SSE2, которая необходима для работы ${APPNAME} ${VERSION_LONG}. Продолжить?"

; closesecondlife function (install)
LangString CloseSecondLifeInstDP ${LANG_RUSSIAN} "Ожидаю завершения работы ${APPNAME}..."
LangString CloseSecondLifeInstMB ${LANG_RUSSIAN} "${APPNAME} уже работает, выполнить установку невозможно.$\n$\nЗавершите текущую операцию и нажмите кнопку «OK», чтобы закрыть ${APPNAME} и продолжить установку.$\nНажмите кнопку «ОТМЕНА» для отказа от установки."

; closesecondlife function (uninstall)
LangString CloseSecondLifeUnInstDP ${LANG_RUSSIAN} "Ожидаю завершения работы ${APPNAME}..."
LangString CloseSecondLifeUnInstMB ${LANG_RUSSIAN} "${APPNAME} уже работает, выполнить удаление невозможно.$\n$\nЗавершите текущую операцию и нажмите кнопку «OK», чтобы закрыть ${APPNAME} и продолжить удаление.$\nНажмите кнопку «ОТМЕНА» для отказа от удаления."

; CheckNetworkConnection
LangString CheckNetworkConnectionDP ${LANG_RUSSIAN} "Проверка подключения к сети..."

; removecachefiles
LangString RemoveCacheFilesDP ${LANG_RUSSIAN} "Удаление файлов кэша из папки «Documents and Settings»"

; delete program files
LangString DeleteProgramFilesMB ${LANG_RUSSIAN} "В каталоге программы ${APPNAME} остались файлы.$\n$\nВероятно, это файлы, созданные или перемещенные вами в $\n$INSTDIR$\n$\nУдалить их?"

; uninstall text
LangString UninstallTextMsg ${LANG_RUSSIAN} "Программа ${APPNAME} ${VERSION_LONG} будет удалена из вашей системы."
