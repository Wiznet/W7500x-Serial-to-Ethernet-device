copy /b /y obj\W7500x_S2E_Boot.bin bin\
copy /b /y obj\W7500x_S2E_Boot.hex bin\
copy /b /y bin\W7500x_S2E_Boot.hex + ..\S2E_App\bin\W7500x_S2E_App.hex ..\W7500x_S2E_Production.hex
..\hex2bin.exe ..\W7500x_S2E_Production.hex

