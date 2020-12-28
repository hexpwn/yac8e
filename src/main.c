#include <ncurses.h>

WINDOW *create_newwin(int height, int width, int starty, int startx);
void destroy_win(WINDOW *local_win);

int main(int argc, char argv[])
{
	/* Window */
	WINDOW *game_win;
	int startx, starty, width, height;
	int ch;

	/* Start and config curses */
	initscr();
	cbreak(); 				// Line buffering disabled
	keypad(stdscr, TRUE);   // Enable Function keys (ex: F1)
	noecho(); 				// Do not display the users keypresses
	nodelay(stdscr, TRUE);  // Non-blocking getch
	curs_set(0); 			// Set cursor invisible


	/* Window init */
	height 	= 32;
	width 	= 64;
	starty = (LINES - height) / 2;
	startx = (COLS - width) / 2;
	
	printw("Window size: %d x %d", COLS, LINES);
	refresh();

	game_win = create_newwin(height, width, starty, startx);
	
	
	// Game loop
	int i = 0;
	while(getch() != KEY_F(1)){
		i++;
		if(i % 60 == 0){
			mvprintw(1, 0, "Frame: %d", i);
		}
	}
	
	
	getch();
	endwin();
	return 0;
}

/* Creates a new window that will represent the playable area */
WINDOW *create_newwin(int height, int width, int starty, int startx)
{
	WINDOW *local_win;

	local_win = newwin(height, width, starty, startx);
	box(local_win, 0, 0);
	wrefresh(local_win);
	return local_win;
}

/* Destroys a window, deleting it's border */
void destroy_win(WINDOW *local_win)
{
	wborder(local_win, ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ');
	wrefresh(local_win);
	delwin(local_win);
}
