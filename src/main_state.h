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


#ifndef _LAIR_DEMO_TEMPLATE_MAIN_STATE_H
#define _LAIR_DEMO_TEMPLATE_MAIN_STATE_H


#include <lair/core/signal.h>

#include <lair/utils/game_state.h>
#include <lair/utils/interp_loop.h>
#include <lair/utils/input.h>

#include <lair/render_gl2/orthographic_camera.h>

#include <lair/ec/entity.h>
#include <lair/ec/entity_manager.h>
#include <lair/ec/sprite_component.h>
#include <lair/ec/bitmap_text_component.h>

#include "map.h"


using namespace lair;


class Game;

typedef decltype(Transform().translation().head<2>()) Vec2;

typedef std::vector<EntityRef> EntityVector;

class MainState : public GameState {
public:
	MainState(Game* game);
	virtual ~MainState();

	virtual void initialize();
	virtual void shutdown();

	virtual void run();
	virtual void quit();

	Game* game();

	unsigned shipShapeCount() const;
	Vector2 partExpectedPosition(unsigned shape, unsigned part) const;

	void startGame();
	void updateTick();
	void updateFrame();

	void renderBeam(const Matrix4& trans, TextureSP tex, const Vector2& p0,
	                const Vector2& p1, const Vector4& color,
	                float texOffset, unsigned row, unsigned rowCount);
	void renderBeams(float interp);

	void resizeEvent();

	EntityRef loadEntity(const Path& path, EntityRef parent = EntityRef(),
	                     const Path& cd = Path());

	float blockSize() const { return _blockSize; }
	const Matrix4& screenTransform() const { return _root.transform().matrix(); }

	SpriteRenderer* spriteRenderer() { return &_spriteRenderer; }

protected:
	// More or less system stuff

	EntityManager              _entities;
	SpriteRenderer             _spriteRenderer;
	SpriteComponentManager     _sprites;
	BitmapTextComponentManager _texts;
//	AnimationComponentManager  _anims;
	InputManager               _inputs;

	SlotTracker _slotTracker;

	OrthographicCamera _camera;

	bool       _initialized;
	bool       _running;
	InterpLoop _loop;
	int64      _fpsTime;
	unsigned   _fpsCount;

	Input* _quitInput;
	Input* _accelInput;
	Input* _brakeInput;
	Input* _climbInput;
	Input* _diveInput;
	Input* _stretchInput;
	Input* _shrinkInput;

	AssetSP _beamsTex;

	EntityRef    _root;
	EntityRef    _scoreText;
	EntityRef    _speedText;
	EntityRef    _distanceText;
	EntityRef    _ship;
	EntityVector _shipParts;

	Map _map;

	// Game states
	Vec2 shipPosition();
	Vec2 partPosition(unsigned part);
	Box2 partBox (unsigned part);

	float       _prevScrollPos;
	float       _scrollPos;
	float       _distance;
	unsigned    _score;
	Vector4     _levelColor;
	Vector4     _levelColor2;
	Vector4     _beamColor;
	Vector4     _laserColor;
	Vector4     _textColor;

	float       _shipHSpeed;
	float       _shipVSpeed;
	float       _climbCharge;
	float       _diveCharge;

	std::vector<bool>    _partAlive;
	unsigned             _shipShape;

	// Happenings
	float collide     (unsigned part);
	void  collect     (unsigned part);
	void  destroyPart (unsigned part);

	// Constant params
	std::vector<Vector2> _shipShapes;
	unsigned _shipPartCount;

	float _blockSize;

	float _hSpeedDamping;
	float _acceleration;
	float _braking;

	float _thrustMaxCharge;
	float _thrustRateCharge;
	float _thrustPower;

	float _vSpeedDamping;
	float _vSpeedFloor;
	float _vSpeedCap;
	float _vLockTime;

	float _scratchThreshold;
	float _crashThreshold;
	float _bumpawayTime;

	float _partBaseSpeed;
	float _snapDistance;
	float _massRatio;
};


#endif
