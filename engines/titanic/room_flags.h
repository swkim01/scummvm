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
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#ifndef TITANIC_ROOM_FLAGS_H
#define TITANIC_ROOM_FLAGS_H

#include "titanic/support/string.h"

namespace Titanic {

class CRoomFlags {
private:
	uint _data;
private:
	int getConditionally() const;

	/**
	 * Returns true if the current flags appear in the
	 * list of transport rooms
	 */
	bool isTransportRoom() const;

	int getRoomCategory() const;

	int getRoomArea() const;

	/**
	 * Set the bits for the elevator number
	 */
	void setElevatorBits(uint val);

	/**
	 * Set the bits for the floor number
	 */
	void setFloorBits(uint val);

	/**
	 * Translates bits for floor into a floor number
	 */
	uint decodeFloorBits(uint bits) const;

	static bool is2To19(uint v) { return v >= 2 && v <= 19; }
	static bool is20To27(uint v) { return v >= 20 && v <= 27; }
	static bool is28To38(uint v) { return v >= 28 && v <= 38; }
public:
	/**
	 * Compares the current flags against the specified flags
	 * for a matching elevator, floor, and room
	 */
	static bool compareLocation(uint flags1, uint flags2);

	/**
	 * Compares two room flags together
	 */
	static bool compareClassElevator(uint flags1, uint flags2);

	/**
	 * Returns true if the current flags is for Titania's room
	 */
	static bool isTitania(uint flags1, uint flags2);
public:
	CRoomFlags() : _data(0) {}
	CRoomFlags(uint data) : _data(data) {}
	operator uint() { return _data; }

	/**
	 * Set the flags value
	 */
	void set(uint data) { _data = data; }

	/**
	 * Get the flags value
	 */
	uint get() const { return _data; }

	/**
	 * Gets the special flags for a transport or succubus room
	 */
	static uint getSpecialRoomFlags(const CString &roomName);

	/**
	 * Returns true if the current flags are in the succubus list
	 */
	bool isSuccUBusRoomFlags() const;

	/**
	 * Get a description for the room
	 */
	CString getRoomDesc() const;

	/**
	 * Get the bits for the elevator number
	 */
	uint getElevatorBits() const;

	/**
	 * Set the elevator number
	 */
	void setElevatorNum(uint val) { setElevatorBits(val - 1); }

	/**
	 * Get the elevator number
	 */
	uint getElevatorNum() const { return getElevatorBits() + 1; }

	/**
	 * Get a description for the elevator number
	 */
	CString getElevatorDesc() const {
		return CString::format("Elevator %d", getElevatorNum());
	}

	/**
	 * Gets the bits for the passenger class
	 */
	uint getPassengerClassBits() const;

	/**
	 * Set the bits for the passenger class
	 */
	void setPassengerClassBits(uint val);

	/**
	 * Gets the passenger class number
	 */
	uint getPassengerClassNum() const { return getPassengerClassBits(); }

	/**
	 * Get a description for the passenger class
	 */
	CString getPassengerClassDesc() const;

	/**
	 * Gets the bits for the floor number
	 */
	uint getFloorBits() const;

	/**
	 * Sets the floor number
	 */
	void setFloorNum(uint floorNum);

	/**
	 * Gets the floor number
	 */
	uint getFloorNum() const;

	/**
	 * Get a description for the floor number
	 */
	CString getFloorDesc() const {
		return CString::format("Floor %d", getFloorNum());
	}

	/**
	 * Sets the bits for the room number
	 */
	void setRoomBits(uint roomBits);

	/**
	 * Gets the bits for the room number
	 */
	uint getRoomBits() const;

	/**
	 * Gets the room number
	 */
	uint getRoomNum() const { return getRoomBits(); }

	/**
	 * Gets a string for the room number
	 */
	CString getRoomNumDesc() const {
		return CString::format("Room %d", getRoomNum());
	}

	bool getBit0() const;

	/**
	 * Change the passenger class
	 */
	void changeLocation(int action);

	/**
	 * Sets a random destination in the flags
	 */
	void setRandomLocation(int classNum, bool flag);

	/**
	 * Gets the succubus number associated with a given room
	 */
	uint getSuccUBusNum(const CString &roomName) const;

	/**
	 * Gets the succubus room name associated with the current room flags
	 */
	CString getSuccUBusRoomName() const;

	/**
	 * Returns what passenger class a particular floor number belongs to
	 */
	static int whatPassengerClass(int floorNum);

	bool not5() const { return getConditionally() != 5; }

	bool is59706() const { return _data == 0x59706; }
};

} // End of namespace Titanic

#endif /* TITANIC_ROOM_FLAGS_H */
