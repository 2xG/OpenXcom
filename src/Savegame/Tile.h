/*
 * Copyright 2010 OpenXcom Developers.
 *
 * This file is part of OpenXcom.
 *
 * OpenXcom is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * OpenXcom is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with OpenXcom.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef OPENXCOM_TILE_H
#define OPENXCOM_TILE_H

#include <string>
#include <vector>
#include "../Battlescape/Position.h"
#include "../Resource/TerrainObject.h"

namespace OpenXcom
{

class Surface;
class TerrainObject;
class BattleUnit;
class BattleItem;

/**
 * Basic element of which a battle map is build.
 * @sa http://www.ufopaedia.org/index.php?title=MAPS
 */
class Tile
{
protected:
	std::string _name[4];
	TerrainObject *_terrainObjects[4];
	int _currentFrame[4];
	bool _discovered; // tile has been seen in the past
	std::vector<BattleUnit *> _visibleByUnit; // units this tile is now seen by 
	int _shadeSun;
	int _light, _lastLight, _smoke, _fire, _sessionID;
	Position _pos;
	bool _cached;
	BattleUnit *_unit; // unit on this tile
	std::vector<BattleItem *> _inventory; // items on this tile

public:
	/// Creates a tile.
	Tile(const Position& pos);
	/// Cleans up a tile.
	~Tile();
	/// Gets name for the part
	std::string getName(int part);
	/// sets name for the part
	void setName(std::string name, int part);
	/// Gets a pointer to the terrainobject for a specific part of the tile.
	TerrainObject *getTerrainObject(int part);
	/// Sets the pointer to the terrainobject for a specific part of the tile
	void setTerrainObject(TerrainObject *tob, int part);
	/// Get the TU cost to walk over a certain part of the tile.
	int getTUCost(int part, MovementType movementType);
	/// Checks if this tile has a floor.
	bool hasNoFloor();
	/// Checks if this tile is a big wall.
	bool isBigWall();
	/// Get terrain level.
	int getTerrainLevel();
	/// Gets the tile's position.
	const Position& getPosition() const;
	/// Gets the floor object footstep sound.
	int getFootstepSound();
	/// Open a door, returns the ID, 0(normal), 1(ufo) or -1 if no door opened.
	int openDoor(int part);
	/// Check if ufo door is open.
	bool isUfoDoorOpen(int part);
	/// Set the cached flag.
	void setCached(bool cached);
	/// Check if tile is cached.
	bool isCached();
	void setDiscovered(bool flag);
	bool isDiscovered();
	/// Set the shade amount.
	void setShadeSun(int shade);
	/// Change a light amount (+ ligther / - dimmer).
	void resetLight();
	void addLight(int light, int sessionID);
	void isSeenBy(BattleUnit *unit, int sessionID);
	void setLight();
	/// Get the shade amount.
	int getShade();
	/// Destroy a tile part.
	void destroy(int part);
	/// Animated the tile parts.
	void animate();
	/// Get surfaces.
	Surface *getSprite(int part);
	void setUnit(BattleUnit *unit);
	BattleUnit *getUnit();
};

}

#endif