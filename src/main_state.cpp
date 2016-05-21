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


#include <functional>

#include <lair/core/json.h>

#include "game.h"
#include "splash_state.h"

#include "main_state.h"


#define ONE_SEC (1000000000)

//FIXME?
#define SCREEN_WIDTH  1920
#define SCREEN_HEIGHT 1080

#define BUFSIZE 128

#define FRAMERATE 60


Vector4 parseColor(const Json::Value& color) {
	lairAssert(color.isArray() && color.size() == 4);
	Vector4 c;
	for(int i = 0; i < 4; ++i) {
		c(i) = color[i].asFloat() / 255.f;
	}
	return c;
}


void dumpEntities(EntityRef entity, int level) {
	dbgLogger.log(std::string(2*level, ' '), entity.name(), ", ", entity.worldTransform()(2, 3));
	EntityRef e = entity.firstChild();
	while(e.isValid()) {
		dumpEntities(e, level + 1);
		e = e.nextSibling();
	}
}

MainState::MainState(Game* game)
	: GameState(game),

      _entities(log()),
      _renderPass(renderer()),
      _spriteRenderer(renderer()),
      _sprites(assets(), loader(), &_renderPass, &_spriteRenderer),
      _texts(loader(), &_renderPass, &_spriteRenderer),

      _inputs(sys(), &log()),

      _camera(),

      _initialized(false),
      _running(false),
      _loop(sys()),
      _fpsTime(0),
      _fpsCount(0),
      _prevFrameTime(0),

      _quitInput    (nullptr),
      _restartInput (nullptr),
      _accelInput   (nullptr),
      _brakeInput   (nullptr),
      _climbInput   (nullptr),
      _diveInput    (nullptr),
      _stretchInput (nullptr),
      _shrinkInput  (nullptr),

      _map(this),

      _currentLevel (-1),

      _shipPartCount(6),
      _blockSize    (48),

      _hSpeedDamping(500),
      _acceleration (400),
      _minShipHSpeed(1000),
      _brakingFactor(0.99),

/* One full-charge tap up or down will instantly reach 1/8 of a block.
 * - This is about enough for a "human" tap at 60FPS to move one block away.
 * Tap power is restored over half a second.
 * Thrust is the base climb/dive power ; worth one tap every 1/6th second.
 */
      _thrustMaxCharge  (_blockSize / 8),
      _thrustRateCharge (2 * _thrustMaxCharge / FRAMERATE),
      _thrustPower      (_thrustMaxCharge / 10),

/* Damping is a factor (about .5~.9) to let vertical speed decrease over time.
 * - Below .5, one tap does not reach one block ; above .8 it may jump two.
 * Below min speed (0.2 block/s), the ship halts and snaps to grid.
 * The ship should never go above max speed (0.5 block/f) for safety reasons.
 * When left adrift, the ship will line in, covering 10% of the gap each frame.
 */
      _vSpeedDamping (0.8),
      _vSpeedFloor   (0.2 * _blockSize / FRAMERATE),
      _vSpeedCap     (_blockSize / 2),
      _vLockFactor   (0.1),

/* Collisions above scratch threshold will bump the player.
 * Collisions above crash threshold will kill the player.
 * When bumping against a wall, the player will be ejected within two frames.
 */
      _scratchThreshold (_blockSize / 4),
      _crashThreshold   (_blockSize / 2),
      _bumpawayTime     (2),

/* Parts move at a top rate of 1/4 block per frame.
 * Destroyed parts are bumped up then fall offscreen at 1/5 block per frame.
 * Parts are lost when going further away (by 1 block) than the farthest part.
 * The mass ratio reduces the drag feedback from the parts.
 */
      _partBaseSpeed (_blockSize / 4),
      _partDropSpeed (_blockSize / 3),
      _snapDistance  (_blockSize * (6+1)),
      _massRatio     (1.0/16)
{
	_entities.registerComponentManager(&_sprites);
	_entities.registerComponentManager(&_texts);
}


MainState::~MainState() {
}


