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
#include "gb_apu.h"
#include "gb_cpu.h"
#include "gb_debug.h"
#include "gb_mbc.h"
#include "gb_memory.h"
#include "gb_ppu.h"

#define MAX_LINE_LENGTH 1024

static uint8_t dir_input = 0;
static uint8_t but_input = 0;
static uint32_t framebuffer[GAMEBOY_SCREEN_HEIGHT * GAMEBOY_SCREEN_WIDTH] = {0};

int load_rom(gb_config_t *gb_config);

char *find_value_for_name(const char *file_path, const char *name)
{
	FILE *file = fopen(file_path, "ab+");

	char line[MAX_LINE_LENGTH];
	char *value = NULL;
	while (fgets(line, sizeof(line), file) != NULL) {
		char current_name[MAX_LINE_LENGTH];
		char current_value[MAX_LINE_LENGTH];
		if (sscanf(line, "%[^=]=%s", current_name, current_value) == 2) {
			if (strcmp(current_name, name) == 0) {
				value = strdup(current_value);
				break;
			}
		}
	}

	fclose(file);
	return value;
}

int update_or_add_pair(const char *file_path, const char *name, const char *value)
{

	bool needs_update = false;
	char *existing_value = find_value_for_name(file_path, name);

	if (existing_value == NULL || strcmp(existing_value, value) != 0) {
		needs_update = true;
	}

	if (existing_value != NULL) {
		free(existing_value);
	}

	FILE *file = fopen(file_path, "r+");

	char temp_file_path[MAX_LINE_LENGTH];
	snprintf(temp_file_path, sizeof(temp_file_path), "%s.tmp", file_path);
	FILE *temp_file = fopen(temp_file_path, "w+");
	if (temp_file == NULL) {
		return -1;
	}

	if (needs_update) {
		rewind(file);
		char line[MAX_LINE_LENGTH];
		while (fgets(line, sizeof(line), file) != NULL) {
			char current_name[MAX_LINE_LENGTH];
			if (sscanf(line, "%[^=]", current_name) == 1 &&
			    strcmp(current_name, name) != 0) {
				fputs(line, temp_file);
			}
		}
		fprintf(temp_file, "%s=%s\n", name, value);

		fclose(file);
		fclose(temp_file);
		if (remove(file_path) != 0 || rename(temp_file_path, file_path) != 0) {
			return -1;
		}
	} else {
		fclose(file);
		fclose(temp_file);
	}

	return 0;
}

int read_file_into_buffer(const char *file_path, char **buffer, size_t *file_size)
{
	size_t file_sz;

	FILE *file = fopen(file_path, "rb");
	if (file == NULL) {
		LOG_ERR("Error: Unable to open file %s", file_path);
		return -1;
	}

	fseek(file, 0, SEEK_END);
	file_sz = ftell(file);
	fseek(file, 0, SEEK_SET);

	*buffer = (char *)malloc(file_sz);
	if (*buffer == NULL) {
		LOG_ERR("Memory allocation failed");
		fclose(file);
		return -2;
	}

	fread(*buffer, 1, file_sz, file);
	fclose(file);
	LOG_INF("File size: %zu bytes", file_sz);

	if (file_size != NULL) {
		*file_size = file_sz;
	}

	return 0;
}

int nfd_read_file(char **buffer, size_t *file_size, char **out_path_returned)
{
	nfdchar_t *out_path = NULL;
	nfdfilteritem_t filter_item[2] = {{"GB ROM", "gb"}, {"BIN File", "bin"}};
	nfdresult_t result = NFD_OpenDialog(&out_path, filter_item, 2, NULL);

	if (result == NFD_OKAY) {
		LOG_INF("Success!");
		LOG_INF("%s", out_path);

		*out_path_returned = strdup(out_path);
		if (*out_path_returned == NULL) {
			LOG_ERR("Error duplicating outPath");
			NFD_FreePath(out_path);
			return -4;
		}

		int readResult = read_file_into_buffer(out_path, buffer, file_size);
		NFD_FreePath(out_path);
		return 0;
	} else if (result == NFD_CANCEL) {
		LOG_ERR("User pressed cancel.");
		return 1;
	} else {
		LOG_ERR("Error: %s", NFD_GetError());
		return -3;
	}
}

void rom_running_input(gb_config_t *gb_config, SDL_Event *event)
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
			gb_config->state = PAUSE_MENU;
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

