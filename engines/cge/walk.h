/* ScummVM - Graphic Adventure Engine
 *
 * ScummVM is the legal property of its developers, whose names
 * are too numerous to list here. Please refer to the COPYRIGHT
 * file distributed with this source distribution.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

/*
 * This code is based on original Soltys source code
 * Copyright (c) 1994-1995 Janus B. Wisniewski and L.K. Avalon
 */

#ifndef __CGE_WALK__
#define __CGE_WALK__

#include "cge/wav.h"
#include "cge/vga13h.h"
#include "cge/events.h"

namespace CGE {

#define MAP_XCNT       40
#define MAP_ZCNT       20
#define MAP_TOP        80
#define MAP_HIG        80
#define MAP_XGRID      (SCR_WID / MAP_XCNT)
#define MAP_ZGRID      (MAP_HIG / MAP_ZCNT)


class Cluster : public Couple {
public:
	static uint8 _map[MAP_ZCNT][MAP_XCNT];
	uint8 &cell();
	Cluster() : Couple() { }
	Cluster(int a, int b) : Couple(a, b) { }
	bool Protected();
};


class WALK : public Sprite {
public:
	Cluster _here;
	int _tracePtr;

	enum DIR { NO_DIR = -1, NN, EE, SS, WW } Dir;
	WALK(CGEEngine *vm, BMP_PTR *shpl);
	void tick();
	void findWay(Cluster c);
	void findWay(Sprite *spr);
	int distance(Sprite *spr);
	void turn(DIR d);
	void park();
	bool lower(Sprite *spr);
	void reach(Sprite *spr, int mode = -1);
private:
	CGEEngine *_vm;

};

Cluster XZ(int x, int y);
Cluster XZ(Couple xy);

extern WALK *_hero;

} // End of namespace CGE

#endif
