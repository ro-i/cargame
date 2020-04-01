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

#include <curses.h>
#include <errno.h>
#include <limits.h>
#include <locale.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <wchar.h>

#include "config.h"


/* macros */
#define DIE(...)        (die(__LINE__, __VA_ARGS__))
#define GAMEWIN_LINES   (game.borders ? curses.lines-3 : curses.lines-1)
#define GAMEWIN_COLS    (game.borders ? curses.cols-2 : curses.cols)


typedef enum { LEFT, RIGHT, UP, DOWN } Direction;

typedef struct {
	int x;
	int y;
} Pos;

typedef struct {
	cchar_t car;
	cchar_t left;
	cchar_t right;
	cchar_t up;
	cchar_t down;
	Direction start_direction;
	Direction direction;
	Pos pos;
	Pos start_pos;
	Pos old_pos;
} Car;

typedef struct {
	int mscl_attrs;
	int cols;
	int lines;
	WINDOW *mainwin;
	WINDOW *gamewin;
	WINDOW *msclwin;
} Curses;

typedef struct {
	const char *level_file;
	wchar_t **field;
	Pos goal_pos;
	Pos start_pos;
	bool borders;
	sig_atomic_t need_resize;
	sig_atomic_t need_terminate;
	int cols;
	int lines;
	int x_offset;
	int y_offset;
} Game;

typedef struct {
	cchar_t goal;
	Pos pos;
} Goal;

typedef struct {
	timer_t id;
	struct itimerspec its;
	sig_atomic_t finished;
} Timer;


/* BEGIN: helper functions */
static void  *_malloc(size_t size);
static void  *_realloc(void *p, size_t size);
/* END: helper functions */
static void   cleanup(void);
static void   die(int line, const char *format, ...);
static void   car_go_down(void);
static void   car_go_left(void);
static void   car_go_right(void);
static void   car_go_up(void);
static void   car_move(void);
static bool   car_position_changed(void);
static void   car_reset(void);
static void   car_set_direction(Direction direction);
static void   car_turn_left(void);
static void   car_turn_right(void);
static void   curses_check_size(void);
static void   curses_end(void);
static void   curses_resize(void);
static void   curses_set_colors(short color_car, short color_goal);
static void   curses_set_windows(void);
static void   curses_setup(void);
static void   curses_setup_colors(void);
static int    curses_wgetch(WINDOW *win, wint_t *wch);
static bool   game_continue(void);
static void   game_loop(void);
static bool   game_over(void);
static bool   game_play_again(const wchar_t *msg);
static void   game_reset(void);
static void   game_resize(void);
static void   game_update_screen(void);
static bool   game_won(void);
static void   goal_place(void);
static void   level_file_read(const char *file);
static size_t level_file_readlines(FILE *in);
static void   level_init(void);
static void   level_init_positions(Pos goal_index, Pos car_start_index);
static void   level_resize(void);
static void   level_setup(void);
static void   mscl(const wchar_t *msg);
static void   mscl_clear(void);
static void   print_max_levelsize(void);
static void   sig_hdl(int sig);
static void   setup(void);
static void   timer_accelerate(void);
static void   timer_end(void);
static void   timer_reset(void);
static void   timer_set(long nanosec);
static void   timer_slowdown(void);
static void   timer_start(void);
static void   usage(void);
static void   version(void);


/* 
 * declaration of some global variables
 * (cf. "config.h", too)
 *
 * Regarding the initial values, cf. C99 Standard (WG14/N1256 Committee Draft 
 * — Septermber 7, 2007 ISO/IEC 9899:TC3), page 126:
 * "If an object that has static storage duration is not initialized explicitly,
 * then:
 * — if it has pointer type, it is initialized to a null pointer;
 * — if it has arithmetic type, it is initialized to (positive or unsigned) zero;
 * — if it is an aggregate, every member is initialized (recursively) according to these rules;
 * — if it is a union, the first named member is initialized (recursively) according to these
 *   rules."
 */
static Car     car;
static Curses  curses;
static Game    game = { .borders = default_borders };
static Goal    goal;
static Timer   timer;


/* BEGIN: helper functions */
void *
_malloc(size_t size)
{
	void *p;

	p = malloc(size);
	if (!p)
		DIE(strerror(errno));
	return p;
}

void *
_realloc(void *p, size_t size)
{
	p = realloc(p, size);
	if (!p)
		DIE(strerror(errno));
	return p;
}
/* END: helper functions */


