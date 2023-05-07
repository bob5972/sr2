/*
 * textDump.cpp -- part of SpaceRobots2
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


#include "textDump.hpp"

void TextDump_Convert(const MBString &str, MBVector<uint> &v)
{
    uint i = 0;
    MBString s;

    v.makeEmpty();

    while (i < str.length()) {
        s.makeEmpty();

        while (i < str.length() && !isdigit(str.getChar(i))) {
            i++;
        }

        while (i < str.length() && isdigit(str.getChar(i))) {
            s += str.getChar(i);
            i++;
        }

        if (s.length() > 0) {
            v.push(atoi(s.CStr()));
        }
    }
}

void TextDump_Convert(const MBString &strSrc, MBVector<float> &vDest)
{
    uint i = 0;
    MBString s;

    // XXX: This is not robust.

    vDest.makeEmpty();

    while (i < strSrc.length()) {
        s.makeEmpty();

        while (i < strSrc.length() &&
              !isdigit(strSrc.getChar(i)) &&
              strSrc.getChar(i) != '.') {
            i++;
        }

        while (i < strSrc.length() &&
               (isdigit(strSrc.getChar(i)) || strSrc.getChar(i) == '.')) {
            s += strSrc.getChar(i);
            i++;
        }

        if (s.length() > 0) {
            vDest.push(atof(s.CStr()));
        }
    }
}

void TextDump_Convert(const MBVector<uint> &vSrc, MBString &strDest)
{
    uint i = 0;

    strDest.makeEmpty();

    strDest += "{";

    for (i = 0; i < vSrc.size(); i++) {
        char *cs;
        asprintf(&cs, "%d, ", vSrc[i]);
        strDest += cs;
        free(cs);
    }

    strDest += "}";
}


void TextDump_Convert(const MBVector<float> &vSrc, MBString &strDest)
{
    uint i = 0;

    strDest.makeEmpty();

    strDest += "{";

    for (i = 0; i < vSrc.size(); i++) {
        char *cs;
        int ret = asprintf(&cs, "%f, ", (float)(vSrc[i]));
        VERIFY(ret > 0);
        strDest += cs;
        free(cs);
    }

    strDest += "}";
}

const char *TextMap_ToStringD(int value, const TextMapEntry *tms, uint numTMs,
                              const char *missingValue)
{
    for (uint i = 0; i < numTMs; i++) {
        if (tms[i].value == value) {
            return tms[i].str;
        }
    }
    return missingValue;
}

const char *TextMap_ToString(int value, const TextMapEntry *tms, uint numTMs)
{
    for (uint i = 0; i < numTMs; i++) {
        if (tms[i].value == value) {
            return tms[i].str;
        }
    }
    PANIC("%s: value=%d not in table\n", __FUNCTION__, value);
}

int TextMap_FromString(const char *str, const TextMapEntry *tms, uint numTMs)
{
    for (uint i = 0; i < numTMs; i++) {
        if (strcmp(str, tms[i].str) == 0) {
            return tms[i].value;
        }
    }
    PANIC("%s: string=%s not in table\n", __FUNCTION__, str);
}

