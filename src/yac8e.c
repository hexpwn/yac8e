#include <ncurses.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>

WINDOW *create_newwin(int width, int height, int starty, int startx);
WINDOW **initGraphics(int debug);
struct CPU *new_cpu();
void destroy_win(WINDOW *local_win);
void tick(struct CPU *cpu, WINDOW **windows);
void draw(struct CPU *cpu, WINDOW *gamewindow);

// A struct representing the CPU. Will be separated later
struct CPU {
	unsigned char memory[4096]; // memory
	unsigned char V[16]; 		// registers
	unsigned short stack[16];   // call stack
	unsigned char gfx[64 * 32]; // frame buffer (64x32 pixels)
	unsigned char inputs[16];   // keyboard inputs
	unsigned short I; 			// index registers
	unsigned short pc; 			// program counter
};

int main(int argc, char **argv)
{
	// Check if ROM was passed and flags
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

	// Initialize CPU
	struct CPU *chip8 = new_cpu();
	chip8->pc = 0x200;

	// Load ROM
	FILE *rom = fopen(filename, "rb");
	assert(rom != NULL);

	fseek(rom, 0, SEEK_END);
	long lsize = 0;
	lsize = ftell(rom);
	rewind(rom);

	// Must load at offset 0x200 of memory
	size_t n = fread(&chip8->memory[512], 1, lsize, rom);
	printf("Read %ld bytes from %s\n", n, filename);
	fclose(rom);

	// Initialize graphic interface
	WINDOW **windows = initGraphics(DEBUG);
	WINDOW *debug_w = windows[0];
	WINDOW *game_frame_w = windows[1];
	WINDOW *game_w = windows[2];

	// Run game loop 
	int ticks = 0;
	int drawFlag = 1;
	while(getch() != KEY_F(1)){
		// Run a tick
		tick(chip8, windows);
		
		// Update game window (if necessary)
		if(drawFlag == 1){
			draw(chip8, game_w);
			wrefresh(game_frame_w);
			wrefresh(game_w);
		}
			
		// Update debug information (if necessary)
		ticks++;
		if(DEBUG){
			mvwprintw(debug_w, 1, 1, 
					"Window size: %d x %d - ROM Filename: %s", 
					COLS, LINES, filename);
			mvwprintw(debug_w, 3, 1, "Ticks: %d", ticks);
			wrefresh(debug_w);
			sleep(1);
		}
	}
	
	// Destroy graphic interface
	free(windows);
	endwin();

	// Destroy CPU
	free(chip8);
	return 0;
}
// Creates a new window with a frame border
WINDOW *create_newwin(int width, int height, int starty, int startx)
{
	WINDOW *local_win;
	local_win = newwin(height, width, starty, startx);
	box(local_win, 0, 0);
	return local_win;
}

// Destroys a window, deleting it's border
void destroy_win(WINDOW *local_win)
{
	wborder(local_win, ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ');
	wrefresh(local_win);
	delwin(local_win);
}

// Initializes the graphical interface
WINDOW **initGraphics(int debug)
{
	int startx, starty, 
		i_width, i_height, 
		gf_width, gf_height, 
		g_width, g_height;

	// Start and config curses
	initscr();
	cbreak(); 				// Line buffering disabled
	keypad(stdscr, TRUE);   // Enable Function keys (ex: F1)
	noecho(); 				// Do not display the users keypresses
	nodelay(stdscr, TRUE);  // Non-blocking getch
	curs_set(0); 			// Set cursor invisible

	// Debug info window config
	i_height 	= 6;
	i_width 	= COLS - 2;

	// Game window frame config
	gf_height 	= 34;
	gf_width 	= 66;

	// Game window config
	g_height 	= 32;
	g_width 	= 64;

	WINDOW **windows = malloc(sizeof(WINDOW)*2);

	// Create the debug info window
	if(debug == 1){
		windows[0] = create_newwin(i_width, i_height, 0, 0);
		mvwprintw(windows[0], 2, 1, "Game size: %d x %d", g_width, g_height);
		starty = i_height + 1;
	}
	else{
		starty = ((LINES - gf_height) / 2);
	}
	// Creates the game frame window
	startx = (COLS - gf_width) / 2;
	windows[1] = create_newwin(gf_width, gf_height, starty, startx);

	// Creates the game window
	startx += 1;
	starty += 1;
	windows[2] = newwin(g_height, g_width, starty, startx);
	return windows;
}

struct CPU *new_cpu()
{
	struct CPU *cpu = malloc(sizeof(struct CPU));
	assert(cpu != NULL);
	return cpu;
}

void tick(struct CPU *cpu, WINDOW **windows)
{
	WINDOW *debug_w = windows[0];
	unsigned short opcode;

	// Update opcode
	opcode = cpu->memory[cpu->pc] << 8 | cpu->memory[cpu->pc + 1];

	// Decode opcode
	char mnemonic[128];

	switch(opcode & 0xF000){
		case 0x0000:
			switch(opcode & 0x00FF){
				case 0x00E0:
					snprintf(mnemonic, sizeof(mnemonic), "CLR");
					break;
				case 0x00EE:
					snprintf(mnemonic, sizeof(mnemonic), "RET");
					break;
				default:
					snprintf(mnemonic, sizeof(mnemonic), "CALL");
					break;
			}
		case 0x1000:
			snprintf(mnemonic, sizeof(mnemonic), "JMP");
			break;
		case 0x2000:{
			unsigned short addr = opcode & 0xFFF;
			snprintf(mnemonic, sizeof(mnemonic), "CALL 0x%03x", addr);
			break;
			}
		case 0x3000:
			snprintf(mnemonic, sizeof(mnemonic), "SEQI");
			break;
		case 0x4000:
			snprintf(mnemonic, sizeof(mnemonic), "SNEQ");
			break;
		case 0x5000:
			snprintf(mnemonic, sizeof(mnemonic), "SEQR");
			break;
		case 0x6000:
			snprintf(mnemonic, sizeof(mnemonic), "STRI");
			break;
		case 0x7000:
			snprintf(mnemonic, sizeof(mnemonic), "ADD");
			break;
		case 0x8000:
			switch(opcode & 0x000F){
				case 0x0:{
					int X, Y;
					X = opcode & 0x0F00;
					Y = opcode & 0x00F0;
					snprintf(mnemonic, sizeof(mnemonic), "STR V%d, V%d", X, Y);
					break;
				}
			}
		case 0x9000:
			snprintf(mnemonic, sizeof(mnemonic), "SNEQ");
			break;
		case 0xa000:
			snprintf(mnemonic, sizeof(mnemonic), "SETM");
			break;
		case 0xb000:
			snprintf(mnemonic, sizeof(mnemonic), "JMPP");
			break;
		case 0xc000:
			snprintf(mnemonic, sizeof(mnemonic), "RAND");
			break;
		case 0xd000:
			snprintf(mnemonic, sizeof(mnemonic), "DRAW");
			break;
		case 0xe000:
			break;
		case 0xf000:
			snprintf(mnemonic, sizeof(mnemonic), "FFFF");
			break;
		default:
			snprintf(mnemonic, sizeof(mnemonic), "UNK");
	}

	// Update pc
	cpu->pc += 2;

	// Update debug info
	mvwprintw(debug_w, 4, 1, "opcode: %04x Mnemonic: %s", opcode, mnemonic);
}

void draw(struct CPU *cpu, WINDOW *gamewindow)
{
}