void
car_go_down(void)
{
	if (!game.borders)
		car.pos.y = (car.pos.y+1 == GAMEWIN_LINES) ? 0 : car.pos.y+1;
	else
		car.pos.y++;
}

void
car_go_left(void)
{
	if (!game.borders)
		car.pos.x = car.pos.x ? car.pos.x-1 : GAMEWIN_COLS-1;
	else
		car.pos.x--;
}

void
car_go_right(void)
{
	if (!game.borders)
		car.pos.x = (car.pos.x+1 == GAMEWIN_COLS) ? 0 : car.pos.x+1;
	else
		car.pos.x++;
}

void
car_go_up(void)
{
	if (!game.borders)
		car.pos.y = car.pos.y ? car.pos.y-1 : GAMEWIN_LINES-1;
	else
		car.pos.y--;
}

void
car_move(void)
{
	switch (car.direction) {
	case DOWN:
		car_go_down();
		break;
	case LEFT:
		car_go_left();
		break;
	case RIGHT:
		car_go_right();
		break;
	case UP:
		car_go_up();
		break;
	}
}

bool
car_position_changed(void)
{
	return (car.pos.x != car.old_pos.x || car.pos.y != car.old_pos.y);
}

void
car_reset(void)
{
	car.pos = car.old_pos = car.start_pos;
	car.direction = car.start_direction;
	car_set_direction(car.direction);
}

void
car_set_direction(Direction direction)
{
	switch (direction) {
	case DOWN:
		car.car = car.down;
		break;
	case LEFT:
		car.car = car.left;
		break;
	case RIGHT:
		car.car = car.right;
		break;
	case UP:
		car.car = car.up;
		break;
	}
}

void
car_turn_left(void)
{
	switch (car.direction) {
	case DOWN:
		car.direction = RIGHT;
		break;
	case LEFT:
		car.direction = DOWN;
		break;
	case RIGHT:
		car.direction = UP;
		break;
	case UP:
		car.direction = LEFT;
		break;
	}
	car_set_direction(car.direction);
}

void
car_turn_right(void)
{
	switch (car.direction) {
	case DOWN:
		car.direction = LEFT;
		break;
	case LEFT:
		car.direction = UP;
		break;
	case RIGHT:
		car.direction = DOWN;
		break;
	case UP:
		car.direction = RIGHT;
		break;
	}
	car_set_direction(car.direction);
}

void
cleanup(void)
{
	int i;
	
	for (i = 0; i < game.lines; i++)
		free(game.field[i]);
	free(game.field);

	curses_end();
}

void
curses_check_size(void)
{
	if (game.cols > GAMEWIN_COLS || game.lines > GAMEWIN_LINES)
		DIE("ERROR: Your terminal is too small for this level.");
	else if (car.pos.x >= GAMEWIN_COLS || car.pos.y >= GAMEWIN_COLS)
		DIE("ERROR: Car out of screen after resize.");
}

void
curses_end(void)
{
	if (curses.gamewin)
		delwin(curses.gamewin);
	if (curses.msclwin)
		delwin(curses.msclwin);
	if (curses.mainwin)
		delwin(curses.mainwin);
	if (stdscr)
		endwin();
}

void
curses_resize(void)
{
	endwin();
	refresh();

	curses.cols = COLS;
	curses.lines = LINES;
	curses_check_size();

	wresize(curses.gamewin, GAMEWIN_LINES, GAMEWIN_COLS);
	wresize(curses.mainwin, curses.lines-1, curses.cols);

	curses_set_windows();
}

/*
 * Caution: nasty trap! The "color_pair" argument for setcchar() is the
 * *number* of the color pair, *not* the color_pair (= return value of
 * COLOR_PAIR()) itself!
 */
void
curses_set_colors(short color_car, short color_goal)
{
	wchar_t wcs[2];

	wcs[1] = L'\0';

	*wcs = chars.car_left;
	setcchar(&car.left, wcs, WA_NORMAL, color_car, NULL);
	*wcs = chars.car_right;
	setcchar(&car.right, wcs, WA_NORMAL, color_car, NULL);
	*wcs = chars.car_up;
	setcchar(&car.up, wcs, WA_NORMAL, color_car, NULL);
	*wcs = chars.car_down;
	setcchar(&car.down, wcs, WA_NORMAL, color_car, NULL);

	*wcs = chars.goal;
	setcchar(&goal.goal, wcs, WA_NORMAL, color_goal, NULL);
}

