/*
 *  Copyright (C) 2016 the authors (see AUTHORS)
 *
 *  This file is part of ld35.
 *
 *  lair is free software: you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  lair is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with lair.  If not, see <http://www.gnu.org/licenses/>.
 *
 */


#include <lair/ec/sprite_renderer.h>

#include "main_state.h"

#include "map.h"


Box2 offsetBox(const Box2& box, const Vector2& offset) {
	Box2 b = box;
	b.min() += offset;
	b.max() += offset;
	return b;
}


Map::Map(MainState* mainState)
	: _state(mainState),
      _length(0) {
}


unsigned Map::beginIndex(int col) const {
	return std::lower_bound(_blocks.begin(), _blocks.end(),
	                 Block{ Vector2i(col, 0), WALL }, blockCmp) - _blocks.begin();
}


unsigned Map::endIndex(int col) const {
	return beginIndex(col + 41);
}


Box2 Map::hit(const Box2& box, int bi, float dScroll) const {
	Box2 bb = blockBox(bi);
	bb.min() -= Vector2(dScroll, 0);
	return box.intersection(bb);
}


Box2 Map::blockBox(int i) const {
	const Block& b = _blocks[i];
	Vector2 p = b.pos.cast<float>() * _state->blockSize();
	return Box2(p, p + Vector2(_state->blockSize(), _state->blockSize()));
}


void Map::initialize() {
	AssetSP bgAsset = _state->loader()->loadAsset<ImageLoader>("bg.png");
	_bgTex = _state->renderer()->createTexture(bgAsset);

	AssetSP tilesAsset = _state->loader()->loadAsset<ImageLoader>("ship_block.png");
	_tilesTex = _state->renderer()->createTexture(tilesAsset);
}


void Map::generate() {
	_blocks.clear();

	_length = 20;
	for(int i = 0; i < 3; ++i) {
		_blocks.push_back(Block{ Vector2i(10, 10-i), WALL });
	}
	for(int i = 0; i < 3; ++i) {
		_blocks.push_back(Block{ Vector2i(10+i, 12), WALL });
		_blocks.push_back(Block{ Vector2i(10+i, 14), WALL });
	}
	for(int i = 0; i < 400; ++i) {
		_blocks.push_back(Block{ Vector2i(i, 0), WALL });
	}

	std::sort(_blocks.begin(), _blocks.end(), blockCmp);
}


void Map::render(float scroll) {
	SpriteRenderer* renderer = _state->spriteRenderer();

	_state->renderer()->uploadPendingTextures();

	Matrix4 trans = _state->screenTransform();
	Vector4 color(1, 1, 1, 1);

	TextureSP bgTex = _bgTex->_get();
	float bgScroll = scroll / 1920.f;
	Box2 bgBox(Vector2(0, 0), Vector2(1920, 1080));
	Box2 bgTexBox(Vector2(bgScroll, 0), Vector2(bgScroll + 1, 1));
	renderer->addSprite(trans, bgBox, color, bgTexBox, bgTex,
	                    Texture::TRILINEAR, BLEND_NONE);

	TextureSP tilesTex = _tilesTex->_get();
	Box2 texCoord(Vector2(0, 0), Vector2(1, 1));

	unsigned beginCol = scroll / _state->blockSize();
	unsigned endCol   = beginCol + 41;
	unsigned i = beginIndex(beginCol);
	while(i < _blocks.size() && _blocks[i].pos(0) < endCol) {
		Box2 coords = offsetBox(blockBox(i), Vector2(-scroll, 0));
		renderer->addSprite(trans, coords, color, texCoord, tilesTex,
		                    Texture::TRILINEAR, BLEND_NONE);
		++i;
	}
}


bool Map::blockCmp(const Block& b0, const Block& b1) {
	return  b0.pos(0) <  b1.pos(0)
	    || (b0.pos(0) == b1.pos(0) && b0.pos(1) < b1.pos(1));
}

