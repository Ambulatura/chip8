u8 memory[MEMORY_SIZE];
u16 program_counter;
u8 v[16];
u16 stack[STACK_SIZE];
u8 stack_pointer;
u16 index_register;
u8 sound_timer;
u8 delay_timer;
u8 random_number_index;
u8 keys[KEY_SIZE];
u8 display_buffer[DISPLAY_X * DISPLAY_Y];
u8 is_drawing;

static void Chip8DebugPrint(u16 opcode, u16 nnn, u8 n, u8 x, u8 y, u8 kk)
{
	printf("START================================================\n");
	printf("opcode = 0x%04X\nnnn = 0x%04X n = 0x%02X x = 0x%02X y = 0x%02X kk = 0x%02X\n\n", opcode, nnn, n, x, y, kk);
	printf("v0 = 0x%02X v4 = 0x%02X v8 = 0x%02X vC = 0x%02X\n" \
		   "v1 = 0x%02X v5 = 0x%02X v9 = 0x%02X vD = 0x%02X\n" \
		   "v2 = 0x%02X v6 = 0x%02X vA = 0x%02X vE = 0x%02X\n" \
		   "v3 = 0x%02X v7 = 0x%02X vB = 0x%02X vF = 0x%02X\n\n",
		   v[0x0], v[0x4], v[0x8], v[0xC],
		   v[0x1], v[0x5], v[0x9], v[0xD],
		   v[0x2], v[0x6], v[0xA], v[0xE],
		   v[0x3], v[0x7], v[0xB], v[0xF]);
	printf("Program Counter = 0x04%X Index Register = 0x04%X\n", program_counter, index_register);
	printf("END==================================================\n\n");
}

static void Chip8RandomSeed(u32 value)
{
	random_number_index = value % RANDOM_NUMBER_TABLE_SIZE;
}

static u8 Chip8RandomNext()
{
	u8 random_value = chip8_random_number_table[random_number_index++];
	
	if (random_number_index >= RANDOM_NUMBER_TABLE_SIZE) {
		random_number_index = 0;
	}

	return random_value;
}

static void Chip8Draw(u8 x, u8 y, u8 n)
{
	v[0xF] = 0x00;

	for (u8 byte_index = 0; byte_index < n; ++byte_index) {
		u8 byte = memory[index_register + byte_index];

		for (u8 bit_index = 0; bit_index < 8; ++bit_index) {
			u8 bit = byte & (0x80 >> bit_index);

			if (bit) {
				u8 position_x = (x + bit_index) % DISPLAY_X;
				u8 position_y = (y + byte_index) % DISPLAY_Y;
				u8* pixel = display_buffer + position_x + position_y * DISPLAY_X;

				if (*pixel == 0x01) {
					v[0xF] = 0x01;
				}

				*pixel = *pixel ^ 0x01;
			}
		}
	}
}

static void Chip8Initialize()
{
	program_counter = PROGRAM_START;
	stack_pointer = 0;
	index_register = 0;

	// for (u16 i = 0; i < FONT_BUFFER_SIZE; ++i) {
	// 	memory[i + FONT_MEMORY_OFFSET] = chip8_font[i];
	// }
	memcpy(memory + FONT_MEMORY_OFFSET, chip8_font, FONT_BUFFER_SIZE);
	
	Chip8RandomSeed(5555);
}

static void Chip8LoadProgram(char* program_file_name)
{
	FILE* program_file;
	
	fopen_s(&program_file, program_file_name, "rb");
	fread(memory + PROGRAM_START, MAX_PROGRAM_SIZE, 1, program_file);
	fclose(program_file);
}