void
curses_set_windows(void)
{	
	if (curses.msclwin)
		delwin(curses.msclwin);
	curses.msclwin = newwin(1, curses.cols, curses.lines-1, 0);
	if (!curses.msclwin)
		DIE("ERROR: failed to resize properly.");
	wattron(curses.msclwin, curses.mscl_attrs);

	keypad(curses.gamewin, TRUE);
	werase(curses.mainwin);
	if (game.borders)
		box(curses.mainwin, 0, 0);
}

void
curses_setup(void)
{
	initscr();
	cbreak();
	curs_set(0);
	noecho();
	nonl();
	intrflush(stdscr, FALSE);

	curses.lines = LINES;
	curses.cols = COLS;
	curses_check_size();

	curses.mainwin = newwin(curses.lines-1, curses.cols, 0, 0);
	curses.gamewin = subwin(curses.mainwin, GAMEWIN_LINES, GAMEWIN_COLS,
			game.borders ? 1 : 0, game.borders ? 1 : 0);
	curses.msclwin = newwin(1, curses.cols, curses.lines-1, 0);
	if (!curses.mainwin || !curses.gamewin || !curses.msclwin)
		DIE("ERROR: ncurses could not be intialized properly.");

	curses_setup_colors();

	curses_set_windows();
}

void
curses_setup_colors(void)
{
	if (!has_colors()) {
		curses_set_colors(0, 0);
		return;
	}

	start_color();
	use_default_colors();

	init_pair(1, colors.car_fg, colors.car_bg);
	init_pair(2, colors.goal_fg, colors.goal_bg);
	init_pair(3, colors.mscl_fg, colors.mscl_bg);

	curses_set_colors(1, 2);
	curses.mscl_attrs = COLOR_PAIR(3);
}

int
curses_wgetch(WINDOW *win, wint_t *wch)
{
	bool mscl_used = false;
	int r;

	for (;;) {
		r = wget_wch(win, wch);
		if (r == KEY_CODE_YES && *wch == KEY_RESIZE) {
			game_resize();
		} else if (r != ERR) {
			break;
		} else if (errno == EINTR) {
			/* interrupted by a signal */
			break;
		} else {
			mscl_clear();
			mscl(L"Input Error");
			mscl_used = true;
		}
	}
	if (mscl_used)
		mscl_clear();

	return r;
}

void
die(int line, const char *format, ...)
{
	va_list ap;

	cleanup();
	if (format) {
		fprintf(stderr, "%s, line %d: ", NAME_STR, line);
		va_start(ap, format);
		vfprintf(stderr, format, ap);
		va_end(ap);
		fputc('\n', stderr);
	}
	exit(EXIT_FAILURE);
}

bool
game_continue(void)
{
	wchar_t wch[2];

	if (car.pos.x == goal.pos.x && car.pos.y == goal.pos.y) {
		/* place car on parking place */
		game_update_screen();
		return game_won();
	}

	/* check if car moved */
	if (!car_position_changed())
		return true;

	/* check if car crashed the borders */
	if (game.borders && (car.pos.x < 0 || car.pos.y < 0
				|| car.pos.x == GAMEWIN_COLS
				|| car.pos.y == GAMEWIN_LINES))
		return game_over();

	if (mvwinnwstr(curses.gamewin, car.pos.y, car.pos.x, wch, 1) == ERR)
		return false;

	if (*wch != L' ')
		return game_over();

	return true;
}

void
game_loop(void)
{
	int r;
	wint_t wch;

	timer_start();

	for (;;) {
		game_update_screen();

		r = curses_wgetch(curses.gamewin, &wch);

		if (r == ERR) {
			if (timer.finished) {
				car_move();
				if (!game_continue())
					break;
				timer.finished = 0;
			} else if (game.need_resize) {
				game_resize();
				game.need_resize = 0;
			} else if (game.need_terminate) {
				DIE(NULL);
			}
		} else if (r == KEY_CODE_YES) {
			switch (wch) {
			case KEY_DOWN:
				timer_slowdown();
				break;
			case KEY_LEFT:
				car_turn_left();
				break;
			case KEY_RIGHT:
				car_turn_right();
				break;
			case KEY_UP:
				timer_accelerate();
				break;
			}
		} else if (wch == L'a') {
			timer_accelerate();
		} else if (wch == L'b') {
			timer_slowdown();
		} else if (wch == L'q') {
			break;
		}
	}

	timer_end();
}

bool
game_over(void)
{
	bool result = game_play_again(L"Game over. Play again? [y/n]");
	if (result)
		goal_place();
	return result;
}

