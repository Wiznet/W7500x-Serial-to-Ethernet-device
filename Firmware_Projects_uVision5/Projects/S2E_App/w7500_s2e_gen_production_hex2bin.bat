copy /b /y obj\W7500x_S2E_App.bin bin\
copy /b /y obj\W7500x_S2E_App.hex bin\
copy /b /y ..\S2E_Boot\bin\W7500x_S2E_Boot.hex + bin\W7500x_S2E_App.hex ..\W7500x_S2E_Production.hex
..\hex2bin.exe ..\W7500x_S2E_Production.hex