static void Chip8FetchExecuteCycle()
{
	u16 opcode = memory[program_counter] << 8 | memory[program_counter + 1];
	program_counter += 2;
	
	u16 nnn = opcode & 0x0FFF;
	u8 n = opcode & 0x000F;
	u8 x = (opcode >> 8) & 0x000F;
	u8 y = (opcode >> 4) & 0x000F;
	u8 kk = opcode & 0x00FF;
	
	switch (opcode & 0xF000) {
		case 0x0000: {
			switch (kk) {
				case 0xE0: {
					memset(display_buffer, 0, DISPLAY_X * DISPLAY_Y);
				} break;
				
				case 0xEE: {
					program_counter = stack[stack_pointer--];
				} break;

				default : {
					// TODO: Invalid instruction.
				};
			}
		} break;

		case 0x1000: {
			program_counter = nnn;
		} break;

		case 0x2000: {
			stack[++stack_pointer] = program_counter;
			program_counter = nnn;
			
		} break;

		case 0x3000: {
			if (v[x] == kk) {
				program_counter += 2;
			}
		} break;

		case 0x4000: {
			if (v[x] != kk) {
				program_counter += 2;
			}
		} break;

		case 0x5000: {
			if (v[x] == v[y]) {
				program_counter += 2;
			}
		} break;

		case 0x6000: {
			v[x] = kk;
		} break;

		case 0x7000: {
			v[x] = v[x] + kk;
		} break;

		case 0x8000: {
			switch (n) {
				case 0x0: {
					v[x] = v[y];
				} break;
				
				case 0x1: {
					v[x] = v[x] | v[y];
				} break;

				case 0x2: {
					v[x] = v[x] & v[y];
				} break;

				case 0x3: {
					v[x] = v[x] ^ v[y];
				} break;

				case 0x4: {
					v[0xF] = (u16)v[x] + (u16)v[y] > 0xFF ? 0x01 : 0x00;
					v[x] = v[x] + v[y];
				} break;

				case 0x5: {
					v[0xF] = v[x] > v[y] ? 0x01 : 0x00;
					v[x] = v[x] - v[y];
				} break;

				case 0x6: {
					v[0xF] = v[x] & 0x01 ? 0x01 : 0x00;
					v[x] = v[x] >> 1;
				} break;

				case 0x7: {
					v[0xF] = v[y] > v[x] ? 0x01 : 0x00;
					v[x] = v[y] - v[x];
				} break;

				case 0xE: {
					v[0xF] = v[x] >> 7 ? 0x01 : 0x00;
					v[x] = v[x] << 1;
				} break;

				default: {
					// TODO: Invalid instruction.
				} break;
			}
		} break;

		case 0x9000: {
			if (v[x] != v[y]) {
				program_counter += 2;
			}
		} break;

		case 0xA000: {
			index_register = nnn;
		} break;

		case 0xB000: {
			program_counter = nnn + v[0];
		} break;

		case 0xC000: {
			v[x] = Chip8RandomNext() & kk;
		} break;

		case 0xD000: {
			Chip8Draw(v[x], v[y], n);
			is_drawing = 1;
		} break;

		case 0xE000: {
			switch (kk) {
				case 0x9E: {
					if (keys[v[x]]) {
						program_counter += 2;
					}
				} break;

				case 0xA1: {
					if (!keys[v[x]]) {
						program_counter += 2;
					}
				} break;

				default: {
					// TODO: Invalid instruction.
				} break;
			}
			
		} break;

		case 0xF000: {
			switch (kk) {
				case 0x07: {
					v[x] = delay_timer;
				} break;

				case 0x0A: {
					u8 key_pressed = 0;
					while (!key_pressed) {
						for (u8 i = 0; !key_pressed && (i < KEY_SIZE); ++i) {
							if (keys[i]) {
								v[x] = i;
								key_pressed = 1;
							}
						}
					}
				} break;

				case 0x15: {
					delay_timer = v[x];
				} break;

				case 0x18: {
					sound_timer = v[x];
				} break;

				case 0x1E: {
					index_register = index_register + v[x];
				} break;

				case 0x29: {
					index_register = FONT_MEMORY_OFFSET + FONT_CHAR_BYTE_WIDTH * v[x];
				} break;
				
				case 0x33: {
					memory[index_register + 0] = (v[x] % 1000) / 100;
					memory[index_register + 1] = (v[x] % 100) / 10;
					memory[index_register + 2] = v[x] % 10;
				} break;

				case 0x55: {
					for (u8 i = 0; i <= x; ++i) {
						memory[index_register + i] = v[i];
					}
				} break;

				case 0x65: {
					for (u8 i = 0; i <= x; ++i) {
						v[i] = memory[index_register + i];
					}
				} break;
			}
			
		} break;

		default: {
			// TODO: Invalid instruction.
		} break;
	}

#if CHIP8_DEBUG
	Chip8DebugPrint(opcode, nnn, n, x, y, kk);
#endif
}