void MainState::initialize() {
	// Set to true to debug OpenGL calls
	renderer()->context()->setLogCalls(false);

	_loop.reset();
	_loop.setTickDuration(  ONE_SEC / 60);
	_loop.setFrameDuration( ONE_SEC / 60);
	_loop.setMaxFrameDuration(_loop.frameDuration() * 3);
	_loop.setFrameMargin(     _loop.frameDuration() / 2);

	window()->onResize.connect(std::bind(&MainState::resizeEvent, this))
	        .track(_slotTracker);

	_quitInput    = _inputs.addInput("quit");
	_restartInput = _inputs.addInput("restart");
	_accelInput   = _inputs.addInput("accel");
	_brakeInput   = _inputs.addInput("brake");
	_climbInput   = _inputs.addInput("climb");
	_diveInput    = _inputs.addInput("dive");
	_stretchInput = _inputs.addInput("stretch");
	_shrinkInput  = _inputs.addInput("shrink");
	_skipInput    = _inputs.addInput("skip");;

	_inputs.mapScanCode(_quitInput,    SDL_SCANCODE_ESCAPE);
	_inputs.mapScanCode(_restartInput, SDL_SCANCODE_F5);
	_inputs.mapScanCode(_accelInput,   SDL_SCANCODE_RIGHT);
	_inputs.mapScanCode(_brakeInput,   SDL_SCANCODE_LEFT);
	_inputs.mapScanCode(_climbInput,   SDL_SCANCODE_UP);
	_inputs.mapScanCode(_diveInput,    SDL_SCANCODE_DOWN);
	_inputs.mapScanCode(_stretchInput, SDL_SCANCODE_X);
	_inputs.mapScanCode(_shrinkInput,  SDL_SCANCODE_Z);
	_inputs.mapScanCode(_skipInput,    SDL_SCANCODE_SPACE);

	parseJson(_animations, _game->dataPath() / "animations.json",
	          "animations.json", log());

	_beamsTex = loader()->loadAsset<ImageLoader>("beams.png");
	renderer()->createTexture(_beamsTex);

	_warningSound = loader()->loadAsset<SoundLoader>("warning.wav");
	_pointSound   = loader()->loadAsset<SoundLoader>("ping.wav");
	_crashSound   = loader()->loadAsset<SoundLoader>("crash.wav");

	_map.initialize();
	_map.setBgScroll(0, .4);
	_map.setBgScroll(1, .7);

	parseJson(_mapInfo, _game->dataPath() / "maps.json",
	          "maps.json", log());

	_shipShapes.push_back(Vector2(0,  1));
	_shipShapes.push_back(Vector2(1,  1));
	_shipShapes.push_back(Vector2(2,  1));
	_shipShapes.push_back(Vector2(0, -1));
	_shipShapes.push_back(Vector2(1, -1));
	_shipShapes.push_back(Vector2(2, -1));

	_shipShapes.push_back(Vector2(0,  2));
	_shipShapes.push_back(Vector2(0,  1));
	_shipShapes.push_back(Vector2(1,  1));
	_shipShapes.push_back(Vector2(0, -2));
	_shipShapes.push_back(Vector2(0, -1));
	_shipShapes.push_back(Vector2(1, -1));

	_shipShapes.push_back(Vector2(-1,  2));
	_shipShapes.push_back(Vector2( 0,  2));
	_shipShapes.push_back(Vector2( 1,  2));
	_shipShapes.push_back(Vector2(-1, -2));
	_shipShapes.push_back(Vector2( 0, -2));
	_shipShapes.push_back(Vector2( 1, -2));

	_shipShapes.push_back(Vector2(-1,  3));
	_shipShapes.push_back(Vector2( 0,  3));
	_shipShapes.push_back(Vector2( 1,  2));
	_shipShapes.push_back(Vector2(-1, -3));
	_shipShapes.push_back(Vector2( 0, -3));
	_shipShapes.push_back(Vector2( 1, -2));

	_shipShapes.push_back(Vector2(-2,  4));
	_shipShapes.push_back(Vector2(-1,  4));
	_shipShapes.push_back(Vector2( 1,  2));
	_shipShapes.push_back(Vector2(-2, -4));
	_shipShapes.push_back(Vector2(-1, -4));
	_shipShapes.push_back(Vector2( 1, -2));

	_shipShapes.push_back(Vector2(-2,  5));
	_shipShapes.push_back(Vector2(-1,  4));
	_shipShapes.push_back(Vector2( 1,  2));
	_shipShapes.push_back(Vector2(-2, -5));
	_shipShapes.push_back(Vector2(-1, -4));
	_shipShapes.push_back(Vector2( 1, -2));

	_shipShapes.push_back(Vector2(-3,  6));
	_shipShapes.push_back(Vector2(-1,  4));
	_shipShapes.push_back(Vector2( 1,  2));
	_shipShapes.push_back(Vector2(-3, -6));
	_shipShapes.push_back(Vector2(-1, -4));
	_shipShapes.push_back(Vector2( 1, -2));

	_gameLayer = _entities.createEntity(_entities.root(), "game_layer");
	_hudLayer  = _entities.createEntity(_entities.root(), "hud_layer");

//	dbgLogger.warning("initialize");
//	_ship = loadEntity("ship.json");
//	_ship.sprite()->setTileIndex(4);

//	EntityRef ship2 = _ship.clone(_ship, "ship2");

//	_shipParts.resize(_shipPartCount);
//	_partAlive.resize(_shipPartCount);
//	for (int i = 0 ; i < _shipPartCount ; ++i)
//	{
//		_shipParts[i] = _ship.clone(_ship, ("shipPart" + std::to_string(i)).c_str());
//		_shipParts[i].sprite()->setTileGridSize(Vector2i(3, 6));
//		_shipParts[i].sprite()->setTileIndex(i + ((i<3)? 9: 12));

//		EntityRef part2 = _shipParts[i].clone(_shipParts[i],
//		                                      (_shipParts[i].name() + std::string("2")).c_str());
//		part2.sprite()->setTileIndex(i + ((i<3)? 0: 3));
//		part2.place(Vector3(0, 0, 0));

//		_partAlive[i] = true;
//	}

	_charSprite = _entities.createEntity(_hudLayer, "char");
	_sprites.addComponent(_charSprite);
	_charSprite.sprite()->setTexture("hero.png"); // Dirty way to preload everything
	_charSprite.sprite()->setTexture("mecano.png");
	_charSprite.sprite()->setTexture("rival.png");
	_charSprite.sprite()->setAnchor(Vector2(0, 0));
	_charSprite.sprite()->setBlendingMode(BLEND_ALPHA);
	_charSprite.place(Vector3(-550, 0, 0.6));

	_dialogBg = _entities.createEntity(_hudLayer, "dialog_bg");
	_sprites.addComponent(_dialogBg);
	_dialogBg.sprite()->setTexture("dialog.png");
	_dialogBg.sprite()->setAnchor(Vector2(1, 0));
	_dialogBg.sprite()->setBlendingMode(BLEND_ALPHA);
	_dialogBg.place(Vector3(1920 - 96, -450, 0.7));

	_dialogText = loadEntity("text.json", _hudLayer);
	_dialogText.place(Vector3(0, 0, 0.8));
	_texts.get(_dialogText)->setSize(Vector2i(1230, 315));
	_texts.get(_dialogText)->setAnchor(Vector2(0, 1));

	EntityRef hudTop = _entities.createEntity(_hudLayer, "hud_top");
	_sprites.addComponent(hudTop);
	hudTop.sprite()->setTexture("hud_top.png");
	hudTop.sprite()->setAnchor(Vector2(0, 1));
	hudTop.sprite()->setBlendingMode(BLEND_ALPHA);
	hudTop.place(Vector3(0, 1080, 0.9));

	EntityRef hudBottom = _entities.createEntity(_hudLayer, "hud_bottom");
	_sprites.addComponent(hudBottom);
	hudBottom.sprite()->setTexture("hud_bottom.png");
	hudBottom.sprite()->setAnchor(Vector2(0, 0));
	hudBottom.sprite()->setBlendingMode(BLEND_ALPHA);
	hudBottom.place(Vector3(0, 0, 0.9));

	// ad-hoc value to compensate the fact that the baseline is wrong...
	float tvOff = 8;
	_scoreText = loadEntity("text.json", _hudLayer);
	_scoreText.place(Vector3(1120, 1070 - tvOff, 1));
	_texts.get(_scoreText)->setAnchor(Vector2(1, 1));

	_speedText = _scoreText.clone(_hudLayer, "speedText");
	_speedText.place(Vector3(230, 1070 - tvOff, 1));
	_texts.get(_speedText)->setAnchor(Vector2(1, 1));

	_distanceText = _scoreText.clone(_hudLayer, "distanceText");
	_distanceText.place(Vector3(230, -tvOff, 1));
	_texts.get(_distanceText)->setAnchor(Vector2(1, 0));

	_shipSound = loader()->loadAsset<SoundLoader>("engine0.wav");
	//loader()->load<MusicLoader>("music.ogg");

	loader()->waitAll();
	renderer()->uploadPendingTextures();

	Mix_Volume(-1, 64);


	_initialized = true;
}


