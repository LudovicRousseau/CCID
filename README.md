USB CCID IFD Handler
====================

This package provides the source code for a generic USB CCID (Chip/Smart
Card Interface Devices) and ICCD (Integrated Circuit(s) Card Devices)
driver. See the USB CCID [1] and ICCD [2] specifications from the USB
working group.

* [1] https://www.usb.org/sites/default/files/DWG_Smart-Card_CCID_Rev110.pdf
* [2] https://www.usb.org/sites/default/files/DWG_Smart-Card_USB-ICC_ICCD_rev10.pdf

Authors:
========

- Ludovic Rousseau <ludovic.rousseau@free.fr>
- Carlos Prados for the PPS and ATR parsing code (taken from his
  towitoto driver) in towitoko/ directory.
- Olaf Kirch for the T=1 TPDU code (from the OpenCT package) in openct/
  directory. I (Ludovic Rousseau) greatly improved this code.


CCID and ICCD readers:
======================

A reader can be in one of these list:
- supported
    See https://ccid.apdu.fr/ccid/supported.html
- should work
    See https://ccid.apdu.fr/ccid/shouldwork.html
- unsupported
    See https://ccid.apdu.fr/ccid/unsupported.html
- disabled
    See https://ccid.apdu.fr/ccid/disabled.html


Supported operating systems:
============================

- GNU/Linux (libusb 1.0)
- MacOS X/Darwin (libusb 1.0)

See also https://ccid.apdu.fr/ for more information.


Debug information:
==================

The driver uses the debug function provided by pcscd. So if pcscd sends
its debug to stdout (`pcscd --foreground`) then the CCID driver will also
send its debug to stdout. If pcscd sends its debug to syslog (by
default) then the CCID driver will also send its debug to syslog.

You can change the debug level using the `Info.plist` configuration file.
The `Info.plist` file is installed, by default, in
`/usr/local/pcsc/drivers/ifd-ccid.bundle/Contents/Info.plist`
or set the environment variable `LIBCCID_ifdLogLevel`.

The debug level is set in the `ifdLogLevel` field. It is a binary OR
combination of 4 different levels.
- 1: critical: important error messages
- 2: info:     informative messages like what reader was detected
- 4: comm:     a dump of all the bytes exchanged between the host and the
               reader
- 8: periodic: periodic info when pcscd test if a card is present (every
               1/10 of a second)

By default the debug level is set to 3 (1 + 2) and correspond to the
critical and info levels.

You have to restart the driver so it reads the configuration file again
and uses the new debug level value.  To restart the driver you just need
to unplug all your CCID readers so the driver is unloaded and then replug
your readers. You can also restart pcscd.


Voltage selection
=================

You can change the voltage level using the `Info.plist` configuration
file.  The `Info.plist` is installed, by default, in
`/usr/local/pcsc/drivers/ifd-ccid.bundle/Contents/Info.plist`

The voltage level is set in the `ifdDriverOptions` field. It is a binary OR
combination of 4 different levels.

-  0: power on the card at 5V (default value)
- 16: power on the card at 3V and, if 3V fails then use 5V
- 32: power on the card at 1.8V, then 3V and then 5V
- 48: let the reader decide

By default the voltage level is set to 0 and corresponds to 5V.

You have to restart the driver so it reads the configuration file again
and uses the new voltage level value.  To restart the driver you just need
to unplug all your CCID readers so the driver is unloaded and then replug
your readers.  You can also restart pcscd.


Licence:
========

  This library is free software; you can redistribute it and/or modify it
under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation; either version 2.1 of the License, or (at
your option) any later version.

  This library is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser
General Public License for more details.

  You should have received a copy of the GNU Lesser General Public License
along with this library; if not, write to the Free Software Foundation,
Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.


History:
========

1.5.1 - 14 November 2022, Ludovic Rousseau
   - Add support of
     - Access IS ATR220 with idProduct: 0x0184
     - Alcor Link AK9567
     - Alcor Link AK9572
     - BLUTRONICS TAURUS NFC
     - CHERRY SmartTerminal ST-1144
     - CREATOR CRT-603(CZ1) CCR
     - Dexon Tecnologias Digitais LTDA DXToken
     - ESMART Reader ER433x ICC
     - ESMART Reader ER773x Dual & 1S
     - Flight system consulting Incredist
     - Ledger Nano S
     - Ledger Nano S Plus
     - Ledger Nano SP
     - Ledger Nano X
     - SafeNet eToken Fusion
     - Sensyl SSC-NFC Reader
   - Adjust USB drivers path at run-time via environment variable PCSCLITE_HP_DROPDIR
   - configure.ac: add --enable-strict option
   - Fix a problem with AUTO PPS readers and ATR convention inverse cards
   - examples/scardcontrol:
    - add support of 6A xx error codes
    - check WinSCard error early
    - parse wLcdLayout & bEntryValidationCondition
   - macOS: log non sensitive strings as "%{public}s"
   - Some other minor improvements

1.5.0 - 27 January 2022, Ludovic Rousseau
   - Add support of
     - ACS ACR1281U
     - Circle CCR7125 ICC
     - Circle CIR125 ICC
     - Circle CIR125-DOT ICC
     - Circle CIR215 CL with iProduct 0x2100
     - Circle CIR315 DI
     - Circle CIR315 with idProduct: 0x0324
     - Circle CIR315 with idProduct: 0x7004
     - Circle CIR415 CL
     - Circle CIR515 ICC
     - Circle CIR615 CL
     - Circle CIR615 CL & 1S
     - ELYCTIS CL reader
     - Nitrokey Nitrokey 3
     - Thales Shield M4 Reader
   - Add support of simultaneous slot access on multi slots readers
   - Use FeliCa instead of Felica on SONY request
   - Fix SafeNet eToken 5110 SC issue
   - Allow vendor control commands for Omnikey 5427 CK
   - always compute readTimeout to use a value greater than default 3 seconds
   - Check the bSeq value when receiving a CCID frame
   - Avoid logging errors when a reader is removed
   - Some other minor improvements

1.4.36 - 30 August 2021, Ludovic Rousseau
   - Add support of
     - Lenovo Lenovo Smartcard Wired Keyboard II
     - REINER SCT tanJack USB
     - SafeNet eToken 5110+ FIPS
     - SafeNet eToken 5300 C
     - jSolutions s.r.o. Multi SIM card reader 4/8
   - parse: fix check when bNumDataRatesSupported = 0

1.4.35 - 25 July 2021, Ludovic Rousseau
   - Add support of
     - ArkSigner Connect2Sign
     - Circle CCR7115 ICC
     - Circle CCR7315
     - Circle CIR215 CL
     - Circle CIR215 PICC
     - Circle CIR315
     - Circle CIR315 (idProduct: 0x3100)
     - Circle CIR315 CL
     - Circle CIR315 Dual & 1S
     - Circle CIR415 CL & 1S
     - Circle Idaxis SecurePIV
     - DUALi DE-ABCM6 RFRW
     - Feitian R701
     - Generic EMV Smartcard Reader (0x058C:0x9590)
     - INMAX DWR18 HC
     - INMAX DWR18 HPC
     - Identiv Identiv uTrust 4711 F CL + SAM Reader
     - Identiv uTrust 3721 Contactless Reader
     - Infocrypt HWDSSL DEVICE
     - Infocrypt Token++ lite
     - MK Technology KeyPass D1
     - SONY Felica RC-S300/P
     - SONY Felica RC-S300/S
     - SONY Felica RC-S660/U
     - SYNNIX CL-2100R
     - SoloKeys Solo 2
     - Spyrus Inc PocketVault P-3X (idProduct: 0x3203)
  - parse: use "ICCD token" for ICCD tokens
  - Support 4 card slots with Feitian R502 C9
  - ccid_usb: ask for bNumDataRatesSupported data rates
  - Solve a performance issue with T=1 and CCID_CLASS_AUTO_PPS_PROP
  - Fix a possible buffer overflow in T0ProcACK
  - IFDHSetProtocolParameters: set IFSC/IFSD only for TPDU readers
  - CCID serial: Reset buffers on failed read
  - Fix yylex missing symbol
  - Gemalto pinpad: fix incorrect bEntryValidationCondition for
    SecurePINVerify and SecurePINModify
  - Fix bit4id miniLector-EVO pinpad support
  - The Kobil TriBank reader does NOT support extended APDU

