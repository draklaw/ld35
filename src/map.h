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

#include <lair/render_gl2/texture.h>


using namespace lair;

class MainState;


Box2 offsetBox(const Box2& box, const Vector2& offset);


class Map {
public:
	enum BlockType {
		WALL,
		POINTS
	};

public:
	Map(MainState* mainState);

	unsigned beginIndex(int col) const;
	unsigned endIndex(int col) const;

	Box2 blockBox(int i) const;

	Box2 hit(const Box2& box, int bi, float dScroll) const;

	void initialize();
	void generate();

	void render(float scroll);

private:
	struct Block {
		Vector2i  pos;
		BlockType type;
	};
	typedef std::vector<Block> BlockVector;

	static bool blockCmp(const Block& b0, const Block& b1);

private:
	MainState*      _state;

	TextureAspectSP _bgTex;
	TextureAspectSP _tilesTex;

	int             _length;
	BlockVector     _blocks;
};


#endif
