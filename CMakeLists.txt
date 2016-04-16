##
##  Copyright (C) 2015 Simon Boyé
##
##  This file is part of alice_hie.
##
##  lair is free software: you can redistribute it and/or modify it
##  under the terms of the GNU General Public License as published by
##  the Free Software Foundation, either version 3 of the License, or
##  (at your option) any later version.
##
##  lair is distributed in the hope that it will be useful, but
##  WITHOUT ANY WARRANTY; without even the implied warranty of
##  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
##  General Public License for more details.
##
##  You should have received a copy of the GNU General Public License
##  along with lair.  If not, see <http://www.gnu.org/licenses/>.
##

cmake_minimum_required(VERSION 2.8.12.1)

project(fast_forward)

set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)


add_subdirectory(third-party)
add_subdirectory(src)