void MainState::shutdown() {
	_slotTracker.disconnectAll();

	_initialized = false;
}


void MainState::run() {
	lairAssert(_initialized);

	log().log("Starting main state...");
	_running = true;
	_loop.start();
	_fpsTime  = sys()->getTimeNs();
	_fpsCount = 0;

	startGame(0);

	do {
		switch(_loop.nextEvent()) {
		case InterpLoop::Tick:
			updateTick();
			_entities.updateWorldTransform();
			break;
		case InterpLoop::Frame:
			updateFrame();
			break;
		}
	} while (_running);
	_loop.stop();
}


void MainState::quit() {
	Mix_UnregisterAllEffects(MIX_CHANNEL_POST);
	_running = false;
}


Game* MainState::game() {
	return static_cast<Game*>(_game);
}


unsigned MainState::shipShapeCount() const {
	return _shipShapes.size() / _shipPartCount;
}


Vector2 MainState::partExpectedPosition(unsigned shape, unsigned part) const {
	unsigned index = shape * _shipPartCount + part;
	assert(index < _shipShapes.size());
	return _shipShapes[index] * _blockSize;
}


float MainState::warningScrollDist() const {
	return _shipHSpeed;
}


void MainState::playAnimation(const std::string& name) {
	if(_animations.isMember(name) && _animations[name].isArray()) {
		_animCurrent = name;
		_animStep = -1;
		nextAnimationStep();
	}
	else {
		log().error("Unable to play animation \"", name,"\".");
	}
}


void MainState::updateAnimation(float time) {
	if(_anim) {
		float t = time + _animPos;
//		dbgLogger.error("updateAnimation: ", time, ", ", t);
		_anim->update(t);
		_animPos = t;
	}
	if(_animState == ANIM_PLAY && (!_anim || _animPos > _anim->length)) {
		nextAnimationStep();
	}
}


