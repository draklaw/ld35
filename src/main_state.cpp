/*
 *  Copyright (C) 2015, 2016 Simon Boyé
 *
 *  This file is part of lair.
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

#include "main_state.h"


#define ONE_SEC (1000000000)


#define BLOCK_SIZE 48

#define VSPEED_THRUST (BLOCK_SIZE / 60.0)
#define VSPEED_FALLOFF 0.8
#define VSPEED_MIN (10.0 / 60.0)
#define VSPEED_MAX (BLOCK_SIZE * 10.0 / 60.0) // 10 blocks/s


MainState::MainState(Game* game)
	: GameState(game),

      _entities(log()),
      _spriteRenderer(renderer()),
      _sprites(assets(), loader(), &_spriteRenderer),
      _texts(loader(), &_spriteRenderer),

      _inputs(sys(), &log()),

      _camera(),

      _initialized(false),
      _running(false),
      _loop(sys()),
      _fpsTime(0),
      _fpsCount(0),

      _quitInput      (nullptr),
      _accelerateInput(nullptr),
      _slowDownInput  (nullptr),

      _blockSize   (48),
      _speedFactor (.001),
      _acceleration(100),
      _slowDown    (100),
      _speedDamping(250) {

	_entities.registerComponentManager(&_sprites);
	_entities.registerComponentManager(&_texts);
}


MainState::~MainState() {
}


void MainState::initialize() {
	// Set to true to debug OpenGL calls
	renderer()->context()->setLogCalls(false);

	_loop.reset();
	_loop.setTickDuration(    ONE_SEC /  60);
	_loop.setFrameDuration(   ONE_SEC /  60);
	_loop.setMaxFrameDuration(_loop.frameDuration() * 3);
	_loop.setFrameMargin(     _loop.frameDuration() / 2);

	window()->onResize.connect(std::bind(&MainState::resizeEvent, this))
	        .track(_slotTracker);

	_quitInput       = _inputs.addInput("quit");
	_accelerateInput = _inputs.addInput("accel");
	_slowDownInput   = _inputs.addInput("brake");
	_thrustUpInput   = _inputs.addInput("up");
	_thrustDownInput = _inputs.addInput("down");

	_inputs.mapScanCode(_quitInput,       SDL_SCANCODE_ESCAPE);
	_inputs.mapScanCode(_accelerateInput, SDL_SCANCODE_RIGHT);
	_inputs.mapScanCode(_slowDownInput,   SDL_SCANCODE_LEFT);
	_inputs.mapScanCode(_thrustUpInput,   SDL_SCANCODE_UP);
	_inputs.mapScanCode(_thrustDownInput, SDL_SCANCODE_DOWN);

	// TODO: load stuff.
	_root = _entities.createEntity(_entities.root(), "root");
	_bg = loadEntity("bg.json");
	_ship = loadEntity("ship.json");

//	EntityRef text = loadEntity("text.json", _entities.root());
//	text.place(Vector3(160, 90, .5));

	loader()->load<SoundLoader>("sound.ogg");
	//loader()->load<MusicLoader>("music.ogg");

	loader()->waitAll();
	renderer()->uploadPendingTextures();

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

	startGame();

	do {
		switch(_loop.nextEvent()) {
		case InterpLoop::Tick:
			updateTick();
			break;
		case InterpLoop::Frame:
			updateFrame();
			break;
		}
	} while (_running);
	_loop.stop();
}


void MainState::quit() {
	_running = false;
}


Game* MainState::game() {
	return static_cast<Game*>(_game);
}


void MainState::startGame() {
	float bgScale = 1080.f / float(_bg.sprite()->texture()->get()->height());
	_bg.place(Transform(Eigen::Scaling(bgScale, bgScale, 1.f)));
	_scrollPos     = 0;
	_prevScrollPos = _scrollPos;

	_ship.place(Vector3(4*BLOCK_SIZE, 21.5, 0));
	_shipSpeed = 0;

	//audio()->playMusic(assets()->getAsset("music.ogg"));
	audio()->playSound(assets()->getAsset("sound.ogg"), 2);
}

void MainState::updateTick() {
	_inputs.sync();

	if(_quitInput->justPressed()) {
		quit();
	}

	// Gameplay.
	double time    = double(_loop.frameTime()) / double(ONE_SEC);
	double tickDur = double(_loop.tickDuration()) / double(ONE_SEC);

	Vector3& speed = shipSpeed();
	Vec3 pos = shipPosition();

	// Control.
	if(_thrustUpInput->justPressed()) {
		pos[1] += BLOCK_SIZE;
// 		speed += VSPEED_THRUST;
	}
	if(_thrustDownInput->justPressed()) {
		pos[1] -= BLOCK_SIZE;
// 		speed -= VSPEED_THRUST;
	}
	if(_accelerateInput->isPressed()) {
		float damping = (1 + _shipSpeed / _speedDamping);
		_shipSpeed += _acceleration / (damping * damping);
	}
	if(_slowDownInput->isPressed()) {
		_shipSpeed -= _slowDown;
	}

	// Horizontal physics
	_shipSpeed = std::max(_shipSpeed, 0.f);
	dbgLogger.log(_shipSpeed);
	_scrollPos += _shipSpeed * _speedFactor * tickDur;

	// Vertical physics
// 	pos += speed;
// 	speed *= VSPEED_FALLOFF;
// 	if (speed[1] < VSPEED_MIN)
// 		speed[1] = 0;

	_prevScrollPos = _scrollPos;
	_entities.updateWorldTransform();
}


void MainState::updateFrame() {
//	double time = double(_loop.frameTime()) / double(ONE_SEC);
	_bg.sprite()->setView(Box2(Vector2(_scrollPos, 0), Vector2(_scrollPos + 1, 1)));

	// Rendering
	Context* glc = renderer()->context();

	glc->clear(gl::COLOR_BUFFER_BIT | gl::DEPTH_BUFFER_BIT);

	_spriteRenderer.beginFrame();

	_sprites.render(_loop.frameInterp(), _camera);
	_texts.render(_loop.frameInterp());

	_spriteRenderer.endFrame(_camera.transform());

	window()->swapBuffers();
	glc->setLogCalls(false);

	uint64 now = sys()->getTimeNs();
	++_fpsCount;
	if(_fpsCount == 60) {
		log().info("Fps: ", _fpsCount * float(ONE_SEC) / (now - _fpsTime));
		_fpsTime  = now;
		_fpsCount = 0;
	}
}


void MainState::resizeEvent() {
	Box3 viewBox(Vector3::Zero(),
	             Vector3(window()->width(), window()->height(), 1));
	_camera.setViewBox(viewBox);
	renderer()->context()->viewport(0, 0, window()->width(), window()->height());

	float scale =  float(window()->height()) / 1080.;
	_root.place(Transform(Eigen::Scaling(scale, scale, 1.f)));
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
		parent = _root;
	}

	return _entities.createEntityFromJson(parent, json, localPath.dir());
}


Vec3 MainState::shipPosition()
{
	return _ship.transform().translation();
}


Vector3& MainState::shipSpeed()
{
	return _currentSpeed;
}