/* ask user to play again with question "msg" */
bool
game_play_again(const wchar_t *msg)
{
	wint_t wch;

	mscl(msg);
	do {
		if (curses_wgetch(curses.msclwin, &wch) != OK)
			continue;
	} while (wch != L'y' && wch != L'n');

	mscl_clear();

	if (wch == L'y') {
		game_reset();
		return true;
	} else {
		return false;
	}
}

void
game_reset(void)
{
	mvwaddch(curses.gamewin, car.old_pos.y, car.old_pos.x, ' ');
	car_reset();
	timer_reset();
}

void
game_resize(void)
{
	curses_resize();
	level_resize();
	goal_place();
	game_update_screen();
}

void
game_update_screen(void)
{
	mvwadd_wch(curses.gamewin, car.pos.y, car.pos.x, &car.car);

	if (car_position_changed()) {
		mvwaddch(curses.gamewin, car.old_pos.y, car.old_pos.x, ' ');
		car.old_pos = car.pos;
	}

	wnoutrefresh(curses.gamewin);
	wnoutrefresh(curses.mainwin);

	doupdate();
}

bool
game_won(void)
{
	bool result = game_play_again(L"Won! Play again? [y/n]");
	if (result)
		goal_place();
	return result;
}

void
goal_place(void)
{
	mvwadd_wch(curses.gamewin, goal.pos.y, goal.pos.x, &goal.goal);
}

void
level_draw(void)
{
	for (int i = 0; i < game.lines; i++) {
		mvwaddwstr(curses.gamewin, game.y_offset+i, game.x_offset,
				game.field[i]);
	}
}

void
level_file_read(const char *file)
{
	FILE *in;

	if (!(in = fopen(file, "r")))
		DIE(strerror(errno));

	game.lines = level_file_readlines(in);

	if (ferror(in)) {
		fclose(in);
		DIE("Error while trying to read level file.");
	}
	fclose(in);

	if (!game.lines)
		DIE("ERROR: level file contains no lines.");
}

size_t
level_file_readlines(FILE *in)
{
	size_t i, limit = INT_MAX, size;
	wchar_t *ptr;

	for (i = 0, size = BUFSIZ; ; i++) {
		if (i == limit)
			DIE("ERROR: level dimension too big.");

		game.field = _realloc(game.field, sizeof(wchar_t *)*(i+1));
		game.field[i] = _malloc(size);

		if (!fgetws(game.field[i], size, in)) {
			free(game.field[i]);
			game.field = _realloc(game.field, sizeof(wchar_t *)*i);
			return i;
		}

		if ((ptr = wcschr(game.field[i], L'\n'))) {
			*ptr = L'\0';
			continue;
		}

		do {
			size += BUFSIZ;
			if (size > limit)
				DIE("ERROR: level dimension too big.");
			game.field[i] = _realloc(game.field[i], size);
			if (!fgetws(game.field[i], size, in))
				return i+1;
		} while (!(ptr = wcschr(game.field[i], L'\n')));

		*ptr = L'\0';
		size = BUFSIZ;
	}
}

/*
 * Get maximum number of columns, determine the position of the goal and the
 * start position of the car and validate them.
 *
 * Remember to delete the car character from the level content - otherwise,
 * it will be erroneously drawn on resize.
 */
void
level_init(void)
{
	Pos goal_index = { 0, 0 }, car_start_index = { 0, 0 };
	size_t goal_count = 0, start_count = 0;
	int i, len;
	wchar_t *ptr;

	for (i = 0; i < game.lines; i++) {
		len = wcswidth(game.field[i], INT_MAX);
		if (len == -1)
			DIE("ERROR: wcswidth() returned -1.");
		else if (len > game.cols)
			game.cols = len;

		if ((ptr = wcschr(game.field[i], chars.goal))) {
			goal_index = (Pos) { .x = ptr-game.field[i], .y = i };
			goal_count++;
			continue;
		}

		if ((ptr = wcschr(game.field[i], chars.car_down)))
			car.start_direction = DOWN;
		else if ((ptr = wcschr(game.field[i], chars.car_left)))
			car.start_direction = LEFT;
		else if ((ptr = wcschr(game.field[i], chars.car_right)))
			car.start_direction = RIGHT;
		else if ((ptr = wcschr(game.field[i], chars.car_up)))
			car.start_direction = UP;
		else
			continue;
		*ptr = L' ';
		car_start_index = (Pos) { .x = ptr-game.field[i], .y = i };
		start_count++;
	}

	if (goal_count != 1) {
		DIE("ERROR: %zu goal positions in your level file."
				" (Must be 1).", goal_count);
	} else if (start_count != 1) {
		DIE("ERROR: %zu start positions in your level file."
				" (Must be 1).", start_count);
	}

	level_init_positions(goal_index, car_start_index);
}

