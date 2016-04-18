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
#include <lair/core/json.h>

#include <lair/utils/game_state.h>
#include <lair/utils/interp_loop.h>
#include <lair/utils/input.h>

#include <lair/render_gl2/orthographic_camera.h>

#include <lair/ec/entity.h>
#include <lair/ec/entity_manager.h>
#include <lair/ec/sprite_component.h>
#include <lair/ec/bitmap_text_component.h>

#include "animation.h"

#include "map.h"


using namespace lair;


class Game;

typedef decltype(Transform().translation().head<2>()) Vec2;

typedef std::vector<EntityRef> EntityVector;

void shipSoundCb(int chan, void *stream, int len, void *udata);


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
	float warningScrollDist() const;

	void playAnimation(const std::string& name);
	void updateAnimation(float time);
	void nextAnimationStep();
	void endAnimation();

	void startGame(int level);
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
	const Matrix4& screenTransform() const { return _gameLayer.transform().matrix(); }

	SpriteRenderer* spriteRenderer() { return &_spriteRenderer; }

	// Callback to play ship soudn
	friend void shipSoundCb(int chan, void *stream, int len, void *udata);

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
	uint64     _prevFrameTime;

	Input* _quitInput;
	Input* _restartInput;
	Input* _accelInput;
	Input* _brakeInput;
	Input* _climbInput;
	Input* _diveInput;
	Input* _stretchInput;
	Input* _shrinkInput;
	Input* _skipInput;

	AssetSP _beamsTex;

	enum SoundChannel {
		CHANN_WARNING,
		CHANN_POINT,
		CHANN_CRASH
	};

	AssetSP _warningSound;
	AssetSP _pointSound;
	AssetSP _crashSound;

	EntityRef    _gameLayer;
	EntityRef    _hudLayer;
	EntityRef    _scoreText;
	EntityRef    _speedText;
	EntityRef    _distanceText;
	EntityRef    _charSprite;
	EntityRef    _dialogBg;
	EntityRef    _dialogText;
	EntityRef    _ship;
	EntityVector _shipParts;

	Map _map;

	enum AnimState {
		ANIM_NONE,
		ANIM_PLAY,
		ANIM_WAIT
	};

	Json::Value  _animations;
	AnimationSP  _anim;
	float        _animPos;
	AnimState    _animState;
	std::string  _animCurrent;
	int          _animStep;

	// Game states
	Vec2 shipPosition();
	Vec2 partPosition(unsigned part);
	Box2 partBox (unsigned part);

	int         _levelCount;
	int         _currentLevel;
	std::vector<Vector4> _levelColors;

	bool        _pause;

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

	AssetWP     _shipSound;
	int         _shipSoundSample;
	int64       _lastPointSound;
	int         _warningTileEndIndex;
	std::vector<bool> _warningMap;

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
	float _partDropSpeed;
	float _snapDistance;
	float _massRatio;
};


#endif
