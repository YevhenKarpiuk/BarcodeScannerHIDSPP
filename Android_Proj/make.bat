@if NOT DEFINED JAVA_HOME (
@echo Please set enviroment variable JAVA_HOME. For example "c:\android studio\jre"
goto :eof
)
@if NOT DEFINED ANDROID_HOME (
@echo Please set enviroment variable ANDROID_HOME. For example "c:\android\sdk"
goto :eof
)
@if NOT DEFINED ANDROID_NDK (
@echo Please set enviroment variable ANDROID_NDK. For example "c:\android\sdk\ndk-bundle"
goto :eof
)

set COMPONENT_NAME=ka_soft_BarcodeScannerHID
set ADDIN_BUILD_TYPE=release

@rem ==== Build Java Code ====

call gradlew.bat clean assemble%ADDIN_BUILD_TYPE%

if not %errorlevel%==0 exit /b %errorlevel%

@rem ==== Build C/C++ Code ====
call %ANDROID_NDK%\ndk-build.cmd

if not %errorlevel%==0 exit /b %errorlevel%

@rem ==== copy libraries ====
@echo "%~dp0publish"
call mkdir "%~dp0publish"              2>nul
call mkdir "%~dp0publish\armeabi-v7a"  2>nul
call mkdir "%~dp0publish\arm64-v8a"    2>nul
call mkdir "%~dp0publish\x86"          2>nul
call mkdir "%~dp0publish\x86_64"       2>nul

call copy /Y "%~dp0addin\build\outputs\apk\release\addin-release-unsigned.apk" "%~dp0publish\%COMPONENT_NAME%.apk"
call copy /Y "%~dp0libs\armeabi-v7a\lib%COMPONENT_NAME%.so" "%~dp0publish\armeabi-v7a\lib%COMPONENT_NAME%_ARM.so"
call copy /Y "%~dp0libs\arm64-v8a\lib%COMPONENT_NAME%.so" "%~dp0publish\arm64-v8a\lib%COMPONENT_NAME%_ARM64.so"
call copy /Y "%~dp0libs\x86\lib%COMPONENT_NAME%.so" "%~dp0publish\x86\lib%COMPONENT_NAME%_x86.so"
call copy /Y "%~dp0libs\x86_64\lib%COMPONENT_NAME%.so" "%~dp0publish\x86_64\lib%COMPONENT_NAME%_x64.so"
