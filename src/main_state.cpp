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
 * Below min speed (0.5 bloc/s), the ship halts and snaps to grid.
 * The ship should never go above max speed (0.5 block/f) for safety reasons.
 *
 * Collisions above scratch threshold will bump the player.
 * Collisions above crash threshold will kill the player.
 *
 * When bumping against a wall, the player will be ejected within two frames.
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

	_beamsTex = loader()->loadAsset<ImageLoader>("beams.png");
	renderer()->createTexture(_beamsTex);

	_map.initialize();

	_root = _entities.root();
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


Vector3 MainState::partPos(unsigned shape, unsigned part) const {
	unsigned index = shape * _shipPartCount + part;
	assert(index < _shipShapes.size());
	return (Vector3() << _shipShapes[index], 0).finished() * _blockSize;
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
	for(int i = 0; i < _shipPartCount; ++i) {
		_shipParts[i] = _ship.clone(_ship, "shipPart");
		_shipParts[i].sprite()->setTileGridSize(Vector2i(3, 6));
		_shipParts[i].sprite()->setTileIndex(i + ((i<3)? 0: 3));
		_shipParts[i].place(partPos(_shipShape, i));
	}

	_distance = 0;
	_score    = 0;

	_scoreText = loadEntity("text.json", _root);
	_scoreText.place(Vector3(800, 1000, .5));

	_speedText = _scoreText.clone(_root);
	_speedText.place(Vector3(48, 1000, .5));

	_distanceText = _scoreText.clone(_root);
	_distanceText.place(Vector3(48, 32, .5));

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
	if(_nextShapeInput->justPressed()) {
		++_shipShape;
	}
	if(_prevShapeInput->justPressed()) {
		--_shipShape;
	}
	_shipShape = std::max(0, std::min(int(shipShapeCount()) - 1, int(_shipShape)));

	for(int i = 0; i < _shipPartCount; ++i) {
		Vector3 p = _shipParts[i].transform().translation();
		Vector3 q = partPos(_shipShape, i);
		Vector3 v = q - p;
		float dist = v.norm();
		if(dist > _partSpeed) {
			v *= _partSpeed / dist;
		}
		_shipParts[i].moveTo(p + v);
	}

	// Horizontal control and physics.
	if(_accelerateInput->isPressed()) {
		float damping = (1 + _shipHSpeed / _speedDamping);
		_shipHSpeed += _acceleration / (damping * damping);
	}
	if(_slowDownInput->isPressed()) {
		_shipHSpeed -= _slowDown;
	}

	_shipHSpeed = std::max(_shipHSpeed, 0.f);
	_scrollPos += _shipHSpeed * _speedFactor * tickDur;

	// Vertical control and physics
	float& vspeed = _shipVSpeed;

	// > Recharging thrusters.
	_climbPower = std::min(_climbPower + POWER_BUILDUP, POWER_MAX);
	_divePower  = std::min(_divePower  + POWER_BUILDUP, POWER_MAX);

	// > Activating thrusters.
	if (_thrustUpInput->justPressed()) {
		vspeed += _climbPower;
		_climbPower = 0;
	}
	if (_thrustDownInput->justPressed()) {
		vspeed -= _divePower;
		_divePower = 0;
	}
	if (_thrustUpInput->isPressed())   { vspeed += VSPEED_THRUST; }
	if (_thrustDownInput->isPressed()) { vspeed -= VSPEED_THRUST; }

	// > Automatic slowdown.
	if ( !(_thrustUpInput->isPressed() || _thrustDownInput->isPressed()) )
		vspeed *= VSPEED_FALLOFF;

	// > Locking speed.
	if (std::abs(vspeed) > VSPEED_MAX)
		vspeed = std::min(std::max(vspeed, -VSPEED_MAX), VSPEED_MAX);

	if (std::abs(vspeed) < VSPEED_MIN)
		vspeed = 0.0f;

	// > Bouncing (or crashing) on walls.
	float bump = collide(_ship);
	if (bump == INFINITY)
		dbgLogger.error("u ded. 'sploded hed");
	else if (bump != 0)
		vspeed = bump;
	
	for (unsigned i = 0 ; i < _shipPartCount ; ++i)
	{
		bump = collide(_shipParts[i]);
		if (bump == INFINITY)
			destroyPart(i);
		else if (bump != 0)
			vspeed = bump;
	}

	// > Looting
	collect (_ship);
	for (unsigned i = 0 ; i < _shipPartCount ; ++i)
		collect (_shipParts[i]);

	// > Snapping to grid.
	if (!vspeed)
	{
		float delta = std::fmod(shipPosition()[1],_blockSize);
		if (delta > 1)
			vspeed = delta / FRAMERATE * (delta > _blockSize/2.0?1:-1);
	}

	shipPosition()[1] += vspeed;

	_prevScrollPos = _scrollPos;
	_entities.updateWorldTransform();
}


