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

#include "main_state.h"


#define ONE_SEC (1000000000)


#define FRAMERATE 60

#define BLOCK_SIZE 48

/*
 * One full tap instantly reaches 1/8 of a block.
 * Tap power is restored over half a second.
 *
 * Falloff is a factor (about .5~.9) to let vertical speed decrease over time.
 * - Below .5, one tap does not reach one block ; above .8 it may jump two.
 * Thrust is the base climb/dive power ; worth one tap every 1/6th second.
 * Below min speed (0.5 block/s), the ship halts and snaps to grid.
 * The ship should never go above max speed (0.5 block/f) for safety reasons.
 *
 * Collisions above scratch threshold will bump the player.
 * Collisions above crash threshold will kill the player.
 * When bumping against a wall, the player will be ejected within two frames.
 *
 * Parts are lost when going further away (by >1 block) than the farthest part.
 */

#define POWER_MAX     ( BLOCK_SIZE / 8.0f )
#define POWER_BUILDUP ( POWER_MAX * 2.0f / FRAMERATE )

#define VSPEED_FALLOFF ( 0.8f )
#define VSPEED_THRUST  ( POWER_MAX / 10.0f )
#define VSPEED_MIN     ( BLOCK_SIZE *  0.5f / FRAMERATE )
#define VSPEED_MAX     ( BLOCK_SIZE / 2.0f )

#define SCRATCH_THRESHOLD ( BLOCK_SIZE / 4.0f )
#define CRASH_THRESHOLD   ( BLOCK_SIZE / 2.0f )
#define BUMP_TIME ( 2.0f )

