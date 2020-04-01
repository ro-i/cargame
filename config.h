/*
 * cargame - a simple curses car game
 * Copyright (C) 2018-2020 Robert Imschweiler
 * 
 * This file is part of cargame.
 * 
 * cargame is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * cargame is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with cargame.  If not, see <https://www.gnu.org/licenses/>.
 */

/*
 * define the characters to use:
 *
 * car_left       horizontal (left-turned) car
 * car_right      horizontal (right-turned) car
 * car_up         vertical (down-turned) car
 * car_down       vertical (up-turned) car
 *
 * goalc          marker of the parking place in the level file
 * start          merker of the start place of the car in the level file
 */
typedef struct {
	wchar_t car_left;
	wchar_t car_right;
	wchar_t car_up;
	wchar_t car_down;
	wchar_t goal;
	wchar_t start;
} Characters;

/*
 * background (bg) and foreground (fg) colors for car, parking place (goal)
 * and message line (mscl)
 */
typedef struct {
	short car_bg;
	short car_fg;
	short goal_bg;
	short goal_fg;
	short mscl_bg;
	short mscl_fg;
} Colors;

/* 
 * variables:
 *
 * chars          character definitions
 * colors         color definitions
 */
const Characters chars = {
	.car_left = L'←',
	.car_right = L'→',
	.car_up = L'↑',
	.car_down = L'↓',
	.goal = L'x',
	.start = L'@'
};

/* set to "-1" for terminal default value */
const Colors colors = {
	.car_bg = -1,
	.car_fg = COLOR_RED,
	.goal_bg = -1,
	.goal_fg = COLOR_GREEN,
	.mscl_bg = COLOR_RED,
	.mscl_fg = COLOR_WHITE
};


/*
 * acceleration/break step width in nanoseconds
 *
 * Must be smaller than 1 sec (= 1000000000 nanoseconds)!
 */
const long speed_step_width = 10000000;
const long start_speed = 250000000;

/*
 * Show borders around the game field?
 * If no, you can leave the game field on the one side and enter on the other
 * (like the old snake games... :-) )
 *
 * May be toggled using "-b" option.
 */
const bool default_borders = true;