float MainState::collide (const EntityRef part)
{
	float dvspeed = 0;

	Vector2 offset = Vector2(0,0);
	if (part != _ship)
		offset = part.transform().translation().head<2>();

	Vector2 partCorner = _ship.transform().translation().head<2>() + offset;
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


void MainState::collect (const EntityRef part)
{
	//TODO
}


void MainState::destroyPart (unsigned part)
{
	//TODO
	dbgLogger.warning ("Kabooom ! ", part);
}


void MainState::updateFrame() {
//	double time = double(_loop.frameTime()) / double(ONE_SEC);
	char buff[128];

	snprintf(buff, 128, "%.0f km/h", _shipHSpeed);
	_texts.get(_speedText)->setText(buff);

	snprintf(buff, 128, "%.0f km", _distance);
	_texts.get(_distanceText)->setText(buff);

	snprintf(buff, 128, "%d", _score);
	_texts.get(_scoreText)->setText(buff);

	// Rendering
	Context* glc = renderer()->context();

	glc->clear(gl::COLOR_BUFFER_BIT | gl::DEPTH_BUFFER_BIT);

	_spriteRenderer.beginFrame();

	float scroll = lerp(_loop.frameInterp(), _prevScrollPos, _scrollPos);
	float screenWidth = window()->width() * 1080. / window()->height();
	_map.render(scroll);
	renderBeams(_loop.frameInterp());
	_sprites.render(_loop.frameInterp(), _camera);
	_map.renderPreview(scroll, 50*_blockSize, screenWidth, 70);
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

//	_spriteRenderer.setDrawCall(tex, Texture::TRILINEAR, BLEND_ALPHA);

	unsigned index = _spriteRenderer.vertexCount();
	_spriteRenderer.addVertex(trans, p0 - n, color, texCoord.corner(Box2::TopLeft));
	_spriteRenderer.addVertex(trans, p1 - n, color, texCoord.corner(Box2::TopRight));
	_spriteRenderer.addVertex(trans, p0 + n, color, texCoord.corner(Box2::BottomLeft));
	_spriteRenderer.addVertex(trans, p1 + n, color, texCoord.corner(Box2::BottomRight));

	_spriteRenderer.addIndex(index + 0);
	_spriteRenderer.addIndex(index + 1);
	_spriteRenderer.addIndex(index + 2);
	_spriteRenderer.addIndex(index + 2);
	_spriteRenderer.addIndex(index + 1);
	_spriteRenderer.addIndex(index + 3);

	_spriteRenderer.endSprite();
}


void MainState::renderBeams(float interp) {
	TextureAspectSP texAspect = _beamsTex->aspect<TextureAspect>();
	TextureSP tex = texAspect->get();

	_spriteRenderer.setDrawCall(tex, Texture::TRILINEAR, BLEND_ALPHA);
	Matrix4 wt = lerp(interp,
	                  _ship._get()->prevWorldTransform.matrix(),
	                  _ship._get()->worldTransform.matrix());
	Vector4 mid4(_blockSize/2.f, _blockSize/2.f, 0, 1);
	Vector2 mid = mid4.head<2>();
	Vector2 laserOffset(1920, 0);

	Vector4 laserColor(1, 0, 0, 1);
	Vector4 beamsColor(0, .5, 1, 1);

	renderBeam(wt, tex, mid, mid + laserOffset, laserColor, 0, 0, 2);
	Vector2 shipPos[3];
	for(int i = 0; i < 3; ++i) {
		shipPos[i] = (wt * (mid4 + Vector4(_blockSize * i, 0, 0, 0))).head<2>();
	}
	for(int i = 0; i < _shipPartCount; ++i) {
		Matrix4 wt = lerp(interp,
		                  _shipParts[i]._get()->prevWorldTransform.matrix(),
		                  _shipParts[i]._get()->worldTransform.matrix());
		renderBeam(wt, tex, mid, mid + laserOffset, laserColor, 0, 0, 2);

		Vector2 partPos = (wt * mid4).head<2>();
		float advance = float(_loop.frameTime()) / float(ONE_SEC) + i * .1;
		renderBeam(Matrix4::Identity(), tex, shipPos[i%3], partPos, beamsColor,
		           advance, 1, 2);
	}
}


void MainState::resizeEvent() {
	Box3 viewBox(Vector3::Zero(),
	             Vector3(1080 * window()->width() / window()->height(), 1080, 1));
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
		parent = _root;
	}

	return _entities.createEntityFromJson(parent, json, localPath.dir());
}


Vec3 MainState::shipPosition()
{
	return _ship.transform().translation();
}
