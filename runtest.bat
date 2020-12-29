@ECHO OFF

SETLOCAL

RMDIR /S /Q out_test
MKDIR out_test
CD out_test

ECHO Configuring...
cmake ../ -G Ninja || EXIT /b

ECHO Building...
cmake --build . || EXIT /b

ECHO Running CTest...
ctest --output-on-failure || EXIT /b

ENDLOCAL
