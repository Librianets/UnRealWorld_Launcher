# Лаунчер UnReal World

Исходники GUI-лаунчера0.24: ENG/RUS/FIN, подписи English/Русский/Suomi,
настройки ConsoleMode0/1/2 и LoggingEnabled0/1.

Сборка: Visual Studio 2022, компонент Desktop development with C++, Windows SDK.
В x86 Native Tools Command Prompt запустите `build.cmd`. Результат —
`URW_Launcher.exe`. Отдельная установка игры для сборки не нужна.

Для игры скопируйте EXE к `urw.exe`. Для RUS/FIN также нужны `URWTrace.exe`,
`URWTraceHook.dll` из соседнего проекта UnRealWorld_HookTranslate, каталоги
языков и общий `URWTrace.ini` из UnRealWorld_MultiTranslate той же версии.
Шаблон INI — `config/URWTrace.ini`, публичные консоль/журналы0/0.

Лаунчер запускает игру/загрузчик, управляет настройками и установкой режима Steam. Код
внедрения DLL находится во втором проекте. Состояние лицензии нашего кода
описано в LICENSE_SCOPE.txt. Локальные EXE, журналы и игра в Git не включаются.

## Запуск из Steam (0.24)

В блоке «Запуск из Steam» выберите язык, нажмите «Сменить язык игры», затем
«Включить». Запускайте кнопкой Steam. Оригинал — urw-original.exe, на месте
urw.exe — копия загрузчика; отдельный URWTrace.exe остаётся рядом. Все три
прямые кнопки запуска блокируются. Язык меняется отдельно при закрытой игре;
English запускается без DLL. Отключение режима: «Вернуть оригинальный запуск».

«Восстановить после обновления» сохраняет новый EXE Steam и снова ставит
загрузчик. Неизвестная версия отклоняется. Откат не заменяет новый EXE старым.
Отличающаяся старая копия сохраняется с SHA в имени. URWSteam.state вручную
не удаляйте. Проверяется блокировка файлов/запущенная игра.

Сначала соберите загрузчик HookTranslate, затем GUI: build.cmd берёт его SHA
из локального URWTrace.exe либо соседнего проекта. Если файла нет, заголовок
содержит SHA опубликованного bootstrap0.24. Произвольные версии не смешивать.

##0.24 / 26 October2026

Linen, hemp and nettle yarn now belong to the shared item grammar, including
length suffixes and item descriptions. The new entries are editable in each
language catalogue. Game identifiers and crafting logic remain unchanged.
Includes0.23 presentation-only parsing of L inspection and F2 journal blocks.
L was confirmed by a player. Actual F2 quest gameplay needs more feedback.
Unknown spans remain English. No new native hook addresses were added.
