# Лаунчер UnReal World

Исходники GUI-лаунчера0.20: ENG/RUS/FIN, подписи English/Русский/Suomi,
настройки ConsoleMode0/1/2 и LoggingEnabled0/1.

Сборка: Visual Studio 2022, компонент Desktop development with C++, Windows SDK.
В x86 Native Tools Command Prompt запустите `build.cmd`. Результат —
`URW_Launcher.exe`. Отдельная установка игры для сборки не нужна.

Для игры скопируйте EXE к `urw.exe`. Для RUS/FIN также нужны `URWTrace.exe`,
`URWTraceHook.dll` из соседнего проекта UnRealWorld_HookTranslate, каталоги
языков и общий `URWTrace.ini` из UnRealWorld_MultiTranslate той же версии.
Шаблон INI — `config/URWTrace.ini`, публичные консоль/журналы0/0.

Лаунчер только запускает игру/загрузчик и управляет настройками. Код
внедрения DLL находится во втором проекте. Состояние лицензии нашего кода
описано в LICENSE_SCOPE.txt. Локальные EXE, журналы и игра в Git не включаются.
