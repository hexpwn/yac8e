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
void end(struct CPU *cpu, WINDOW **windows);
void panic(struct CPU *cpu, WINDOW **windows);
bool push_stack(unsigned short value, struct CPU *cpu);
unsigned short pop_stack(struct CPU *cpu);

// A struct representing the CPU. Will be separated later
struct CPU {
	unsigned char memory[4096]; // memory
	unsigned char V[16]; 		// registers
	unsigned short stack[16];   // call stack
	unsigned char gfx[64 * 32]; // frame buffer (64x32 pixels)
	unsigned char inputs[16];   // keyboard inputs
	unsigned short I; 			// index registers
	unsigned short pc; 			// program counter
	unsigned int sp; 			// stack pointer
	bool draw; 					// draw flag
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
	while(getch() != KEY_F(1)){
		// Run a tick
		tick(chip8, windows);
		
		// Draw game window (if necessary)
		if(chip8->draw){
			draw(chip8, game_w);
			wrefresh(game_frame_w);
			wrefresh(game_w);
			chip8->draw = false;
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
	end(chip8, windows);
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
	i_height 	= 7;
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
					{
					// Returns from a subroutine. 
					unsigned short ret_addr = pop_stack(cpu);
					assert(ret_addr != 0xffff);
					cpu->pc = ret_addr;

					// Debug info.
					snprintf(mnemonic, sizeof(mnemonic), "RET");
					break;
					}
				default:
					{
 					unsigned short NNN = opcode & 0x0FFF;
					snprintf(mnemonic, sizeof(mnemonic), "CALL 0x%03x", NNN);
					break;
					}
			};
		case 0x1000:
			{
			// Jumps to address NNN.
 			unsigned short NNN = opcode & 0x0FFF;
			cpu->pc = NNN - 2; // this is disgusting...

			// Debug info
			snprintf(mnemonic, sizeof(mnemonic), "JMP 0x%03x", NNN);
			break;
			}
		case 0x2000:
			{
			// Calls subroutine at NNN.
			unsigned short NNN = opcode & 0x0FFF;
			bool s = push_stack(cpu->pc+2, cpu);
			assert(s != false);
			cpu->pc = NNN - 2;
			
			snprintf(mnemonic, sizeof(mnemonic), "CALL 0x%03x", NNN);
			break;
			}
		case 0x3000:
			{
			// Skips the next instruction if VX equals NN. 
			// (Usually the next instruction is a jump to skip a code block) 
			unsigned int X = opcode >> 8 & 0xF;
			unsigned short NN = opcode & 0x00FF;
			if(cpu->V[X] == NN){
				cpu->pc += 2;
			}
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
			// Sets VX to NN. 
			unsigned int X = opcode >> 8 & 0xF;
			unsigned short NN = opcode & 0x00FF;

			cpu->V[X] = NN;

			// Debug info
			snprintf(mnemonic, sizeof(mnemonic), "STRI 0x%02x, V%d", NN, X);
			break;
			}
		case 0x7000:
			{
			// Adds NN to VX. (Carry flag is not changed) 
			unsigned int X = opcode >> 8 & 0xF;
			unsigned short NN = opcode & 0x00FF;

			cpu->V[X] += NN;

			// Debug info.
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
				default:
					snprintf(mnemonic, sizeof(mnemonic), "UNK OPCODE");
					panic(cpu, windows);
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
			// Sets I to the address NNN.
			unsigned short NNN = opcode & 0x0FFF;
			cpu->I = NNN;

			// Debug info
			snprintf(mnemonic, sizeof(mnemonic), "MEMS 0x%03x", NNN);
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
			{
			// Draws a sprite at coordinate (VX, VY) that has a width of 8 
			// pixels and a height of N+1 pixels. Each row of 8 pixels is read 
			// as bit-coded starting from memory location I; I value doesn’t 
			// change after the execution of this instruction. As described 
			// above, VF is set to 1 if any screen pixels are flipped from set 
			// to unset when the sprite is drawn, and to 0 if that doesn’t 
			// happen 
			unsigned int X = opcode >> 8 & 0xF; 
			unsigned int Y = opcode >> 4 & 0xF; 
			unsigned int N = opcode & 0xF;

			// Debug info.
			snprintf(mnemonic, sizeof(mnemonic), "DRAW");
			cpu->draw = true;
			break;
			}
		case 0xe000:
			break;
		case 0xf000:
			switch(opcode & 0x00FF){
				case 0x0007:
					{
					// Sets VX to the value of the delay timer. 
					unsigned int X = opcode >> 8 & 0xF;

					// Debug info.
					snprintf(mnemonic, sizeof(mnemonic), "TIME V%d, delay", X);
					break;
					}
				case 0x000A:
					{
					// A key press is awaited, and then stored in VX. 
					// (Blocking Operation. All instruction halted until 
					// next key event)  
					unsigned int X = opcode >> 8 & 0xF;

					// Debug info.
					snprintf(mnemonic, sizeof(mnemonic), "GKEY V%d", X);
					break;
					}
				case 0x0015:
					{
					// Sets the delay timer to VX.. 
					unsigned int X = opcode >> 8 & 0xF;
					// Debug info.
					snprintf(mnemonic, sizeof(mnemonic), "TIME delay, V%d", X);
					break;
					}
				case 0x0018:
					{
					// Sets the sound timer to VX.  
					unsigned int X = opcode >> 8 & 0xF;
					// Debug info.
					snprintf(mnemonic, sizeof(mnemonic), "SNDT V%d", X);
					break;
					}
				case 0x001e:
					{
					// Adds VX to I. VF is not affected. 
					unsigned int X = opcode >> 8 & 0xF;
					cpu->I += cpu->V[X]; 

					// Debug info.
					snprintf(mnemonic, sizeof(mnemonic), "MEMA V%d", X);
					break;
					}
				default:
					snprintf(mnemonic, sizeof(mnemonic), "UNK OPCODE");
					panic(cpu, windows);
			}
		break;
	}

	// Update pc
	cpu->pc += 2;

	// Update debug info
	mvwprintw(debug_w, 4, 1, "opcode: %04x Mnemonic: %s\n", opcode, mnemonic);
	mvwprintw(debug_w, 5, 1, "PC+2: %04x I: 0x%04x V0: 0x%02x V1: 0x%02x\
 V2: 0x%02x - Stack[%04x %04x %04x]\n", cpu->pc, cpu->I, cpu->V[0],\
 cpu->V[1], cpu->V[2], cpu->stack[0], cpu->stack[1], cpu->stack[2]);
}

void draw(struct CPU *cpu, WINDOW *gamewindow)
{
}

// #TODO: Send this to a helper file
bool push_stack(unsigned short value, struct CPU *cpu)
{
	if(cpu->sp >= sizeof(cpu->stack) - 1){
		return false;
	}
	cpu->stack[cpu->sp] = value;
	cpu->sp++;
	return true;
}

unsigned short pop_stack(struct CPU *cpu)
{
	if(cpu->sp == -1){
		return 0xffff;
	}
	unsigned short ret = cpu->stack[cpu->sp];
	cpu->sp--;
	return ret;
}

void end(struct CPU *cpu, WINDOW **windows)
{
	free(windows);
	endwin();
	free(cpu);
}
void panic(struct CPU *cpu, WINDOW **windows)
{
	printf("PANIC!");
	end(cpu, windows);
}
