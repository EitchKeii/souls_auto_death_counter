@ECHO OFF
cl /nologo souls_death_counter.c
IF %ERRORLEVEL% NEQ 0 (echo Error:%ERRORLEVEL% && exit)