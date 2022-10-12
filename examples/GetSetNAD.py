#! /usr/bin/env python3

"""
#   GetSetNAD.py: get/set NAD value
#   Copyright (C) 2022  Ludovic Rousseau
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

from smartcard.System import readers
from smartcard.pcsc.PCSCPart10 import SCARD_LEAVE_CARD, SCARD_CTL_CODE
from smartcard import Exceptions
from smartcard.util import toHexString


def get_nad(cardConnection):
    print("Get NAD")

    get_NAD = [0x3F, 0, 0]
    print(" command:", toHexString(get_NAD))
    res = cardConnection.control(SCARD_CTL_CODE(3600), get_NAD)
    print("  result:", toHexString(res))
    status = res[3]
    if status is 0:
        print("Success")
        nad = res[4]
        print("NAD:", nad)
    else:
        print("Failed!")


def set_nad(cardConnection, nad):
    print("Set NAD")

    set_NAD = [0x3E, 0, 1, nad]
    print(" command:", toHexString(set_NAD))
    res = cardConnection.control(SCARD_CTL_CODE(3600), set_NAD)
    print("  result:", toHexString(res))
    status = res[3]
    if status is 0:
        print("Success")
    else:
        print("Failed!")


if __name__ == "__main__":
    import sys

    reader_idx = 0
    if len(sys.argv) > 1:
        reader_idx = int(sys.argv[1])

    reader = readers()[reader_idx]
    print("Reader:", reader)

    cardConnection = reader.createConnection()
    cardConnection.connect(disposition=SCARD_LEAVE_CARD)

    get_nad(cardConnection)
    set_nad(cardConnection, 42)
    get_nad(cardConnection)
