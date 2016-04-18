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


#include <random>

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
      _length(0),
      _hTiles(4),
      _vTiles(4),
      _nRows (22){
}


unsigned Map::beginIndex(int col) const {
	return std::lower_bound(_blocks.begin(), _blocks.end(),
	                 Block{ Vector2i(col, 0), WALL }, blockCmp) - _blocks.begin();
}


unsigned Map::endIndex(int col) const {
	return beginIndex(col + 41);
}


Box2 Map::hit(const Box2& box, int bi, float dScroll) const {
	//FIXME: Hack !
	if (_blocks[bi].type != WALL)
		return Box2(Vector2(0,0),Vector2(0,0));

	Box2 bb = blockBox(bi);
	bb.min() -= Vector2(dScroll, 0);

	return box.intersection(bb);
}


Box2 Map::pickup(const Box2& box, int bi, float dScroll) {
	if (_blocks[bi].type != POINT)
		return Box2(Vector2(0,0),Vector2(0,0));

	Box2 bb = blockBox(bi);
	bb.min() -= Vector2(dScroll, 0);

	return box.intersection(bb);
}


void Map::clearBlock(int bi)
{
	_blocks[bi].type = EMPTY;
}


Box2 Map::blockBox(int i) const {
	const Block& b = _blocks[i];
	Vector2 p = b.pos.cast<float>() * _state->blockSize();
	return Box2(p, p + Vector2(_state->blockSize(), _state->blockSize()));
}


void Map::initialize() {
	AssetSP tilesAsset = _state->loader()->loadAsset<ImageLoader>("tiles.png");
	_tilesTex = _state->renderer()->createTexture(tilesAsset);

	AssetSP warningAsset = _state->loader()->loadAsset<ImageLoader>("warning.png");
	_warningTex = _state->renderer()->createTexture(warningAsset);

	registerSection("segment_1.png");
	registerSection("segment_2.png");
	registerSection("segment_3.png");
}


void Map::setBg(unsigned i, const Path& path) {
	lairAssert(i < 3);
	AssetSP bgAsset = _state->loader()->loadAsset<ImageLoader>(path);
	_bgTex[i] = _state->renderer()->createTexture(bgAsset);
}


void Map::setBgScroll(unsigned i, float scroll) {
	lairAssert(i < 3);
	_bgScroll[i] = scroll;
}


void Map::registerSection(const Path& path) {
	AssetSP asset = _state->loader()->loadAsset<ImageLoader>(path);
	_sections.push_back(asset->aspect<ImageAspect>());
}


void Map::clear() {
	_length = 0;
	_blocks.clear();
}


void Map::appendSection(unsigned i) {
	lairAssert(i < _sections.size());
	const ImageSP img = _sections[i].lock()->get();
	lairAssert(img->format() == Image::FormatRGBA8
	        || img->format() == Image::FormatRGB8);
	const uint8* pixels = reinterpret_cast<const uint8*>(img->data());
	unsigned pxSize = Image::formatByteSize(img->format());
	for(unsigned col = 0; col < img->width(); ++col) {
		for(unsigned row = 0; row < img->height(); ++row) {
			unsigned frow = img->height() - row - 1; // vertical flip
			const uint8* pixel = pixels + ((col + frow*img->width()) * pxSize);
			uint8 r = pixel[0];
			uint8 g = pixel[1];
			uint8 b = pixel[2];
			if(r == 0 && g == 0 && b == 0) {
				_blocks.push_back(Block{ Vector2i(_length, row), WALL });
			}
			if(r == 0 && g == 255 && b == 0) {
				_blocks.push_back(Block{ Vector2i(_length, row), POINT });
			}
		}
		_length += 1;
	}
}


void Map::generate(unsigned seed, unsigned minLength, float difficulty,
                   float variance) {
	clear();

	for(int i = 0; i < _sections.size(); ++i) {
		appendSection(i);
	}
//	unsigned size = _sections.size();
//	unsigned rangeSize = size * variance;
//	unsigned begin = std::max(size - rangeSize, 0u);
//	unsigned end   = std::min(begin + rangeSize, size);

//	std::seed_seq seedSeq{ seed };
//	std::mt19937 rEngine(seedSeq);
//	std::uniform_int_distribution<unsigned> rand(begin, end-1);

//	while(_length < minLength) {
//		appendSection(rand(rEngine));
//	}

//	std::sort(_blocks.begin(), _blocks.end(), blockCmp);
}


