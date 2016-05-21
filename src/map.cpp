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

#include <lair/sys_sdl2/image_loader.h>

#include <lair/ec/sprite_renderer.h>

#include "main_state.h"

#include "map.h"


#define NOHIT Box2(Vector2(0,0),Vector2(0,0))


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
	if (_blocks[bi].type != WALL)
		return NOHIT;

	Box2 bb = blockBox(bi);
	bb.min() -= Vector2(dScroll, 0);

	return box.intersection(bb);
}


Box2 Map::pickup(const Box2& box, int bi, float dScroll) {
	if (_blocks[bi].type != POINT)
		return NOHIT;

	Box2 bb = blockBox(bi);
	bb.min() -= Vector2(dScroll, 0);

	return box.intersection(bb);
}


void Map::clearBlock(int bi)
{
	_blocks[bi].type = EMPTY;
}


bool Map::hasWallAtYInRange(int y, int begin, int end) const {
	for(int i = begin; i < end; ++i) {
		if(_blocks[i].pos(1) == y && _blocks[i].type == WALL)
			return true;
	}
	return false;
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
	registerSection("segment.png");
	registerSection("segment_20.png");
	registerSection("segment_19.png");
	registerSection("segment_18.png");
	registerSection("segment_17.png");
	registerSection("segment_16.png");
	registerSection("segment_15.png");
	registerSection("segment_14.png");
	registerSection("segment_13.png");
	registerSection("segment_12.png");
	registerSection("segment_11.png");
	registerSection("segment_10.png");
	registerSection("segment_9.png");
	registerSection("segment_8.png");
	registerSection("segment_7.png");
	registerSection("segment_6.png");
	registerSection("segment_5.png");
	registerSection("segment_4.png");
	registerSection("segment_3.png");
	registerSection("segment_2.png");
	registerSection("segment_1.png");
	registerSection("segment_empty.png");
	registerSection("segment_6_obvious.png");
	registerSection("segment_6_tricky.png");
	registerSection("segment_5_hard.png");
	registerSection("segment_t0.png");
	registerSection("segment_t1_long.png");
	registerSection("segment_t1.png");
	registerSection("segment_t2.png");
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


void Map::setWarningColor(const Vector4& color) {
	_warningColor = color;
}


void Map::setPointColor(const Vector4& color) {
	_pointColor = color;
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
	appendSection(img);
}


void Map::appendSection(const ImageSP img) {
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


void Map::appendSection(const Path& path) {
	AssetSP asset = _state->loader()->loadAsset<ImageLoader>(path);
	_state->loader()->waitAll();
	ImageAspectSP aspect = asset->aspect<ImageAspect>();
	appendSection(aspect->get());
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


void Map::render(float scroll, float pDist, float screenWidth, float pWidth,
                 const OrthographicCamera& camera) {
	RenderPass* renderPass = _state->renderPass();
	SpriteRenderer* renderer = _state->spriteRenderer();

	RenderPass::DrawStates states;
	states.shader = renderer->shader().shader;
	states.buffer = renderer->buffer();
	states.format = renderer->format();
	states.textureFlags = Texture::TRILINEAR | Texture::REPEAT;
	states.blendingMode = BLEND_ALPHA;

	const ShaderParameter* params = renderer->addShaderParameters(
	            renderer->shader(), camera.transform(), 0);

	Matrix4 trans = Matrix4::Identity();
	Vector4 color(1, 1, 1, 1);

	// Backgrounds
	for(int i = 0; i < 3; ++i) {
		if(!_bgTex[i] || !_bgTex[i]->get())
			continue;

		TextureSP bgTex = _bgTex[i]->_get();

		trans(2, 3) = float(i) / 10.f;
		float bgScroll = scroll * _bgScroll[i] / bgTex->width();
		Box2 bgBox(Vector2(0, 0), Vector2(1920, 1080));
		Box2 bgTexBox(Vector2(bgScroll, 0), Vector2(bgScroll + 1920.f / bgTex->width(), 1));

		unsigned vxIndex = renderer->indexCount();
		renderer->addSprite(trans, bgBox, color, bgTexBox);
		unsigned vxCount = renderer->indexCount() - vxIndex;

		states.texture = bgTex;
		renderPass->addDrawCall(states, params, 1.f - trans(2, 3), vxIndex, vxCount);
	}

	// Warnings
	float rightScroll = scroll + screenWidth;
	unsigned wbeginCol = beginIndex(rightScroll / _state->blockSize() - screenWidth);
	unsigned wendCol   = beginIndex((rightScroll + pDist) / _state->blockSize());
	TextureSP warningTex = _warningTex->get();
	std::vector<float> warnings(_nRows, 0);
	for(unsigned i = wbeginCol; i < wendCol; ++i) {
		const Block& b = _blocks[i];
		if(b.pos(1) != 0 && b.pos(1) < 21 /*&& warnings[b.pos(1)] == 0*/ && b.type == WALL) {
			float w = (b.pos(0) + 1) * _state->blockSize() - scroll - screenWidth;
			w = (w > 0)? 1 - w / pDist: 1 + w / screenWidth;
			warnings[b.pos(1)] = std::max(warnings[b.pos(1)], w);
		}
	}
	Vector4 wColor = _warningColor;
	wColor(3) *= .7;
	trans(2, 3) = 0.25;
	unsigned vxIndex = renderer->indexCount();
	for(unsigned i = 1; i < _nRows-1; ++i) {
		if(warnings[i] > 0) {
			Box2 pos(Vector2(screenWidth * (1 - warnings[i]), i * _state->blockSize()),
			         Vector2(screenWidth * (2 - warnings[i]), (i+1) * _state->blockSize()));
			Box2 texCoord(Vector2(0, 0), Vector2(1, 1));
			renderer->addSprite(trans, pos, wColor, texCoord);
		}
	}
	unsigned vxCount = renderer->indexCount() - vxIndex;

	states.texture = warningTex;
	renderPass->addDrawCall(states, params, 1.f - trans(2, 3), vxIndex, vxCount);

	// Tiles
	TextureSP tilesTex = _tilesTex->_get();
	Vector2 tileSize(1. / _hTiles, 1. / _vTiles);

	trans(2, 3) = 0.3;

	unsigned beginCol = scroll / _state->blockSize();
	unsigned endCol   = beginCol + 41;
	unsigned i = beginIndex(beginCol);
	vxIndex = renderer->indexCount();
	while(i < _blocks.size() && _blocks[i].pos(0) < endCol) {
		unsigned ti = _blocks[i].type;
		Vector2 tilePos(float(ti % _hTiles) / float(_hTiles),
		                float(ti / _hTiles) / float(_vTiles));
		Box2 texCoord(tilePos, tilePos + tileSize);
		Box2 coords = offsetBox(blockBox(i), Vector2(-scroll, 0));
		renderer->addSprite(trans, coords, color, texCoord);
		++i;
	}
	vxCount = renderer->indexCount() - vxIndex;

	states.texture = tilesTex;
	renderPass->addDrawCall(states, params, 1.f - trans(2, 3), vxIndex, vxCount);

	// Preview
	{
		trans(2, 3) = 0.55;

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

		vxIndex = renderer->indexCount();
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

			Vector4 color = (ti == PREVIEW_OFFSET)? _warningColor: _pointColor;
			renderer->addSprite(trans, coords, color, texCoord);
		}
		vxCount = renderer->indexCount() - vxIndex;

		renderPass->addDrawCall(states, params, 1.f - trans(2, 3), vxIndex, vxCount);
	}
}


bool Map::blockCmp(const Block& b0, const Block& b1) {
	return  b0.pos(0) <  b1.pos(0)
	    || (b0.pos(0) == b1.pos(0) && b0.pos(1) < b1.pos(1));
}