void main_menu_input(gb_config_t *gb_config, SDL_Event *event)
{
	int r = 0;
	switch (event->type) {
	case SDL_KEYDOWN: {
		switch (event->key.keysym.sym) {
		case SDLK_UP:
			gb_config->main_menu.cursor--;
			if (gb_config->main_menu.cursor < 0) {
				gb_config->main_menu.cursor = gb_config->main_menu.size - 1;
			}
			break;
		case SDLK_DOWN:
			gb_config->main_menu.cursor++;
			if (gb_config->main_menu.cursor >= gb_config->main_menu.size) {
				gb_config->main_menu.cursor = 0;
			}
			break;
		case SDLK_RETURN:
			LOG_DBG("Main Menu Option: %d selected!", gb_config->main_menu.cursor + 1);

			switch (gb_config->main_menu.cursor) {
			case 0:
				if ((gb_config->boot_rom.valid || gb_config->boot_skip) &&
				    gb_config->game_rom.valid) {
					load_rom(gb_config);
					gb_config->state = ROM_RUNNING;
				}
				break;
			case 1:
				gb_config->boot_rom.valid = false;
				r = nfd_read_file(&gb_config->boot_rom.data, NULL,
						  &gb_config->boot_rom.path);
				if (r == 0) {
					gb_config->boot_rom.valid = true;
					update_or_add_pair(gb_config->cache_file, "boot_rom",
							   gb_config->boot_rom.path);
				}
				break;
			case 2:
				gb_config->game_rom.valid = false;
				r = nfd_read_file(&gb_config->game_rom.data, NULL,
						  &gb_config->game_rom.path);
				if (r == 0) {
					gb_config->game_rom.valid = true;
					update_or_add_pair(gb_config->cache_file, "game_rom",
							   gb_config->game_rom.path);
				}
				break;
			}

			break;
		}
	}
	}
}

void pause_menu_input(gb_config_t *gb_config, SDL_Event *event)
{
	int r = 0;
	switch (event->type) {
	case SDL_KEYDOWN: {
		switch (event->key.keysym.sym) {
		case SDLK_UP:
			gb_config->pause_menu.cursor--;
			if (gb_config->pause_menu.cursor < 0) {
				gb_config->pause_menu.cursor = gb_config->pause_menu.size - 1;
			}
			break;
		case SDLK_DOWN:
			gb_config->pause_menu.cursor++;
			if (gb_config->pause_menu.cursor >= gb_config->pause_menu.size) {
				gb_config->pause_menu.cursor = 0;
			}
			break;
		case SDLK_RETURN:
			LOG_DBG("Pause Menu Option: %d selected!",
				gb_config->pause_menu.cursor + 1);

			switch (gb_config->pause_menu.cursor) {
			case 0:
				gb_config->state = ROM_RUNNING;
				break;
			case 1:
				gb_config->state = MAIN_MENU;
				break;
			}

			break;
		}
	}
	}
}

static void update_input(gb_config_t *gb_config)
{
	SDL_Event event;
	while (SDL_PollEvent(&event)) {

		if (event.type == SDL_QUIT) {
			exit(0);
		}

		if (event.type == SDL_WINDOWEVENT) {
			if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
				SDL_GetWindowSize(gb_config->av.window, &gb_config->av.window_width,
						  &gb_config->av.window_height);
				break;
			}
		}

		if (gb_config->state == ROM_RUNNING) {
			rom_running_input(gb_config, &event);
		} else if (gb_config->state == MAIN_MENU) {
			main_menu_input(gb_config, &event);
		} else if (gb_config->state == PAUSE_MENU) {
			pause_menu_input(gb_config, &event);
		}
	}
}

