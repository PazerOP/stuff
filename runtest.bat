@ECHO OFF

SETLOCAL

RMDIR /S /Q out_test
MKDIR out_test
CD out_test

ECHO Configuring...
cmake ../ -G Ninja -DBUILD_SHARED_LIBS=ON -DBUILD_TESTING=OFF || EXIT /b

ECHO Building...
cmake --build . --config Debug --target install || EXIT /b

@REM ECHO Running CTest...
@REM ctest --output-on-failure || EXIT /b

ENDLOCAL