/*
 * Note: To get the position of the goal / the start position of the car
 * *on the screen*, we have to calculate the display width (wcswidth) of the
 * characters before.
 */
void
level_init_positions(Pos goal_index, Pos car_start_index)
{
	wchar_t buf[game.cols+1];

	game.x_offset = (GAMEWIN_COLS-game.cols)/2;
	game.y_offset = (GAMEWIN_LINES-game.lines)/2;

	wcsncpy(buf, game.field[goal_index.y], goal_index.x);
	buf[goal_index.x] = L'\0';

	goal.pos.x = wcswidth(buf, INT_MAX) + game.x_offset;
	goal.pos.y = goal_index.y + game.y_offset;

	wcsncpy(buf, game.field[car_start_index.y], car_start_index.x);
	buf[car_start_index.x] = L'\0';

	car.start_pos.x = wcswidth(buf, INT_MAX) + game.x_offset;
	car.start_pos.y = car_start_index.y + game.y_offset;
}

void
level_resize(void)
{
	int delta_x, delta_y, x_offset, y_offset;

	x_offset = (GAMEWIN_COLS-game.cols)/2;
	y_offset = (GAMEWIN_LINES-game.lines)/2;

	delta_x = x_offset-game.x_offset;
	delta_y = y_offset-game.y_offset;

	goal.pos.x += delta_x;
	goal.pos.y += delta_y;

	car.start_pos.x += delta_x;
	car.start_pos.y += delta_y;

	car.pos.x += delta_x;
	car.pos.y += delta_y;

	car.old_pos.x += delta_x;
	car.old_pos.y += delta_y;

	game.x_offset = x_offset;
	game.y_offset = y_offset;

	level_draw();
}

void
level_setup(void)
{
	level_file_read(game.level_file);
	level_init();
	curses_check_size();

	game.x_offset = (GAMEWIN_COLS-game.cols)/2;
	game.y_offset = (GAMEWIN_LINES-game.lines)/2;

	level_draw();

	goal_place();
	car_reset();
}

/* write message (msg) to message line (msclwin) */
void
mscl(const wchar_t *msg)
{
	wmove(curses.msclwin, 0, 0);
	waddnwstr(curses.msclwin, msg, curses.cols);
	wrefresh(curses.msclwin);

	wmove(curses.gamewin, car.pos.y, car.pos.x);
}

void
mscl_clear(void)
{
	wmove(curses.msclwin, 0, 0);
	wclrtoeol(curses.msclwin);
	wrefresh(curses.msclwin);
}

void
print_car_chars(void)
{
	setlocale(LC_ALL, "");
	printf("car down: %lc\ncar left: %lc\ncar right: %lc\ncar up: %lc\n",
			chars.car_down, chars.car_left, chars.car_right,
			chars.car_up);
}

void
print_max_levelsize(void)
{
	initscr();
	curses.cols = COLS;
	curses.lines = LINES;
	endwin();

	game.borders = true;
	printf("max level size with borders:\n  lines: %d\n  cols: %d\n",
			GAMEWIN_LINES, GAMEWIN_COLS);
	game.borders = false;
	printf("max level size without borders:\n  lines: %d\n  cols: %d\n",
			GAMEWIN_LINES, GAMEWIN_COLS);
}

void
sig_hdl(int sig)
{
	switch (sig) {
	case SIGALRM:
		timer.finished = 1;
		break;
	case SIGCONT:
		game.need_resize = 1;
		break;
	case SIGINT: /* fall through */
	case SIGTERM:
		game.need_terminate = 1;
		break;
	}
}

void
setup(void)
{
	static struct sigaction sig_act;

	/* install signal handler */
	memset(&sig_act, 0, sizeof(sig_act));
	sig_act.sa_handler = sig_hdl;
	if (sigaction(SIGALRM, &sig_act, NULL))
		DIE(strerror(errno));
	if (sigaction(SIGCONT, &sig_act, NULL))
		DIE(strerror(errno));
	if (sigaction(SIGINT, &sig_act, NULL))
		DIE(strerror(errno));
	if (sigaction(SIGTERM, &sig_act, NULL))
		DIE(strerror(errno));

	if (!setlocale(LC_ALL, ""))
		DIE("ERROR: setlocale() failed");

	curses_setup();
	level_setup();
}