1.4.34 - 24 January 2021, Ludovic Rousseau
   - Add support of
     - ACS ACR1252IMP Reader
     - ACS CryptoMate EVO
     - Aktiv Rutoken SCR 3001 Reader
     - Avtor KP-375BLE
     - Avtor SC Reader KP382
     - BIT4ID mLector AIR DI V3
     - BIT4ID miniLector AIR NFC v3
     - Bit4id Digital-DNA Key (ProductID 0x2354)
     - Canokeys Canokey
     - DESKO GmbH IDenty chrom
     - DESKO GmbH PENTA Scanner
     - FT Biopass CCID
     - FT Biopass FIDO2
     - FT Biopass KB CCID
     - FT Biopass KB FIDO CCID
     - Feitian BLE CCID Dongle
     - Feitian R805
     - Feitian vR504 Contactless Reader
     - GoTrust Idem Key
     - Identiv uTrust 3720 Contactless Reader
     - Sunrex HP USB Business Slim Smartcard CCID Keyboard
     - sysmocom - s.f.m.c. GmbH sysmoOCTSIM
   - Fail if the requested protocol is not supported by reader
   - Disable USB suspend for the AlcorMicro AU9520 reader
   - Return "no smart card" if we get notified during a transmit
   - Minor improvements reported by Maksim Ivanov
   - Some other minor improvements


1.4.33 - 25 June 2020, Ludovic Rousseau
   - Add support of
     - Genesys Logic CCID Card Reader (idProduct: 0x0771)
     - Swissbit Secure USB PU-50n SE/PE
     - TOPPAN FORMS CO.,LTD TC63CUT021
   - add --enable-oslog argument for macOS
     use os_log(3) for macOS >= 10.12 (Sierra)
   - Update PCSC submodule to get Unicode support
   - Some minor improvements


1.4.32 - 22 April 2020, Ludovic Rousseau
   - Add support of
     - AF Care One (idProduct: 0xAFC0)
     - AF Care One (idProduct: 0xAFC1)
     - AF Care Two (idProduct: 0xAFC2)
     - AF Care Two (idProduct: 0xAFC3)
     - Access IS ATR210
     - Access IS ATR220
     - Cherry GmbH CHERRY SECURE BOARD 1.0
     - Doctolib SR with idProduct: 0xAFD0
     - Doctolib SR with idProduct: 0xAFD1
     - Doctolib SR with idProduct: 0xAFD2
     - Doctolib SR with idProduct: 0xAFD3
     - F-Secure Foundry USB Armory Mk II
     - Gemalto RF CR5400
     - Ledger Nano X support
     - Purism, SPC Librem Key
     - SPECINFOSYSTEMS DIAMOND HSM
     - SPECINFOSYSTEMS DIAMOND PLUS token
     - SPECINFOSYSTEMS DIAMOND PRO token
     - SpringCard E518 (idProduct: 0x6112)
     - SpringCard E518 (idProduct: 0x611A)
     - SpringCard H518 (idProduct: 0x6122)
     - SpringCard H518 (idProduct: 0x612A)
     - SpringCard Puck
     - SpringCard Puck (dProduct: 0x613A)
     - SpringCard SpringCore (idProduct: 0x6012)
     - SpringCard SpringCore (idProduct: 0x601A)
     - Sysking MII136C
   - Add SCardGetAttrib(.., SCARD_ATTR_CHANNEL_ID, ..) for USB devices
   - Increase the timeout used to detect the Identiv uTrust 3700/3701 F readers
   - Fix PowerOn bug for ICCD type A & B devices
   - Fix "Bus Error" on SPARC64 CPU and Solaris C compiler
   - Cherry KC 1000 SC
     - Add support of min & max PIN size
     - Fix a bNumberMessage issue
   - Add support of min & max PIN size for the Omnikey 3821
   - Disable pinpad for Chicony HP Skylab USB Smartcard Keyboard
   - Some minor improvements


1.4.31 - 10 August 2019, Ludovic Rousseau
   - Add support of
     - ACS ACR1252 Reader
     - Aladdin R.D. JaCartaReader
     - Alcor Link AK9563
     - AvestUA AvestKey
     - Avtor SecureToken (idProduct: 0x0020)
     - Bit4id TokenME EVO v2
     - Bit4id miniLector AIR EVO
     - Bit4id miniLector Blue
     - Broadcom Corp 58200 (idProduct: 0x5843)
     - Broadcom Corp 58200 (idProduct: 0x5844)
     - Broadcom Corp 58200 (idProduct: 0x5845)
     - Certgate GmbH ONEKEY ID 2 USB
     - HID Global Crescendo Key 0x0028
     - HID Global Crescendo Key 0x0029
     - HID Global Crescendo Key 0x002B
     - HID Global Crescendo Key 0x002D
     - Identiv SCR3500 C Contact Reader
     - InfoCert WirelessKey
     - NXP PN7462AU CCID
     - Route1 MobiKEY Fusion3
     - SPECINFOSYSTEMS DIAMOND token
   - MacOSX/configure: fix checking error for dynamic library libusb
   - Some minor improvements for debug


1.4.30 - 19 September 2018, Ludovic Rousseau
   - The project moved to https://ccid.apdu.fr/
   - Add support of
     - ACS ACR33 ICC Reader
     - BIFIT ANGARA
     - Broadcom Corp 58200
     - Certgate GmbH AirID 2 USB
     - DC.Ltd DC4 5CCID READER
     - Genesys Logic CCID Card Reader
     - Genesys Logic Combo Card Reader
     - InfoThink IT-500U Reader
     - Spyrus Inc WorkSafe Pro (ProductID 0x3117)
   - Disabled readers
     - REINER SCT cyberJack RFID standard
   - Update reader names for
     - Fujitsu Keyboard KB100 SCR
     - Fujitsu Keyboard KB100 SCR eSIG
     - FujitsuTechnologySolutions GmbH Keyboard KB SCR2
     - Yubico YubiKey CCID
     - Yubico YubiKey FIDO+CCID
     - Yubico YubiKey OTP+CCID
     - Yubico YubiKey OTP+FIDO+CCID
   - Fix libusb config descriptor leak
   - Fix leaking an allocated bundle in case no matching reader was found


1.4.29 - 21 February 2018, Ludovic Rousseau
   - Add support of
     - Access IS NFC Smart Module (With idProduct 0x0164)
     - Bit4id Digital-DNA Key
     - Bit4id Digital-DNA Key BT
     - Bluink Ltd. Bluink CCID
     - Chicony HP Skylab USB Smartcard Keyboard
     - HID Global OMNIKEY 5023 Smart Card Reader
     - HID Global OMNIKEY 5027CK CCID CONFIG IF
     - KeyXentic Inc. KX906 Smart Card Reader
     - Spyrus Inc Rosetta USB
     - Spyrus Inc WorkSafe Pro
     - Watchdata USB Key (idProduct: 0x0418)
   - The C3PO LTC31 v2 wrongly declares PIN support
   - Remove extra EGT patch because if has bad side effects


1.4.28 - 11 October 2017, Ludovic Rousseau
   - Add support of
     - Athena IDProtect Flash
     - Elatec TWN4/B1.06/CPF3.05/S1SC1.32/P (Beta 3)
     - HID Global OMNIKEY 5122 Dual
     - HID Global OMNIKEY 5122 Smartcard Reader
     - IIT E.Key Crystal-1
     - KRONEGGER Micro Core Platform
     - KRONEGGER NFC blue Reader Platform
     - Ledger Nano S
     - REINER SCT cyberJack RFID standard
     - REINER SCT cyberJack one
     - SAFETRUST SABRE SCR
     - SafeNet eToken 5300
     - Unicept GmbH AirID USB Dongle
     - Watchdata USB Key
     - mCore SCard-Reader
   - Disabled readers
     - Jinmuyu Electronics Co., Ltd. MR800
   - Fix non-pinpad HID global devices
   - udev rules:
     - allow rule overwrite
     - Disable USB autosuspend on C3PO LTC31 v1 reader
   - Some minor improvements


