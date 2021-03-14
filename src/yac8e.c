// TODOS
// - Decouple timers from the clock rate
// - Create multithreaded input reader (ncurses not working properly)
// - (OPTIONAL) Reset command
// - (OPTIONAL) Control refresh rate via command
//
#include <ncurses.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <time.h>
#include <pthread.h>

// A struct representing the CPU. Will be separated later
typedef struct { 
	unsigned char memory[4096];		// memory
	unsigned char V[16];			// registers
	unsigned short stack[16];		// call stack
	unsigned short gfx[64 * 32];	// frame buffer (64x32 pixels)
	unsigned char input[16];		// keyboard inputs
	unsigned short I;				// index registers
	unsigned short pc;				// program counter
	unsigned char delay_timer;		// delay timer
	unsigned char sound_timer;		// sound timer
	int sp;							// stack pointer
	bool draw;						// draw flag
	bool key_is_pressed;			// self explanatory
} CPU;

WINDOW *create_newwin(int width, int height, int starty, int startx);
void initGraphics(int DEBUG);
void initFonts();
CPU *new_cpu();
void createWindows();
void tick(int DEBUG);
void draw();
void end();
void panic();
void *updateKeys(void *cpu);
bool push_stack(unsigned short value, CPU *cpu);
unsigned short pop_stack(CPU *cpu);



// Can I escape globals?
CPU *chip8;
WINDOW **windows;

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
	chip8 = new_cpu();
	chip8->pc = 0x200;

	// Initialize internal fonts
	initFonts();	

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

	// Initialize ncurses interface 
	initGraphics(DEBUG);

	// Create windows
	createWindows();

	// Run game loop 
	int ticks = 0;

	// Create the keyboard listening thread
	pthread_t keythread;
	int pt = pthread_create(&keythread, NULL, updateKeys, (void *)chip8);
	if(pt) {
		perror("Failed creating keyboard thread\n");
		exit(-1);
	}

	while(1){
		WINDOW *debug_w = windows[0]; 
		if(DEBUG){
			// Erase debug window in preparation for tick data
			werase(debug_w);
		}

		// Run a tick
		tick(DEBUG);
		
		// Draw game window (if necessary)
		if(chip8->draw){
			chip8->draw = false;
			draw();
		}
			
		// Draw debug information (if necessary)
		if(DEBUG){
			ticks++;
			mvwprintw(debug_w, 1, 1, 
					"Window size: %d x %d - ROM Filename: %s", 
					COLS, LINES, filename);
			mvwprintw(debug_w, 3, 1, "Ticks: %d", ticks);
			box(debug_w, 0, 0);
			wrefresh(debug_w);
		}

	}
	
	// Destroy graphic interface
	end();
}
// Creates a new window with a frame border
WINDOW *create_newwin(int width, int height, int starty, int startx)
{
	WINDOW *local_win;
	local_win = newwin(height, width, starty, startx);
	return local_win;
}

// Updates graphical interface
void createWindows()
{
	int startx, starty, 
		d_width, d_height, 
		g_width, g_height;

	// Debug info window config
	d_height 	= 7;
	d_width 	= COLS - 2;

	// Game window config
	g_height 	= 32;
	g_width 	= 64;

	// Create the debug info window
	windows[0] = create_newwin(d_width, d_height, 0, 0);

	// Creates the game window
	startx = (COLS - g_width) / 2;
	starty = d_height + 1;
	windows[1] = create_newwin(g_width, g_height, starty, startx);
}

// Initializes ncurses
void initGraphics(int DEBUG)
{
	initscr();
	//cbreak(); 				// Line buffering disabled
	keypad(stdscr, TRUE);   // Enable Function keys (ex: F1)
	noecho(); 				// Do not display the users keypresses
	//nodelay(stdscr, TRUE);  // Non-blocking getch
	halfdelay(1);
	curs_set(0); 			// Set cursor invisible
	windows = malloc(sizeof(WINDOW)*2); // Allocate Windows memory
}

