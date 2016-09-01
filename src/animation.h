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


#ifndef _LAIR_DEMO_TEMPLATE_ANIMATION_H
#define _LAIR_DEMO_TEMPLATE_ANIMATION_H


#include <lair/core/signal.h>

#include <lair/ec/entity.h>
#include <lair/ec/sprite_component.h>
#include <lair/ec/bitmap_text_component.h>

#include "map.h"


using namespace lair;


class Animation {
public:
	Animation(float length);
	virtual ~Animation();

	virtual void update(float time) = 0;

public:
	float length;
};
typedef std::shared_ptr<Animation> AnimationSP;


class MoveAnim : public Animation {
public:
	MoveAnim(float length, EntityRef entity,
	         const Vector3& from, const Vector3& to);

	virtual void update(float time);

public:
	EntityRef entity;
	Vector3   from;
	Vector3   to;
};
typedef std::shared_ptr<MoveAnim> MoveAnimSP;


class ColorAnim : public Animation {
public:
	ColorAnim(SpriteComponentManager* sprites, float length, EntityRef entity,
	          const Vector4& fromColor, const Vector4& toColor);

	virtual void update(float time);

public:
	SpriteComponentManager* sprites;
	EntityRef entity;
	Vector4   fromColor;
	Vector4   toColor;
};
typedef std::shared_ptr<ColorAnim> ColorAnimSP;


class CompoundAnim : public Animation {
public:
	CompoundAnim();

	void addAnim(AnimationSP anim);
	virtual void update(float time);

public:
	std::vector<AnimationSP> anims;
};
typedef std::shared_ptr<CompoundAnim> CompoundAnimSP;


class SequenceAnim : public Animation {
public:
	SequenceAnim();

	void addAnim(AnimationSP anim);
	virtual void update(float time);

public:
	std::vector<AnimationSP> anims;
};
typedef std::shared_ptr<SequenceAnim> SequenceAnimSP;


#endif