void MainState::nextAnimationStep() {
	float animLen = .4;
	float leftDialogPos = 1920 - 96;
	float dialogY = 96;

	const Json::Value& stepList = _animations[_animCurrent];
	lairAssert(stepList.isArray());

	++_animStep;
	_anim.reset();
	_animState = ANIM_NONE;
	_pause = false;
//	dbgLogger.error("nextAnimationStep:", _animCurrent, ":", _animStep);
	if(_animStep < stepList.size()) {
		try {
			const Json::Value& step = stepList[_animStep];
			if(!step.isArray()) {
				log().error("Animation ", _animCurrent, ":", _animStep, " is not an array.");
				return;
			}
			const std::string& cmd = step[0].asString();
			_animPos = 0;
			_animState = ANIM_PLAY;
			_pause = true;
//			dbgLogger.error("  play ", cmd);
			if(cmd == "show_char") {
				auto a = std::make_shared<CompoundAnim>();
				_charSprite.sprite()->setTexture(step[1].asString());
				a->addAnim(std::make_shared<MoveAnim>(
				               animLen, _charSprite,
				               _charSprite.transform().translation().head<3>(),
				               Vector3(0, 0, _charSprite.transform()(2, 3))));
				a->addAnim(std::make_shared<ColorAnim>(
				               animLen, _charSprite,
				               _charSprite.sprite()->color(),
				               Vector4(1, 1, 1, 1)));
				_dialogBg.sprite()->setAnchor(Vector2(1, 0));
				a->addAnim(std::make_shared<MoveAnim>(
				               animLen, _dialogBg,
				               _dialogBg.transform().translation().head<3>(),
				               Vector3(leftDialogPos, dialogY, _dialogBg.transform()(2, 3))));
				_anim = a;
			}
			if(cmd == "hide_char") {
				auto a = std::make_shared<CompoundAnim>();
				a->addAnim(std::make_shared<MoveAnim>(
				               animLen, _charSprite,
				               _charSprite.transform().translation().head<3>(),
				               Vector3(-550, 0, _charSprite.transform()(2, 3))));
				a->addAnim(std::make_shared<ColorAnim>(
				               animLen, _charSprite,
				               _charSprite.sprite()->color(),
				               Vector4(0, 0, 0, 1)));
				_anim = a;
			}
			if(cmd == "end_dialog") {
				auto a = std::make_shared<CompoundAnim>();
				a->addAnim(std::make_shared<MoveAnim>(
				               animLen, _charSprite,
				               _charSprite.transform().translation().head<3>(),
				               Vector3(-550, 0, _charSprite.transform()(2, 3))));
				a->addAnim(std::make_shared<ColorAnim>(
				               animLen, _charSprite,
				               _charSprite.sprite()->color(),
				               Vector4(0, 0, 0, 1)));
				_dialogBg.sprite()->setAnchor(Vector2(1, 0));
				a->addAnim(std::make_shared<MoveAnim>(
				               animLen, _dialogBg,
				               _dialogBg.transform().translation().head<3>(),
				               Vector3(leftDialogPos, -450, _dialogBg.transform()(2, 3))));
				_texts.get(_dialogText)->setText("");
				_anim = a;
			}
			if(cmd == "show_text") {
				auto a = std::make_shared<CompoundAnim>();
				_dialogText.place(Vector3(550, 475, _dialogText.transform()(2, 3)));
				_texts.get(_dialogText)->setText(step[1].asString());
				_animState = ANIM_WAIT;
			}
		}
		catch(Json::LogicError e) {
			log().error("Animation ", _animCurrent, ":", _animStep, ": ", e.what());
			return;
		}
	}
}


void MainState::endAnimation() {
//	dbgLogger.error("endAnimation");
	if(_animState == ANIM_WAIT) {
		if(_anim) {
			_anim->update(_anim->length);
		}
		nextAnimationStep();
	}
	else {
		while(_animState == ANIM_PLAY) {
			if(_anim) {
				_anim->update(_anim->length);
			}
			nextAnimationStep();
		}
	}
}


