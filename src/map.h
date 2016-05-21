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


#ifndef _LD35_MAP_H
#define _LD35_MAP_H


#include <lair/core/lair.h>
#include <lair/core/log.h>

#include <lair/render_gl2/orthographic_camera.h>
#include <lair/render_gl2/texture.h>


using namespace lair;

class MainState;


Box2 offsetBox(const Box2& box, const Vector2& offset);


class Map {
public:
	enum BlockType {
		WALL  = 0,
		POINT = 2,
		EMPTY = 5,

		PREVIEW_OFFSET = 12,
	};

public:
	Map(MainState* mainState);

	unsigned beginIndex(int col) const;
	unsigned endIndex(int col) const;

	Box2 blockBox(int i) const;
	int length() const { return _length; }

	Box2 hit(const Box2& box, int bi, float dScroll) const;
	Box2 pickup(const Box2& box, int bi, float dScroll);
	void clearBlock(int bi);

	bool hasWallAtYInRange(int y, int begin, int end) const;

	void initialize();
	void registerSection(const Path& path);
	void setBg(unsigned i, const Path& path);
	void setBgScroll(unsigned i, float scroll);

	void setWarningColor(const Vector4& color);
	void setPointColor(const Vector4& color);

	void clear();
	void appendSection(unsigned i);
	void appendSection(const ImageSP aspect);
	void appendSection(const Path& path);
	void generate(unsigned seed, unsigned minLength, float difficulty,
	              float variance=.3);

	void updateComming(float scroll, float pDist, float screenWidth);
	void render(float scroll, float pDist, float screenWidth, float pWidth,
	            const OrthographicCamera& camera);

private:
	struct Block {
		Vector2i  pos;
		BlockType type;
	};
	typedef std::vector<Block> BlockVector;

	static bool blockCmp(const Block& b0, const Block& b1);

	typedef std::vector<ImageAspectWP> SectionVector;

	typedef std::vector<int> CommingVector;

private:
	MainState*      _state;

	TextureAspectSP _bgTex[3];
	float           _bgScroll[3];
	TextureAspectSP _tilesTex;
	unsigned        _hTiles;
	unsigned        _vTiles;
	TextureAspectSP _warningTex;

	Vector4         _warningColor;
	Vector4         _pointColor;

	SectionVector   _sections;
	unsigned        _nRows;

	int             _length;
	BlockVector     _blocks;
	CommingVector   _comming;
};


#endif