1.4.27 - 21 May 2017, Ludovic Rousseau
   - Add support of
     - ACS ACR1255U-J1
     - ACS CryptoMate (T2)
     - ANCUD CCID USB Reader & RNG
     - DUALi DE-620 Combi
     - FT CCID
     - FT CCID KB
     - FT U2F CCID KB
     - FT U2F CCID KBOARD
     - HID Global OMNIKEY 5422 Smartcard Reader
     - InfoThink IT-102MU Reader
     - Kapsch TrafficCom USB SAM reader
     - MK Technology KeyPass S1
     - Mulann PVT
     - Regula RFID Reader
     - Spyrus Inc PocketVault P-3X
     - Unicept GmbH AirID USB
   - Add Microchip SEC1210 UART support (when connected on a serial port)
   - Add Zero Length Packet (ZLP) support for Gemalto IDBridge CT30 and K30
      enable the patch using ./configure --enable-zlp
   - Add support of HID Omnikey 5422 as multi slot reader (for macOS)
   - Escape command: signals buffer overflow instead of silently
      truncating the buffer
   - Fix a bug with multi readers and pcscd uses hotplug_libusb (not the
      recommended configuration)
   - Some minor improvements


1.4.26 - 7 January 2017, Ludovic Rousseau
   - Add support of
     - Bit4id Digital DNA Key
     - Bit4id tokenME FIPS v3
     - INGENICO Leo
     - appidkey GmbH ID60-USB
   - PowerOn: the default algorithm is now 5V then 1.8V then 3V then fail.
      It is still possible to change the initial voltage in the
      Info.plist file.  Now, in any case, all the values are tried
      before failing.
   - Negotiate maximum baud rate when bNumDataRatesSupported = 0
   - Some minor improvements


1.4.25 - 30 September 2016, Ludovic Rousseau
   - Add support of
     - Aladdin R.D. JaCarta (idProduct: 0x0402)
     - Broadcom Corp 5880 (idProduct: 0x5832)
     - Broadcom Corp 5880 (idProduct: 0x5833)
     - Broadcom Corp 5880 (idProduct: 0x5834)
     - ESMART Token GOST X2 ET1020-A
     - Feitian VR504 VHBR Contactless & Contact Card Reader
     - Feitian bR500
     - Gemalto K50
     - appidkey GmbH ID100-USB  SC Reader
     - appidkey GmbH ID50 -USB
   - Remove suport of
     - Broadcom Corp 5880 (idProduct: 0x5800)
     - Broadcom Corp 5880 (idProduct: 0x5805)
     - KEBTechnology KONA USB SmartCard
   - macOS: Fix composite device enumeration
   - Fix crash with GemCore Pos Pro and GemCore Sim Pro
   - Some minor improvements


1.4.24 - 22 May 2016, Ludovic Rousseau
   - Add support of
     - Generic USB Smart Card Reader
     - Giesecke & Devrient GmbH StarSign CUT S
     - HID AVIATOR Generic
   - better support of Elatec TWN4 SmartCard NFC
   - better support of SCM SCL011
   - betetr support of HID Aviator generic
   - fix SCARD_ATTR_VENDOR_IFD_SERIAL_NO attribute size
   - fix a race condition on card events with multiple readers
   - Some minor improvements


1.4.23 - 20 April 2016, Ludovic Rousseau
   - Add support of
     - ACS ACR3901U ICC Reader
     - Alcor Micro AU9560
     - Cherry SmartTerminal XX44
     - HID Global OMNIKEY 3x21 Smart Card Reader
     - HID Global OMNIKEY 5022 Smart Card Reader
     - HID Global OMNIKEY 6121 Smart Card Reader
     - IonIDe Smartcard Reader reader
     - KACST HSID Reader
     - KACST HSID Reader Dual Storage
     - KACST HSID Reader Single Storage
   - Remove support of
     - VMware Virtual USB CCID
   - Do NOT add support of
     - DUALi DE-ABCM6
   - Fix a busy loop consuming 100% of CPU for some composite USB devices
      impacted readers: Yubico Yubikey NEO U2F+CCID and Broadcom BCM5880
   - Remove support of (unused) option DRIVER_OPTION_RESET_ON_CLOSE
   - log libusb error name instead of decimal value
   - Some minor improvements


1.4.22 - 10 January 2016, Ludovic Rousseau
   - Add support of
     - Aktiv Rutoken PINPad 2
     - Aladdin R.D. JC-WebPass (JC600)
     - Aladdin R.D. JCR-770
     - Aladdin R.D. JaCarta
     - Aladdin R.D. JaCarta Flash
     - Aladdin R.D. JaCarta LT
     - Aladdin R.D. JaCarta U2F (JC602)
     - Athena ASEDrive IIIe Combo Bio PIV
     - Athena ASEDrive IIIe KB Bio PIV
     - GEMALTO CT1100
     - GEMALTO K1100
     - Hitachi, Ltd. Hitachi Biometric Reader
     - Hitachi, Ltd. Hitachi Portable Biometric Reader
     - Nitrokey Nitrokey Storage
     - THURSBY SOFTWARE TSS-PK1
     - Thursby Software Systems, Inc. TSS-PK7
     - Thursby Software Systems, Inc. TSS-PK8
   - Patch for Microchip SEC1110 reader on Mac OS X (card events notification)
   - Patch for Cherry KC 1000 SC (problem was with a T=1 card and case 2 APDU)
   - Fix support of FEATURE_MCT_READER_DIRECT for the Kobil mIDentity
      visual reader
   - Set timeout to 90 sec for PPDU (Pseudo APDU) commands. This change
      allows the use of a Secure Verify command sent as a PPDU through
      SCardTransmit().
   - Fix a crash when reader reader initialization failed
   - Fix initialization bug with Gemalto Pinpad reader on Mac OS X
   - Some minor bugs fixed


1.4.21 - 21 October 2015, Ludovic Rousseau
   - Add support of
     - ACS ACR1252 Dual Reader
     - Chicony HP USB Smartcard CCID Keyboard JP
     - Chicony HP USB Smartcard CCID Keyboard KR
     - FT ePass2003Auto
     - Feitian bR301 BLE
     - Feitian iR301 (ProductID 0x0619)
     - Feitian iR301 (ProductID 0x061C)
     - Identiv @MAXX ID-1 Smart Card Reader
     - Identiv @MAXX Light2 token
     - Identiv CLOUD 2980 F Smart Card Reader
     - Identiv Identiv uTrust 4701 F Dual Interface Reader
     - Identiv SCR3500 A Contact Reader
     - Identiv SCR3500 B Contact Reader
     - Identiv SCR35xx USB Smart Card Reader
     - Identiv uTrust 2900 R Smart Card Reader
     - Identiv uTrust 2910 R Smart Card Reader
     - Identiv uTrust 2910 R Taglio SC Reader
     - Identiv uTrust 3512 SAM slot Token
     - Identiv uTrust 3522 embd SE RFID Token
     - Identiv uTrust 3700 F CL Reader
     - Identiv uTrust 3701 F CL Reader
     - Identive Identive CLOUD 4000 F DTC
     - Liteon HP SC Keyboard - Apollo (Liteon)
     - Liteon HP SC Keyboard - Apollo JP (Liteon)
     - Liteon HP SC Keyboard - Apollo KR (Liteon)
     - Nitrokey Nitrokey HSM
     - Nitrokey Nitrokey Pro
     - Nitrokey Nitrokey Start
     - Rocketek RT-SCR1
     - VASCO DIGIPASS 875
     - WatchCNPC USB CCID Key
   - Remove support of
     - Crypto Stick Crypto Stick v1.4 is an old version of Nitrokey Nitrokey Pro
     - Free Software Initiative of Japan Gnuk Token is an old version
        of Nitrokey Nitrokey Start
   - Add Feitain R502 dual interface (composite) reader on Mac OS X
   - display a human readable version of the error code returned by
      libusb
   - Mac OS X: wait until libusb/the reader is ready
   - some minor bugs fixed


1.4.20 - 5 August 2015, Ludovic Rousseau
   - Add support of
     - ACS ACR1251 Dual Reader
     - Access IS NFC Smart Module
     - BIFIT iToken
     - BLUTRONICS BLUDRIVE II CCID (idProduct: 0x1079)
     - Generic MultiCard Device
     - NXP Pegoda 2 N
     - SafeNet eToken 5100
     - SafeNet eToken 7300
     - Yubico Yubikey 4 CCID
     - Yubico Yubikey 4 OTP+CCID
     - Yubico Yubikey 4 OTP+U2F+CCID
     - Yubico Yubikey 4 U2F+CCID
   - Depends on libusb version 1.0.9 instead of 1.0.8
   - The O2 Micro Oz776 reader only supports 9600 bps
   - Change installation directory for Mac OS X El Capitan 10.11