void MainState::startGame(int level) {
	if(_ship.isValid()) {
		_entities.destroyEntity(_ship);
	}

	_currentLevel = level % _mapInfo.size();

	_pause = false;

	_scrollPos     = 0;
	_prevScrollPos = _scrollPos;
	_levelFinished = false;

	const Json::Value& info = _mapInfo[_currentLevel];
	_map.setBg(0, info["bg1"].asString());
	_map.setBg(1, info["bg2"].asString());
	_map.setWarningColor(parseColor(info["warning_color"]));
	_map.setPointColor(parseColor(info["point_color"]));
	_levelColor  = parseColor(info["color"]);
	_levelColor2 = parseColor(info["alt_color"]);
	_beamColor   = parseColor(info["beam_color"]);
	_laserColor  = parseColor(info["laser_color"]);
	_textColor   = parseColor(info["text_color"]);

	const Json::Value& mapAnims = info["anims"];
	_mapAnims.clear();
	for(int i = 0; i < mapAnims.size(); ++i) {
		_mapAnims.push_back(std::make_pair(mapAnims[i][0].asInt(),
		                                   mapAnims[i][1].asString()));
	}
	_mapAnimIndex = 0;

	_deathTimer = -1;

	_shipSoundSample = 0;
	_lastPointSound  = -ONE_SEC;
	_warningTileX    = 0;
	_warningMap.assign(21, false);

	_ship = loadEntity("ship.json", _gameLayer);
//	dbgLogger.error(_ship.name());
	_ship.place(Vector3(4*_blockSize, 11*_blockSize, 0.4));
	_ship.sprite()->setColor(_levelColor2);
	_ship.sprite()->setTileIndex(4);
	_shipHSpeed = 2*_minShipHSpeed;
	_shipVSpeed = 0;
	_climbCharge = _thrustMaxCharge;
	_diveCharge  = _thrustMaxCharge;

//	EntityRef ship2 = _ship.firstChild();
//	while(ship2.nextSibling().isValid()) ship2 = ship2.nextSibling();
	EntityRef ship2 = _ship.clone(_ship, "ship_2");
//	dbgLogger.error(ship2.name());
	ship2.sprite()->setColor(_levelColor);
	ship2.sprite()->setTileIndex(1);
	ship2.place(Vector3(0, 0, .1));

	_shipShape = 0;
	_shipParts.resize(_shipPartCount);
	_partAlive.resize(_shipPartCount);
	for (int i = 0 ; i < _shipPartCount ; ++i)
	{
		_shipParts[i] = _ship.clone(_ship, ("shipPart_" + std::to_string(i)).c_str());
//		dbgLogger.error(_shipParts[i].name());
		_shipParts[i].sprite()->setTileGridSize(Vector2i(3, 6));
		_shipParts[i].sprite()->setTileIndex(i + ((i<3)? 9: 12));
		_shipParts[i].sprite()->setColor(_levelColor2);
		Vector2 pos = partExpectedPosition(_shipShape, i);
		_shipParts[i].place(Vector3(pos[0], pos[1], 0));

//		EntityRef part2 = _shipParts[i].firstChild();
		EntityRef part2 = _shipParts[i].clone(_shipParts[i], (_shipParts[i].name() + std::string("_2")).c_str());
		part2.sprite()->setColor(_levelColor);
		part2.sprite()->setTileIndex(i + ((i<3)? 0: 3));
		part2.place(Vector3(0, 0, 0.1));

		_partAlive[i] = true;
	}

	_distance = 0;
	_score    = 0;

	_texts.get(_scoreText)->setColor(_textColor);
	_texts.get(_speedText)->setColor(_textColor);
	_texts.get(_distanceText)->setColor(_textColor);
	_texts.get(_dialogText)->setColor(_textColor);

	loader()->waitAll();
	renderer()->uploadPendingTextures();

	// Need map images to be loaded.
	//_map.generate(0, 300, .5, 1);
	_map.clear();
	const Json::Value& segments = info["segments"];
	for(int i = 0; i < segments.size(); ++i) {
		Path path = segments[i].asString();
		if(!path.empty()) {
			_map.appendSection(path);
		}
	}

//	audio()->playSound(assets()->getAsset("sound.ogg"), 2);
//	Mix_RegisterEffect(MIX_CHANNEL_POST, shipSoundCb, NULL, this);

	_charSprite.place(Vector3(-550, 0, 0.6));
	_dialogBg.place(Vector3(SCREEN_WIDTH - 96, -450, 0.7));
	_dialogText.place(Vector3(0, 0, 0.8));
	_prevFrameTime = _loop.tickTime();

	_entities.updateWorldTransform();

	_animState = ANIM_NONE;
}


