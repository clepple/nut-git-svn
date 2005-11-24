/*
 * hidparser.h: HID Parser header file
 *
 * This file is part of the MGE UPS SYSTEMS HID Parser.
 *
 * Copyright (C) 1998-2003 MGE UPS SYSTEMS,
 *		Written by Luc Descotils.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * -------------------------------------------------------------------------- */

#ifndef HIDPARS_H
#define HIDPARS_H


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include "hidtypes.h"

/*
 * Parse_ReportDesc
 * -------------------------------------------------------------------------- */
HIDDesc *Parse_ReportDesc(u_char *ReportDesc, int n);

/*
 * Free_ReportDesc
 * -------------------------------------------------------------------------- */
void Free_ReportDesc(HIDDesc *pDesc);

/*
 * FindObject
 * -------------------------------------------------------------------------- */
int FindObject(HIDDesc *pDesc, HIDData* pData);

HIDData *FindObject_with_Path(HIDDesc *pDesc, HIDPath *Path, u_char Type);

HIDData *FindObject_with_ID(HIDDesc *pDesc, u_char ReportID, u_char Offset, u_char Type);

/*
 * GetValue
 * -------------------------------------------------------------------------- */
void GetValue(const u_char* Buf, HIDData* pData, long *pValue);

/*
 * SetValue
 * -------------------------------------------------------------------------- */
void SetValue(const HIDData* pData, u_char* Buf, long Value);


#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif
