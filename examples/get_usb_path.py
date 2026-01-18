#! /usr/bin/env python3

"""
get_usb_path.py: get USB path of readers
Copyright (C) 2026  Ludovic Rousseau
"""

#   This program is free software; you can redistribute it and/or modify
#   it under the terms of the GNU General Public License as published by
#   the Free Software Foundation; either version 3 of the License, or
#   (at your option) any later version.
#
#   This program is distributed in the hope that it will be useful,
#   but WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#   GNU General Public License for more details.
#
#   You should have received a copy of the GNU General Public License along
#   with this program; if not, see <http://www.gnu.org/licenses/>.

import struct
import smartcard
from smartcard.pcsc.PCSCPart10 import SCARD_CTL_CODE
from smartcard.scard import (
    SCARD_E_INVALID_PARAMETER,
    SCARD_ATTR_CHANNEL_ID,
    SCARD_SHARE_DIRECT,
    SCARD_LEAVE_CARD,
)
from smartcard.util import toASCIIString


def get_usb_path(reader):
    """
    Display USB topology
    """
    print("Using:", reader)
    card_connection = reader.createConnection()
    card_connection.connect(mode=SCARD_SHARE_DIRECT, disposition=SCARD_LEAVE_CARD)

    # special control code
    ioctl_get_usb_path = SCARD_CTL_CODE(3601)
    try:
        res = card_connection.control(ioctl_get_usb_path)
    except smartcard.Exceptions.SmartcardException as ex:
        if ex.hresult == SCARD_E_INVALID_PARAMETER:
            print("Your driver does not (yet) support SCARD_CTL_CODE(3601)")
            return
        raise
    print("USB path:", toASCIIString(res))

    # get Channel ID
    attrib = card_connection.getAttrib(SCARD_ATTR_CHANNEL_ID)
    ddddcccc = struct.unpack("i", bytearray(attrib))[0]
    dddd = ddddcccc >> 16
    if dddd == 0x0020:
        bus = (ddddcccc & 0xFF00) >> 8
        addr = ddddcccc & 0xFF
        print(f" USB: bus: {bus}, addr: {addr}")
    print()


def main():
    """
    main
    """
    # for all the available readers
    for reader in smartcard.System.readers():
        get_usb_path(reader)


if __name__ == "__main__":
    main()