void MainState::updateTick() {
	_inputs.sync();

	if(_quitInput->justPressed()) {
		quit();
		return;
	}
	if(_restartInput->justPressed()) {
		startGame((_currentLevel + 1) % _mapInfo.size());
	}

	bool alive = _deathTimer < 0;
	if(!alive)
		_deathTimer += _loop.tickDuration();

	if(alive && _animState == ANIM_NONE && _mapAnimIndex < _mapAnims.size()
	&& int(_scrollPos) >= _mapAnims[_mapAnimIndex].first) {
		playAnimation(_mapAnims[_mapAnimIndex].second);
		++_mapAnimIndex;
	}

	bool levelFinished = _scrollPos >= _map.length() * _blockSize;
	bool levelSucceded = _score >= _mapInfo[_currentLevel].get("min_score", 0).asFloat();
	if(alive && _animState == ANIM_NONE && levelFinished && !_levelFinished) {
		std::string anim = _mapInfo[_currentLevel]
		        .get(levelSucceded? "end_anim": "fail_anim", "").asString();
		if(!anim.empty())
			playAnimation(anim);
	}
	_levelFinished = levelFinished;

	if(_skipInput->justPressed()) {
		endAnimation();
	}

	// Gameplay
	double time    = double(_loop.frameTime()) / double(ONE_SEC);
	double tickDur = double(_loop.tickDuration()) / double(ONE_SEC);

	if(_pause) {
		return;
	}

	if(alive && _levelFinished) {
		int next = _currentLevel + levelSucceded;
		if(next >= _mapInfo.size()) {
			game()->splashState()->setup(nullptr, "credits.png");
			game()->setNextState(game()->splashState());
			quit();
			return;
		}

		startGame(next);
		return;
	}

	if(_deathTimer > int64(ONE_SEC)) {
		startGame(_currentLevel);
		return;
	}

	// Shapeshift !
	if(alive && _stretchInput->justPressed()) { ++_shipShape; }
	if(alive && _shrinkInput->justPressed()) { --_shipShape; }
	_shipShape = std::max(0, std::min(int(shipShapeCount()) - 1, int(_shipShape)));

	// Horizontal control and physics.
	if (alive && _accelInput->isPressed()) {
		float damping = (1 + _shipHSpeed / _hSpeedDamping);
		_shipHSpeed += _acceleration / (damping * damping);
	}
	if (alive && _brakeInput->isPressed())
		_shipHSpeed = std::max(_shipHSpeed * _brakingFactor, _minShipHSpeed);

	_shipHSpeed = std::max(_shipHSpeed, 0.f);
	_scrollPos += _shipHSpeed * tickDur;
	_distance  += _shipHSpeed * tickDur;

	// Gathering parts
	float magDrag = 0;
	std::vector<Vector2> partSpeeds(_shipPartCount);
	for (unsigned i = 0 ; i < _shipPartCount ; ++i)
	{
		if (!_partAlive[i]) { continue; }

		Vector2 origin = partPosition(i),
		   destination = partExpectedPosition(_shipShape, i);
		Vector2 gap = destination - origin;

		if (gap[1] > _snapDistance)
			destroyPart(i);

		float dist = gap.norm();
		if (dist > _partBaseSpeed)
			gap *= _partBaseSpeed / dist;

		partSpeeds[i] = gap;
	}

	// Vertical speed control and physics.
	float& vspeed = _shipVSpeed;

	// Recharging thrusters.
	_climbCharge = std::min(_climbCharge + _thrustRateCharge, _thrustMaxCharge);
	_diveCharge  = std::min(_diveCharge  + _thrustRateCharge, _thrustMaxCharge);

	// Activating thrusters.
	if (alive && _climbInput->justPressed()) {
		vspeed += _climbCharge;
		_climbCharge = 0;
	}
	if (alive && _diveInput->justPressed()) {
		vspeed -= _diveCharge;
		_diveCharge = 0;
	}
	if (alive && _climbInput->isPressed()) { vspeed += _thrustPower; }
	if (alive && _diveInput->isPressed())  { vspeed -= _thrustPower; }

	// Automatic vertical slowdown.
	if ( alive && !(_climbInput->isPressed() || _diveInput->isPressed()) )
		vspeed *= _vSpeedDamping;

	if(alive) {
		// Bouncing (or crashing) on walls.
		float bump = collide(_shipPartCount);
		if (bump == INFINITY) {
			_deathTimer = 0;
			audio()->playSound(_crashSound, 0, CHANN_CRASH);
			dbgLogger.error("u ded. 'sploded hed");
		}
		else if (bump != 0)
			vspeed = bump;

		for (unsigned i = 0 ; i < _shipPartCount ; ++i)
		{
			if (!_partAlive[i]) { continue; }

			bump = collide(i);
			if (bump == INFINITY)
				destroyPart(i);
			else if (bump != 0)
				partSpeeds[i] = Vector2(partSpeeds[i][0],bump);
		}

		// Looting
		collect (_shipPartCount);
		for (unsigned i = 0 ; i < _shipPartCount ; ++i)
			if (_partAlive[i])
				collect (i);

		// Shifting parts.
		for (unsigned i = 0 ; i < _shipPartCount ; i++)
			if (_partAlive[i])
			{
				partPosition(i) += partSpeeds[i];
				magDrag += partSpeeds[i][1];
			}
	}

	// In Soviet Russia, parts gather you !
	vspeed += -magDrag * _massRatio;

	// Clamping vertical speed.
	if (std::abs(vspeed) > _vSpeedCap)
		vspeed = std::min(std::max(vspeed, -_vSpeedCap), _vSpeedCap);


	// Shifting ship.
	if (alive)
		shipPosition()[1] += vspeed;
	else
		shipPosition()[1] -= _partDropSpeed;

	if(alive) {
		// Halting ship and snapping to grid .
		if (std::abs(vspeed) < _vSpeedFloor)
			shipPosition()[1] -= _vLockFactor *
			  (std::fmod(shipPosition()[1] + _blockSize/2,_blockSize) - _blockSize/2);
	}

	// Warning sound
	int warningTileX = (_scrollPos + SCREEN_WIDTH + warningScrollDist()) / _blockSize;
	int warningTileBegin = _map.beginIndex(std::max(0, _warningTileX-1));
	int warningTileEnd   = _map.beginIndex(warningTileX);
	for(int y = 1; y < 21; ++y) {
		bool hasWall = _map.hasWallAtYInRange(y, warningTileBegin, warningTileEnd);
		if(hasWall && !_warningMap[y]) {
			audio()->playSound(_warningSound, 0, CHANN_WARNING);
		}
		_warningMap[y] = hasWall;
	}
	_warningTileX = std::max(warningTileX, _warningTileX);

	_prevScrollPos = _scrollPos;
}


Box2 MainState::partBox (unsigned part)
{
	Vector2 partCorner = shipPosition(),
	        partSize   = Vector2(_blockSize,_blockSize); // Buh.
	if (part < _shipPartCount)
	{
		partCorner += partPosition(part);
		partSize += Vector2(2 * _blockSize,0); // Ick !
	}

	partCorner += Vector2(_scrollPos,0);

	return Box2(partCorner, partCorner + partSize);
}


// Check a part for collision, and return the strength of the vertical bump.
// If the bump is INFINITY, the part has crashed.
// If part == _shipPartCount, check the ship itself.
float MainState::collide (unsigned part)
{
	float dvspeed = 0;

	Box2 pBox = partBox(part);
	float dScroll = _scrollPos - _prevScrollPos;

	int firstBlock = _map.beginIndex((pBox.min()[0] - dScroll) / _blockSize),
	     lastBlock = _map.beginIndex(pBox.min()[0] / _blockSize + 2);

	for (int bi = firstBlock ; bi < lastBlock ; bi++)
	{
		Box2 hit = _map.hit(pBox, bi, dScroll);
		float amount = hit.sizes()[1];

		if (hit.isEmpty())
			continue;

		if (amount > _crashThreshold)
			dvspeed = INFINITY;
		else if (amount > _scratchThreshold)
		{
			if (hit.min()[1] > pBox.min()[1])
				dvspeed = -amount / _bumpawayTime;
			else
				dvspeed = amount / _bumpawayTime;
		}
	}

	return dvspeed;
}


