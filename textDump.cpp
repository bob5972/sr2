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
    uint ni = 0;
    MBString s;

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
            v[ni++] = atoi(s.CStr());
        }
    }
}

void TextDump_Convert(const MBString &str, MBVector<float> &v)
{
    uint i = 0;
    uint ni = 0;
    MBString s;

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
            v[ni++] = atof(s.CStr());
        }
    }
}