CPU *new_cpu()
{
	CPU *cpu = malloc(sizeof(CPU));
	assert(cpu != NULL);
	// Zero registers
	cpu->sp = -1;
	cpu->pc = 0x0;
	cpu->I 	= 0x0;
	// Zero memory
	memset(&cpu->memory, 0x0, 4096);
	memset(&cpu->V, 0x0, 16);
	memset(&cpu->stack, 0x0, 16*sizeof(short));
	memset(&cpu->gfx, 0x0, 64*32*sizeof(short));
	// Zero timers
	cpu->delay_timer = 0;
	cpu->sound_timer = 0;
	
	return cpu;
}

void tick(int DEBUG)
{
	// Used for tick timer
	clock_t start = clock(), diff;

	WINDOW *debug_w = windows[0];
	unsigned short opcode;

	// Update opcode
	opcode = chip8->memory[chip8->pc] << 8 | chip8->memory[chip8->pc + 1];

	// Decode opcode
	char mnemonic[128];

	switch(opcode & 0xF000){
		case 0x0000:
			switch(opcode & 0x00FF){
				case 0x00E0:
					{
					// Clears the screen.
					memset(&chip8->gfx, 0x00, sizeof(chip8->gfx));
					chip8->draw = true;
					chip8->pc += 2;

					// Debug info.
					snprintf(mnemonic, sizeof(mnemonic), "CLR");
					break;
					}
				case 0x00EE:
					{
					// Returns from a subroutine. 
					unsigned short ret_addr = pop_stack(chip8);
					assert(ret_addr != 0xffff);
					chip8->pc = ret_addr;

					// Debug info.
					snprintf(mnemonic, sizeof(mnemonic), "RET");
					break;
					}
				default:
					{
					// 0x0000
					// #TODO
					printf("pc: %04x opcode: 0x%04x", chip8->pc, opcode);
					endwin();
					exit(-1);
					/*
 					unsigned short NNN = opcode & 0x0FFF;
					snprintf(mnemonic, sizeof(mnemonic), "CALL 0x%03x", NNN);
					break;
					*/
					}
			};
			break;
		case 0x1000:
			{
			// Jumps to address NNN.
 			unsigned short NNN = opcode & 0x0FFF;
			chip8->pc = NNN;

			// Debug info
			snprintf(mnemonic, sizeof(mnemonic), "JMP 0x%03x", NNN);
			break;
			}
		case 0x2000:
			{
			// Calls subroutine at NNN.
			unsigned short NNN = opcode & 0x0FFF;
			bool s = push_stack(chip8->pc+2, chip8);
			assert(s != false);

			chip8->pc = NNN;
			
			// Debug info.
			snprintf(mnemonic, sizeof(mnemonic), "CALL 0x%03x", NNN);
			break;
			}
		case 0x3000:
			{
			// Skips the next instruction if VX equals NN. 
			// (Usually the next instruction is a jump to skip a code block) 
			unsigned int X = opcode >> 8 & 0xF;
			unsigned short NN = opcode & 0x00FF;
			if(chip8->V[X] == NN){
				chip8->pc += 4;
			} else {
				chip8->pc += 2;
			}

			// Debug info.
			snprintf(mnemonic, sizeof(mnemonic), "SEQ V%d, 0x%02x", X, NN);
			break;
			}
		case 0x4000:
			{
			// Skips the next instruction if VX doesn't equal NN. (Usually the 
			// next instruction is a jump to skip a code block) 
			unsigned int X = opcode >> 8 & 0xF;
			unsigned short NN = opcode & 0x00FF;
			if(chip8->V[X] != NN){
				chip8->pc += 4;
			} else{
				chip8->pc += 2;
			}

			// Debug info.
			snprintf(mnemonic, sizeof(mnemonic), "SNEQ V%d, 0x%02x", X, NN);
			break;
			}
		case 0x5000:
			{
			// Skips the next instruction if VX equals VY. (Usually the next 
			// instruction is a jump to skip a code block) 
			unsigned int X = opcode >> 8 & 0xF;
			unsigned int Y = opcode >> 4 & 0xF;
			if(chip8->V[X] == chip8->V[Y]){
				chip8->pc += 4;
			} else {
				chip8->pc += 2;
			}
			
			// Debug info.
			snprintf(mnemonic, sizeof(mnemonic), "SEQ V%d, V%d", X, Y);
			break;
			}
		case 0x6000:
			{
			// Sets VX to NN. 
			unsigned int X = opcode >> 8 & 0xF;
			unsigned short NN = opcode & 0x00FF;

			chip8->V[X] = NN;
			chip8->pc += 2;

			// Debug info
			snprintf(mnemonic, sizeof(mnemonic), "STR 0x%02x, V%d", NN, X);
			break;
			}
		case 0x7000:
			{
			// Adds NN to VX. (Carry flag is not changed) 
			unsigned int X = opcode >> 8 & 0xF;
			unsigned short NN = opcode & 0x00FF;

			chip8->V[X] += NN;
			chip8->pc += 2;

			// Debug info.
			snprintf(mnemonic, sizeof(mnemonic), "ADD V%d, 0x%02x", X, NN);
			break;
			}
		case 0x8000:
			switch(opcode & 0x000F){
				case 0x0:
					{
					// Sets VX to the value of VY. 
					unsigned int X = opcode >> 8 & 0xF;
					unsigned int Y = opcode >> 4 & 0xF;
					chip8->V[X] = chip8->V[Y];
					chip8->pc += 2;

					// Debug info.
					snprintf(mnemonic, sizeof(mnemonic), "STR V%x, V%x", Y, X);
					break;
					}
				case 0x1:
					{
					// Sets VX to VX OR VY. (Bitwise OR operation)
					unsigned int X = opcode >> 8 & 0xF;
					unsigned int Y = opcode >> 4 & 0xF;
					chip8->V[X] |= chip8->V[Y];
					chip8->pc += 2;

					// Debug info.
					snprintf(mnemonic, sizeof(mnemonic), "OR V%d, V%d", X, Y);
					break;
					}
				case 0x2:
					{
					// Sets VX to VX AND VY. (Bitwise AND operation) 
					unsigned int X = opcode >> 8 & 0xF;
					unsigned int Y = opcode >> 4 & 0xF;
					chip8->V[X] &= chip8->V[Y];
					chip8->pc += 2;

					// Debug info.
					snprintf(mnemonic, sizeof(mnemonic), "AND V%d, V%d", X, Y);
					break;
					}
				case 0x3:
					{
					// Sets VX to VX XOR VY. 
					unsigned int X = opcode >> 8 & 0xF;
					unsigned int Y = opcode >> 4 & 0xF;
					chip8->V[X] ^= chip8->V[Y];
					chip8->pc += 2;

					// Debug info.
					snprintf(mnemonic, sizeof(mnemonic), "XOR V%d, V%d", X, Y);
					break;
					}
				case 0x4:
					{
					// Adds VY to VX. VF is set to 1 when there's a carry, and 
					// to 0 when there isn't. 
					unsigned int X = opcode >> 8 & 0xF;
					unsigned int Y = opcode >> 4 & 0xF;
					// Check overflow condition
					if(chip8->V[X] > 0 && chip8->V[Y] > (0xFF - chip8->V[X])){
						chip8->V[0xF] = 1;
					}
					else {
						chip8->V[0xF] = 0;
					}
					chip8->V[X] += chip8->V[Y];
					chip8->pc += 2;

					// Debug info.
					snprintf(mnemonic, sizeof(mnemonic), "ADD V%d, V%d", X, Y);
					break;
					}
				case 0x5:
					{
					// VY is subtracted from VX. VF is set to 0 when there's a 
					// borrow, and 1 when there isn't. 
					unsigned int X = opcode >> 8 & 0xF;
					unsigned int Y = opcode >> 4 & 0xF;
					// Check overflow condition
					if(chip8->V[X] < chip8->V[Y]){
						chip8->V[0xF] = 0;
					}
					else {
						chip8->V[0xF] = 1;
					}
					chip8->V[X] -= chip8->V[Y];
					chip8->pc += 2;

					// Debug info.
					snprintf(mnemonic, sizeof(mnemonic), "SUB V%d, V%d", X, Y);
					break;
					}
				case 0x6:
					{
					// Stores the least significant bit of VX in VF and then 
					// shifts VX to the right by 1.
					unsigned int X = opcode >> 8 & 0xF;
					chip8->V[0xF] = chip8->V[X] & 0x1;
					chip8->V[X] >>= 1;
					chip8->pc += 2;

					// Debug info.
					snprintf(mnemonic, sizeof(mnemonic), "SHR V%d, 1", X);
					break;
					}
				case 0x7:
					{
					// Sets VX to VY minus VX. VF is set to 0 when there's a 
					// borrow, and 1 when there isn't. 
					unsigned int X = opcode >> 8 & 0xF;
					unsigned int Y = opcode >> 4 & 0xF;
					if(chip8->V[X] > chip8->V[Y]){
						chip8->V[0xF] = 0;
					} else {
						chip8->V[0xF] = 1;
					}
					chip8->V[X] = chip8->V[Y] - chip8->V[X];
					chip8->pc += 2;

					// Debug info.
					snprintf(mnemonic, sizeof(mnemonic), "SUBI V%d, V%d", X, Y);
					break;
					}
				case 0xE:
					{
					// Stores the most significant bit of VX in VF and then 
					// shifts VX to the left by 1.
					unsigned int X = opcode >> 8 & 0xF;
					chip8->V[0xF] = chip8->V[X] >> 0x7;
					chip8->V[X] <<= 1;
					chip8->pc += 2;

					// Debug info.
					snprintf(mnemonic, sizeof(mnemonic), "SHL V%d, 1", X);
					break;
					}
				default:
					snprintf(mnemonic, sizeof(mnemonic), "UNK OPCODE");
					printf("panic! opcode: 0x%04x\n", opcode);
					panic();
			};
			break;
		case 0x9000:
			{
			// Skips the next instruction if VX doesn't equal VY. (Usually the 
			// next instruction is a jump to skip a code block) 
			unsigned int X = opcode >> 8 & 0xF;
			unsigned int Y = opcode >> 4 & 0xF;
			if(chip8->V[X] != chip8->V[Y]){
				chip8->pc += 4;
			} else {
				chip8->pc += 2;
			}

			// Debug info.
			snprintf(mnemonic, sizeof(mnemonic), "SNEQ V%d, V%d", X, Y);
			break;
			}
		case 0xa000:
			{
			// Sets I to the address NNN.
			unsigned short NNN = opcode & 0x0FFF;
			chip8->I = NNN;
			chip8->pc += 2;

			// Debug info
			snprintf(mnemonic, sizeof(mnemonic), "MSTR 0x%03x", NNN);
			break;
			}
		case 0xb000:
			{
			// Jumps to the address NNN plus V0. 
			unsigned short NNN = opcode & 0x0FFF;
			chip8->pc = chip8->V[0] + NNN;

			// Debug info.
			snprintf(mnemonic, sizeof(mnemonic), "JMPA V0, 0x%03x", NNN);
			break;
			}
		case 0xc000:
			{
		  	// Sets VX to the result of a bitwise and operation on a random 
		  	// number (Typically: 0 to 255) and NN. 
			unsigned int X = opcode >> 8 & 0x0F;	
			unsigned short NN = opcode & 0xFF;
			unsigned int r = rand() % 0xFF;
			
			chip8->V[X] = r & NN;
			chip8->pc += 2;
			
			// Debug info.
			snprintf(mnemonic, sizeof(mnemonic), "RAND 0x%02x", NN);
			break;
			}
		case 0xd000:
			{
			// Draws a sprite at coordinate (VX, VY) that has a width of 8 
			// pixels and a height of N+1 pixels. Each row of 8 pixels is read 
			// as bit-coded starting from memory location I; I value doesn’t 
			// change after the execution of this instruction. As described 
			// above, VF is set to 1 if any screen pixels are flipped from set 
			// to unset when the sprite is drawn, and to 0 if that doesn’t 
			// happen 
			unsigned short x = chip8->V[opcode >> 8 & 0xF]; 
			unsigned short y = chip8->V[opcode >> 4 & 0xF]; 
			unsigned short N = opcode & 0xF;
			unsigned short pixel_line;

			chip8->V[0xF] = 0;
			for(int ydepth = 0; ydepth < N; ydepth++){
				pixel_line = chip8->memory[chip8->I + ydepth];
				for(int xline = 0; xline < 8; xline++){
					if((pixel_line & (0x80 >> xline)) != 0){
						if(chip8->gfx[(x + xline + ((y + ydepth) * 64))] == 1){
							chip8->V[0xF] = 1;
						}
						chip8->gfx[x + xline + ((y + ydepth) * 64)] ^= 1;
					}
				}
			}
			chip8->draw = true;
			chip8->pc += 2;
			
			// Debug info.
			snprintf(mnemonic, sizeof(mnemonic), "DRAW");
			break;
			}
		case 0xe000:
			switch(opcode & 0xFF){
				case 0x9E:
					{
					// Skips the next instruction if the key stored in VX is 
					// pressed. (Usually the next instruction is a jump to skip
					// a code block) 
					unsigned int X = opcode >> 8 & 0xF;
					if(chip8->input[chip8->V[X]] != 0x0){
						chip8->pc += 4;
					} else {
						chip8->pc += 2;
					}

					// Debug info.
					snprintf(mnemonic, sizeof(mnemonic), "SKP V%d", X);
					break;
					}
				case 0xA1:
					{
					// Skips the next instruction if the key stored in VX isn't
					// pressed. (Usually the next instruction is a jump to skip
					// a code block) 
					unsigned int X = opcode >> 8 & 0xF;
					if(chip8->input[chip8->V[X]] == 0x0){
						chip8->pc += 4;
					} else {
						chip8->pc += 2;
					}

					// Debug info.
					snprintf(mnemonic, sizeof(mnemonic), "SKNP V%d", X);
					break;
					}
				default:
					snprintf(mnemonic, sizeof(mnemonic), "UNK OPCODE");
					printf("panic! opcode: 0x%04x\n", opcode);
					panic();
			}
			break;
		case 0xf000:
			switch(opcode & 0x00FF){
				case 0x0007:
					{
					// Sets VX to the value of the delay timer. 
					unsigned int X = opcode >> 8 & 0xF;
					chip8->V[X] = chip8->delay_timer;
					chip8->pc += 2;

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
					while(chip8->key_is_pressed == false){
					}
					for(int k = 0; k < 16; k++){
						if(chip8->input[k] != 0x0){
							chip8->V[X] = k;
						}
					}
					chip8->pc+= 2;
					
					// Debug info.
					snprintf(mnemonic, sizeof(mnemonic), "LD V%d, K", X);
					break;
					}
				case 0x0015:
					{
					// Sets the delay timer to VX.. 
					unsigned int X = opcode >> 8 & 0xF;
					chip8->delay_timer = chip8->V[X];
					chip8->pc += 2;

					// Debug info.
					snprintf(mnemonic, sizeof(mnemonic), "TIME delay, V%d", X);
					break;
					}
				case 0x0018:
					{
					// Sets the sound timer to VX.  
					unsigned int X = opcode >> 8 & 0xF;
					chip8->sound_timer = chip8->V[X];
					chip8->pc += 2;

					// Debug info.
					snprintf(mnemonic, sizeof(mnemonic), "SNDT V%d", X);
					break;
					}
				case 0x001e:
					{
					// Adds VX to I. VF is not affected. 
					unsigned int X = opcode >> 8 & 0xF;
					chip8->I += chip8->V[X]; 
					chip8->pc += 2;

					// Debug info.
					snprintf(mnemonic, sizeof(mnemonic), "MEMA V%d", X);
					break;
					}
				case 0x0029:
					{
					// Sets I to the location of the sprite for the character
					// in VX. Characters 0-F (in hexadecimal) are represented 
					// by a 4x5 font. 
					unsigned int X = opcode >> 8 & 0xF;
					chip8->I = chip8->V[X] << 4;
					chip8->pc += 2;

					// Debug info.
					snprintf(mnemonic, sizeof(mnemonic), "CHAR V%d", X);
					break;
					}
				case 0x0033:
					{
					// Stores the binary-coded decimal representation of VX, 
					// with the most significant of three digits at the address 
					// in I, the middle digit at I plus 1, and the least 
					// significant digit at I plus 2. (In other words, take the
					// decimal representation of VX, place the hundreds digit 
					// in memory at location in I, the tens digit at location 
					// I+1, and the ones digit at location I+2.)
					unsigned int X = opcode >> 8 & 0x0F;
					chip8->memory[chip8->I]	 = chip8->V[X] / 100;
					chip8->memory[chip8->I+1] = (chip8->V[X] % 100) / 10;
					chip8->memory[chip8->I+2] = chip8->V[X] % 10;
					chip8->pc += 2;

					// Debug info.
					snprintf(mnemonic, sizeof(mnemonic), "BCD V%d", X);
					break;

					};
				case 0x0055:
					{
					// Stores V0 to VX (including VX) in memory starting at 
					// address I. The offset from I is increased by 1 for each 
					// value written, but I itself is left unmodified.
					unsigned int X = opcode >> 8 & 0x0F;
					for(int i = 0; i <= X; i++){
						chip8->memory[chip8->I + i] = chip8->V[i];
					}
					chip8->pc += 2;

					// Debug info.
					snprintf(mnemonic, sizeof(mnemonic), "REGD V0-V%d", X);
					break;

					}
				case 0x0065:
					{
					// Fills V0 to VX (including VX) with values from memory 
					// starting at address I. The offset from I is increased by
					// 1 for each value written, but I itself is left 
					// unmodified.
					unsigned int X = opcode >> 8 & 0xF;
					for(int i = 0; i <= X; i++){
						chip8->V[i] = chip8->memory[chip8->I + i];
					}

					chip8->pc += 2;

					// Debug info.
					snprintf(mnemonic, sizeof(mnemonic), "LDR V0-V%d", X);
					break;
					}

				default:
					snprintf(mnemonic, sizeof(mnemonic), "UNK OPCODE");
					printf("panic! opcode: 0x%04x\n", opcode);
					panic();
			}
			break;
		}	

	if(DEBUG){
		// Update debug info
		mvwprintw(debug_w, 4, 1, "opcode: %04x Mnemonic: %s", opcode, mnemonic);
		mvwprintw(debug_w, 5, 1, "PC+2: %04x I: 0x%04x V0: 0x%02x V1: 0x%02x\
	 V2: 0x%02x - Stack[%04x %04x %04x] - Inputs: 0:%d 1:%d 2:%d 3:%d 4:%d 5:%d\
 6:%d 7:%d 8:%d 9:%d A:%d B:%d C:%d D:%d E:%d F:%d - Key pressed: %d\n", \
	chip8->pc, chip8->I,\
	 chip8->V[0], chip8->V[1], chip8->V[2], chip8->stack[0], chip8->stack[1],\
	 chip8->stack[2], chip8->input[0], chip8->input[1], chip8->input[2],\
	 chip8->input[3], chip8->input[4], chip8->input[5], chip8->input[6],\
	 chip8->stack[7], chip8->input[8], chip8->input[9], chip8->input[0xa],\
	 chip8->input[0xb], chip8->input[0xc], chip8->input[0xd], chip8->input[0xe],\
	 chip8->input[0xf], chip8->key_is_pressed);
	}

	// Decrement timers
	if(chip8->delay_timer > 0){ 
		chip8->delay_timer--;
	}
	if(chip8->sound_timer > 0) {
		chip8->sound_timer--;
		mvwprintw(debug_w, 2, 1, "BEEP!");
	}

	// Each tick should take (at least) 1/60s
	// 16ms per tick seemed too slow... decreased it to 1.6ms
	bool tick_end = false;
	while(tick_end == false){
		diff = clock() - start;
		float msec = diff * 1000 / CLOCKS_PER_SEC;
		if(msec >= 1.67) tick_end = true;
	}	


}