void MainState::collect (unsigned part)
{
	Box2 pBox = partBox(part);
	float dScroll = _scrollPos - _prevScrollPos;

	int firstBlock = _map.beginIndex((pBox.min()[0] - dScroll) / _blockSize),
	     lastBlock = _map.beginIndex(pBox.min()[0] / _blockSize + 2);

	unsigned prevScore = _score;
	for (int bi = firstBlock ; bi < lastBlock ; bi++)
	{
		if (_map.pickup(pBox, bi, dScroll).sizes()[1] > _crashThreshold)
		{
			_map.clearBlock(bi);
			_score += (_shipHSpeed / 1000) - 1;

			if(_lastPointSound + ONE_SEC / 15 < int64(_loop.tickTime())) {
				audio()->playSound(_pointSound, 0, CHANN_POINT);
				_lastPointSound = _loop.tickTime();
			}
		}
	}
}


void MainState::destroyPart (unsigned part)
{
	assert (part < _shipPartCount);
	assert (_partAlive[part]);

	_partAlive[part] = false;
	partPosition(part)[1] += _blockSize;

	audio()->playSound(_crashSound, 0, CHANN_CRASH);
}


void MainState::updateFrame() {
//	double time = double(_loop.frameTime()) / double(ONE_SEC);
	double etime = double(_loop.frameTime() - _prevFrameTime) / double(ONE_SEC);

	char buff[BUFSIZE];

	snprintf(buff, BUFSIZE, "%.0f m/s", _shipHSpeed);
	_texts.get(_speedText)->setText(buff);

	snprintf(buff, BUFSIZE, "%.2f km", _distance/1000);
	_texts.get(_distanceText)->setText(buff);

	snprintf(buff, BUFSIZE, "%.0f", _score*1000.0);
	_texts.get(_scoreText)->setText(buff);

	// Killin' parts !
	for (unsigned i = 0 ; i < _shipPartCount ; ++i)
		if (!_partAlive[i] && partPosition(i)[1] > -SCREEN_HEIGHT)
			partPosition(i)[1] -= _partDropSpeed;

	updateAnimation(etime);

	renderer()->uploadPendingTextures();

	// Rendering
	Context* glc = renderer()->context();

	glc->clearColor(_levelColor(0), _levelColor(1), _levelColor(2), _levelColor(3));
	glc->clear(gl::COLOR_BUFFER_BIT | gl::DEPTH_BUFFER_BIT);

	_renderPass.clear();
	_spriteRenderer.clear();

	float scroll = lerp(_loop.frameInterp(), _prevScrollPos, _scrollPos);
	float screenWidth = float(window()->width() * SCREEN_HEIGHT)
	                  / window()->height();
	_map.render(scroll, warningScrollDist(), screenWidth, 70, _camera);
	renderBeams(_loop.frameInterp(), _camera);
	_sprites.render(_loop.frameInterp(), _camera);
	_texts.render(_loop.frameInterp(), _camera);

	_renderPass.render();

	window()->swapBuffers();
	glc->setLogCalls(false);

	uint64 now = sys()->getTimeNs();
	++_fpsCount;
	if(_fpsCount == FRAMERATE) {
		log().info("Fps: ", _fpsCount * float(ONE_SEC) / (now - _fpsTime));
		_fpsTime  = now;
		_fpsCount = 0;
	}

	_prevFrameTime = _loop.frameTime();
}


void MainState::renderBeam(const Matrix4& trans, TextureSP tex, const Vector2& p0,
                           const Vector2& p1, const Vector4& color,
                           float texOffset, unsigned row, unsigned rowCount) {
	float width = tex->height() / rowCount;
	Vector2 n = p1 - p0;
	float dist = n.norm();
	n = n / dist * width / 2.f;
	n = Vector2(-n(1), n(0));

	Box2 texCoord(Vector2(texOffset, float(row) / float(rowCount)),
	              Vector2(dist / tex->width() + texOffset,
	                      float(row + 1) / float(rowCount)));

	unsigned index = _spriteRenderer.vertexCount();
	Vector4 p;
	p << p0 - n, 0, 1;
	p = trans * p;
	_spriteRenderer.addVertex(p, color, texCoord.corner(Box2::TopLeft));
	p << p1 - n, 0, 1;
	p = trans * p;
	_spriteRenderer.addVertex(p, color, texCoord.corner(Box2::TopRight));
	p << p0 + n, 0, 1;
	p = trans * p;
	_spriteRenderer.addVertex(p, color, texCoord.corner(Box2::BottomLeft));
	p << p1 + n, 0, 1;
	p = trans * p;
	_spriteRenderer.addVertex(p, color, texCoord.corner(Box2::BottomRight));

	_spriteRenderer.addIndex(index + 0);
	_spriteRenderer.addIndex(index + 1);
	_spriteRenderer.addIndex(index + 2);
	_spriteRenderer.addIndex(index + 2);
	_spriteRenderer.addIndex(index + 1);
	_spriteRenderer.addIndex(index + 3);
}


