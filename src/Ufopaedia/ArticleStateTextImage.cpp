/*
 * Copyright 2011 OpenXcom Developers.
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

#include "Ufopaedia.h"
#include "../Ruleset/ArticleDefinition.h"
#include "ArticleStateTextImage.h"
#include "../Engine/Game.h"
#include "../Engine/Palette.h"
#include "../Engine/Surface.h"
#include "../Engine/Language.h"
#include "../Resource/ResourcePack.h"
#include "../Interface/Text.h"
#include "../Interface/TextButton.h"

namespace OpenXcom
{
	
	ArticleStateTextImage::ArticleStateTextImage(Game *game, ArticleDefinitionTextImage *defs) : ArticleState(game, defs->id)
	{
		// add screen elements
		_txtTitle = new Text(140, 32, 5, 24);
		
		// Set palette
		_game->setPalette(_game->getResourcePack()->getPalette("PALETTES.DAT_3")->getColors());
		
		ArticleState::initLayout();
		
		// add other elements
		add(_txtTitle);
		
		// Set up objects
		_game->getResourcePack()->getSurface(defs->image_id)->blit(_bg);
		_btnOk->setColor(Palette::blockOffset(5)+3);
		_btnPrev->setColor(Palette::blockOffset(5)+3);
		_btnNext->setColor(Palette::blockOffset(5)+3);
		
		_txtTitle->setColor(Palette::blockOffset(15)+4);
		_txtTitle->setBig();
		_txtTitle->setAlign(ALIGN_LEFT);
		_txtTitle->setWordWrap(true);
		_txtTitle->setText(Ufopaedia::buildText(_game, defs->title));

		int text_height = _txtTitle->getTextHeight();
		
		_txtInfo = new Text(150, 166 - text_height, 5, 25 + text_height);
		add(_txtInfo);
		
		_txtInfo->setColor(Palette::blockOffset(14)+15);
		_txtInfo->setAlign(ALIGN_LEFT);
		_txtInfo->setWordWrap(true);
		_txtInfo->setText(Ufopaedia::buildText(_game, defs->text));
	}
	
	ArticleStateTextImage::~ArticleStateTextImage()
	{}
	
}