/*
 * Accelerate = Make the car faster = decrease the interval.
 */
void
timer_accelerate(void)
{
	/* if timer has not been initialized yet */
	if (timer.its.it_interval.tv_sec == 0
			&& timer.its.it_interval.tv_nsec == 0) {
		timer_set(start_speed);
		return;
	}

	if (timer.its.it_value.tv_nsec >= speed_step_width) {
		timer.its.it_value.tv_nsec -= speed_step_width;
	} else if (!timer.its.it_value.tv_sec) {
		return;
	} else {
		timer.its.it_value.tv_sec--;
		timer.its.it_value.tv_nsec =
			LONG_MAX-(speed_step_width-timer.its.it_value.tv_nsec);
	}
	timer.its.it_interval = timer.its.it_value;

	if (timer_settime(timer.id, 0, &timer.its, NULL))
		DIE(strerror(errno));
}

void
timer_end(void)
{
	if (timer_delete(timer.id))
		DIE(strerror(errno));
}

void
timer_reset(void)
{
	timer.its = (struct itimerspec) {
		.it_value = { 0, 0 }, .it_interval = { 0, 0 }
	};

	if (timer_settime(timer.id, 0, &timer.its, NULL))
		DIE(strerror(errno));
}

void
timer_set(long nanosec)
{
	timer.its.it_value.tv_nsec = nanosec;
	timer.its.it_interval.tv_nsec = nanosec;

	if (timer_settime(timer.id, 0, &timer.its, NULL))
		DIE(strerror(errno));
}

/*
 * Break = Make the car slower = increase the interval.
 */
void
timer_slowdown(void)
{
	/* return if timer has not been initialized yet */
	if (timer.its.it_interval.tv_sec == 0
			&& timer.its.it_interval.tv_nsec == 0)
		return;

	if (LONG_MAX-speed_step_width >= timer.its.it_value.tv_nsec) {
		timer.its.it_value.tv_nsec += speed_step_width;
	} else {
		timer.its.it_value.tv_sec++;
		timer.its.it_value.tv_nsec +=
			speed_step_width-(LONG_MAX-timer.its.it_value.tv_nsec);
	}
	timer.its.it_interval = timer.its.it_value;

	if (timer_settime(timer.id, 0, &timer.its, NULL))
		DIE(strerror(errno));
}

void
timer_start(void)
{
	if (timer_create(CLOCK_MONOTONIC, NULL, &timer.id))
		DIE(strerror(errno));
	if (timer_settime(timer.id, 0, &timer.its, NULL))
		DIE(strerror(errno));
}

void
usage(void)
{
	printf("usage: %s [OPTION]... LEVELFILE\n"
			"   or: %s OPTION\n"
			"\noptions available:\n"
			"    -b      toggle use of game borders\n"
			"    -c      print car characters\n"
			"    -h      show this help\n"
			"    -s      show maximum level size\n"
			"    -v      show version information\n\n"
			"See manpage for additional usage information.\n",
			NAME_STR, NAME_STR);
}

void
version(void)
{
	printf("%s, version %s\n"
			"Copyright (C) 2018-2020 Robert Imschweiler.\n"
			"License GPLv3+: GNU GPL version 3 or later "
			"<https://gnu.org/licenses/gpl.html>\n"
			"This is free software; you are free to change and "
			"redistribute it.\n"
			"There is NO WARRANTY, to the extent permitted by law.\n",
			NAME_STR, VERSION_STR
	      );
}

int
main(int argc, char **argv)
{
	int opt;

	while ((opt = getopt(argc, argv, "bchsv")) != -1) {
		switch (opt) {
		case 'b':
			game.borders = !default_borders;
			break;
		case 'c':
			print_car_chars();
			return EXIT_SUCCESS;
		case 'h':
			usage();
			return EXIT_SUCCESS;
		case 's':
			print_max_levelsize();
			return EXIT_SUCCESS;
		case 'v':
			version();
			return EXIT_SUCCESS;
		default:
			usage();
			return EXIT_FAILURE;
		}
	}
	if (optind >= argc) {
		usage();
		return EXIT_FAILURE;
	}
	game.level_file = argv[optind];

	setup();

	game_loop();

	cleanup();
	return EXIT_SUCCESS;
}