void Map::render(float scroll, float pDist, float screenWidth) {
	SpriteRenderer* renderer = _state->spriteRenderer();

	_state->renderer()->uploadPendingTextures();

	Matrix4 trans = _state->screenTransform();
	Vector4 color(1, 1, 1, 1);

	// Backgrounds
	for(int i = 0; i < 3; ++i) {
		if(!_bgTex[i] || !_bgTex[i]->get())
			continue;

		TextureSP bgTex = _bgTex[i]->_get();
		float bgScroll = scroll * _bgScroll[i] / bgTex->width();
		Box2 bgBox(Vector2(0, 0), Vector2(1920, 1080));
		Box2 bgTexBox(Vector2(bgScroll, 0), Vector2(bgScroll + 1920.f / bgTex->width(), 1));
		renderer->addSprite(trans, bgBox, color, bgTexBox, bgTex,
							Texture::TRILINEAR, BLEND_ALPHA);
	}

	// Warnings
	float rightScroll = scroll + screenWidth;
	unsigned wbeginCol = beginIndex(rightScroll / _state->blockSize());
	unsigned wendCol   = beginIndex((rightScroll + pDist) / _state->blockSize());
	TextureSP warningTex = _warningTex->get();
	std::vector<float> warnings(_nRows, 0);
	for(unsigned i = wbeginCol; i < wendCol; ++i) {
		const Block& b = _blocks[i];
		if(b.pos(1) != 0 && b.pos(1) < 21 && warnings[b.pos(1)] == 0 && b.type == WALL) {
			warnings[b.pos(1)] = 1 - ((b.pos(0) + 1) * _state->blockSize() - scroll - screenWidth) / pDist;
		}
	}
	for(unsigned i = 1; i < _nRows-1; ++i) {
		if(warnings[i] > 0) {
			Box2 pos(Vector2(screenWidth * (1 - warnings[i]), i * _state->blockSize()),
			         Vector2(screenWidth * (2 - warnings[i]), (i+1) * _state->blockSize()));
			Box2 texCoord(Vector2(0, 0), Vector2(1, 1));
			Vector4 color(1, 0, 0, .5);
			renderer->addSprite(trans, pos, color, texCoord, warningTex,
			                    Texture::TRILINEAR, BLEND_ALPHA);
		}
	}

	// Tiles
	TextureSP tilesTex = _tilesTex->_get();
	Vector2 tileSize(1. / _hTiles, 1. / _vTiles);

	unsigned beginCol = scroll / _state->blockSize();
	unsigned endCol   = beginCol + 41;
	unsigned i = beginIndex(beginCol);
	while(i < _blocks.size() && _blocks[i].pos(0) < endCol) {
		unsigned ti = _blocks[i].type;
		Vector2 tilePos(float(ti % _hTiles) / float(_hTiles),
		                float(ti / _hTiles) / float(_vTiles));
		Box2 texCoord(tilePos, tilePos + tileSize);
		Box2 coords = offsetBox(blockBox(i), Vector2(-scroll, 0));
		renderer->addSprite(trans, coords, color, texCoord, tilesTex,
		                    Texture::TRILINEAR, BLEND_ALPHA);
		++i;
	}
}


void Map::renderPreview(float scroll, float pDist, float screenWidth, float pWidth) {
	SpriteRenderer* renderer = _state->spriteRenderer();

	Matrix4 trans = _state->screenTransform();
	Vector4 colorPoint(.2, 1, .2, 1);
	Vector4 colorWarning(1, .3, 0, 1);
	TextureSP tilesTex = _tilesTex->_get();
	Vector2 tileSize(1. / _hTiles, 1. / _vTiles);

	float rightScroll = scroll + screenWidth;
	unsigned beginCol = beginIndex(rightScroll / _state->blockSize());
	unsigned endCol   = beginIndex((rightScroll + pDist) / _state->blockSize());

	std::vector<unsigned> blocks;
	for(unsigned row = 1; row < _nRows-1; ++ row) {
		bool gotPoint = false;
		for(unsigned i = beginCol; i < endCol; ++i) {
			const Block& b = _blocks[i];
			if(b.pos(1) == row) {
				if(b.type == WALL) {
					blocks.push_back(i);
					break;
				}
				if(b.type == POINT && !gotPoint) {
					blocks.push_back(i);
					gotPoint = true;
				}
			}
		}
	}

	for(unsigned bi = 0; bi < blocks.size(); ++bi) {
		unsigned i = blocks[bi];
		unsigned ti = _blocks[i].type + PREVIEW_OFFSET;
		Vector2 tilePos(float(ti % _hTiles) / float(_hTiles),
						float(ti / _hTiles) / float(_vTiles));
		Box2 texCoord(tilePos, tilePos + tileSize);
		Box2 coords = blockBox(i);
		float scale = ((ti == PREVIEW_OFFSET)? 2: 1.2) - (coords.max()(0) - scroll - screenWidth) / pDist;

		coords.min()(0) = (coords.min()(0) - rightScroll) * pWidth / pDist
						+ screenWidth - pWidth - _state->blockSize();
		coords.max()(0) = coords.min()(0) + _state->blockSize();

		Vector2 a(coords.max()(0), (coords.min()(1) + coords.max()(1)) / 2);
		coords.min() = (coords.min() - a) * scale + a;
		coords.max() = (coords.max() - a) * scale + a;

		Vector4 color = (ti == PREVIEW_OFFSET)? colorWarning: colorPoint;
		renderer->addSprite(trans, coords, color, texCoord, tilesTex,
							Texture::TRILINEAR, BLEND_ALPHA);
	}
}


bool Map::blockCmp(const Block& b0, const Block& b1) {
	return  b0.pos(0) <  b1.pos(0)
	    || (b0.pos(0) == b1.pos(0) && b0.pos(1) < b1.pos(1));
}