void display_fps(gb_av_t *gb_av)
{
	static uint32_t frame_count = 0;
	static uint32_t last_time = 0;
	static uint32_t fps = 0;
	static char title[100];

	frame_count++;
	uint32_t current_time = SDL_GetTicks();
	if (current_time > last_time + 1000) {
		fps = frame_count;
		frame_count = 0;
		last_time = current_time;

		// Update window title with FPS
		snprintf(title, 100, "FPS: %d", fps);
		SDL_SetWindowTitle(gb_av->window, title);
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

bool enqueue(message_queue_t *q, const char *message)
{
	if (q->count == QUEUE_SIZE) {
		return false; // Queue is full
	}
	strncpy(q->messages[q->rear], message, MESSAGE_LENGTH);
	q->rear = (q->rear + 1) % QUEUE_SIZE;
	q->count++;
	return true;
}

bool dequeue(message_queue_t *q, char *message)
{
	if (q->count == 0) {
		return false;
	}
	strncpy(message, q->messages[q->front], MESSAGE_LENGTH);
	q->front = (q->front + 1) % QUEUE_SIZE;
	q->count--;
	return true;
}

int listen_to_stdin(void *debug_ctx)
{
	char input[MESSAGE_LENGTH];
	gb_debug_t *gb_debug = debug_ctx;
	printf("> ");
	while (true) {
		if (fgets(input, sizeof(input), stdin) != NULL) {
			input[strcspn(input, "\n")] = '\0';
			SDL_LockMutex(gb_debug->queue_mutex);
			enqueue(&gb_debug->queue, input);
			SDL_UnlockMutex(gb_debug->queue_mutex);
		}
	}
	return 0;
}

bool check_queue(void *debug_ctx, char *message)
{
	bool message_present;
	gb_debug_t *gb_debug = debug_ctx;
	SDL_LockMutex(gb_debug->queue_mutex);
	message_present = dequeue(&gb_debug->queue, message);
	SDL_UnlockMutex(gb_debug->queue_mutex);
	return message_present;
}

void flush_stdout(void)
{
	printf("> ");
	fflush(stdout);
}

void menu_init(gb_config_t *gb_config)
{
	if (TTF_Init() == -1) {
		LOG_ERR("SDL_ttf could not initialize! TTF_Error: %s", TTF_GetError());
	}
	gb_config->font.ttf = TTF_OpenFont(gb_config->font.path, gb_config->font.size);
	if (gb_config->font.ttf == NULL) {
		LOG_ERR("Failed to load font! SDL_ttf Error: %s", TTF_GetError());
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

int init(gb_config_t *gb_config)
{
	int r = 0;
	if (gb_config->av.enable) {
		SDL_Init(SDL_INIT_VIDEO);

		gb_config->av.window =
			SDL_CreateWindow("Knowboy", SDL_WINDOWPOS_UNDEFINED,
					 SDL_WINDOWPOS_UNDEFINED, gb_config->av.window_width,
					 gb_config->av.window_height, SDL_WINDOW_RESIZABLE);

		gb_config->av.renderer =
			SDL_CreateRenderer(gb_config->av.window, -1,
					   SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

		gb_config->av.texture = SDL_CreateTexture(
			gb_config->av.renderer, SDL_PIXELFORMAT_ARGB8888,
			SDL_TEXTUREACCESS_STREAMING, GAMEBOY_SCREEN_WIDTH, GAMEBOY_SCREEN_HEIGHT);

		menu_init(gb_config);

		SDL_SetRenderDrawColor(gb_config->av.renderer, 0, 0, 0, 255);
		SDL_RenderClear(gb_config->av.renderer);

		if (!SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear")) {
			LOG_ERR("Warning: Linear texture filtering not enabled!");
		}

		/* init SDL audio */
		audio_init();

		/* initialize native file dialog */
		if (NFD_Init() != NFD_OKAY) {
			LOG_ERR("NFD failed to initialize ");
		}
	}

	gb_config->boot_rom.path = find_value_for_name(gb_config->cache_file, "boot_rom");
	if (gb_config->boot_rom.path != NULL) {
		r = read_file_into_buffer(gb_config->boot_rom.path, &gb_config->boot_rom.data,
					  NULL);
		if (r == 0) {
			gb_config->boot_rom.valid = true;
		}
	}

	gb_config->game_rom.path = find_value_for_name(gb_config->cache_file, "game_rom");
	if (gb_config->game_rom.path != NULL) {
		r = read_file_into_buffer(gb_config->game_rom.path, &gb_config->game_rom.data,
					  NULL);
		if (r == 0) {
			gb_config->game_rom.valid = true;
		}
	}

	if (gb_config->menu_skip == true) {
		if ((gb_config->boot_rom.valid || gb_config->boot_skip) &&
		    gb_config->game_rom.valid) {
			load_rom(gb_config);
			gb_config->state = ROM_RUNNING;
		} else {
			LOG_ERR("Can't Skip menu, bad boot/game rom selection");
		}
	}

	return 0;
}

void render_menu(gb_config_t *gb_config, gb_menu_t *gb_menu)
{
	if (gb_config->av.enable) {
		SDL_SetRenderDrawColor(gb_config->av.renderer, COLOR_4_R, COLOR_4_G, COLOR_1_B,
				       COLOR_4_A);
		SDL_RenderClear(gb_config->av.renderer);

		int y = 100;
		for (int i = 0; i < gb_menu->size; i++) {
			SDL_Color color = (i == gb_menu->cursor) ? gb_config->font.select_color
								 : gb_config->font.default_color;
			SDL_Surface *surface = TTF_RenderText_Solid(gb_config->font.ttf,
								    gb_menu->options[i], color);
			SDL_Texture *texture =
				SDL_CreateTextureFromSurface(gb_config->av.renderer, surface);
			int text_width = surface->w;
			int text_height = surface->h;

			SDL_Rect render_quad = {(gb_config->av.window_width - text_width) / 2, y,
						text_width, text_height};
			SDL_RenderCopy(gb_config->av.renderer, texture, NULL, &render_quad);

			SDL_DestroyTexture(texture);
			SDL_FreeSurface(surface);

			y += 50;
		}
		SDL_RenderPresent(gb_config->av.renderer);
	}
}

void render_main_menu(gb_config_t *gb_config)
{
	render_menu(gb_config, &gb_config->main_menu);
}

void render_pause_menu(gb_config_t *gb_config)
{
	render_menu(gb_config, &gb_config->pause_menu);
}

void render_frame_buffer(gb_av_t *gb_av)
{
	int new_width;
	int new_height;
	if (gb_av->enable) {
		SDL_Rect src_rect = {0, 0, GAMEBOY_SCREEN_WIDTH, GAMEBOY_SCREEN_HEIGHT};

		SDL_UpdateTexture(gb_av->texture, NULL, framebuffer,
				  GAMEBOY_SCREEN_WIDTH * sizeof(uint32_t));

		SDL_SetRenderDrawColor(gb_av->renderer, 0, 0, 0, 255); // Black color
		SDL_RenderClear(gb_av->renderer);

		if (gb_av->window_width / gb_av->aspect_ratio <= gb_av->window_height) {
			// Window is taller than the image
			new_width = gb_av->window_width;
			new_height = (int)(gb_av->window_width / gb_av->aspect_ratio);
		} else {
			// Window is wider than the image
			new_height = gb_av->window_height;
			new_width = (int)(gb_av->window_height * gb_av->aspect_ratio);
		}

		SDL_Rect dstRect = {(gb_av->window_width - new_width) / 2,
				    (gb_av->window_height - new_height) / 2, new_width, new_height};

		SDL_RenderCopy(gb_av->renderer, gb_av->texture, &src_rect, &dstRect);
		SDL_RenderPresent(gb_av->renderer);
	}
}

int load_rom(gb_config_t *gb_config)
{
	if (gb_config->av.enable) {
		SDL_PauseAudio(0);
	}
	if (gb_config->debug.enable) {
		gb_config->debug.queue_mutex = SDL_CreateMutex();
		gb_config->debug.queue.front = 0;
		gb_config->debug.queue.rear = 0;
		gb_config->debug.queue.count = 0;
		SDL_Thread *thread_id =
			SDL_CreateThread(listen_to_stdin, "stdinListener", &gb_config->debug);
		gb_debug_init(check_queue, flush_stdout, &gb_config->debug);
	}
	gb_cpu_init();
	gb_ppu_init();
	gb_apu_init(gb_config->av.audio_buf, &gb_config->av.audio_buf_pos, AUDIO_BUF_SIZE);
	gb_memory_init(gb_config->boot_rom.data, gb_config->game_rom.data, gb_config->boot_skip);
	gb_memory_set_control_function(controls_joypad);
	gb_ppu_set_display_frame_buffer(copy_frame_buffer);
	return 0;
}

int run_rom(gb_config_t *gb_config)
{
	gb_debug_check_msg_queue();
	for (int Tstates = 0; Tstates < 70224; Tstates += 4) {
		while (gb_debug_step()) {
			if (gb_config->av.enable) {
				update_input(gb_config);
				render_frame_buffer(&gb_config->av);
				SDL_Delay(16);
			}
		}
		gb_cpu_step();
		gb_ppu_step();
		gb_apu_step();
	}

	if (gb_config->av.enable) {
		SDL_QueueAudio(1, gb_config->av.audio_buf, (gb_config->av.audio_buf_pos) * 2);
	}
	gb_config->av.audio_buf_pos = 0;
	return 0;
}

void app_close(gb_av_t *gb_av)
{
	if (gb_av->enable) {
		NFD_Quit();
		SDL_DestroyTexture(gb_av->texture);
		SDL_DestroyWindow(gb_av->window);
		SDL_DestroyRenderer(gb_av->renderer);
		SDL_Quit();
	}
}

int parse_arguments(int argc, char *argv[], gb_config_t *gb_config)
{
	int r = 0;
	bool start = false;
	FILE *file;
	for (int i = 1; i < argc; i++) {

		if (strcmp(argv[i], "--bootrom") == 0 && i + 1 < argc) {
			char *boot_rom = argv[++i];
			if (strcmp(boot_rom, "none") == 0 || strcmp(boot_rom, "None") == 0) {
				gb_config->boot_skip = true;
			} else if ((file = fopen(boot_rom, "r"))) {
				update_or_add_pair(gb_config->cache_file, "boot_rom", boot_rom);
				fclose(file);
			} else {
				LOG_ERR("Invalid boot rom argument");
				exit(1);
			}

		} else if (strcmp(argv[i], "--gamerom") == 0 && i + 1 < argc) {
			char *game_rom = argv[++i];
			if ((file = fopen(game_rom, "r"))) {
				update_or_add_pair(gb_config->cache_file, "game_rom", game_rom);
				fclose(file);
			} else {
				LOG_ERR("Invalid game rom argument");
				exit(1);
			}

		} else if (strcmp(argv[i], "--start") == 0) {
			gb_config->menu_skip = true;

		} else if (strcmp(argv[i], "--noninteractive") == 0) {
			gb_config->av.enable = false;
		} else {
			LOG_ERR("Error: Unrecognized argument '%s'\n", argv[i]);
			return -1;
		}
	}

	return 0;
}

int main(int argc, char *argv[])
{
	gb_config_t gb_config = {
		.av =
			{
				.enable = true,
				.window = NULL,
				.renderer = NULL,
				.texture = NULL,
				.window_width = GAMEBOY_SCREEN_WIDTH * 3,
				.window_height = GAMEBOY_SCREEN_HEIGHT * 3,
				.aspect_ratio =
					(float)GAMEBOY_SCREEN_WIDTH / (float)GAMEBOY_SCREEN_HEIGHT,
			},
		.font =
			{
				.ttf = NULL,
				.path = "resources/GameBoy.ttf",
				.size = 24,
				.select_color = {COLOR_1_R, COLOR_1_G, COLOR_1_B},
				.default_color = {COLOR_3_R, COLOR_3_G, COLOR_3_B},
			},
		.main_menu =
			{
				.options = {"Start Game", "Load Boot ROM", "Load ROM"},
				.size = 3,
				.cursor = 0,
			},
		.pause_menu =
			{
				.options = {"Resume Game", "Return to Menu"},
				.size = 2,
				.cursor = 0,
			},
		.game_rom =
			{
				.data = NULL,
				.path = NULL,
				.valid = false,
			},
		.boot_rom =
			{
				.data = NULL,
				.path = NULL,
				.valid = false,
			},
		.debug =
			{
				.enable = true,
			},
		.state = MAIN_MENU,
		.menu_skip = false,
		.boot_skip = false,
		.cache_file = "cache.txt",
	};

	if (parse_arguments(argc, argv, &gb_config) != 0) {
		app_close(&gb_config.av);
		exit(1);
	}

	if (init(&gb_config) != 0) {
		LOG_ERR("Failed to initialize emulator");
		app_close(&gb_config.av);
		exit(1);
	}

	while (1) {

		if (gb_config.av.enable) {
			display_fps(&gb_config.av);
			update_input(&gb_config);
		}

		switch (gb_config.state) {
		case MAIN_MENU:
			render_main_menu(&gb_config);
			break;
		case PAUSE_MENU:
			render_pause_menu(&gb_config);
			break;
		case ROM_RUNNING:
			run_rom(&gb_config);
			render_frame_buffer(&gb_config.av);
			break;
		}
	}

	app_close(&gb_config.av);
	return 0;
}
