@echo off
cls
del /q .\output\USBHIDBootloader_440F256H.hex
.\make\make.exe -f Makefile_440F256H clean
.\make\make.exe -f Makefile_440F256H all -j8
copy .\output\USBHIDBootloader.hex .\output\USBHIDBootloader_440F256H.hex
copy .\output\USBHIDBootloader.map .\output\USBHIDBootloader_440F256H.map
.\make\make.exe -f Makefile_440F256H clean
del /q .\output\*.d

del /q .\output\USBHIDBootloader_470F512H.hex
.\make\make.exe -f Makefile_470F512H clean
.\make\make.exe -f Makefile_470F512H all -j8
copy .\output\USBHIDBootloader.hex .\output\USBHIDBootloader_470F512H.hex
copy .\output\USBHIDBootloader.map .\output\USBHIDBootloader_470F512H.map
.\make\make.exe -f Makefile_470F512H clean
del /q .\output\*.d

del /q .\output\USBHIDBootloader_460F512L.hex
.\make\make.exe -f Makefile_460F512L clean
.\make\make.exe -f Makefile_460F512L all -j8
copy .\output\USBHIDBootloader.hex .\output\USBHIDBootloader_460F512L.hex
copy .\output\USBHIDBootloader.map .\output\USBHIDBootloader_460F512L.map
.\make\make.exe -f Makefile_460F512L clean
del /q .\output\USBHIDBootloader.map
del /q .\output\*.d

del /q .\output\USBHIDBootloader_470F512L.hex
.\make\make.exe -f Makefile_470F512L clean
.\make\make.exe -f Makefile_470F512L all -j8
copy .\output\USBHIDBootloader.hex .\output\USBHIDBootloader_470F512L.hex
copy .\output\USBHIDBootloader.map .\output\USBHIDBootloader_470F512L.map
.\make\make.exe -f Makefile_470F512L clean
del /q .\output\USBHIDBootloader.map
del /q .\output\*.d
