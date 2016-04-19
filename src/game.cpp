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


#include "main_state.h"
#include "splash_state.h"

#include "game.h"


Game::Game(int argc, char** argv)
    : GameBase(argc, argv),
      _mainState(),
      _splashState() {
}


Game::~Game() {
}


void Game::initialize() {
	GameBase::initialize();

	window()->setUtf8Title("Lair - Shapeout");
//	window()->resize(1920 / 4, 1080 / 4);
//	window()->setFullscreen(true);

	_splashState.reset(new SplashState(this));
	_mainState.reset(new MainState(this));

	_splashState->initialize();
	_mainState->initialize();

	_splashState->setup(_mainState.get(), Path(), 3);

	AssetSP music = _loader->loadAsset<MusicLoader>("shapeout.ogg");
	_loader->waitAll();
	audio()->playMusic(music);
}


void Game::shutdown() {
	_mainState->shutdown();
	_splashState->shutdown();

	// Required to ensure that everything is freed
	_mainState.reset();
	_splashState.reset();

	GameBase::shutdown();
}


MainState* Game::mainState() {
	return _mainState.get();
}


SplashState* Game::splashState() {
	return _splashState.get();
}
