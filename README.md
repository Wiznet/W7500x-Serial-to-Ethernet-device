# W7500x Serial-to-Ethernet Device 
Serial to Ethernet gateway (referred to as S2E) module device development resources based on WIZnet W7500x chip

## Concept and Features
 
### WIZnet WIZ107SR S2E module compatible
 - Fully software compatible with WIZ107SR device
 - Can be used same serial AT commands and configuration tools
 - Support WIZ VSP (Virtual Serial Port) program for windows
 - [WIZ107SR Product Page](http://www.wiznet.co.kr/product-item/wiz107sr/)
 
### Extended Benefits
 - Serial Port Separation: debug port and data port
 - Status Pins: PHY link / TCP connection
   - Pin shared with DTR / DSR pins 
 - Expansion 4-GPIOs for users customization


## Software
### Development Environment
  - KEIL uVision5 IDE v5.10

### Firmware
#### Firmware Composition

  - Memory map (128kB Code Flash)
    - Boot: 0x00000000 ~ 0x00006FFF (28kB)
    - App : 0x00007000 ~ 0x000137FF (50kB)
    - App_bak : 0x00013800 ~ 0x00020000 (50kB)

  - Support functions in each area
    - Boot: Production (MAC) / Search and settings via config-tool / Firmware update 
    - App : Main application - S2E function / Search and settings via config-tool / Settings via serial AT command
    - App_bak : Backup area for application binary 

#### Path
  - KEIL uVision5 project file (.uvproj)
    - Boot: W7500x-Serial-to-Ethernet-device/Firmware_Projects_uVision5/Projects/S2E_Boot/ 
    - App : W7500x-Serial-to-Ethernet-device/Firmware_Projects_uVision5/Projects/S2E_App/
    
  - Binary file (.bin)
    - Boot: W7500x-Serial-to-Ethernet-device/Firmware_Projects_uVision5/Projects/S2E_Boot/bin/
    - App : W7500x-Serial-to-Ethernet-device/Firmware_Projects_uVision5/Projects/S2E_App/bin/
    - Production : W7500x-Serial-to-Ethernet-device/Firmware_Projects_uVision5/Projects/
      - Production firmware = App + Boot

#### Firmware update method
  - Application firmware binary: Update using configuration tool only
    - Available bin file for: W7500x_S2E_App.bin
    - Download: [WIZ107SR /WIZ108SR Config Tool for Windows](http://www.wiznet.co.kr/wp-content/uploads/wiznethome/S2E%20Module/WIZ107_108SR/Utility/WIZ107_108_config_tool.zip)

  - Boot and Production firmware binary: Update using ISP program
    - Available bin file for: W7500x_S2E_Boot.bin / W7500x_S2E_Production.bin
    - Guide Docs: [How to write the firmware into WIZwiki-W7500ECO](http://wizwiki.net/wiki/doku.php?id=products:wizwiki_w7500eco:start_getting_started:write_firmware) 
    - Download: [W7500 ISP Program for Windows](http://wizwiki.net/wiki/lib/exe/fetch.php?media=products:wizwiki_w7500:w7500_isp_20150820_.zip)

  
## Hardware
### Technical Reference
  - Schematic
  - Demension


## Development
### Target Board
#### WIZ750SR

<!-- WIZ750SR Board pic -->

##### Features
 - MCU: W7500P chip
   - ARM Cortex-M0 Core, up to 48MHz, 128kB Flash / 16kB RAM
   - Hardwired TCP/IP Core, 32KB RAM for TCP/IP (Can be extended to system RAM)
   - Peripherals (GPIO, ADC, SWD, Timer/PWM, UART, SPI, I2C and Etc.)
   - 10/100 Ethernet MAC and PHY