#define SNAP_DISTANCE ( BLOCK_SIZE * 7.0f )

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
      _thrustUpInput  (nullptr),
      _thrustDownInput(nullptr),
      _nextShapeInput (nullptr),
      _prevShapeInput (nullptr),

      _map(this),

      _blockSize    (BLOCK_SIZE),
      _speedFactor  (1),
      _acceleration (100),
      _speedDamping (250),
      _slowDown     (100),
      _shipPartCount(6),
      _partSpeed    (12) {

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
	_thrustUpInput   = _inputs.addInput("climb");
	_thrustDownInput = _inputs.addInput("dive");
	_nextShapeInput  = _inputs.addInput("next_shape");
	_prevShapeInput  = _inputs.addInput("prev_shape");

	_inputs.mapScanCode(_quitInput,       SDL_SCANCODE_ESCAPE);
	_inputs.mapScanCode(_accelerateInput, SDL_SCANCODE_RIGHT);
	_inputs.mapScanCode(_slowDownInput,   SDL_SCANCODE_LEFT);
	_inputs.mapScanCode(_thrustUpInput,   SDL_SCANCODE_UP);
	_inputs.mapScanCode(_thrustDownInput, SDL_SCANCODE_DOWN);
	_inputs.mapScanCode(_nextShapeInput,  SDL_SCANCODE_X);
	_inputs.mapScanCode(_prevShapeInput,  SDL_SCANCODE_Z);

	_map.initialize();

	_root = _entities.createEntity(_entities.root(), "root");
	_ship = loadEntity("ship.json");

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


unsigned MainState::shipShapeCount() const {
	return _shipShapes.size() / _shipPartCount;
}


Vector2 MainState::partPos(unsigned shape, unsigned part) const {
	unsigned index = shape * _shipPartCount + part;
	assert(index < _shipShapes.size());
	return _shipShapes[index] * _blockSize;
}


void MainState::startGame() {
	_scrollPos     = 0;
	_prevScrollPos = _scrollPos;

	//FIXME
	_ship.place(Vector3(4*BLOCK_SIZE, 5*BLOCK_SIZE, 0));
	_shipHSpeed = 0;
	_shipVSpeed = 0;
	_climbPower = POWER_MAX;
	_divePower  = POWER_MAX;

	_shipShape = 0;
	_shipParts.resize(_shipPartCount);
	_partAlive.resize(_shipPartCount);
	_partSpeeds.resize(_shipPartCount);
	for(int i = 0; i < _shipPartCount; ++i)
	{
		_shipParts[i] = _ship.clone(_ship, "shipPart");
		_shipParts[i].sprite()->setTileGridSize(Vector2i(3, 3));
		_shipParts[i].sprite()->setTileIndex(i + ((i<3)? 0: 3));
		Vector2 pos = partPos(_shipShape, i);
		_shipParts[i].place(Vector3(pos[0],pos[1],0));

		_partAlive[i] = true;
		_partSpeeds[i] = Vector2(0,0);
	}

	_scoreText = loadEntity("text.json", _root);
	_scoreText.place(Vector3(800, 1000, .5));
	_texts.get(_scoreText)->setText("Score: 000000000");

	_speedText = _scoreText.clone(_root);
	_speedText.place(Vector3(48, 1000, .5));
	_texts.get(_speedText)->setText("00000 km/h");

	_distanceText = _scoreText.clone(_root);
	_distanceText.place(Vector3(48, 32, .5));
	_texts.get(_distanceText)->setText("00000 km");

	loader()->waitAll();
	renderer()->uploadPendingTextures();

	// Need map images to be loaded.
	_map.generate(0, 300, .5, 1);

	audio()->playSound(assets()->getAsset("sound.ogg"), 2);
}


void MainState::updateTick() {
	_inputs.sync();

	if(_quitInput->justPressed()) {
		quit();
	}

	// Gameplay
	double time    = double(_loop.frameTime()) / double(ONE_SEC);
	double tickDur = double(_loop.tickDuration()) / double(ONE_SEC);

	// Shapeshift !
	if(_nextShapeInput->justPressed()) { ++_shipShape; }
	if(_prevShapeInput->justPressed()) { --_shipShape; }
	_shipShape = std::max(0, std::min(int(shipShapeCount()) - 1, int(_shipShape)));

	// Gathering parts
	for (unsigned i = 0 ; i < _shipPartCount ; ++i)
	{
		Vector2 origin = partPosition(i),
		   destination = partPos(_shipShape, i);
		Vector2 gap = destination - origin;

		if (gap[1] > SNAP_DISTANCE)
		{
			dbgLogger.warning("Oh, snap !");
			destroyPart(i);
		}

		float dist = gap.norm();
		if (dist > _partSpeed)
			gap *= _partSpeed / dist;

		_partSpeeds[i] = gap;
	}

	// Horizontal control and physics.
	if (_accelerateInput->isPressed()) {
		float damping = (1 + _shipHSpeed / _speedDamping);
		_shipHSpeed += _acceleration / (damping * damping);
	}
	if (_slowDownInput->isPressed()) {
		_shipHSpeed -= _slowDown;
	}

	_shipHSpeed = std::max(_shipHSpeed, 0.f);
	_scrollPos += _shipHSpeed * _speedFactor * tickDur;

	// Vertical speed control and physics.
	float& vspeed = _shipVSpeed;

	// Recharging thrusters.
	_climbPower = std::min(_climbPower + POWER_BUILDUP, POWER_MAX);
	_divePower  = std::min(_divePower  + POWER_BUILDUP, POWER_MAX);

	// Activating thrusters.
	if (_thrustUpInput->justPressed()) {
		vspeed += _climbPower;
		_climbPower = 0.f;
	}
	if (_thrustDownInput->justPressed()) {
		vspeed -= _divePower;
		_divePower = 0.f;
	}
	if (_thrustUpInput->isPressed())   { vspeed += VSPEED_THRUST; }
	if (_thrustDownInput->isPressed()) { vspeed -= VSPEED_THRUST; }

	// Automatic vertical slowdown.
	if ( !(_thrustUpInput->isPressed() || _thrustDownInput->isPressed()) )
		vspeed *= VSPEED_FALLOFF;

	// Clamping/locking vertical speed.
	if (std::abs(vspeed) > VSPEED_MAX)
		vspeed = std::min(std::max(vspeed, -VSPEED_MAX), VSPEED_MAX);

	if (std::abs(vspeed) < VSPEED_MIN)
		vspeed = 0.f;

	// Bouncing (or crashing) on walls.
	float bump = collide(_shipPartCount);
	if (bump == INFINITY)
		dbgLogger.error("u ded. 'sploded hed");
	else if (bump != 0.f)
		vspeed = bump;

	for (unsigned i = 0 ; i < _shipPartCount ; ++i)
	{
		bump = collide(i);
		if (bump == INFINITY)
			destroyPart(i);
		else if (bump != 0.f)
			_partSpeeds[i] = Vector2(_partSpeeds[i][0],bump);
	}

	// Looting
	collect (_shipPartCount);
	for (unsigned i = 0 ; i < _shipPartCount ; ++i)
		collect (i);

	// Snapping to grid.
	if (!vspeed)
	{
		float delta = std::fmod(shipPosition()[1],_blockSize);
		if (delta > 1)
			vspeed = delta / FRAMERATE * (delta > _blockSize/2.0?1:-1);
	}

	shipPosition()[1] += vspeed;
	for (unsigned i = 0 ; i < _shipPartCount ; i++)
		partPosition(i) += _partSpeeds[i];

	_prevScrollPos = _scrollPos;
	_entities.updateWorldTransform();
}


// Check a part for collision, and return the strength of the vertical bump.
// If the bump is INFINITY, the part has crashed.
// If part == _shipPartCount, check the ship itself.
float MainState::collide (unsigned part)
{
	float dvspeed = 0;

	Vector2 partOffset = Vector2(0,0);
	if (part < _shipPartCount)
		partOffset = partPosition(part);

	Vector2 partCorner = shipPosition() + partOffset;
	Box2 partBox = Box2(partCorner, partCorner + Vector2(_blockSize,_blockSize));
	float partX = partCorner[0], partY = partCorner[1];

	unsigned firstBlock = _map.beginIndex((_prevScrollPos + partX) / _blockSize),
	          lastBlock = _map.beginIndex((_scrollPos + partX) / _blockSize + 2);
	float dScroll = _scrollPos - _prevScrollPos;

	for (int bi = firstBlock ; bi < lastBlock ; bi++)
	{
		Box2 hit = _map.hit(partBox, bi, dScroll);
		float amount = hit.sizes()[1];

		if (amount > CRASH_THRESHOLD)
			dvspeed = INFINITY;
		else if (amount > SCRATCH_THRESHOLD)
		{
			if (hit.min()[1] > partY)
				dvspeed = -amount / BUMP_TIME;
			else
				dvspeed = amount / BUMP_TIME;
		}
	}

	return dvspeed;
}


void MainState::collect (unsigned part)
{
	//TODO
}


void MainState::destroyPart (unsigned part)
{
	//TODO
	assert (part < _shipPartCount);
	//assert (_partAlive[i]);
	dbgLogger.warning ("Kabooom ! ", part);
}


void MainState::updateFrame() {
//	double time = double(_loop.frameTime()) / double(ONE_SEC);
	char buff[128];
	snprintf(buff, 128, "%5.f km/h", _shipHSpeed);
	_texts.get(_speedText)->setText(buff);

	// Rendering
	Context* glc = renderer()->context();

	glc->clear(gl::COLOR_BUFFER_BIT | gl::DEPTH_BUFFER_BIT);

	_spriteRenderer.beginFrame();

	_map.render(lerp(_loop.frameInterp(), _prevScrollPos, _scrollPos));
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


Vec2 MainState::shipPosition()
{
	return _ship.transform().translation().head<2>();
}


Vec2 MainState::partPosition(unsigned part)
{
	assert (part < _shipPartCount);
	return _shipParts[part].transform().translation().head<2>();
}