void MainState::renderBeams(float interp, const OrthographicCamera& camera) {
	RenderPass::DrawStates states;
	states.shader = _spriteRenderer.shader().shader;
	states.buffer = _spriteRenderer.buffer();
	states.format = _spriteRenderer.format();
	states.textureFlags = Texture::TRILINEAR | Texture::REPEAT;
	states.blendingMode = BLEND_ALPHA;

	const ShaderParameter* params = _spriteRenderer.addShaderParameters(
	            _spriteRenderer.shader(), camera.transform(), 0);

	TextureAspectSP texAspect = _beamsTex->aspect<TextureAspect>();
	TextureSP tex = texAspect->get();
	states.texture = tex;

	Matrix4 wt = lerp(interp,
	                  _ship._get()->prevWorldTransform.matrix(),
	                  _ship._get()->worldTransform.matrix());
	wt(2, 3) = .35;
	Vector4 mid4(_blockSize/2.f, _blockSize/2.f, 0, 1);
	Vector2 mid = mid4.head<2>();
	Vector2 laserOffset(SCREEN_WIDTH, 0);

	unsigned vxIndex = _spriteRenderer.indexCount();
	renderBeam(wt, tex, mid, mid + laserOffset, _laserColor, 0, 0, 2);
	Vector2 shipPos[3];
	for(int i = 0; i < 3; ++i) {
		shipPos[i] = (wt * (mid4 + Vector4(_blockSize * i, 0, 0, 0))).head<2>();
	}
	for(int i = 0; i < _shipPartCount; ++i) {
		if (!_partAlive[i]) { continue; }

		Matrix4 wt = lerp(interp,
		                  _shipParts[i]._get()->prevWorldTransform.matrix(),
		                  _shipParts[i]._get()->worldTransform.matrix());
		wt(2, 3) = .35;
		renderBeam(wt, tex, mid, mid + laserOffset, _laserColor, 0, 0, 2);
	}
	unsigned vxCount = _spriteRenderer.indexCount() - vxIndex;
	_renderPass.addDrawCall(states, params, 1.f - wt(2, 3), vxIndex, vxCount);

	vxIndex = _spriteRenderer.indexCount();
	for(int i = 0; i < _shipPartCount; ++i) {
		if (!_partAlive[i]) { continue; }

		Matrix4 wt = lerp(interp,
		                  _shipParts[i]._get()->prevWorldTransform.matrix(),
		                  _shipParts[i]._get()->worldTransform.matrix());
		wt(2, 3) = .35;

		Vector4 pp(.25 * _blockSize, ((i < 3)? .25: .75) * _blockSize, 0, 1);
		Vector2 partPos = (wt * pp).head<2>();
		float advance = float(_loop.frameTime()) / float(ONE_SEC) + i * .1;
		renderBeam(Matrix4::Identity(), tex, shipPos[i%3], partPos, _beamColor,
		           advance, 1, 2);
	}
	vxCount = _spriteRenderer.indexCount() - vxIndex;
	_renderPass.addDrawCall(states, params, 1.f - wt(2, 3), vxIndex, vxCount);
}


void MainState::resizeEvent() {
	Box3 viewBox(Vector3::Zero(),
	             Vector3(SCREEN_HEIGHT * window()->width() / window()->height(),
	                     SCREEN_HEIGHT,
	                     1));
	_camera.setViewBox(viewBox);
	renderer()->context()->viewport(0, 0, window()->width(), window()->height());
}


EntityRef MainState::loadEntity(const Path& path, EntityRef parent, const Path& cd) {
	Path localPath = make_absolute(cd, path);
	log().info("Load entity \"", localPath, "\"");

	Json::Value json;
	Path realPath = game()->dataPath() / localPath;
	if(!parseJson(json, realPath, localPath, log())) {
		return EntityRef();
	}

	if(!parent.isValid()) {
		parent = _gameLayer;
	}

	return _entities.createEntityFromJson(parent, json, localPath.dir());
}


void shipSoundCb(int /*chan*/, void *stream, int len, void *udata) {
	MainState* state = static_cast<MainState*>(udata);

	AssetSP asset = state->_shipSound.lock();
	if(!asset) return;
	SoundAspectSP aspect = asset->aspect<SoundAspect>();
	if(!aspect) return;
	const SoundSP snd = aspect->get();
	if(!snd) return;
	const Mix_Chunk* chunk = snd->chunk();

	float speed = 1 - std::exp(-state->_shipHSpeed / 1000);
	int max = (chunk->alen - len) / 2;
	int sample = std::max(0, std::min(int(speed * max), max));
	int16* dst = reinterpret_cast<int16*>(stream);
	int16* src1 = reinterpret_cast<int16*>(chunk->abuf) + state->_shipSoundSample;
	int16* src2 = reinterpret_cast<int16*>(chunk->abuf) + sample;
	dbgLogger.error("sample: ", sample, ", ", state->_shipSoundSample, ", ",
	                speed, ", ", max);
	for(int i = 0; i < len; ++i) {
		dst[i] = std::max(std::numeric_limits<int16>::min(),
		                  std::min(std::numeric_limits<int16>::max(),
		                           lerp(float(i) / float(len), src1[i], src2[i])));
	}
	state->_shipSoundSample = sample;
}


Vec2 MainState::shipPosition()
{
	return _ship.transform().translation().head<2>();
}


Vec2 MainState::partPosition(unsigned part)
{
	assert (part < _shipPartCount);
	return _shipParts[part].transform().translation().head<2>();
}
