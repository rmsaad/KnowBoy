#include <stdio.h>

#define SDL_MAIN_HANDLED
#include "logging.h"

#include <SDL.h>

// gameboyLib
#include "gb_cpu.h"
#include "gb_mbc.h"
#include "gb_memory.h"
#include "gb_papu.h"
#include "gb_ppu.h"

// ROMS
#include "KDL.gb.h"
#include "dmg_boot.bin.h"

extern const unsigned char Tetris_gb[];
SDL_Renderer *renderer;
SDL_Surface *surface;
SDL_Texture *texture;

uint8_t dir_input = 0;
uint8_t but_input = 0;
uint32_t framebuffer[GAMEBOY_SCREEN_HEIGHT * GAMEBOY_SCREEN_WIDTH] = {0};

static void update_input(SDL_Window *window, SDL_Renderer *renderer, SDL_Texture *texture,
			 int *window_width, int *window_height)
{
	SDL_Event event;
	while (SDL_PollEvent(&event)) {
		switch (event.type) {
		case SDL_KEYDOWN: {
			switch (event.key.keysym.sym) {

			case SDLK_RIGHT:
				dir_input |= (0x1 << 0);
				break;
			case SDLK_LEFT:
				dir_input |= (0x1 << 1);
				break;
			case SDLK_UP:
				dir_input |= (0x1 << 2);
				break;
			case SDLK_DOWN:
				dir_input |= (0x1 << 3);
				break;

			case SDLK_a:
				but_input |= (0x1 << 0);
				break;
			case SDLK_s:
				but_input |= (0x1 << 1);
				break;
			case SDLK_SPACE:
				but_input |= (0x1 << 2);
				break;
			case SDLK_RETURN:
				but_input |= (0x1 << 3);
				break;
			}
			break;
		}
		case SDL_KEYUP: {
			switch (event.key.keysym.sym) {

			case SDLK_RIGHT:
				dir_input &= ~(0x1 << 0);
				break;
			case SDLK_LEFT:
				dir_input &= ~(0x1 << 1);
				break;
			case SDLK_UP:
				dir_input &= ~(0x1 << 2);
				break;
			case SDLK_DOWN:
				dir_input &= ~(0x1 << 3);
				break;

			case SDLK_a:
				but_input &= ~(0x1 << 0);
				break;
			case SDLK_s:
				but_input &= ~(0x1 << 1);
				break;
			case SDLK_SPACE:
				but_input &= ~(0x1 << 2);
				break;
			case SDLK_RETURN:
				but_input &= ~(0x1 << 3);
				break;
			}
			break;
		}

		case SDL_QUIT:
			exit(0);
			break;

		case SDL_WINDOWEVENT:
			if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
				SDL_GetWindowSize(window, window_width, window_height);
				break;
			}
		}
	}
}

void copy_frame_buffer(uint32_t *buffer)
{
	memcpy(framebuffer, buffer, GAMEBOY_SCREEN_HEIGHT * GAMEBOY_SCREEN_WIDTH * 4);
}

uint8_t prvControlsJoypad(uint8_t *ucJoypadSELdir, uint8_t *ucJoypadSELbut)
{
	uint8_t mask = 0;
	// poll keys then set mask

	if (*ucJoypadSELdir == 0x10) {
		mask = but_input;
	} else if (*ucJoypadSELbut == 0x20) {
		mask = dir_input;
	}

	return 0xC0 | (0xF ^ mask) | (*ucJoypadSELbut | *ucJoypadSELdir);
}

void audio_init(void)
{
	SDL_setenv("SDL_AUDIODRIVER", "directsound", 1);
	SDL_Init(SDL_INIT_AUDIO);

	SDL_AudioSpec AudioSettings = {0};

	AudioSettings.freq = 44100;
	AudioSettings.format = AUDIO_S16SYS;
	AudioSettings.channels = 2;
	AudioSettings.samples = 739;

	SDL_OpenAudio(&AudioSettings, 0);

	SDL_PauseAudio(0);
}

void display_fps(SDL_Window *window)
{
	static uint32_t frameCount = 0;
	static uint32_t lastTime = 0;
	static uint32_t fps = 0;
	static char title[100];

	frameCount++;
	uint32_t currentTime = SDL_GetTicks();
	if (currentTime > lastTime + 1000) {
		fps = frameCount;
		frameCount = 0;
		lastTime = currentTime;

		// Update window title with FPS
		snprintf(title, 100, "FPS: %d", fps);
		SDL_SetWindowTitle(window, title);
	}
}

int main(void)
{
	/* Initialize SDL */
	SDL_Init(SDL_INIT_VIDEO);
	SDL_Window *window;

	int window_width = GAMEBOY_SCREEN_WIDTH * 3;
	int window_height = GAMEBOY_SCREEN_HEIGHT * 3;
	float aspect_ratio = (float)GAMEBOY_SCREEN_WIDTH / (float)GAMEBOY_SCREEN_HEIGHT;
	int new_width, new_height;

	window = SDL_CreateWindow("Knowboy", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
				  window_width, window_height, SDL_WINDOW_RESIZABLE);

	renderer = SDL_CreateRenderer(window, -1,
				      SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

	surface = SDL_CreateRGBSurface(0, GAMEBOY_SCREEN_WIDTH, GAMEBOY_SCREEN_HEIGHT, 32,
				       0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000);

	texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING,
				    GAMEBOY_SCREEN_WIDTH, GAMEBOY_SCREEN_HEIGHT);

	SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
	SDL_RenderClear(renderer);
	audio_init();

	if (!SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear")) {
		printf("Warning: Linear texture filtering not enabled!");
	}

	/* initialize emulator */
	gb_memory_set_rom(KDL_gb);
	gb_memory_load(gb_memory_get_rom_pointer(), 32768);
	gb_memory_load(dmg_boot_bin, 256);
	gb_memory_init();

	gb_memory_set_control_function(prvControlsJoypad);
	gb_ppu_init();
	gb_ppu_set_display_frame_buffer(copy_frame_buffer);

	int Tstates = 0;
	while (1) {

		display_fps(window);
		update_input(window, renderer, texture, &window_width, &window_height);
		audio_buf_t buf;
		buf.buffer = NULL;
		buf.len = NULL;

		while (Tstates < 70224) {
			gb_cpu_step();
			gb_ppu_step();
			buf = gb_papu_step();
			Tstates += 4;
		}

		Tstates -= 70224;
		SDL_UpdateTexture(texture, NULL, framebuffer, surface->pitch);

		SDL_Rect srcRect = {0, 0, GAMEBOY_SCREEN_WIDTH, GAMEBOY_SCREEN_HEIGHT};

		SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255); // Black color
		SDL_RenderClear(renderer);
		if (window_width / aspect_ratio <= window_height) {
			// Window is taller than the image
			new_width = window_width;
			new_height = (int)(window_width / aspect_ratio);
		} else {
			// Window is wider than the image
			new_height = window_height;
			new_width = (int)(window_height * aspect_ratio);
		}

		SDL_Rect dstRect = {(window_width - new_width) / 2,   // Center horizontally
				    (window_height - new_height) / 2, // Center vertically
				    new_width, new_height};

		SDL_RenderCopy(renderer, texture, &srcRect, &dstRect);
		SDL_RenderPresent(renderer);

		SDL_QueueAudio(1, buf.buffer, (*buf.len) * 2);
		*buf.len = 0;
	}

	SDL_FreeSurface(surface);
	SDL_DestroyWindow(window);
	SDL_DestroyRenderer(renderer);
	SDL_Quit();

	return 0;
}
