INSTALLATION PROCEDURE
======================

Dependencies:
-------------

You need to install:
- meson
- flex
- libusb-1

Installation from source:
-------------------------

Get the ccid-x.y.z.tar.xz archive from https://ccid.apdu.fr/files/ and do:

```
tar xjvf ccid-*.tar.xz
cd ccid-*
meson setup builddir
cd builddir
meson compile
sudo meson install
```

Installation from git repo:
---------------------------

```
git clone https://salsa.debian.org/rousseau/CCID.git
cd CCID
meson setup builddir
cd builddir
meson compile
sudo meson install
```

building serial reader driver
-----------------------------

A serial CCID reader can also be connected on a serial port. By default
the serial driver is not built. You must explicitly do:

```
meson setup builddir -Dserial=true
cd builddir
meson compile
```

configuring the driver for the serial reader
--------------------------------------------

You have to create a file in the `/etc/reader.conf.d/` directory. The file
should contain something like:

```
# Gemalto reader with serial communication
#  - n is the serial port to use n in [0..3]
#  - reader is the reader name. It is needed for multi-slot readers.
#    Possible reader values are: 
#     GemCorePOSPro
#     GemCoreSIMPro
#     GemCoreSIMPro2
#     GemPCPinPad
#     GemPCTwin (default value)
#     SEC1210UR2 (Dual slot SEC1210 Reader)
#     SEC1210URT (single slot SEC1210 Reader)
# example: /dev/ttyS0:GemPCPinPad
#DEVICENAME        /dev/ttySn[:reader]
#FRIENDLYNAME      "GemPCTwin serial"
#LIBPATH           /usr/lib/pcsc/drivers/serial/libccidtwin.so

FRIENDLYNAME      "GemPC Twin serial"
DEVICENAME        /dev/ttyS0
LIBPATH           /usr/lib/pcsc/drivers/serial/libccidtwin.so
```

You will have to adapt the library path to your configuration.

By default the GemPC Twin serial reader parameters are loaded by the
driver, if you use a GemPC PinPad, a GemCore POS Pro, a GemCore SIM
Pro or GemCore SIM Pro 2 (or IDBridge CR30) you have to indicate it in the
DEVICENAME field. Supported values are:
- GemCorePOSPro for GemCore POS Pro
- GemCoreSIMPro for GemCore SIM Pro
- GemCoreSIMPro2 for IDBridge CR30
- GemPCPinPad for GemPC PinPad
- GemPCTwin for GemPC Twin (default value)
- SEC1210UR2 for Microchip SEC1210 with 2 slots
- SEC1210URT for Microchip SEC1210 with 1 slot

You will then have something like:
```
DEVICENAME /dev/ttyS0:GemPCPinPad
```

`/dev/ttyS0` (DEVICENAME field) is the first serial port under Linux
(known as COM1 under DOS/Windows). Of course if your reader is connected
to another serial port you have to adapt that.


Binary installation:
--------------------

Contact your distribution support.
