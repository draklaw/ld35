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


#include "animation.h"


Animation::Animation(float length)
	: length(length) {
}

Animation::~Animation() {
}


MoveAnim::MoveAnim(float length, EntityRef entity,
         const Vector2& from, const Vector2& to)
	: Animation(length),
	  entity(entity),
	  from(from),
	  to(to) {
}

void MoveAnim::update(float time) {
	time = std::min(time, length);
	Vector3 p;
	p << lerp(time / length, from, to), 0;
	entity.place(p);
}


ColorAnim::ColorAnim(float length, EntityRef entity,
          const Vector4& fromColor, const Vector4& toColor)
	: Animation(length),
	  entity(entity),
	  fromColor(fromColor),
	  toColor(toColor) {
}

void ColorAnim::update(float time) {
	time = std::min(time, length);
	if(entity.sprite()) {
		entity.sprite()->setColor(lerp(time / length, fromColor, toColor));
	}
}


CompoundAnim::CompoundAnim()
	: Animation(0) {
}

void CompoundAnim::addAnim(AnimationSP anim) {
	length = std::max(length, anim->length);
	anims.push_back(anim);
}

void CompoundAnim::update(float time) {
	for(AnimationSP anim: anims) {
		anim->update(time);
	}
}


SequenceAnim::SequenceAnim()
	: Animation(0) {
}

void SequenceAnim::addAnim(AnimationSP anim) {
	length += anim->length;
	anims.push_back(anim);
}

void SequenceAnim::update(float time) {
	float start = 0;
	for(AnimationSP anim: anims) {
		if(start > time) {
			return;
		}
		anim->update(time - start);
		start += anim->length;
	}
}