void draw()
{
	WINDOW *game_w = windows[1];
	werase(game_w);
	for(int i = 0; i < 32*64; i++){
		if(chip8->gfx[i] == 1){
			if(chip8->sound_timer > 0){
				wprintw(game_w, " ");
			} else {
				waddch(game_w, ACS_CKBOARD);
			}
		} else {
			if(chip8->sound_timer > 0){
				waddch(game_w, ACS_CKBOARD);
			} else {
				wprintw(game_w, " ");
			}
		}
	}
	//box(game_w, 0, 0);
	wrefresh(game_w);
}

bool push_stack(unsigned short value, CPU *cpu)
{
	if(cpu->sp >= 0xFF){
		return false;
	}
	cpu->sp++;
	cpu->stack[cpu->sp] = value;
	return true;
}

unsigned short pop_stack(CPU *cpu)
{
	if(cpu->sp == -1){
		return 0xffff;
	}
	unsigned short ret = cpu->stack[cpu->sp];
	cpu->sp--;
	return ret;
}

void end()
{
	endwin();
	pthread_exit(NULL);
	free(windows);
	free(chip8);
	exit(0);
}

void panic()
{
	printf("PANIC! PC: %04x", chip8->pc);
	end();
}

void *updateKeys(void* cpu){
	CPU *chip8;
	chip8 = (CPU *)cpu;
	while(1){
		int key = getch();
		switch(key){
			case KEY_F(1): // F1 pressed. Close program
				end();
			case 49:
				chip8->input[0x1] = 0x1;
				chip8->key_is_pressed = true;
				break;
			case 50:
				chip8->input[0x2] = 0x1;
				chip8->key_is_pressed = true;
				break;
			case 51:
				chip8->input[0x3] = 0x1;
				chip8->key_is_pressed = true;
				break;
			case 52:
				chip8->input[0xC] = 0x1;
				chip8->key_is_pressed = true;
				break;
			case 113:
				chip8->input[0x4] = 0x1;
				chip8->key_is_pressed = true;
				break;
			case 119:
				chip8->input[0x5] = 0x1;
				chip8->key_is_pressed = true;
				break;
			case 101:
				chip8->input[0x6] = 0x1;
				chip8->key_is_pressed = true;
				break;
			case 114:
				chip8->input[0xD] = 0x1;
				chip8->key_is_pressed = true;
				break;
			case 97:
				chip8->input[0x7] = 0x1;
				chip8->key_is_pressed = true;
				break;
			case 115:
				chip8->input[0x8] = 0x1;
				chip8->key_is_pressed = true;
				break;
			case 100:
				chip8->input[0x9] = 0x1;
				chip8->key_is_pressed = true;
				break;
			case 102:
				chip8->input[0xE] = 0x1;
				chip8->key_is_pressed = true;
				break;
			case 122:
				chip8->input[0xA] = 0x1;
				chip8->key_is_pressed = true;
				break;
			case 120:
				chip8->input[0x0] = 0x1;
				chip8->key_is_pressed = true;
				break;
			case 99:
				chip8->input[0xB] = 0x1;
				chip8->key_is_pressed = true;
				break;
			case 118:
				chip8->input[0xF] = 0x1;
				chip8->key_is_pressed = true;
				break;
			default:
			case ERR:
				chip8->key_is_pressed = false;
				for(int k = 0; k < 16; k++){
					chip8->input[k] = 0;
				}
				break;
		}
	}
}

