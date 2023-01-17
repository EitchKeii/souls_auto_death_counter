@ECHO OFF
@MKDIR bin 2>nul
@MKDIR obj 2>nul
cl /nologo souls_death_counter.c /Febin\souls_death_counter.exe /Foobj\ /link
IF %ERRORLEVEL% NEQ 0 (echo Error:%ERRORLEVEL% && exit)