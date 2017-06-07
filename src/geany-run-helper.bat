REM USAGE: geany-run-helper DIRECTORY AUTOCLOSE COMMAND...

REM unnecessary, but we get the directory
cd %1
shift
REM save autoclose option and remove it
set autoclose=%1
shift

REM spawn the child
REM it's tricky because shift doesn't affect %*, so hack it out
REM https://en.wikibooks.org/wiki/Windows_Batch_Scripting#Command-line_arguments
set SPAWN=
:argloop
if -%1-==-- goto argloop_end
	set SPAWN=%SPAWN% %1
	shift
goto argloop
:argloop_end
%SPAWN%

REM show the result
echo:
echo:
echo:------------------
echo:(program exited with code: %ERRORLEVEL%)
echo:

REM and if wanted, wait on the user
if not %autoclose%==1 pause