void initFonts()
{
	// Initializes the emulator's default fonts (0-F)
	// located at addresses 0x0000 to 0x00F5
	char characters[] = { 0xF0,0x90,0x90,0x90,0xF0,
						0x20,0x60,0x20,0x20,0x70,
						0xF0,0x10,0xF0,0x80,0xF0, 
						0xF0,0x10,0xF0,0x10,0xF0, 
						0x90,0x90,0xF0,0x10,0x10, 
						0xF0,0x80,0xF0,0x10,0xF0, 
						0xF0,0x80,0xF0,0x90,0xF0, 
						0xF0,0x10,0x20,0x40,0x40, 
						0xF0,0x90,0xF0,0x90,0xF0, 
						0xF0,0x90,0xF0,0x10,0xF0, 
						0xF0,0x90,0xF0,0x90,0x90, 
						0xE0,0x90,0xE0,0x90,0xE0, 
						0xF0,0x80,0x80,0x80,0xF0, 
						0xE0,0x90,0x90,0x90,0xE0, 
						0xF0,0x80,0xF0,0x80,0xF0, 
						0xF0,0x80,0xF0,0x80,0x80};

	for(int i = 0; i <= 0xF; i++){
		memcpy(&chip8->memory[i << 4], &characters[i*5], 5);
	};
}

