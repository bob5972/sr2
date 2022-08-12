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

void TextDump_Convert(const MBString &str, MBVector<uint> &v);
void TextDump_Convert(const MBString &str, MBVector<float> &v);

#endif // _TEXTDUMP_H_202208121346