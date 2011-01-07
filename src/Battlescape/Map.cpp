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
#define _USE_MATH_DEFINES
#include <cmath>
#include <fstream>
#include "Map.h"
#include "UnitSprite.h"
#include "Position.h"
#include "Pathfinding.h"
#include "TerrainModifier.h"
#include "../Resource/TerrainObjectSet.h"
#include "../Resource/TerrainObject.h"
#include "../Resource/ResourcePack.h"
#include "../Engine/Action.h"
#include "../Engine/SurfaceSet.h"
#include "../Engine/Timer.h"
#include "../Engine/Font.h"
#include "../Engine/Language.h"
#include "../Engine/Palette.h"
#include "../Engine/Game.h"
#include "../Savegame/SavedBattleGame.h"
#include "../Savegame/GameTime.h"
#include "../Savegame/Craft.h"
#include "../Savegame/Ufo.h"
#include "../Savegame/Tile.h"
#include "../Savegame/BattleUnit.h"
#include "../Ruleset/RuleTerrain.h"
#include "../Ruleset/RuleCraft.h"
#include "../Ruleset/RuleUfo.h"
#include "../Interface/Text.h"
#include "../Interface/Cursor.h"

#define SCROLL_AMOUNT 20
#define SCROLL_BORDER 5
#define SCROLL_DIAGONAL_EDGE 60
#define DEFAULT_ANIM_SPEED 100
#define RMB_SCROLL false

/*
  1) Map origin is left corner. 
  2) X axis goes downright. (width of the map)
  3) Y axis goes upright. (length of the map
  4) Z axis goes up (height of the map)

          y+
			/\
	    0,0/  \
		   \  /
		    \/
          x+  
*/

