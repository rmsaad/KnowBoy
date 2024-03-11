
#ifndef MAIN_H_
#define MAIN_H_

#include <SDL.h>
#include <SDL_ttf.h>

#define MAX_MENU_OPTIONS 3

typedef struct {
	bool enable;
	SDL_Window *window;
	SDL_Renderer *renderer;
	SDL_Texture *texture;
	int window_width;
	int window_height;
	const float aspect_ratio;
} gb_av_t;

typedef struct {
	TTF_Font *ttf;
	const char *path;
	const int size;
	const SDL_Color select_color;
	const SDL_Color default_color;
} gb_font_t;

typedef struct {
	const char *options[MAX_MENU_OPTIONS];
	const int size;
	int cursor;
} gb_menu_t;

typedef struct {
	char *data;
	char *path;
	bool valid;
} gb_rom_t;

typedef enum {
	MAIN_MENU = 0U,
	PAUSE_MENU,
	ROM_RUNNING,
} gb_state_t;

typedef struct {
	gb_av_t av;
	gb_font_t font;
	gb_menu_t main_menu;
	gb_menu_t pause_menu;
	gb_rom_t game_rom;
	gb_rom_t boot_rom;
	gb_state_t state;
	bool menu_skip;
	bool boot_skip;
	const char *cache_file;
} gb_config_t;

#define COLOR_1 0XFF9BBC0F
#define COLOR_2 0XFF8BAC0F
#define COLOR_3 0XFF306230
#define COLOR_4 0XFF0F380F

#define GET_ALPHA(color) ((color >> 24) & 0xFF)
#define GET_RED(color)	 ((color >> 16) & 0xFF)
#define GET_GREEN(color) ((color >> 8) & 0xFF)
#define GET_BLUE(color)	 (color & 0xFF)

// Definitions for COLOR_1 components
#define COLOR_1_A GET_ALPHA(COLOR_1)
#define COLOR_1_R GET_RED(COLOR_1)
#define COLOR_1_G GET_GREEN(COLOR_1)
#define COLOR_1_B GET_BLUE(COLOR_1)

// Definitions for COLOR_2 components
#define COLOR_2_A GET_ALPHA(COLOR_2)
#define COLOR_2_R GET_RED(COLOR_2)
#define COLOR_2_G GET_GREEN(COLOR_2)
#define COLOR_2_B GET_BLUE(COLOR_2)

// Definitions for COLOR_3 components
#define COLOR_3_A GET_ALPHA(COLOR_3)
#define COLOR_3_R GET_RED(COLOR_3)
#define COLOR_3_G GET_GREEN(COLOR_3)
#define COLOR_3_B GET_BLUE(COLOR_3)

// Definitions for COLOR_4 components
#define COLOR_4_A GET_ALPHA(COLOR_4)
#define COLOR_4_R GET_RED(COLOR_4)
#define COLOR_4_G GET_GREEN(COLOR_4)
#define COLOR_4_B GET_BLUE(COLOR_4)

#endif /* MAIN_H_ */
