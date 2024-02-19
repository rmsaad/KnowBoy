#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SDL_MAIN_HANDLED
#include "logging.h"
#include "main.h"

#include <SDL.h>
#include <SDL_ttf.h>
#include <nfd.h>

// gameboyLib
#include "gb_cpu.h"
#include "gb_mbc.h"
#include "gb_memory.h"
#include "gb_papu.h"
#include "gb_ppu.h"

#define PAUSE_MENU_OPTIONS 2
#define MAIN_MENU_OPTIONS  3
#define MENU_FONT_SIZE	   24
#define MAX_LINE_LENGTH	   1024
#define CACHE_FILE	   "cache.txt"

const char *main_menu_texts[MAIN_MENU_OPTIONS] = {"Start Game", "Load Bootloader", "Load ROM"};
const char *pause_menu_texts[PAUSE_MENU_OPTIONS] = {"Resume Game", "Return to Menu"};
const char *FONT_PATH = "resources/Gameboy.ttf";
TTF_Font *font = NULL;
SDL_Color selectedColor = {COLOR_1_R, COLOR_1_G, COLOR_1_B};
SDL_Color normalColor = {COLOR_3_R, COLOR_3_G, COLOR_3_B};
int selectedOption = 0;
int selectedOption2 = 0;

uint8_t dir_input = 0;
uint8_t but_input = 0;
char *game_rom = NULL;
char *boot_rom = NULL;
bool valid_game_rom = false;
bool valid_boot_rom = false;
char *game_rom_path = NULL;
char *boot_rom_path = NULL;
emulator_mode_t mode = MAIN_MENU;
uint32_t framebuffer[GAMEBOY_SCREEN_HEIGHT * GAMEBOY_SCREEN_WIDTH] = {0};

char *find_value_for_name(const char *filePath, const char *name)
{
	FILE *file = fopen(filePath, "r");
	if (file == NULL) {
		return NULL;
	}

	char line[MAX_LINE_LENGTH];
	char *value = NULL;
	while (fgets(line, sizeof(line), file) != NULL) {
		char currentName[MAX_LINE_LENGTH];
		char currentValue[MAX_LINE_LENGTH];
		if (sscanf(line, "%[^=]=%s", currentName, currentValue) == 2) {
			if (strcmp(currentName, name) == 0) {
				value = strdup(currentValue);
				break;
			}
		}
	}

	fclose(file);
	return value;
}

int update_or_add_pair(const char *filePath, const char *name, const char *value)
{
	char *existingValue = find_value_for_name(CACHE_FILE, name);
	int needToUpdate = existingValue != NULL && strcmp(existingValue, value) != 0;

	if (existingValue == NULL || needToUpdate) {
		FILE *file = fopen(filePath, "a+");
		if (file == NULL) {
			free(existingValue);
			return -1;
		}

		if (needToUpdate) {
			FILE *tempFile = tmpfile();
			rewind(file);
			char line[MAX_LINE_LENGTH];
			while (fgets(line, sizeof(line), file) != NULL) {
				char currentName[MAX_LINE_LENGTH];
				if (sscanf(line, "%[^=]", currentName) == 1 &&
				    strcmp(currentName, name) != 0) {
					fputs(line, tempFile);
				}
			}
			fprintf(tempFile, "%s=%s\n", name, value);

		} else {
			fprintf(file, "%s=%s\n", name, value);
		}

		fclose(file);
	}

	free(existingValue);
	return 0;
}

int read_file_into_buffer(const char *filePath, char **buffer, size_t *fileSize)
{
	FILE *file = fopen(filePath, "rb");
	if (file == NULL) {
		LOG_ERR("Error: Unable to open file %s", filePath);
		return -1;
	}

	fseek(file, 0, SEEK_END);
	*fileSize = ftell(file);
	fseek(file, 0, SEEK_SET);

	*buffer = (char *)malloc(*fileSize);
	if (*buffer == NULL) {
		LOG_ERR("Memory allocation failed");
		fclose(file);
		return -2;
	}

	fread(*buffer, 1, *fileSize, file);
	fclose(file);
	LOG_INF("File size: %zu bytes", *fileSize);

	return 0;
}