namespace OpenXcom
{

/**
 * Sets up a map with the specified size and position.
 * @param width Width in pixels.
 * @param height Height in pixels.
 * @param x X position in pixels.
 * @param y Y position in pixels.
 */
Map::Map(int width, int height, int x, int y) : InteractiveSurface(width, height, x, y), _mapOffsetX(-250), _mapOffsetY(250), _viewHeight(0), _hideCursor(false), _animFrame(0), _scrollX(0), _scrollY(0), _RMBDragging(false)
{
	_scrollTimer = new Timer(50);
	_scrollTimer->onTimer((SurfaceHandler)&Map::scroll);

	_animTimer = new Timer(DEFAULT_ANIM_SPEED);
	_animTimer->onTimer((SurfaceHandler)&Map::animate);
	_animTimer->start();
}

/**
 * Deletes the map.
 */
Map::~Map()
{
	delete _scrollTimer;
	delete _animTimer;


	for (int i = 0; i < _tileCount; i++)
	{
		delete _tileFloorCache[i];
		delete _tileWallsCache[i];
	}
	delete _tileFloorCache;
	delete _tileWallsCache;
}

/**
 * Changes the pack for the map to get resources for rendering.
 * @param res Pointer to the resource pack.
 */
void Map::setResourcePack(ResourcePack *res)
{
	_res = res;
	_spriteWidth = res->getSurfaceSet("BLANKS.PCK")->getFrame(0)->getWidth();
	_spriteHeight = res->getSurfaceSet("BLANKS.PCK")->getFrame(0)->getHeight();
}

/**
 * Changes the saved game content for the map to render.
 * @param save Pointer to the saved game.
 * @param game Pointer to the Game.
 */
void Map::setSavedGame(SavedBattleGame *save, Game *game)
{
	_save = save;
	_game = game;
	_tileCount = _save->getHeight() * _save->getLength() * _save->getWidth();
	_tileFloorCache = new Surface*[_tileCount];
	_tileWallsCache = new Surface*[_tileCount];
	_unitCache.clear();
	for (int i = 0; i < _tileCount; i++)
	{
		_tileFloorCache[i] = 0;
		_tileWallsCache[i] = 0;
	}
	for (std::vector<BattleUnit*>::iterator i = _save->getUnits()->begin(); i != _save->getUnits()->end(); i++)
	{
		_unitCache.push_back(0);
	}
}

/**
 * initializes stuff
 */
void Map::init()
{
	// load the tiny arrow into a surface
	int f = Palette::blockOffset(1)+1; // yellow
	int b = 15; // black
	int pixels[81] = { 0, 0, b, b, b, b, b, 0, 0, 
					   0, 0, b, f, f, f, b, 0, 0,
				       0, 0, b, f, f, f, b, 0, 0, 
					   b, b, b, f, f, f, b, b, b,
					   b, f, f, f, f, f, f, f, b,
					   0, b, f, f, f, f, f, b, 0,
					   0, 0, b, f, f, f, b, 0, 0,
					   0, 0, 0, b, f, b, 0, 0, 0,
					   0, 0, 0, 0, b, 0, 0, 0, 0 };

	_arrow = new Surface(9, 9);
	_arrow->setPalette(this->getPalette());
	_arrow->lock();
	for (int y = 0; y < 9;y++)
		for (int x = 0; x < 9; x++)
			_arrow->setPixel(x, y, pixels[x+(y*9)]);
	_arrow->unlock();
}

/**
 * Keeps the animation timers running.
 */
void Map::think()
{
	_scrollTimer->think(0, this);
	_animTimer->think(0, this);
}

/**
 * Draws the whole map, part by part.
 */
void Map::draw()
{
	clear();
	drawTerrain();
}

/**
* Draw the terrain.
*/
void Map::drawTerrain()
{
	int frameNumber = 0;
	Surface *frame;
	Tile *tile;
	int beginX = 0, endX = _save->getWidth() - 1;
    int beginY = 0, endY = _save->getLength() - 1;
    int beginZ = 0, endZ = _viewHeight;
	Position mapPosition, screenPosition;
	int index;
	int cacheCount = 0;

    for (int itZ = beginZ; itZ <= endZ; itZ++) 
	{
        for (int itX = beginX; itX <= endX; itX++) 
		{
            for (int itY = endY; itY >= beginY; itY--) 
			{
				mapPosition = Position(itX, itY, itZ);
				convertMapToScreen(mapPosition, &screenPosition);
				screenPosition.x += _mapOffsetX;
				screenPosition.y += _mapOffsetY;

				// only render cells that are inside the viewport
				if (screenPosition.x > -_spriteWidth && screenPosition.x < getWidth() + _spriteWidth &&
					screenPosition.y > -_spriteHeight && screenPosition.y < getHeight() + _spriteHeight )
				{
					index = _save->getTileIndex(mapPosition); // index used for tile cache

					// to minize framedrops, only recalculate x tiles per draw.
					if (cacheCount < 10)
					{
						if (cacheTileSprites(index))
							cacheCount++;
					}

					tile = _save->getTile(mapPosition);
					// Draw floor
					if (tile)
					{
						frame = _tileFloorCache[index];
						if (frame)
						{
							frame->setX(screenPosition.x);
							frame->setY(screenPosition.y);
							frame->blit(this);
						}
					}

					BattleUnit *unit = tile->getUnit();

					// Draw cursor back
					if (_selectorX == itY && _selectorY == itX && !_hideCursor)
					{
						if (_viewHeight == itZ)
						{
							if (unit)
								frameNumber = 1; // yellow box
							else
								frameNumber = 0; // red box
						}
						else if (_viewHeight > itZ)
						{
							frameNumber = 2; // blue box
						}
						frame = _res->getSurfaceSet("CURSOR.PCK")->getFrame(frameNumber);
						frame->setX(screenPosition.x);
						frame->setY(screenPosition.y);
						frame->blit(this);
					}

					// Draw walls
					if (tile)
					{
						frame = _tileWallsCache[index];
						if (frame)
						{
							frame->setX(screenPosition.x);
							frame->setY(screenPosition.y);
							frame->blit(this);
						}
					}

					// Draw items

					// Draw soldier
					if (unit)
					{
						frame = _unitCache.at(unit->getId());
						if (frame)
						{
							Position offset;
							calculateWalkingOffset(unit, &offset);
							frame->setX(screenPosition.x + offset.x);
							frame->setY(screenPosition.y + offset.y);
							frame->blit(this);
							if (unit == (BattleUnit*)_save->getSelectedUnit() && !_hideCursor)
							{
								drawArrow(screenPosition + offset);
							}
						}
					}
					// if we can see through the floor, draw the soldier below it if it is on stairs
					if (itZ > 0 && tile->hasNoFloor()) 
					{
						unit = _save->selectUnit(mapPosition + Position(0, 0, -1));
						tile = _save->getTile(mapPosition + Position(0, 0, -1));
						if (unit && tile->getTerrainLevel() < 0)
						{
							frame = _unitCache.at(unit->getId());
							if (frame)
							{
								Position offset;
								calculateWalkingOffset(unit, &offset);
								offset.y += 24;
								frame->setX(screenPosition.x + offset.x);
								frame->setY(screenPosition.y + offset.y);
								frame->blit(this);
								if (unit == (BattleUnit*)_save->getSelectedUnit() && !_hideCursor)
								{
									drawArrow(screenPosition + offset);
								}
							}
						}
					}


					// Draw cursor front
					if (_selectorX == itY && _selectorY == itX && !_hideCursor)
					{
						if (_viewHeight == itZ)
						{
							if (unit)
								frameNumber = 4; // yellow box
							else
								frameNumber = 3; // red box
						}
						else if (_viewHeight > itZ)
						{
							frameNumber = 5; // blue box
						}
						frame = _res->getSurfaceSet("CURSOR.PCK")->getFrame(frameNumber);
						frame->setX(screenPosition.x);
						frame->setY(screenPosition.y);
						frame->blit(this);
					}

					// Draw smoke/fire

				}
			}
		}
	}
}

/**
 * Blits the map onto another surface. 
 * @param surface Pointer to another surface.
 */
void Map::blit(Surface *surface)
{
	Surface::blit(surface);
}

/**
 * Ignores any mouse clicks that are outside the map.
 * @param action Pointer to an action.
 * @param state State that the action handlers belong to.
 */
void Map::mousePress(Action *action, State *state)
{

}

/**
 * Ignores any mouse clicks that are outside the map.
 * @param action Pointer to an action.
 * @param state State that the action handlers belong to.
 */
void Map::mouseRelease(Action *action, State *state)
{

}

/**
 * Ignores any mouse clicks that are outside the map
 * @param action Pointer to an action.
 * @param state State that the action handlers belong to.
 */
/*void Map::mouseClick(Action *action, State *state)
{
	int test=0;
}*/

/**
 * Handles map keyboard shortcuts.
 * @param action Pointer to an action.
 * @param state State that the action handlers belong to.
 */
void Map::keyboardPress(Action *action, State *state)
{
	Position pos;
	getSelectorPosition(&pos);
	InteractiveSurface::keyboardPress(action, state);

	// "d" - destroys all objects on a tile (for testing purposes)
	if (action->getDetails()->key.keysym.sym == SDLK_d)
	{
		_save->getTerrainModifier()->destroyTile(_save->getTile(pos));
	}
}

/**
 * Handles mouse over events.
 * @param action Pointer to an action.
 * @param state State that the action handlers belong to.
 */
void Map::mouseOver(Action *action, State *state)
{
	int posX = action->getDetails()->motion.x;
	int posY = action->getDetails()->motion.y;

	// handle RMB dragging
	if ((action->getDetails()->motion.state & SDL_BUTTON(SDL_BUTTON_RIGHT)) && RMB_SCROLL)
	{
		_RMBDragging = true;
		_scrollX = (int)(-(double)(_RMBClickX - posX) * (action->getXScale() * 2));
		_scrollY = (int)(-(double)(_RMBClickY - posY) * (action->getYScale() * 2));
		_RMBClickX = posX;
		_RMBClickY = posY;
	}
	else
	{
		// handle scrolling with mouse at edge of screen
		if (posX < (SCROLL_BORDER * action->getXScale()) && posX > 0)
		{
			_scrollX = SCROLL_AMOUNT;
			// if close to top or bottom, also scroll diagonally
			if (posY < (SCROLL_DIAGONAL_EDGE * action->getYScale()) && posY > 0)
			{
				_scrollY = SCROLL_AMOUNT;
			}
			else if (posY > (getHeight() - SCROLL_DIAGONAL_EDGE) * action->getYScale())
			{
				_scrollY = -SCROLL_AMOUNT;
			}
		}
		else if (posX > (getWidth() - SCROLL_BORDER) * action->getXScale())
		{
			_scrollX = -SCROLL_AMOUNT;
			// if close to top or bottom, also scroll diagonally
			if (posY < (SCROLL_DIAGONAL_EDGE * action->getYScale()) && posY > 0)
			{
				_scrollY = SCROLL_AMOUNT;
			}
			else if (posY > (getHeight() - SCROLL_DIAGONAL_EDGE) * action->getYScale())
			{
				_scrollY = -SCROLL_AMOUNT;
			}
		}
		else if (posX)
		{
			_scrollX = 0;
		}

		if (posY < (SCROLL_BORDER * action->getYScale()) && posY > 0)
		{
			_scrollY = SCROLL_AMOUNT;
			// if close to left or right edge, also scroll diagonally
			if (posX < (SCROLL_DIAGONAL_EDGE * action->getXScale()) && posX > 0)
			{
				_scrollX = SCROLL_AMOUNT;
			}
			else if (posX > (getWidth() - SCROLL_DIAGONAL_EDGE) * action->getXScale())
			{
				_scrollX = -SCROLL_AMOUNT;
			}
		}
		else if (posY > (getHeight() - SCROLL_BORDER) * action->getYScale())
		{
			_scrollY = -SCROLL_AMOUNT;
			// if close to left or right edge, also scroll diagonally
			if (posX < (SCROLL_DIAGONAL_EDGE * action->getXScale()) && posX > 0)
			{
				_scrollX = SCROLL_AMOUNT;
			}
			else if (posX > (getWidth() - SCROLL_DIAGONAL_EDGE) * action->getXScale())
			{
				_scrollX = -SCROLL_AMOUNT;
			}
		}
		else if (posY && _scrollX == 0)
		{
			_scrollY = 0;
		}
	}

	if ((_scrollX || _scrollY) && !_scrollTimer->isRunning())
	{
		_scrollTimer->start();
	}
	else if ((!_scrollX && !_scrollY) && _scrollTimer->isRunning())
	{
		_scrollTimer->stop();
	}

	setSelectorPosition((int)((double)posX / action->getXScale()), (int)((double)posY / action->getYScale()));
}


/**
 * Sets the value to min if it is below min, sets value to max if above max.
 * @param value pointer to the value
 * @param minimum value
 * @param maximum value
 */
void Map::minMaxInt(int *value, const int minValue, const int maxValue)
{
	if (*value < minValue)
	{
		*value = minValue;
	}
	else if (*value > maxValue)
	{
		*value = maxValue;
	}
}

/**
 * Sets the selector to a certain tile on the map.
 * @param mx mouse x position
 * @param my mouse y position
 */
void Map::setSelectorPosition(int mx, int my)
{
	if (!mx && !my) return; // cursor is offscreen
	convertScreenToMap(mx, my, &_selectorX, &_selectorY);
	minMaxInt(&_selectorX, 0, _save->getWidth() - 1);
	minMaxInt(&_selectorY, 0, _save->getLength() - 1);
}

void Map::convertScreenToMap(int screenX, int screenY, int *mapX, int *mapY)
{
	// add half a tileheight to the mouseposition per layer we are above the floor
    screenY += -_spriteHeight + (_viewHeight + 1) * (_spriteHeight / 2);

	// calculate the actual x/y pixelposition on a diamond shaped map 
	// taking the view offset into account
    *mapX = screenX - _mapOffsetX - 2 * screenY + 2 * _mapOffsetY;
    *mapY = screenY - _mapOffsetY + *mapX / 4;

	// to get the row&col itself, divide by the size of a tile
    *mapY /= (_spriteWidth / 4);
	*mapX /= _spriteWidth;
}


/**
 * Handle scrolling.
 */
void Map::scroll()
{
	_mapOffsetX += _scrollX;
	_mapOffsetY += _scrollY;

	convertScreenToMap((getWidth() / 2), (BUTTONS_AREA / 2), &_centerX, &_centerY);

	// if center goes out of map bounds, hold the scrolling (may need further tweaking)
	if (_centerX > _save->getWidth() - 1 || _centerY > _save->getLength() - 1 || _centerX < 0 || _centerY < 0)
	{
		_mapOffsetX -= _scrollX;
		_mapOffsetY -= _scrollY;
	}

	if (_RMBDragging)
	{
		_RMBDragging = false;
		_scrollX = 0;
		_scrollY = 0;
	}

	draw();
}

/**
 * Handle animating tiles/units/bullets. 8 Frames per animation.
 */
void Map::animate()
{
	_animFrame = _animFrame == 7 ? 0 : _animFrame+1;

	for (int i = 0; i < _tileCount; i++)
	{
		_save->getTiles()[i]->animate();
	}
	cacheTileSprites();
	draw();
}

/**
 * Go one level up.
 */
void Map::up()
{
	_viewHeight++;
	minMaxInt(&_viewHeight, 0, _save->getHeight()-1);
	draw();
}

/**
 * Go one level down.
 */
void Map::down()
{
	_viewHeight--;
	minMaxInt(&_viewHeight, 0, _save->getHeight()-1);
	draw();
}

/**
 * Set viewheight.
 * @param viewheight
 */
void Map::setViewHeight(int viewheight)
{
	_viewHeight = viewheight;
	draw();
}


/**
 * Center map on a certain position.
 * @param mapPos Position to center on.
 */
void Map::centerOnPosition(const Position &mapPos)
{
	Position screenPos;

	convertMapToScreen(mapPos, &screenPos);

	_mapOffsetX = -(screenPos.x - (getWidth() / 2));
	_mapOffsetY = -(screenPos.y - (BUTTONS_AREA / 2));

	convertScreenToMap((getWidth() / 2), (BUTTONS_AREA / 2), &_centerX, &_centerY);

	_viewHeight = mapPos.z;
}

/**
 * Convert map coordinates X,Y,Z to screen positions X, Y.
 * @param mapPos X,Y,Z coordinates on the map.
 * @param screenPos to screen position.
 */
void Map::convertMapToScreen(const Position &mapPos, Position *screenPos)
{
	screenPos->z = 0; // not used
	screenPos->x = mapPos.x * (_spriteWidth / 2) + mapPos.y * (_spriteWidth / 2);
	screenPos->y = mapPos.x * (_spriteWidth / 4) - mapPos.y * (_spriteWidth / 4) - mapPos.z * ((_spriteHeight + _spriteWidth / 4) / 2);
}

/**
 * Draws the small arrow above the selected soldier.
 * @param screenPos
 */
void Map::drawArrow(const Position &screenPos)
{
	_arrow->setX(screenPos.x + (_spriteWidth / 2) - (_arrow->getWidth() / 2));
	_arrow->setY(screenPos.y - _arrow->getHeight() + _animFrame);
	_arrow->blit(this);
}

/**
 * Draws the small arrow above the selected soldier.
 * @param pos pointer to a position
 */
void Map::getSelectorPosition(Position *pos)
{
	// don't know why X and Y are inverted here...
	pos->x = _selectorY;
	pos->y = _selectorX;
	pos->z = _viewHeight;
}

/**
 * Calculate the offset of a soldier, when it is walking in the middle of 2 tiles.
 * @param unit pointer to BattleUnit
 * @param offset pointer to the offset to return the calculation.
 */
void Map::calculateWalkingOffset(BattleUnit *unit, Position *offset)
{
	int offsetX[8] = { 1, 2, 1, 0, -1, -2, -1, 0 };
	int offsetY[8] = { 1, 0, -1, -2, -1, 0, 1, 2 };
	int phase = unit->getWalkingPhase();
	int dir = unit->getDirection();

	if (phase)
	{
		if (phase < 4)
		{
			offset->x = phase * 2 * offsetX[dir];
			offset->y = - phase * offsetY[dir];
		}
		else
		{
			offset->x = (phase - 8) * 2 * offsetX[dir];
			offset->y = - (phase - 8) * offsetY[dir];
		}
	}

	// If we are walking in between tiles, interpolate it's terrain level.
	if (phase)
	{
		if (phase < 4)
		{
			int fromLevel = _save->getTile(unit->getPosition())->getTerrainLevel();
			int toLevel = _save->getTile(unit->getDestination())->getTerrainLevel();
			if (unit->getPosition().z > unit->getDestination().z)
			{
				// going down a level, so toLevel 0 becomes +24, -8 becomes  16
				toLevel += 24*(unit->getPosition().z - unit->getDestination().z);
			}else if (unit->getPosition().z < unit->getDestination().z)
			{
				// going up a level, so toLevel 0 becomes -24, -8 becomes -16
				toLevel = -24*(unit->getDestination().z - unit->getPosition().z) + abs(toLevel);
			}
			offset->y += ((fromLevel * (8 - phase)) / 8) + ((toLevel * (phase)) / 8);
		}
		else
		{
			// from phase 4 onwards the unit behind the scenes already is on the destination tile
			// we have to get it's last position to calculate the correct offset
			int fromLevel = _save->getTile(unit->getLastPosition())->getTerrainLevel();
			int toLevel = _save->getTile(unit->getDestination())->getTerrainLevel();
			if (unit->getLastPosition().z > unit->getDestination().z)
			{
				// going down a level, so fromLevel 0 becomes -24, -8 becomes -32
				fromLevel -= 24*(unit->getLastPosition().z - unit->getDestination().z);
			}else if (unit->getLastPosition().z < unit->getDestination().z)
			{
				// going up a level, so fromLevel 0 becomes +24, -8 becomes 16
				fromLevel = -24*(unit->getDestination().z - unit->getLastPosition().z) + abs(fromLevel);
			}
			offset->y += ((fromLevel * (8 - phase)) / 8) + ((toLevel * (phase)) / 8);
		}
	}
	else
	{
		offset->y += _save->getTile(unit->getPosition())->getTerrainLevel();
	}

}

/**
 * This removes the selection caret and soldier selection arrow.
 * @param flag
 */
void Map::hideCursor(bool flag)
{
	_hideCursor = flag;
}

/**
 * Check if cursor is hidden.
 * @return flag
 */
bool Map::isCursorHidden()
{
	return _hideCursor;
}

void Map::cacheTileSprites()
{
	for (int i = 0; i < _tileCount; i++)
	{
		cacheTileSprites(i);
	}

}

/**
 * Caches tile's sprites.
 */
bool Map::cacheTileSprites(int i)
{
	TerrainObject *object = 0;
	Surface *frame = 0;
	Tile *tile = _save->getTiles()[i];
	bool door = false;
	
	if(tile && !tile->isCached())
	{

		/* draw a floor object on the cache (if any) */
		object = tile->getTerrainObject(O_FLOOR);
		if (object)
		{
			if (_tileFloorCache[i] == 0)
			{
				_tileFloorCache[i] = new Surface(_spriteWidth, _spriteHeight);
				_tileFloorCache[i]->setPalette(this->getPalette());
			}
			else
			{
				_tileFloorCache[i]->clear();
			}

			// Draw floor
			frame = tile->getSprite(O_FLOOR);
			frame->setX(0);
			frame->setY(-object->getYOffset());
			frame->blit(_tileFloorCache[i]);
			_tileFloorCache[i]->setShade(tile->isDiscovered()?tile->getShade():16);
		}
		else if (_tileFloorCache[i] != 0)
		{
			_tileFloorCache[i]->clear();
		}

		/* draw terrain objects on the cache (if any) */
		if (tile->getTerrainObject(O_WESTWALL) != 0 || tile->getTerrainObject(O_NORTHWALL) != 0 || tile->getTerrainObject(O_OBJECT) != 0)
		{
			if (_tileWallsCache[i] == 0)
			{
				_tileWallsCache[i] = new Surface(_spriteWidth, _spriteHeight);
				_tileWallsCache[i]->setPalette(this->getPalette());
			}
			else
			{
				_tileWallsCache[i]->clear();
			}

			// Draw west wall
			object = tile->getTerrainObject(O_WESTWALL);
			if (object)
			{
				frame = tile->getSprite(O_WESTWALL);
				frame->setX(0);
				frame->setY(-object->getYOffset());
				frame->blit(_tileWallsCache[i]);
				door = object->isDoor() || object->isUFODoor();
			}
			// Draw north wall
			object = tile->getTerrainObject(O_NORTHWALL);
			if (object)
			{
				frame = tile->getSprite(O_NORTHWALL);
				frame->setX(0);
				frame->setY(-object->getYOffset());
				// if there is a westwall, cut off some of the north wall (otherwise it will overlap)
				if (tile->getTerrainObject(O_WESTWALL))
				{
					frame->setX(frame->getWidth() / 2);
					frame->getCrop()->x = frame->getWidth() / 2;
					frame->getCrop()->w = frame->getWidth() / 2;
					frame->getCrop()->h = frame->getHeight();
				}else
				{
					frame->getCrop()->w = 0;
					frame->getCrop()->h = 0;
				}
				frame->blit(_tileWallsCache[i]);
				door = object->isDoor() || object->isUFODoor();
			}
			// Draw object
			object = tile->getTerrainObject(O_OBJECT);
			if (object)
			{
				frame = tile->getSprite(O_OBJECT);
				frame->setX(0);
				frame->setY(-object->getYOffset());
				frame->blit(_tileWallsCache[i]);
			}

			if (door && tile->getShade() > 8 && tile->isDiscovered()) // don't shade doors too dark, so you can still see them
			{
				_tileWallsCache[i]->setShade(8);
			}
			else
			{
				_tileWallsCache[i]->setShade(tile->isDiscovered()?tile->getShade():16);
			}
		}else if (_tileWallsCache[i] != 0)
		{
			_tileWallsCache[i]->clear();
		}

		// if lighting changed for a tile, then it also does for a unit standing on it
		if (tile->getUnit() != 0 && tile->getUnit()->getFaction() != FACTION_PLAYER)
		{
			tile->getUnit()->setCached(false);
		}

		tile->setCached(true);
		return true;
	}
	else
	{
		return false;
	}
}

void Map::cacheUnits()
{
	UnitSprite *unitSprite = new UnitSprite(_spriteWidth, _spriteHeight, 0, 0);
	unitSprite->setResourcePack(_res);
	unitSprite->setPalette(this->getPalette());

	for (std::vector<BattleUnit*>::iterator i = _save->getUnits()->begin(); i != _save->getUnits()->end(); i++)
	{
		if (!(*i)->isCached())
		{
			if (_unitCache.at((*i)->getId()) == 0)
			{
				_unitCache.at((*i)->getId()) = new Surface(_spriteWidth, _spriteHeight);
				_unitCache.at((*i)->getId())->setPalette(this->getPalette());
			}
			else
			{
				_unitCache.at((*i)->getId())->clear();
			}
			unitSprite->setBattleUnit((*i));
			unitSprite->draw();
			unitSprite->blit(_unitCache.at((*i)->getId()));

			// non player units get shaded according to the tile's shade
			if ((*i)->getFaction() != FACTION_PLAYER)
			{
				_unitCache.at((*i)->getId())->setShade(_save->getTile((*i)->getPosition())->getShade());
			}
		}
	}

}



}