/*
    utils.c:
    Copyright (C) 2003-2022   Ludovic Rousseau

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

	You should have received a copy of the GNU Lesser General Public License
	along with this library; if not, write to the Free Software Foundation,
	Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

void InitReaderIndex(void);
int GetNewReaderIndex(const int Lun);
int LunToReaderIndex(int Lun);
void ReleaseReaderIndex(const int idx);

uint16_t get_U16(void *);
uint32_t get_U32(void *);
void set_U16(void *, uint16_t);
void set_U32(void *, uint32_t);
void p_bswap_16(void *ptr);
void p_bswap_32(void *ptr);
