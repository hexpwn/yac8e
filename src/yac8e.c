#include <ncurses.h>
#include <stdlib.h>
#include <unistd.h>

WINDOW *create_newwin(int width, int height, int starty, int startx);
WINDOW **initGraphics(int debug);
void destroy_win(WINDOW *local_win);

int main(int argc, char **argv)
{
	/* Check if ROM was passed */
	char *filename;
	int DEBUG;
	if(argc < 2 || argc > 3){
		printf("Usage: yac8e {-d: debug} <filename>\n");
		return 1;
	}
	else if(argc == 2){ 
		filename = argv[1];
		DEBUG = 0;
	}
	else if(argc == 3){
		switch(argv[1][1]){
			case 'd':
				DEBUG = 1;
				filename = argv[2];
				break;
			default:
				DEBUG = 0;
				filename = argv[2];
		}
	}

	/* Initialize CPU */

	/* Initialize graphic interface */
	WINDOW **windows = initGraphics(DEBUG);

	/* Load ROM */
	printf(filename);

	/* Run game loop */
	int i = 0;
	int ticks = 0;
	while(getch() != KEY_F(1)){
		i++;
		if(i % 60 == 0){
			if(DEBUG){
				ticks++;
				mvwprintw(windows[0], 1, 1, 
						"Window size: %d x %d - ROM Filename: %s", 
						COLS, LINES, filename);
				mvwprintw(windows[0], 3, 1, "Cycles: %d", i);
				mvwprintw(windows[0], 4, 1, "Ticks: %d", ticks);

			}
			wrefresh(windows[0]);
			wrefresh(windows[1]);
		}
	}
	
	/* Destroy graphic interface */
	free(windows);
	endwin();
	return 0;
}

/* Creates a new window with a frame border */
WINDOW *create_newwin(int width, int height, int starty, int startx)
{
	WINDOW *local_win;
	local_win = newwin(height, width, starty, startx);
	box(local_win, 0, 0);
	return local_win;
}

/* Destroys a window, deleting it's border */
void destroy_win(WINDOW *local_win)
{
	wborder(local_win, ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ');
	wrefresh(local_win);
	delwin(local_win);
}

/* Initializes the graphical interface */
WINDOW **initGraphics(int DEBUG)
{

	int startx, starty, i_width, i_height, g_width, g_height;

	// Start and config curses
	initscr();
	cbreak(); 				// Line buffering disabled
	keypad(stdscr, TRUE);   // Enable Function keys (ex: F1)
	noecho(); 				// Do not display the users keypresses
	nodelay(stdscr, TRUE);  // Non-blocking getch
	curs_set(0); 			// Set cursor invisible

	// Info window config
	i_height = 6;
	i_width = COLS - 2;

	// Game window config
	g_height = 32;
	g_width = 64;

	WINDOW **windows = malloc(sizeof(WINDOW)*2);

	// Create the debug info window
	if(DEBUG == 1){
		windows[0] = create_newwin(i_width, i_height, 0, 0);
		mvwprintw(windows[0], 2, 1, "Game size: %d x %d", g_width, g_height);
		starty = ((LINES + i_height - g_height) / 2) + 1;
	}
	else{
		starty = ((LINES - g_height) / 2);
	}
	// Creates the game window
	startx = (COLS - g_width) / 2;
	windows[1] = create_newwin(g_width, g_height, starty, startx);

	return windows;
}
