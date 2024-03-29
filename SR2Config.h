/*
 * SR2Config.h -- part of SpaceRobots2
 *
 * Copyright (c) 2021 Michael Banack <github@banack.net>
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

#ifndef _SR2CONFIG_H_202111081544
#define _SR2CONFIG_H_202111081544

#ifdef __cplusplus
	extern "C" {
#endif

#include "MBConfig.h"

#ifdef SR2_GUI
#define sr2_gui 1
#else
#define sr2_gui 0
#endif

#ifdef __cplusplus
	}
#endif

#endif //_SR2CONFIG_H_202111081544
