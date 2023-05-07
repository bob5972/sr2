/*
 * textDump.hpp -- part of SpaceRobots2
 * Copyright (C) 2022 Michael Banack <github@banack.net>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef _TEXTDUMP_H_202208121346
#define _TEXTDUMP_H_202208121346

#include "MBString.hpp"
#include "MBVector.hpp"

typedef struct TextMapEntry {
    int value;
    const char *str;
} TextMapEntry;

#define TMENTRY(_op) _op, #_op

void TextDump_Convert(const MBString &strSrc, MBVector<uint> &vDest);
void TextDump_Convert(const MBString &strSrc, MBVector<float> &vDest);
void TextDump_Convert(const MBVector<uint> &vSrc, MBString &strDest);
void TextDump_Convert(const MBVector<float> &vSrc, MBString &strDest);

const char *TextMap_ToString(int value, const TextMapEntry *tms, uint numTMs);
const char *TextMap_ToStringD(int value, const TextMapEntry *tms, uint numTMs,
                              const char *missingValue);
int TextMap_FromString(const char *str, const TextMapEntry *tms, uint numTMs);

#endif // _TEXTDUMP_H_202208121346