1.4.19 - 13 May 2014, Ludovic Rousseau
   - Add support of
     - AK910 CKey (idProduct 0x0001)
     - AK910 CKey (idProduct 0x0011)
     - AK910 IDONE
     - Broadcom Corp 5880 (idProduct: 0x5804)
     - CASTLES EZCCID Smart Card Reader
     - Cherry KC 1000 SC
     - Cherry KC 1000 SC Z
     - Cherry KC 1000 SC/DI
     - Cherry KC 1000 SC/DI Z
     - Cherry TC 1300
     - Chicony USB Smart Card Keyboard
     - Elatec TWN4 SmartCard NFC
     - Feitian 502-CL
     - Feitian eJAVA Token
     - FujitsuTechnologySolutions GmbH Keyboard KB100 SCR
     - FujitsuTechnologySolutions GmbH Keyboard KB100 SCR eSIG
     - Hewlett-Packard HP lt4112 Gobi 4G Module
     - Identive SCT3522CC token
     - OMNIKEY AG 6121 USB mobile
     - PIVKey T800
     - REINER SCT tanJack Bluetooth
     - Watchdata USB Key
   - Add syslog(3) debug for Mac OS X Yosemite.
      Use: sudo syslog -c "com.apple.ifdreader PID" -d to change the logging level.
      See also "Change syslog logging level on Yosemite"
      http://ludovicrousseau.blogspot.com/2015/03/change-syslog-logging-level-on-yosemite.html
   - Remove ZLP patch for Gemalto IDBridge CT30 and K30. The patch was
      causing problems with the K50.  A new reader firmware (version F)
      solved the problem so the patch is no more needed.
   - Fix a memory leak in an error path
   - some minor bugs removed


1.4.18 - 13 September 2014, Ludovic Rousseau
   - Add support of
     - Cherry Cherry TC 1100
     - Cherry Smart Card Reader USB
     - Cherry Smartcard Keyboard G87-1xx44
     - FujitsuTechnologySolutions GmbH Keyboard KB SCR2
     - Lenovo Lenovo USB Smartcard Keyboard
     - Yubico Yubikey NEO OTP+U2F+CCID
     - Yubico Yubikey NEO U2F+CCID
     - eID_R6 001 X8
   - fix support of Omnikey CardMan 3121
   - reduce memory consumed when configured with --enable-embedded
   - prepare the port to UEFI


1.4.17 - 11 June 2014, Ludovic Rousseau
   - Add support of
     - Feitian R502
     - Free Software Initiative of Japan Gnuk Token
     - German Privacy Foundation Crypto Stick v2.0
     - HID Global veriCLASS Reader
     - HID OMNIKEY 5025-CL
     - Identive Technologies Multi-ISO HF Reader - USB
     - OMNIKEY 5421
     - OMNIKEY AG 3121 USB
     - udea MILKO V1.
   - Fix support of O2 Micro Oz776. The reader is limited to 9600 bps
   - some minor bugs removed


1.4.16 - 23 March 2014, Ludovic Rousseau
   - Add support of
     - Crypto Stick Crypto Stick v1.4
     - Hewlett Packard USB Smartcard CCID Keyboard
     - IID AT90S064 CCID READER
     - INSIDE Secure VaultIC 405 Smart Object
     - INSIDE Secure VaultIC 441 Smart Object
     - Microchip SEC1110
     - Microchip SEC1210
     - Watchdata W5181
   - Add support of DRIVER_OPTION_DISABLE_PIN_RETRIES
      The Gemalto pinpad reader sends a VERIFY command with no PIN value
      in order to retreive the remaining retries from the card.  Some
      cards (like the OpenPGP card) do not support this.
      It is now possible to disable this behavior from the Gemalto
      Pinpad and Covadis Véga Alpha.
   - add support of WTX received before SW during Secure Pin Entry Verify
      The Swiss health care card sends a WTX request before returning
      the SW code. If the reader is in TPDU and the card is in T=1 the
      driver must manage the request itself.


1.4.15 - 14 February 2014, Ludovic Rousseau
   - Add support of
     - DUALi DRAGON NFC READER
     - Feitian bR301
     - Gemalto CR30 reader in serial communication
     - Gemalto Ezio Shield Pro SC
     - IIT E.Key Almaz-1C
   - PIN_MODIFY_STRUCTURE & PIN_VERIFY_STRUCTURE: Fix calculation of
      the command length after pcsc-lite 1.8.9 (October 2013) changed
      the PCSC/reader.h header
   - Add specific PIN min (0) & max (25) sizes for SmartTerminal
      ST-2xxx
   - Do not get the data rates if bNumDataRatesSupported = 0
   - Support Gemalto features for pinpad readers MinimumPINSize,
      MaximumPINSize and bEntryValidationCondition are fetched from the
      reader firmware
   - disable (broken) pinpad for Fujitsu SmartCase KB SCR eSIG
   - examples/scardcontrol.c:
     - Parse codes returned by a pinpad (as SW1/SW2)
        Known codes for now are:
        0x9000: Success
        0x6400: Timeout
        0x6401: Cancelled by user
        0x6402: PIN mismatch
        0x6403: Too short or too long PIN
     - Retrieve min and max PIN sizes from the driver
     - Retrieve bEntryValidationCondition from the driver
   - be more strict for bInterfaceClass = 255 by also checking extra_length
   - some minor bugs removed


1.4.14 - 25 November 2013, Ludovic Rousseau
   - Add support of
     - Gemalto GemCore SIM Pro firmware 2.0 (using USB)
   - report FEATURE_IFD_PIN_PROPERTIES only for pinpad readers
   - Generalize the management of (old) readers with bDeviceClass = 0xFF
   - some minor bugs removed


1.4.13 - 9 October 2013, Ludovic Rousseau
   - Add support of
     - Access IS ePassport Reader
     - Planeta RC700-NFC CCID
   - Add support of Windows value for CM_IOCTL_GET_FEATURE_REQUEST
      Windows uses 0x313520 for SCARD_CTL_CODE(3400) pcsc-lite uses
      0x42000D48 for SCARD_CTL_CODE(3400)
      RDP aplications (like rdesktop) will convert SCardControl()
      commands from a Windows application (so using 0x313520) to
      pcsc-lite.
   - fix multi-slot support for card movement notification (introduced
      in 1.4.12)
   - Mac OS X: differentiate each libccid library by the dynamic linker
      using --prefix=/fake/$BUNDLE_ID
   - some minor bugs removed


1.4.12 - 12 August 2013, Ludovic Rousseau
   - Add support of
     - HID OMNIKEY 5127 CK
     - HID OMNIKEY 5326 DFR
     - HID OMNIKEY 5427 CK
     - Ingenico WITEO USB Smart Card Reader (Base and Badge)
     - SecuTech SecuTech Token
   - Add support of card movement notifications for multi-slot readers
   - Check libusb is at least at version 1.0.8
   - Get the serialconfdir value from pcsc-lite pkg config instead of
      using $(DESTDIR)/$(sysconfdir)/reader.conf.d/
   - Disable class driver on Mac OS X
   - Update the bundle name template to include the vendor name
   - some minor bugs removed


1.4.11 - 12 June 2013, Ludovic Rousseau
   - Add support of
       . Gemalto IDBridge CT30
       . Gemalto IDBridge K30
       . SCM Microsystems Inc. SCL010 Contactless Reader
       . SCM Microsystems Inc. SDI011 Contactless Reader
       . THRC reader
   - Better management of time extension requests
   - parse: better support of devices with bInterfaceClass = 0xFF
   - udev rule file: Remove setting group to pcscd, remove support of
      Linux kernel < 2.6.35 for auto power up management
   - some minor bugs removed


1.4.10 - 16 April 2013, Ludovic Rousseau
   - Add support of
       . ACS APG8201 USB Reader with PID 0x8202
       . GIS Ltd SmartMouse USB
       . Gemalto IDBridge K3000
       . Identive CLOUD 2700 F Smart Card Reader
       . Identive CLOUD 2700 R Smart Card Reader
       . Identive CLOUD 4500 F Dual Interface Reader
       . Identive CLOUD 4510 F Contactless + SAM Reader
       . Identive CLOUD 4700 F Dual Interface Reader
       . Identive CLOUD 4710 F Contactless + SAM Reader
       . Inside Secure AT90SCR050
       . Inside Secure AT90SCR100
       . Inside Secure AT90SCR200
       . SCR3310-NTTCom USB SmartCard Reader
       . SafeTech SafeTouch
       . SpringCard H512 Series
       . SpringCard H663 Series
       . SpringCard NFC'Roll
       . Yubico Yubikey NEO CCID
       . Yubico Yubikey NEO OTP+CCID
   - Add support of time extension for Escape commands


1.4.9 - 16 January 2013, Ludovic Rousseau
   - Add support of
       . Aktiv Rutoken PINPad In
       . Aktiv Rutoken PINPad Ex
       . REINER SCT cyberJack go
   - Info.plist: Correctly handle reader names containing &


1.4.8 - 22 June 2012, Ludovic Rousseau
   - Add support of
       . SCR3310-NTTCom USB (was removed in version 1.4.6)
       . Inside Secure VaultIC 420 Smart Object
       . Inside Secure VaultIC 440 Smart Object
   - Wait up to 3 seconds for reader start up
   - Add support of new PC/SC V2 part 10 properties:
        . dwMaxAPDUDataSize
        . wIdVendor
        . wIdProduct
   - Use helper functions from libPCSCv2part10 to parse the PC/SC v2
      part 10 features


1.4.7 - 22 June 2012, Ludovic Rousseau
   - Add support of
       . ACS ACR101 ICC Reader
       . ACS CryptoMate64
       . Alcor Micro AU9522
       . Bit4id CKey4
       . Bit4id cryptokey
       . Bit4id iAM
       . Bit4id miniLector
       . Bit4id miniLector-s
       . CCB eSafeLD
       . Gemalto Ezio Shield Branch
       . KOBIL Systems IDToken
       . NXP PR533
   - KOBIL Systems IDToken special cases:
       . Give more time (3 seconds instead of 2) to the reader to answer
       . Hack for the Kobil IDToken and Geman eID card. The German eID
         card is bogus and need to be powered off before a power on
       . Add Reader-Info-Commands special APDU/command
         - Manufacturer command
         - Product name command
         - Firmware version command
         - Driver version command
   - Use auto suspend for CCID devices only (Closes Alioth bug
      [#313445] "Do not activate USB suspend for composite devices:
      keyboard")
   - Fix some error management in the T=1 TPDU state machine
   - some minor bugs removed
   - some minor improvements added


1.4.6 - 6 April 2012, Ludovic Rousseau
   - Add support of
       . Avtor SC Reader 371
       . Avtor SecureToken
       . DIGIPASS KEY 202
       . Fujitsu SmartCase KB SCR eSIG
       . Giesecke & Devrient StarSign CUT
       . Inside Secure VaultIC 460 Smart Object
       . Macally NFC CCID eNetPad reader
       . OmniKey 6321 USB
       . SCM SDI 011
       . Teridian TSC12xxF
       . Vasco DIGIPASS KEY 101
   - Remove support of readers without a USB CCID descriptor file
       . 0x08E6:0x34C1:Gemalto Ezio Shield Secure Channel
       . 0x08E6:0x34C4:Gemalto Ezio Generic
       . 0x04E6:0x511A:SCM SCR 3310 NTTCom
       . 0x0783:0x0008:C3PO LTC32 USBv2 with keyboard support
       . 0x0783:0x9002:C3PO TLTC2USB
       . 0x047B:0x020B:Silitek SK-3105
   - Disable SPE for HP USB CCID Smartcard Keyboard. The reader is
      bogus and unsafe.
   - Convert "&" in a reader name into "&amp;" to fix a problem on Mac
      OS X
   - Fix a problem with ICCD type A devices. We now wait for device ready
   - Secure PIN Verify and PIN Modify: set the minimum timeout to 90
      seconds
   - Add support of wIdVendor and wIdProduct properties
   - Add support of dwMaxAPDUDataSize
   - Add support of Gemalto firmware features
   - some minor bugs removed


1.4.5 - 11 October 2011, Ludovic Rousseau
   - Add support of
     - Alcor Micro AU9540
     - BIFIT USB-Token iBank2key
     - BIFIT iBank2Key
     - Gemalto Ezio Shield PinPad reader
     - Gemalto SA .NET Dual
     - Precise Sense MC reader (with fingerprint)
     - SDS DOMINO-Key TWIN Pro
     - Ubisys 13.56MHz RFID (CCID)
   - Add support of bPPDUSupport and FEATURE_CCID_ESC_COMMAND
   - SCARD_ATTR_VENDOR_NAME and SCARD_ATTR_VENDOR_IFD_VERSION are not
      the vendor name and version of the driver but of the IFD:
      InterFace Device i.e. the smart card reader.  We then return the
      USB iManufacturer string as SCARD_ATTR_VENDOR_NAME and USB
      bcdDevice as SCARD_ATTR_VENDOR_IFD_VERSION
   - reduce binary size bu removing unused features from simclist
   - Fix some warnings reported bu Coverity


1.4.4 - 13 May 2011, Ludovic Rousseau
   - Add support of
     - Gemalto Ezio CB+
     - Gemalto Ezio Generic
     - Gemalto Ezio Shield
     - Gemalto Ezio Shield PinPad
     - Gemalto Ezio Shield Secure Channel
   - Activate USB automatic power suspend. The Linux kernel should
      power off the reader automatically if it is not used (pcscd is not
      running).
   - Add support of TLV Properties wLcdMaxCharacters and wLcdMaxLines.
      They just duplicate wLcdLayout
   - some minor bugs removed


1.4.3 - 2 April 2011, Ludovic Rousseau
   - Add support of
     - Feitian ePass2003 readers
     - Neowave Weneo
     - SCM SCL011
     - Vasco DIGIPASS 920
   - use :libudev: instead of :libhal: naming scheme.
   - Do not install RSA_SecurID_getpasswd and Kobil_mIDentity_switch
      and the associated documentation.
   - the Secure Pin Entry of the HP USB Smart Card Keyboard is bogus so
      disable it
   - some minor bugs removed


1.4.2 - 22 February 2011, Ludovic Rousseau
   - Add support of
     - ACS APG8201 PINhandy 1
     - Aktiv Rutoken lite readers
     - Feitian SCR310 reader (also known as 301v2)
     - Oberthur ID-ONE TOKEN SLIM v2
     - Vasco DIGIPASS KEY 200
     - Vasco DIGIPASS KEY 860
     - Xiring Leo v2
     - Xiring MyLeo
     - new Neowave Weneo token
   - Add back support of "bogus" Oz776, REINER SCT and BLUDRIVE II
   - Ease detection of OpenCT by pcsc-lite
   - disable use of interrupt card events for multi slots readers (the
      algorithm is bogus and can't be used)
   - fix minor problems detected by the clang tool
   - some minor bugs removed


1.4.1 - 3 December 2010, Ludovic Rousseau
   - Add support of
     - Akasa AK-CR-03, BZH uKeyCI800-K18
     - Free Software Initiative of Japan Gnuk token readers
     - Gemalto Smart Guardian (SG CCID)
     - ReinerSCT cyberJack RFID basis
   - Remove O2 Micro Oz776 and Blutronics Bludrive II CCID since they
      are no more supported since version 1.4.0
   - SecurePINVerify() & SecurePINModify(): Accept big and little
      endian byte orders for multibytes fields. The application
      should not use HOST_TO_CCID_16() and HOST_TO_CCID_32() any more
      and just use the normal byte order of the architecture.
   - Need pcsc-lite 1.6.5 for TAG_IFD_POLLING_THREAD_WITH_TIMEOUT
   - Add --enable-embedded (default is no) to build libccid for an
      embedded system.  This will activate the NO_LOG option to disable
      logging and limit RAM and disk consumption.
   - Remove --enable-udev option since it is not used anymore with
      libhal. The udev rules file is now used to change the access
      rights of the device and not send a hotplug signal to pcscd.
      See http://ludovicrousseau.blogspot.com/2010/09/pcscd-auto-start.html
   - some minor bugs removed


1.4.0 - 4 August 2010, Ludovic Rousseau
   - add support of
     - ACS AET65
     - Broadcom 5880
     - C3PO LTC36
     - Dectel CI692
     - Gemalto Hybrid Smartcard Reader
     - Kingtrust Multi-Reader
     - Tianyu Smart Card Reader
     - Todos CX00
   - Add support of the SCM SDI 010 again. At least the contact
      interface can be used.
   - Use libusb-1.0 instead of libusb-0.1
   - add support of TAG_IFD_STOP_POLLING_THREAD and use of the
      asynchronous libusb API to be able to stop a transfer.
   - Request pcsc-lite 1.6.2 minimum (instead of 1.6.0) to have
      TAG_IFD_STOP_POLLING_THREAD defined
   - The O2MICRO OZ776 patch (for OZ776, OZ776_7772, REINER_SCT and
      BLUDRIVEII_CCID) is no more supported with libusb-1.0
   - correctly get the IFSC from the ATR (ATR parsing was not always
      correct)
   - some minor bugs removed


1.3.13 - 4 June 2010, Ludovic Rousseau
   - much faster warm start (up to 3.8 second gained)
   - Add support of SCARD_ATTR_VENDOR_IFD_SERIAL_NO to get the serial
      number of the USB device
   - some minor bugs removed


1.3.12 - 8 May 2010, Ludovic Rousseau
   - add support of
     - Ask CPL108
     - Atmel AT90SCR050
     - Atmel AT90SCR100
     - Atmel VaultIC420
     - Atmel VaultIC440
     - Atmel VaultIC460
     - Cherry SmartTerminal XX7X
     - Covadis Auriga
     - German Privacy Foundation Crypto Stick v1.2
     - GoldKey PIV Token
     - KOBIL Smart Token
     - KOBIL mIDentity 4smart
     - KOBIL mIDentity 4smart AES
     - KOBIL mIDentity 4smart fullsize AES
     - KOBIL mIDentity fullsize
     - KOBIL mIDentity visual
     - SCM SCR3500
     - Smart SBV280
     - Todos AGM2 CCID
     - Vasco DIGIPASS KEY 200
     - Vasco DIGIPASS KEY 860
     - Vasco DP855
     - Vasco DP865
     - id3 CL1356T5
   - remove support of Smart SBV280 on manufacturer request. They use
      libusb directly.
   - remove support of SCM SDI 010 on manufacturer request since not
      supported by my driver
   - Enable the Broadcom 5880 reader. It should work after a firmware
      upgrade.
   - better support of Dell keyboard
   - better support of multislot readers (like the GemCore SIM Pro)
   - better support of SCM SCR3310
   - better support of ICCD version A devices
   - The Covadis Véga-Alpha reader is a GemPC pinpad inside. So we use
      the same code to:
     - load the strings for the display
     - avoid limitation of the reader
   - IFDHControl(): the (proprietary) get firmware version escape
      command is allowed with a Gemalto reader
     - the (proprietary) switch interface escape command is allowed on
      the Gemalto GemProx DU
     - return IFD_ERROR_NOT_SUPPORTED instead of
      IFD_COMMUNICATION_ERROR if the dwControlCode value is not
      supported
     - return IFD_ERROR_INSUFFICIENT_BUFFER when appropriate
   - IFDHGetCapabilities(): add support of SCARD_ATTR_ICC_PRESENCE and
      SCARD_ATTR_ICC_INTERFACE_STATUS
   - support FEATURE_GET_TLV_PROPERTIES
   - add support of IOCTL_FEATURE_GET_TLV_PROPERTIES bMinPINSize &
      bMaxPINSize for Gemalto Pinpad V1 & Covadis Véga-Alpha
   - support extended APDU of up to 64kB with APDU readers.
   - get the language selected during Mac OS X installation as language
      to use for Covadis Véga-Alpha and Gemalto GemPC PinPad pinpad
      readers
   - FEATURE_MCT_READER_DIRECT is also supported by the Kobil mIDentity
      visual
   - better support of Sun Studio CC
   - some minor bugs removed


1.3.11 - 28 July 2009, Ludovic Rousseau
   - add support of
     - Raritan D2CIM-DVUSB VM/CCID
     - Feitian SCR301
     - Softforum XecureHSM
     - 2 Neowave Weneo tokens
     - Synnix STD200
     - Aktiv Rutoken ECP
     - Alcor Micro SCR001
     - ATMEL AT91SC192192CT-USB
     - Panasonic USB Smart Card Reader 7A-Smart
     - Gemalto GemProx DU and SU
   - remove support of Reiner-SCT cyberJack pinpad(a) on request of
      Reiner-SCT.  You should user the Reiner-SCT driver instead
   - define CFBundleName to CCIDCLASSDRIVER so that non class drivers
      have a higher priority. Used by pcsc-lite 1.5.5 and up.
      Add a --disable-class configure option so that the Info.plist does
      not define a Class driver. Default is class driver.
   - do not power up a card with a voltage not supported by the reader
   - add support of PIN_PROPERTIES_STRUCTURE structure and
      FEATURE_IFD_PIN_PROPERTIES
   - adds support of FEATURE_MCT_READERDIRECT. Only the Kobil TriB@nk
      reader supports this feature for now. This is used for the Secoder
      functionality in connected mode.
   - add support of a composite device. No change needed with libhal.
      use --enable-composite-as-multislot on Mac OS X since libhal is
      not available on Mac OS X or with libusb on Linux
   - some minor bugs removed


1.3.10 - 7 March 2009, Ludovic Rousseau
   - add support for
     - Aktiv Rutoken Magistra
     - Atmel AT98SC032CT
     - MSI StarReader SMART
     - Noname reader (from Omnikey)
     - Precise Biometrics 200 MC and 250 MC
     - Realtek 43 in 1 + Sim + Smart Card Reader
     - TianYu CCID SmartKey
     - Xiring Xi Sign PKI
   - add a patch to support the bogus OpenPGP card (on board key
      generation sometimes timed out)
   - disable support of the contactless part of SDI010 and SCR331DI
      (this code was reverse engineered and hard to maintain)
   - some minor bugs removed


1.3.9 - 18 November 2008, Ludovic Rousseau
   - add support for
     - ACS ACR122U PICC
     - Aladdin eToken PRO USB 72K Java
     - Atmel AT91SO
     - CSB6 Basic
     - CSB6 Secure
     - CSB6 Ultimate
     - Cherry SmartTerminal ST-1200USB
     - CrazyWriter
     - EasyFinger Standard
     - EasyFinger Ultimate
     - Gemalto PDT
     - HP MFP Smart Card Reader
     - KONA USB SmartCard
     - SpringCard Prox'N'Roll
     - VMware Virtual USB CCID
   - MacOSX/configure: do not overwrite PCSC_CFLAGS, PCSC_LIBS,
      LIBUSB_CFLAGS and LIBUSB_LIBS if already defined by the user
   - by default, link statically against libusb on Mac OS X
   - IFDHPowerICC(): use a very long timeout for PowerUp since the card
      can be very slow to send the full ATR (up to 30 seconds at 4 MHz)
   - SecurePINVerify(): correct a bug when using a Case 1 APDU and a
      SCM SPR532 reader
   - log the reader name instead of just the pcscd Lun
   - some minor bugs removed


1.3.8 - 27 June 2008, Ludovic Rousseau
   - add support for
     - Oberthur ID-One Cosmo Card
   - do not include the release number in the Info.plist to avoid a
      diff of a configuration file when upgrading the package.
   - do not fail compilation if libusb is not available
   - do not crash if the reader firmware is bogus and does not support
      chaining for extended APDU.  This is the case for Kobil readers
   - some minor bugs removed


1.3.7 - 8 June 2008, Ludovic Rousseau
   - add support for
     - ActivCard Activkey Sim
     - Pro-Active CSB6 Ultimate
     - id3 Semiconductors CL1356A HID
   - src/parse: do not try to parse devices with bInterfaceClass=0xFF
      by default (use command line argument -p for proprietary class)
   - configure.in: check if libusb-0.1 is emulated by libusb-compat +
      libusb-1.0 to use or not the USB interrupt feature
   - correct a bug in the serial communication (GemPC twin serial
      reader)
   - correct a pthread problem under Solaris
   - some minor bugs removed


1.3.6 - 30 April 2008, Ludovic Rousseau
   - add support for
     - Covadis Alya
     - Covadis Véga
     - Precise Biometrics 250 MC
     - Validy TokenA sl vt
     - Vasco DP905
   - better support for the O2Micro OZ776, GemCore SIM Pro
   - the environment variable LIBCCID_ifdLogLevel overwrite the value
      of ifdLogLevel read from the Info.plist file
   - add support for DragonFly BSD
   - some minor bugs removed


1.3.5 - 22 February 2008, Ludovic Rousseau
   - add support for Gemplus Gem e-Seal Pro, Cherry SmartBoard,
      G83-6610
   - use usb_interrupt_read() only if libusb > 0.1.12 or
      --enable-usb-interrupt is used. libusb <= 0.1.12 is bogus and will
      consume more CPU than needed.
   - contrib/Kobil_mIDentity_switch/Kobil_mIDentity_switch was broken
      on Linux since release 1.3.1
   - some minor bugs removed


1.3.4 - 8 February 2008, Ludovic Rousseau
   - the serial driver could not be loaded because of the missing
      symbol InterruptRead
   - remove WAIT_FOR_SYSFS="bInterfaceProtocol" to do not delay udev


1.3.3 - 6 February 2008, Ludovic Rousseau
   - add support for Lexar Smart Enterprise Guardian and Blutronics
      Bludrive II CCID
   - add support of TAG_IFD_POLLING_THREAD using IFDHPolling() to
      detect asynchronous card movements. With this feature pcscd will
      not poll the reader every 0.4 second for a card movement but will
      wait until the reader sends a USB interrupt signal
   - correct a bug with an ICCD-B device and a receive buffer smaller
      than 4 bytes
   - remove the sleep in the udev rule. It slows down the detection of
      any USB device
   - some minor bugs removed


1.3.2 - 22 January 2008, Ludovic Rousseau
   - add support of Apple Mac OS X Leopard (10.5.1)
   - solve a hotplug problem on Ubuntu 7.10 (reader was not detected)
   - create a symlink from libccidtwin.so to libccidtwin.so.VERSION so
      that the /etc/reader.conf configuration file do not need to edited
      for each new driver version
   - make driver for the GemPC Serial compile again
   - some minor bugs removed


1.3.1 - 16 November 2007, Ludovic Rousseau
   - add support for "Philips Semiconductors JCOP41V221" ICCD card,
      O2Micro oz776 (ProductID 0x7772), CardMan5321, Giesecke & Devrient
      StarSign Card Token 350 and 550, SafeNet IKey4000, Eutron
      CryptoIdentity, Eutron Smart Pocket, Eutron Digipass 860, Lenovo
      Integrated Smart Card Reader, "Kobil EMV CAP - SecOVID Reader III,
      Charismathics token, Reiner-SCT cyberJack pinpad(a)
   - improve support of Mac OS X and *BSD
   - some minor bugs removed


1.3.0 - 10 May 2007, Ludovic Rousseau
   - add support of ICCD version A and B
   - add support for (new) KOBIL mIDentity, SchlumbergerSema Cyberflex
      Access e-gate ICCD, Fujitsu Siemens Computers SmartCard USB 2A and
      SmartCard Keyboard USB 2A readers, OmniKey CardMan 4321
   - contrib/RSA_SecurID/RSA_SecurID_getpasswd: tool to get the on time
      password also displayed on the token screen
   - contrib/Kobil_mIDentity_switch: tool to activate the CCID reader
      of the Kobil mIDentity. The tool should be started automatically
      by the udev rule on Linux
   - GemPC pinpad: localisation of the string "PIN blocked", add
      Portuguese(pt), Dutch(nl) and Turkish(tr) localisations
   - some minor bugs removed

1.2.1 - 27 January 2007, Ludovic Rousseau
   - pcscd_ccid.rules: add non CCID generic (InterfaceClass: 0xFF)
      readers
   - INSTALL: document how to use --enable-udev on FreeBSD
   - move the O2Micro Oz7762 from the unsupported to the supported list
      since patches to support it are applied by default
      (O2MICRO_OZ776_PATCH)


1.2.0 - 19 January 2007, Ludovic Rousseau
   - add support for SCARD_ATTR_VENDOR_IFD_VERSION,
      SCARD_ATTR_VENDOR_NAME and SCARD_ATTR_MAXINPUT tags used by
      SCardGetAttrib(). Read SCARDGETATTRIB.txt for more documentation
   - add support for OmniKey CardMan 5125, CardMan 1021, C3PO LTC32,
      Teo by Xiring, HP USB Smartcard Reader
   - use --enable-twinserial to build the serial reader driver
   - use --enable-udev to configure for a use with Linux udev hotplug
      mechanism. This will allow pcscd to avoid polling the USB bus
      every 1 second
   - some minor bugs removed


1.1.0 - 11 August 2006, Ludovic Rousseau
   - support Extended APDU (up to 64KB) for readers in TPDU mode (many
      readers) or Extended APDU mode (very rare). This only works for
      T=1 cards.
   - add support for C3PO LTC31 (new version), OmniKey CardMan 3021, HP
      USB Smart Card Keyboard, Actividentity (ActiveCard) Activkey Sim,
      id3 Semiconductors CL1356D and CL1356T, Alcor Micro AU9520
   - support the contactless interface of the SCR331-DI-NTTCOM
   - add support of FreeBSD
   - increase the USB timeout used for PIN verify/modify to not timeout
      before the reader
   - the 4-bytes value returned by CM_IOCTL_GET_FEATURE_REQUEST shall
      be encoded in big endian as documented in PCSC v2 part 10 ch 2.2
      page 2. The applications using this feature shall be updated (to
      respect the PCSC specification).
   - use ./configure --enable-twinserial to compile and install the the
      driver for the GemPC Twin serial
   - some minor bugs removed


1.0.1 - 22 April 2006, Ludovic Rousseau
   - add support for Axalto Reflex USB v3, SCM Micro SDI 010, Winbond
      Electronics W81E381 chipset, Gemplus GemPC Card, Athena ASE IIIe
      KB USB, OmniKey CardMan 3621
   - support Solaris (Solaris uses a different libusb)
   - better documentation for ./configure arguments
   - improve support of Cherry XX44 keyboard for PIN verify and change
      (circumvent firmware bugs)
   - do not use LTPBundleFindValueWithKey() from pcscd since this
      function has been removed from pcscd API
   - use -fvisibility=hidden is available to limit the number of
      exported symbols


1.0.0 - 3 March 2006, Ludovic Rousseau
   - add support for ActivCard USB Reader 3.0, Athena ASE IIIe USB V2,
      SCM Micro SCR 355, SCR 3311, SCR 3320, SCR 3340 ExpressCard54,
      Gemplus GemCore SIM Pro, GemCore POS Pro (serial and USB), GemPC
      Express (ExpressCard/54 interface), SmartEpad (v 2.0), OmniKey
      CardMan 5121
   - greatly improve support of PIN PAD readers. We now support TPDU
      readers with T=1 cards
   - use l10n strings for the Gemplus GemPC PIN PAD (it has a screen).
      Supported languages are: de, en, es, fr, it
   - rename ACS ACR 38 in ACR 38U-CCID since the ACR 38 is a different
      reader and is not CCID compatible
   - allow to select the Power On voltage using Info.plist instead of
      recompiling the source code
   - correct bugs in the support of multi-slots readers
   - if the card is faster than the reader (TA1=97 for example) we try
      to use a not-so-bad speed (corresponding to TA1=96, 95 or 94)
      instead of the default speed of TA1=11
   - the src/parse tool do not use the driver anymore. No need to
      update the Info.plist file first.
   - some minor bugs removed


0.9.4 - 27 November 2005, Ludovic Rousseau
   - add support for Eutron SIM Pocket Combo, Eutron CryptoIdentity,
      Verisign Secure Token and VeriSign Secure Storage Token, GemPC
      Card (PCMCIA), SCM SCR331-DI NTTCom, SCM Micro SCR 3310-NTTCom,
      Cherry ST-1044U, Cherry SmartTerminal ST-2XXX
   - add support of PC/SC v2 part 10 CM_IOCTL_GET_FEATURE_REQUEST add
      support of FEATURE_VERIFY_PIN_DIRECT and FEATURE_MODIFY_PIN_DIRECT
      remove support of IOCTL_SMARTCARD_VENDOR_VERIFY_PIN (now
      obsoleted). A sample code is available in examples/scardcontrol.c
   - we need pcsc-lite 1.2.9-beta9 since some structures used for PIN
      pad readers are defined by pcsc-lite
   - some (bogus) cards require an extra EGT but the ATR does not say
      so. We try to detect the bogus cards and set TC1=2
   - IFDHSetProtocolParameters(): only use a data rate supported by the
      reader in the PPS negociation, otherwise we stay at the default
      speed.
   - calculate and store the read timeout according to the card ATR
      instead of using a fixed value of 60 seconds
   - increase the read timeout if the card sends and WTX request
   - improve support of GemPC Twin and GemPC Card (serial protocol)
   - reset the device on close only if DRIVER_OPTION_RESET_ON_CLOSE is
      set. The problem was that a device reset also disconnects the
      keyboard on a keyboard + reader device.
   - use color logs
   - some minor bugs removed


0.9.3 - 14 March 2005, Ludovic Rousseau
   - change the licence from GNU GPL to GNU Lesser GPL (LGPL)
   - add support for ACS ACR 38, Kobil KAAN Base, Kobil KAAN Advanced,
      Kobil KAAN SIM III, Kobil KAAN mIDentity, SCM Micro SCR 331,
      SCM Micro SCR 331-DI, SCM Micro SCR 335, SCM Micro SCR 3310,
      SCM Micro SCR 532, Cherry XX44 readers
   - improve communication speed with readers featuring "Automatic PPS
      made by the CCID"
   - switch the Cherry xx33 reader in ISO mode if power up in EMV mode
      fails.
   - add support of character level readers. Thanks to O2Micro for the
      patch
   - add support for the O2Micro OZ776S reader but the reader firmware
      is still bogus
   - check firmware version to avoid firmwares with bugs. You can still
      use a bogus firmware by setting DRIVER_OPTION_USE_BOGUS_FIRMWARE
      in Info.plist
   - some minor bugs removed

0.9.2 - 15 August 2004, Ludovic Rousseau
   - T=1 TPDU code:
     - the work on T=1 TPDU code was possible thanks to Gemplus
        validation team who helped me test, debug and bring the code to
        an EMV validation level. Thanks to Jérôme, Jean-Yves, Xavier and
        the Gemplus readers department
     - error code was not checked correctly
     - avoid a (nearly) infinite loop when resynch are needed.
     - correctly initialise an internal value to allow more than one
        reader to work
   - multi-slots readers
     - add support for multi-slots readers. The only one I have is a
        SCM Micro SCR 331-DI with a contact and a contactless interface.
        The contactless interface may or may not work for you since the
        reader uses proprietary (undocumented) commands.
   - GemPC Twin serial reader
     - perform a command (get the reader firmware) to be sure a GemPC
        Twin (serial or pcmcia) reader is connected
     - use a dynamic timeout when reading the serial port.
        The first timeout used when detecting the reader is 2 seconds to
        not wait too long if no reader is connected. Later timeouts are
        set to 1 minute to allow long time APDU.
   - use `pkg-config libpcsclite --cflags` to locate the pcsc-lite
      header files
   - use `pkg-config --print-errors --atleast-version=1.2.9-beta5 libpcsclite`
      to test the pcsc-lite version
   - code improvements thanks to the splint tool (http://www.splint.org/)

0.9.1 - 1 July 2004, Ludovic Rousseau
   - I forgot to define IFD_PARITY_ERROR in a .h file

0.9.0 - 1 July 2004, Ludovic Rousseau
   - The T=1 TPDU automata from Carlos Prados' Towitoko driver is very
      limited and do not support error management mechanisms.
      I then used the T=1 TPDU automata from OpenCT (OpenSC project).
      This automata is much more powerful but still lacks a lot of error
      management code.
      I then added all the needed code to reach the quality level
      requested by the EMV standard.
   - add support for new readers:
     - Advanced Card Systems ACR 38
     - Cherry XX33
     - Dell keyboard SK-3106
     - Dell smart card reader keyboard
     - SCR 333
   - add support of multi procotol cards (T=0 and T=1)
   - the debug level is now dynamic and set in the Info.plist file (no
      need to recompile the driver any more)
   - add support for the libusb naming scheme: usb:%04x/%04x:libusb:%s
   - INSTALL: add a "configuring the driver for the serial reader
      (GemPC Twin)" part
   - use `pkg-config libpcsclite --variable=usbdropdir` so you do not
      have to use --enable-usbdropdir=DIR or --enable-ccidtwindir=DIR
      even if pcscd does not use the default /usr/local/pcsc/drivers
   - add support of IOCTL_SMARTCARD_VENDOR_IFD_EXCHANGE and
      IOCTL_SMARTCARD_VENDOR_VERIFY_PIN in IFDHControl()
   - read ifdDriverOptions from Info.plist to limit the use of
      IOCTL_SMARTCARD_VENDOR_IFD_EXCHANGE (idea from Peter Williams)
   - provide an example of use of SCardControl()
      IOCTL_SMARTCARD_VENDOR_IFD_EXCHANGE and
      IOCTL_SMARTCARD_VENDOR_VERIFY_PIN in example/
   - add a --enable-pcsclite option (default to yes) so that the driver
      can be compiled for a different framework (one needing
      tokenparser.l like Solaris)
   - Reset action is power off and power on, not just power on
   - use the include files from pcsc-lite
   - add a mechanism to allow power on at 1.8V, 3V and then 5V as
      specified by ISO 7816. We still use 5V for now to avoid problems
      with non ISO compliant cards

0.4.1 - 14 February 2004, Ludovic Rousseau
   - distribute missing files readers/supported_readers.txt and
      src/create_Info_plist.pl
      'make install' failed because of this.

0.4.0 - 13 February 2004, Ludovic Rousseau
   - support of T=1 with TPDU readers. A lot of the T=1 code comes from
      Carlos Prados towitoko driver.
      My code is GNU GPL, his code is GNU LGPL so the global driver is
      GNU GPL
   - PPS negotiation if the reader does not do it automatically
   - add support for the Silitek SK-3105 keyboard. It's a USB device
      with multiple interfaces
   - use the create_Info_plist.pl script to generate the installed
      Info.plist from an Info.plist template and a list of supported
      readers. The Info.plist was too "complex" to maintain by hand
      since it now contains 11 entries
   - add support of IFDHCreateChannelByName to avoid wrong reader
      enumeration. This is not complete if you have multiple _identical_
      readers. You need to use a > 1.2.0 pcsc-lite version (not yet
      released at that time)
   - build but do not install the serial ccidtwin driver by default
      since it is useless on computers without a serial port or without
      this reader for example.
   - read and write timeouts are not symmetric. write timout can be
      shorter since the reader and card is not supposed to do anything
      before receiving (write) a command
   - do not try to find usb.h and other libusb files if
      --disable-libusb is used. Needed if you only want to build the
      serial driver.  Thanks to Niki Waibel for the patch
   - add a --enable-ccidtwindir argument to ./configure to specify the
      serial GemPC Twin installation directory
   - debug and code improvements and simplifications

0.3.2 - 4 November 2003, Ludovic Rousseau
   - src/commands.c: correct a stupid bug that occurs with an APDU with
      2 bytes response.
   - Info.plist: add SPR 532 in list of supported readers
   - parse.c: do not exit if the InterfaceClass is 0xFF (proprietary).
      It is the case with old readers manufactured before the final
      release of the CCID specs.
   - move LTC31 reader from unsupported to supported reader list. It
      was my f ault since in used odd INS byte in my test applet and odd
      INS bytes are forbidden by ISO 7816-4 ch. 5.4.2 Instruction byte.
      Thanks to Josep Moné s Teixidor for pointing the problem.
   - src/commands.c: comment out the automatic GET RESPONSE part. I
      don't think it should be in the driver. Maybe in pcscd instead?

0.3.1 - 23 September 2003, Ludovic Rouseau
   - add --enable-multi-thread (enabled by default) for thread safe
      support an APDU multiplexing. You will need pcsc-lite-1.2.0-rc3 or
      above to use this feature.
   - add --enable-libusb=PATH option is your libusb is not installed in
      /usr or /usr/local
   - honor DESTDIR in install rules (closes [ #300110 ]). Thanks to
      Ville Skyttä for the patch.
   - src/ccid.c: do not switch the GemPC Key and GemPC Twin in APDU
      mode since it also swicth in EMV mode and may not work with non
      EMV cards
   - src/ccid_serial.c: complete reimplementation of the Twin serial
      protocol using a finite state automata (code much simpler)

0.3.0 - 10 September 2003, Ludovic Rousseau
   - support of GemPC Twin connected to a serial port. Thanks to Niki
      W. Waibel for a working prototype.
   - support of auto voltage at power up if the reader support it
      instead of forcing a 5V in all cases.
   - support of APDU mode instead of just TPDU if the reader support
      it. Thanks to Jean-Luc Giraud for the idea and inspiration I got
      from his "concurrent" driver.
   - support of "time request" from the card.
   - parse: new indentation for more readability of supported features.
   - switch the GemPC Key and GemPC Twin in APDU mode since they
      support it but do not announce it in the dwFeatures.
   - new build process using autoconf/automake.

0.2.0 - 26 August 2003, Ludovic Rousseau
   - Works under MacOS X
   - Info.plist: use an <array></array> for the alias enumeration
   - Makefile rework for *BSD and MacOS X

0.1.0 - 13 August 2003, Ludovic Rousseau
   - First public release


 vim:ts=20
