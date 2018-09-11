/**
 * MIT License
 * Copyright (c) 2018 Mahyar Koshkouei
 */

#include <errno.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL/SDL.h>
#include <SDL/SDL_main.h>

#include "../gameboy.h"

//#define ENABLE_SOUND 1

struct priv_t
{
	/* Pointer to allocated memory holding GB file. */
	uint8_t *rom;
	/* Pointer to allocated memory holding save file. */
	uint8_t *cart_ram;
};

/**
 * Returns a byte from the ROM file at the given address.
 */
uint8_t gb_rom_read(struct gb_t **gb, const uint32_t addr)
{
    const struct priv_t * const p = (*gb)->priv;
    return p->rom[addr];
}

/**
 * Returns a byte from the cartridge RAM at the given address.
 */
uint8_t gb_cart_ram_read(struct gb_t **gb, const uint32_t addr)
{
	const struct priv_t * const p = (*gb)->priv;
	return p->cart_ram[addr];
}

/**
 * Writes a given byte to the cartridge RAM at the given address.
 */
void gb_cart_ram_write(struct gb_t **gb, const uint32_t addr,
	const uint8_t val)
{
	const struct priv_t * const p = (*gb)->priv;
	p->cart_ram[addr] = val;
}

/**
 * Handles an error reported by the emulator. The emulator context may be used
 * to better understand why the error given in gb_err was reported.
 */
void gb_error(struct gb_t **p, const enum gb_error_e gb_err)
{
	struct gb_t *gb = *p;
	struct priv_t *priv = gb->priv;

	switch(gb_err)
	{
		case GB_INVALID_OPCODE:
			printf("Invalid opcode %#04x", __gb_read(&gb, gb->cpu_reg.pc));
			break;

		case GB_INVALID_WRITE:
		case GB_INVALID_READ:
			return;
			printf("Invalid write");
			break;

		default:
			printf("Unknown error");
			break;
	}

	printf(" at PC: %#06x, SP: %#06x\n", gb->cpu_reg.pc, gb->cpu_reg.sp);

	puts("Press q to exit, or any other key to continue.");
	if(getchar() == 'q')
	{
		free(priv->rom);
		free(priv->cart_ram);
		exit(EXIT_FAILURE);
	}

	return;
}

/**
 * Returns a pointer to the allocated space containing the ROM. Must be freed.
 */
uint8_t *read_rom_to_ram(const char *file_name)
{
	FILE *rom_file = fopen(file_name, "rb");
	long rom_size;
	uint8_t *rom = NULL;

	if(rom_file == NULL)
        return NULL;

	fseek(rom_file, 0, SEEK_END);
	rom_size = ftell(rom_file);
	rewind(rom_file);
	rom = malloc(rom_size);

	if(fread(rom, sizeof(uint8_t), rom_size, rom_file) != rom_size)
    {
		free(rom);
		fclose(rom_file);
		return NULL;
	}

	fclose(rom_file);
	return rom;
}

int main(int argc, char **argv)
{
    struct gb_t gb;
	struct priv_t priv;
	const unsigned int height = 144;
	const unsigned int width = 160;
	unsigned int running = 1;
	SDL_Surface* screen;
	uint32_t fb[height][width];

	/* Make sure a file name is given. */
	if(argc != 2)
	{
		printf("Usage: %s FILE\n", argv[0]);
		return EXIT_FAILURE;
	}

	/* Copy input ROM file to allocated memory. */
	if((priv.rom = read_rom_to_ram(argv[1])) == NULL)
	{
		printf("%s\n", strerror(errno));
		return EXIT_FAILURE;
	}

	/* TODO: Sanity check input GB file. */

    /* Initialise emulator context. */
    gb = gb_init(&gb_rom_read, &gb_cart_ram_read, &gb_cart_ram_write, &gb_error,
			&priv);

	/* TODO: Load Save File. */

	/* Allocate memory for save file if required. */
	priv.cart_ram = malloc(gb_get_save_size(&gb));
	memset(priv.cart_ram, 0xFF, gb_get_save_size(&gb));

	/* Initialise frontend implementation, in this case, SDL. */
	SDL_Init(SDL_INIT_VIDEO);
	screen = SDL_SetVideoMode(width, height, 32, SDL_HWSURFACE | SDL_DOUBLEBUF);
	SDL_WM_SetCaption("DMG Emulator", 0);

	while(running)
	{
		const uint32_t palette[4] = {
			0xFFFFFFFF, 0x99999999, 0x44444444, 0x00000000
		};
		uint32_t *screen_copy;
		SDL_Event event;

		/* TODO: Get joypad input. */
		while(SDL_PollEvent(&event))
		{
			if(event.type == SDL_QUIT)
				running = 0;
		}

		/* Execute CPU cycles until the screen has to be redrawn. */
		gb_run_frame(&gb);

		/* Copy frame buffer from emulator context, converting to colours
		 * defined in the palette. */
		for (unsigned int y = 0; y < height; y++)
		{
			for (unsigned int x = 0; x < width; x++)
				fb[y][x] = palette[gb.gb_fb[y][x] & 3];
		}

		/* Copy frame buffer to SDL screen. */
		SDL_LockSurface(screen);
		screen_copy = (uint32_t *) screen->pixels;
		for(unsigned int y = 0; y < height; y++)
		{
			for (unsigned int x = 0; x < width; x++)
				*(screen_copy + x) = fb[y][x];

			screen_copy += screen->pitch / 4;
		}
		SDL_UnlockSurface(screen);
		SDL_Flip(screen);

		/* Use a delay that will draw the screen at a rate of 59.73 Hz. */
		SDL_Delay(10);
	}

	SDL_Quit();
	free(priv.rom);
	free(priv.cart_ram);

	return EXIT_SUCCESS;
}