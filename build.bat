@echo off
cls
del /q .\output\pic_mp3_1_0_0_440F256H.hex
del /q .\output\pic_mp3_1_0_0_440F256H.map
del /q .\output\pic_mp3_1_0_0_440F256H.elf
call ./prebuild.bat
.\make\make.exe -f Makefile_440F256H clean
.\make\make.exe -f Makefile_440F256H all -j8
copy .\output\pic_mp3_1_0_0.hex .\output\pic_mp3_1_0_0_440F256H.hex
copy .\output\pic_mp3_1_0_0.map .\output\pic_mp3_1_0_0_440F256H.map
.\make\make.exe -f Makefile_440F256H clean

rem del /q .\output\pic_mp3_1_0_0.hex
rem .\make\make.exe -f Makefile_460F512L clean
rem .\make\make.exe -f Makefile_460F512L all -j8
rem copy .\output\pic_mp3_1_0_0.hex .\output\pic_mp3_1_0_0_460F512L.hex
rem copy .\output\pic_mp3_1_0_0.map .\output\pic_mp3_1_0_0_460F512L.map
rem .\make\make.exe -f Makefile_460F512L clean
rem 
rem del /q .\output\pic_mp3_1_0_0.hex
rem .\make\make.exe -f Makefile_470F512H clean
rem .\make\make.exe -f Makefile_470F512H all -j8
rem copy .\output\pic_mp3_1_0_0.hex .\output\pic_mp3_1_0_0_470F512H.hex
rem copy .\output\pic_mp3_1_0_0.map .\output\pic_mp3_1_0_0_470F512H.map
rem .\make\make.exe -f Makefile_470F512H clean
rem 
rem del /q .\output\pic_mp3_1_0_0.hex
rem .\make\make.exe -f Makefile_470F512L clean
rem .\make\make.exe -f Makefile_470F512L all -j8
rem copy .\output\pic_mp3_1_0_0.hex .\output\pic_mp3_1_0_0_470F512L.hex
rem copy .\output\pic_mp3_1_0_0.map .\output\pic_mp3_1_0_0_470F512L.map
rem .\make\make.exe -f Makefile_470F512L clean
del /q .\output\pic_mp3_1_0_0.map

del /q .\output\*.d
