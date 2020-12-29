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
		
		// Draw game window (if necessary)
		if(drawFlag == 1){
			draw(chip8, game_w);
			wrefresh(game_frame_w);
			wrefresh(game_w);
		}
			
		// Draw debug information (if necessary)
		if(DEBUG){
			ticks++;
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
					{
 					unsigned short NNN = opcode & 0x0FFF;
					snprintf(mnemonic, sizeof(mnemonic), "CALL 0x%03x", NNN);
					break;
					}
			};
		case 0x1000:
			{
 			unsigned short NNN = opcode & 0x0FFF;
			snprintf(mnemonic, sizeof(mnemonic), "JMP 0x%03x", NNN);
			break;
			}
		case 0x2000:
			{
			unsigned short NNN = opcode & 0x0FFF;
			snprintf(mnemonic, sizeof(mnemonic), "CALL 0x%03x", NNN);
			break;
			}
		case 0x3000:
			{
			unsigned int X = opcode >> 8 & 0xF;
			unsigned short NN = opcode & 0x00FF;
			snprintf(mnemonic, sizeof(mnemonic), "SEQI V%d, 0x%02x", X, NN);
			break;
			}
		case 0x4000:
			{
			unsigned int X = opcode >> 8 & 0xF;
			unsigned short NN = opcode & 0x00FF;
			snprintf(mnemonic, sizeof(mnemonic), "SNEQ V%d, 0x%02x", X, NN);
			break;
			}
		case 0x5000:
			{
			unsigned int X = opcode >> 8 & 0xF;
			unsigned int Y = opcode >> 4 & 0xF;
			snprintf(mnemonic, sizeof(mnemonic), "SEQR V%d, V%d", X, Y);
			break;
			}
		case 0x6000:
			{
			unsigned int X = opcode >> 8 & 0xF;
			unsigned short NN = opcode & 0x00FF;
			snprintf(mnemonic, sizeof(mnemonic), "STRI 0x%02x, V%d", X, NN);
			break;
			}
		case 0x7000:
			{
			unsigned int X = opcode >> 8 & 0xF;
			unsigned short NN = opcode & 0x00FF;
			snprintf(mnemonic, sizeof(mnemonic), "ADD V%d, 0x%02x", X, NN);
			break;
			}
		case 0x8000:
			switch(opcode & 0x000F){
				case 0x0:
					{
					unsigned int X = opcode >> 8 & 0xF;
					unsigned int Y = opcode >> 4 & 0xF;
					snprintf(mnemonic, sizeof(mnemonic), "STR V%x, V%x", Y, X);
					break;
					}
				case 0x1:
					{
					unsigned int X = opcode >> 8 & 0xF;
					unsigned int Y = opcode >> 4 & 0xF;
					snprintf(mnemonic, sizeof(mnemonic), "OR V%d, V%d", X, Y);
					break;
					}
				case 0x2:
					{
					unsigned int X = opcode >> 8 & 0xF;
					unsigned int Y = opcode >> 4 & 0xF;
					snprintf(mnemonic, sizeof(mnemonic), "AND V%d, V%d", X, Y);
					break;
					}
				case 0x3:
					{
					unsigned int X = opcode >> 8 & 0xF;
					unsigned int Y = opcode >> 4 & 0xF;
					snprintf(mnemonic, sizeof(mnemonic), "XOR V%d, V%d", X, Y);
					break;
					}
				case 0x4:
					{
					unsigned int X = opcode >> 8 & 0xF;
					unsigned int Y = opcode >> 4 & 0xF;
					snprintf(mnemonic, sizeof(mnemonic), "ADD V%d, V%d", X, Y);
					break;
					}
				case 0x5:
					{
					unsigned int X = opcode >> 8 & 0xF;
					unsigned int Y = opcode >> 4 & 0xF;
					snprintf(mnemonic, sizeof(mnemonic), "SUB V%d, V%d", X, Y);
					break;
					}
				case 0x6:
					{
					unsigned int X = opcode >> 8 & 0xF;
					snprintf(mnemonic, sizeof(mnemonic), "SHR V%d, 1", X);
					break;
					}
				case 0x7:
					{
					unsigned int X = opcode >> 8 & 0xF;
					unsigned int Y = opcode >> 4 & 0xF;
					snprintf(mnemonic, sizeof(mnemonic), "SUBI V%d, V%d", X, Y);
					break;
					}
				case 0xE:
					{
					unsigned int X = opcode >> 8 & 0xF;
					snprintf(mnemonic, sizeof(mnemonic), "SHL V%d, 1", X);
					break;
					}
			}
		case 0x9000:
			{
			unsigned int X = opcode >> 8 & 0xF;
			unsigned int Y = opcode >> 4 & 0xF;
			snprintf(mnemonic, sizeof(mnemonic), "SNEQ V%d, V%d", X, Y);
			break;
			}
		case 0xa000:
			{
			unsigned short NNN = opcode & 0x0FFF;
			snprintf(mnemonic, sizeof(mnemonic), "SETM 0x%03x", NNN);
			break;
			}
		case 0xb000:
			{
			unsigned short NNN = opcode & 0x0FFF;
			snprintf(mnemonic, sizeof(mnemonic), "JMPA V0, 0x%03x", NNN);
			break;
			}
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
	mvwprintw(debug_w, 4, 1, "opcode: %04x Mnemonic: %s\n", opcode, mnemonic);
}

void draw(struct CPU *cpu, WINDOW *gamewindow)
{
}
