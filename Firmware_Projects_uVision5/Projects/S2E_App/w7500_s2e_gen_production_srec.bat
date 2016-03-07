copy /b /y obj\W7500x_S2E_App.bin bin\
copy /b /y obj\W7500x_S2E_App.hex bin\
..\sRecord\srec_cat.exe ..\S2E_Boot\bin\W7500x_S2E_Boot.hex -intel bin\W7500x_S2E_App.hex -intel -o ..\W7500x_S2E_Production.hex -intel
..\sRecord\srec_cat.exe ..\W7500x_S2E_Production.hex -intel -o ..\W7500x_S2E_Production.bin -binary