int nfd_read_file(char **buffer, size_t *fileSize, char **outPathReturned)
{
	nfdchar_t *outPath = NULL;
	nfdfilteritem_t filterItem[2] = {{"GB ROM", "gb"}, {"BIN File", "bin"}};
	nfdresult_t result = NFD_OpenDialog(&outPath, filterItem, 2, NULL);

	if (result == NFD_OKAY) {
		LOG_INF("Success!");
		LOG_INF("%s", outPath);

		*outPathReturned = strdup(outPath);
		if (*outPathReturned == NULL) {
			LOG_ERR("Error duplicating outPath");
			NFD_FreePath(outPath);
			return -4;
		}

		int readResult = read_file_into_buffer(outPath, buffer, fileSize);
		NFD_FreePath(outPath);
		return 0;
	} else if (result == NFD_CANCEL) {
		LOG_ERR("User pressed cancel.");
		return 1;
	} else {
		LOG_ERR("Error: %s", NFD_GetError());
		return -3;
	}
}

void rom_running_input(SDL_Event *event)
{
	switch (event->type) {
	case SDL_KEYDOWN: {
		switch (event->key.keysym.sym) {

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
		case SDLK_ESCAPE:
			mode = PAUSE_MENU;
		}
		break;
	}

	case SDL_KEYUP: {
		switch (event->key.keysym.sym) {

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
	}
}

void main_menu_input(SDL_Event *event)
{
	int r = 0;
	size_t dummy_temp;
	size_t *dummy = &dummy_temp;
	switch (event->type) {
	case SDL_KEYDOWN: {
		switch (event->key.keysym.sym) {
		case SDLK_UP:
			selectedOption--;
			if (selectedOption < 0) {
				selectedOption = MAIN_MENU_OPTIONS - 1;
			}
			break;
		case SDLK_DOWN:
			selectedOption++;
			if (selectedOption >= MAIN_MENU_OPTIONS) {
				selectedOption = 0;
			}
			break;
		case SDLK_RETURN:
			printf("Option %d selected!\n", selectedOption + 1);

			switch (selectedOption) {
			case 0:
				if (valid_boot_rom && valid_game_rom) {
					load_rom();
					mode = ROM_RUNNING;
				}
				break;
			case 1:
				valid_boot_rom = false;
				r = nfd_read_file(&boot_rom, dummy, &boot_rom_path);
				if (r == 0) {
					valid_boot_rom = true;
					update_or_add_pair(CACHE_FILE, "boot_rom", boot_rom_path);
				}
				break;
			case 2:
				valid_game_rom = false;
				r = nfd_read_file(&game_rom, dummy, &game_rom_path);
				if (r == 0) {
					valid_game_rom = true;
					update_or_add_pair(CACHE_FILE, "game_rom", game_rom_path);
				}
				break;
			}

			break;
		}
	}
	}
}

void pause_menu_input(SDL_Event *event)
{
	int r = 0;
	switch (event->type) {
	case SDL_KEYDOWN: {
		switch (event->key.keysym.sym) {
		case SDLK_UP:
			selectedOption2--;
			if (selectedOption2 < 0) {
				selectedOption2 = PAUSE_MENU_OPTIONS - 1;
			}
			break;
		case SDLK_DOWN:
			selectedOption2++;
			if (selectedOption2 >= PAUSE_MENU_OPTIONS) {
				selectedOption2 = 0;
			}
			break;
		case SDLK_RETURN:
			printf("Option %d selected!\n", selectedOption2 + 1);

			switch (selectedOption2) {
			case 0:
				mode = ROM_RUNNING;
				break;
			case 1:
				mode = MAIN_MENU;
				break;
			}

			break;
		}
	}
	}
}

static void update_input(SDL_Window *window, int *window_width, int *window_height)
{
	SDL_Event event;
	while (SDL_PollEvent(&event)) {

		if (event.type == SDL_QUIT) {
			exit(0);
		}

		if (event.type == SDL_WINDOWEVENT) {
			if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
				SDL_GetWindowSize(window, window_width, window_height);
				break;
			}
		}

		if (mode == ROM_RUNNING) {
			rom_running_input(&event);
		} else if (mode == MAIN_MENU) {
			main_menu_input(&event);
		} else if (mode == PAUSE_MENU) {
			pause_menu_input(&event);
		}
	}
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

void copy_frame_buffer(uint32_t *buffer)
{
	memcpy(framebuffer, buffer, GAMEBOY_SCREEN_HEIGHT * GAMEBOY_SCREEN_WIDTH * 4);
}

uint8_t controls_joypad(uint8_t *ucJoypadSELdir, uint8_t *ucJoypadSELbut)
{
	uint8_t mask = 0;

	if (*ucJoypadSELdir == 0x10) {
		mask = but_input;
	} else if (*ucJoypadSELbut == 0x20) {
		mask = dir_input;
	}

	return 0xC0 | (0xF ^ mask) | (*ucJoypadSELbut | *ucJoypadSELdir);
}

void menu_init(void)
{
	int r = 0;
	size_t dummy_temp;
	size_t *dummy = &dummy_temp;
	if (TTF_Init() == -1) {
		LOG_ERR("SDL_ttf could not initialize! TTF_Error: %s", TTF_GetError());
	}
	font = TTF_OpenFont(FONT_PATH, MENU_FONT_SIZE);
	if (font == NULL) {
		LOG_ERR("Failed to load font! SDL_ttf Error: %s", TTF_GetError());
	}

	boot_rom_path = find_value_for_name(CACHE_FILE, "boot_rom");
	if (boot_rom_path != NULL) {
		r = read_file_into_buffer(boot_rom_path, &boot_rom, dummy);
		if (r == 0) {
			valid_boot_rom = true;
		}
	}

	game_rom_path = find_value_for_name(CACHE_FILE, "game_rom");
	if (game_rom_path != NULL) {
		r = read_file_into_buffer(game_rom_path, &game_rom, dummy);
		if (r == 0) {
			valid_game_rom = true;
		}
	}
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

int init(SDL_Window **window, SDL_Renderer **renderer, SDL_Texture **texture, int window_width,
	 int window_height)
{
	SDL_Init(SDL_INIT_VIDEO);

	*window = SDL_CreateWindow("Knowboy", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
				   window_width, window_height, SDL_WINDOW_RESIZABLE);

	*renderer = SDL_CreateRenderer(*window, -1,
				       SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

	*texture =
		SDL_CreateTexture(*renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING,
				  GAMEBOY_SCREEN_WIDTH, GAMEBOY_SCREEN_HEIGHT);

	menu_init();

	SDL_SetRenderDrawColor(*renderer, 0, 0, 0, 255);
	SDL_RenderClear(*renderer);

	if (!SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear")) {
		LOG_ERR("Warning: Linear texture filtering not enabled!");
	}

	/* init SDL audio */
	audio_init();

	/* initialize native file dialog */
	if (NFD_Init() != NFD_OKAY) {
		LOG_ERR("NFD failed to initialize ");
	}

	return 0;
}

int load_rom(void)
{
	SDL_PauseAudio(0);
	gb_cpu_init();
	gb_ppu_init();
	gb_memory_init(boot_rom, game_rom);
	gb_memory_set_control_function(controls_joypad);
	gb_ppu_set_display_frame_buffer(copy_frame_buffer);
	return 0;
}

int run_rom(void)
{
	static int Tstates = 0;
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

	SDL_QueueAudio(1, buf.buffer, (*buf.len) * 2);
	*buf.len = 0;
	return 0;
}

void render_main_menu(SDL_Renderer *renderer, int window_width)
{
	SDL_SetRenderDrawColor(renderer, COLOR_4_R, COLOR_4_G, COLOR_1_B, COLOR_4_A);
	SDL_RenderClear(renderer);

	int y = 100;
	for (int i = 0; i < MAIN_MENU_OPTIONS; i++) {
		SDL_Color color = (i == selectedOption) ? selectedColor : normalColor;
		SDL_Surface *surface = TTF_RenderText_Solid(font, main_menu_texts[i], color);
		SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
		int textWidth = surface->w;
		int textHeight = surface->h;

		SDL_Rect renderQuad = {(window_width - textWidth) / 2, y, textWidth, textHeight};
		SDL_RenderCopy(renderer, texture, NULL, &renderQuad);

		SDL_DestroyTexture(texture);
		SDL_FreeSurface(surface);

		y += 50;
	}
	SDL_RenderPresent(renderer);
}

void render_pause_menu(SDL_Renderer *renderer, int window_width)
{
	SDL_SetRenderDrawColor(renderer, COLOR_4_R, COLOR_4_G, COLOR_1_B, COLOR_4_A);
	SDL_RenderClear(renderer);

	int y = 100;
	for (int i = 0; i < PAUSE_MENU_OPTIONS; i++) {
		SDL_Color color = (i == selectedOption2) ? selectedColor : normalColor;
		SDL_Surface *surface = TTF_RenderText_Solid(font, pause_menu_texts[i], color);
		SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
		int textWidth = surface->w;
		int textHeight = surface->h;

		SDL_Rect renderQuad = {(window_width - textWidth) / 2, y, textWidth, textHeight};
		SDL_RenderCopy(renderer, texture, NULL, &renderQuad);

		SDL_DestroyTexture(texture);
		SDL_FreeSurface(surface);

		y += 50;
	}
	SDL_RenderPresent(renderer);
}

void render_frame_buffer(SDL_Renderer *renderer, SDL_Texture *texture, int window_width,
			 int window_height, float aspect_ratio)
{
	int new_width;
	int new_height;
	SDL_Rect srcRect = {0, 0, GAMEBOY_SCREEN_WIDTH, GAMEBOY_SCREEN_HEIGHT};

	SDL_UpdateTexture(texture, NULL, framebuffer, GAMEBOY_SCREEN_WIDTH * sizeof(uint32_t));

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

	SDL_Rect dstRect = {(window_width - new_width) / 2, (window_height - new_height) / 2,
			    new_width, new_height};

	SDL_RenderCopy(renderer, texture, &srcRect, &dstRect);
	SDL_RenderPresent(renderer);
}

void close(SDL_Window *window, SDL_Renderer *renderer, SDL_Texture *texture)
{
	NFD_Quit();
	SDL_DestroyTexture(texture);
	SDL_DestroyWindow(window);
	SDL_DestroyRenderer(renderer);
	SDL_Quit();
}

int main(void)
{
	SDL_Window *window = NULL;
	SDL_Renderer *renderer = NULL;
	SDL_Texture *texture = NULL;
	int window_width = GAMEBOY_SCREEN_WIDTH * 3;
	int window_height = GAMEBOY_SCREEN_HEIGHT * 3;
	float aspect_ratio = (float)GAMEBOY_SCREEN_WIDTH / (float)GAMEBOY_SCREEN_HEIGHT;

	if (init(&window, &renderer, &texture, window_width, window_height) != 0) {
		LOG_ERR("Failed to initialize emulator");
		close(window, renderer, texture);
		exit(1);
	}

	while (1) {

		display_fps(window);
		update_input(window, &window_width, &window_height);
		switch (mode) {
		case MAIN_MENU:
			render_main_menu(renderer, window_width);
			break;
		case PAUSE_MENU:
			render_pause_menu(renderer, window_width);
			break;
		case ROM_RUNNING:
			run_rom();
			render_frame_buffer(renderer, texture, window_width, window_height,
					    aspect_ratio);
			break;
		}
	}

	close(window, renderer, texture);
	return 0;
}
