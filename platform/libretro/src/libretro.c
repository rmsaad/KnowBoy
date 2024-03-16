#include <math.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32) && !defined(_XBOX)
#include <windows.h>
#endif

#include "gb_cpu.h"
#include "gb_mbc.h"
#include "gb_memory.h"
#include "gb_papu.h"
#include "gb_ppu.h"
#include "libretro.h"
#include "logging.h"

#define VIDEO_WIDTH	   GAMEBOY_SCREEN_WIDTH
#define VIDEO_HEIGHT	   GAMEBOY_SCREEN_HEIGHT
#define VIDEO_PITCH	   GAMEBOY_SCREEN_WIDTH
#define VIDEO_PIXELS	   (VIDEO_WIDTH * VIDEO_HEIGHT)
#define VIDEO_REFRESH_RATE (4194304.0 / 70224.0)

#define SOUND_SAMPLE_RATE 44100

static uint8_t *frame_buf;
static bool use_audio_cb;
char retro_base_directory[4096];
char retro_game_path[4096];

static retro_environment_t environ_cb;

void retro_init(void)
{
	LOG_INF_CB("retro init");
	frame_buf = (uint8_t *)malloc(VIDEO_PIXELS * sizeof(uint32_t));
	const char *dir = NULL;
	if (environ_cb(RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY, &dir) && dir) {
		snprintf(retro_base_directory, sizeof(retro_base_directory), "%s", dir);
	}
}

void retro_deinit(void)
{
	LOG_INF_CB("retro de-init");
	free(frame_buf);
	frame_buf = NULL;
}

unsigned retro_api_version(void)
{
	LOG_INF_CB("retro api version");
	return RETRO_API_VERSION;
}

void retro_set_controller_port_device(unsigned port, unsigned device)
{
	LOG_INF_CB("Plugging device %u into port %u.", device, port);
}

void retro_get_system_info(struct retro_system_info *info)
{
	LOG_INF_CB("retro get system info");
	memset(info, 0, sizeof(*info));
	info->library_name = "KnowBoy";
	info->library_version = "v0.5.0";
	info->need_fullpath = false;
	info->block_extract = false;
	info->valid_extensions = "gb|dmg";
}

static retro_video_refresh_t video_cb;
static retro_audio_sample_t audio_cb;
static retro_audio_sample_batch_t audio_batch_cb;
static retro_input_poll_t input_poll_cb;
static retro_input_state_t input_state_cb;

void retro_get_system_av_info(struct retro_system_av_info *info)
{
	LOG_INF_CB("retro get system av info");

	info->geometry.base_width = VIDEO_WIDTH;
	info->geometry.base_height = VIDEO_HEIGHT;
	info->geometry.max_width = VIDEO_WIDTH;
	info->geometry.max_height = VIDEO_HEIGHT;
	info->geometry.aspect_ratio = (float)VIDEO_WIDTH / (float)VIDEO_HEIGHT;
	info->timing.fps = VIDEO_REFRESH_RATE;
	info->timing.sample_rate = SOUND_SAMPLE_RATE;
}

void retro_set_environment(retro_environment_t cb)
{
	environ_cb = cb;

	if (cb(RETRO_ENVIRONMENT_GET_LOG_INTERFACE, get_log_callback())) {
		set_log_printf();
	}

	LOG_INF_CB("retro set environment");

	static const struct retro_controller_description controllers[] = {
		{"Nintendo DS", RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_JOYPAD, 0)},
	};

	static const struct retro_controller_info ports[] = {
		{controllers, 1},
		{NULL, 0},
	};

	cb(RETRO_ENVIRONMENT_SET_CONTROLLER_INFO, (void *)ports);
}

void retro_set_audio_sample(retro_audio_sample_t cb)
{
	LOG_INF_CB("retro set audio sample");
	audio_cb = cb;
}

void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb)
{
	LOG_INF_CB("retro set audio sample batch");
	audio_batch_cb = cb;
}

void retro_set_input_poll(retro_input_poll_t cb)
{
	LOG_INF_CB("retro set input poll");
	input_poll_cb = cb;
}

void retro_set_input_state(retro_input_state_t cb)
{
	LOG_INF_CB("retro set input state");
	input_state_cb = cb;
}

void retro_set_video_refresh(retro_video_refresh_t cb)
{
	LOG_INF_CB("retro set video refresh");
	video_cb = cb;
}

void retro_reset(void)
{
	LOG_INF_CB("retro reset");
}

uint8_t dir_input = 0;
uint8_t but_input = 0;

static void update_input(void)
{
	input_poll_cb();
	uint16_t joypad_bits = 0;
	for (int j = 0; j < (RETRO_DEVICE_ID_JOYPAD_R3 + 1); j++) {
		if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, j)) {
			joypad_bits |= (1 << j);
		}
	}

	dir_input = 0;
	but_input = 0;
	dir_input |= joypad_bits & (1 << RETRO_DEVICE_ID_JOYPAD_RIGHT) ? 0x1 : 0x0;
	dir_input |= joypad_bits & (1 << RETRO_DEVICE_ID_JOYPAD_LEFT) ? 0x2 : 0x0;
	dir_input |= joypad_bits & (1 << RETRO_DEVICE_ID_JOYPAD_UP) ? 0x4 : 0x0;
	dir_input |= joypad_bits & (1 << RETRO_DEVICE_ID_JOYPAD_DOWN) ? 0x8 : 0x0;

	but_input |= joypad_bits & (1 << RETRO_DEVICE_ID_JOYPAD_A) ? 0x1 : 0x0;
	but_input |= joypad_bits & (1 << RETRO_DEVICE_ID_JOYPAD_B) ? 0x2 : 0x0;
	but_input |= joypad_bits & (1 << RETRO_DEVICE_ID_JOYPAD_SELECT) ? 0x4 : 0x0;
	but_input |= joypad_bits & (1 << RETRO_DEVICE_ID_JOYPAD_START) ? 0x8 : 0x0;
}

static void check_variables(void)
{
	LOG_INF_CB("retro check variables");
}

static void audio_callback(void)
{
	// LOG_INF_CB("retro audio callback");
	// for (unsigned i = 0; i < 30000 / 60; i++, phase++) {
	//   int16_t val = 0x800 * sinf(2.0f * M_PI * phase * 300.0f / 30000.0f);
	//   audio_cb(val, val);
	// }

	// phase %= 100;
}

static void audio_set_state(bool enable)
{
	LOG_INF_CB("retro set state");
	(void)enable;
}

int Tstates = 0;

uint32_t framebuffer[VIDEO_HEIGHT * VIDEO_PITCH] = {0};

void retro_run(void)
{
	update_input();
	audio_buf_t buf;
	buf.buffer = NULL;
	buf.len = NULL;

	bool updated = false;
	if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE, &updated) && updated)
		check_variables();

	/* step 70224 */
	while (Tstates < 70224) {
		gb_cpu_step();
		gb_ppu_step();
		buf = gb_papu_step();
		Tstates += 4;
	}

	Tstates -= 70224;

	video_cb(framebuffer, VIDEO_WIDTH, VIDEO_HEIGHT, VIDEO_PITCH * sizeof(uint32_t));

	uint16_t remaining_frames = *buf.len / 2;
	int16_t buf_pos = 0;

	// for (int i = 0; i < *buf.len; i++) {
	// }
	// audio_batch_cb((int16_t *)&buf.buffer[buf_pos], remaining_frames);

	while (remaining_frames > 0) {

		size_t uploaded_frames = audio_batch_cb(&buf.buffer[buf_pos], remaining_frames);
		buf_pos += 2;
		remaining_frames -= uploaded_frames;
	}
	*buf.len = 0;
}

void prvDisplayLineBuffer(uint32_t *buffer)
{
	memcpy(framebuffer, buffer, VIDEO_HEIGHT * VIDEO_PITCH * 4);
}

uint8_t rom_data[32768 * 100];
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

bool retro_load_game(const struct retro_game_info *info)
{
	LOG_INF_CB("retro load game");

	/* input config */
	struct retro_input_descriptor desc[] = {
		{0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT, "D-Pad Left"},
		{0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP, "D-Pad Up"},
		{0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN, "D-Pad Down"},
		{0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT, "D-Pad Right"},
		{0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B, "B"},
		{0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A, "A"},
		{0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_SELECT, "Select"},
		{0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START, "Start"},
		{0},
	};
	environ_cb(RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS, desc);

	/* video format */
	enum retro_pixel_format fmt = RETRO_PIXEL_FORMAT_XRGB8888;
	if (!environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &fmt)) {
		LOG_INF_CB("XRGB8888 is not supported.");
		return false;
	}

	/* audio */
	struct retro_audio_callback audio_cb = {audio_callback, audio_set_state};
	use_audio_cb = environ_cb(RETRO_ENVIRONMENT_SET_AUDIO_CALLBACK, &audio_cb);

	/* figure this out */
	check_variables();

	/* get the rom path */
	snprintf(retro_game_path, sizeof(retro_game_path), "%s", info->path);
	LOG_INF_CB("Current Selected Game: %s", retro_game_path);

	/* get the rom data */
	LOG_INF_CB("%d bytes of the ROM:", info->size);
	LOG_HEXDUMP_INF_CB(info->data, 256);

	/* get the boot rom */
	char boot_rom_path[256];
	uint8_t boot_rom_data[256];
	snprintf(boot_rom_path, sizeof(boot_rom_path), "%s/dmg_boot.bin", retro_base_directory);
	FILE *boot_rom_file = fopen(boot_rom_path, "rb");

	if (boot_rom_file) {
		size_t read_bytes = fread(boot_rom_data, 1, 256, boot_rom_file);
		if (read_bytes != 256) {
			LOG_ERR_CB("Failed to read boot ROM");
			fclose(boot_rom_file);
			return false;
		}
		fclose(boot_rom_file);
		LOG_INF_CB("%d bytes of the BOOT ROM:", 256);
		LOG_HEXDUMP_INF_CB(boot_rom_data, 256);

	} else {
		LOG_ERR_CB("Failed to open boot ROM: %s", boot_rom_path);
		return false;
	}

	memcpy(rom_data, info->data, info->size);
	gb_cpu_init();
	gb_ppu_init();
	gb_memory_init(boot_rom_data, rom_data, false);
	gb_memory_set_control_function(prvControlsJoypad);
	gb_ppu_set_display_frame_buffer(prvDisplayLineBuffer);
	return true;
}

void retro_unload_game(void)
{
	LOG_INF_CB("retro unload game");
}

unsigned retro_get_region(void)
{
	LOG_INF_CB("retro get region");
	return RETRO_REGION_NTSC;
}

bool retro_load_game_special(unsigned type, const struct retro_game_info *info, size_t num)
{
	LOG_INF_CB("retro load game special");
	return false;
}

size_t retro_serialize_size(void)
{
	LOG_INF_CB("retro serialize size");
	return 0;
}

bool retro_serialize(void *data_, size_t size)
{
	LOG_INF_CB("retro serialize");
	return false;
}

bool retro_unserialize(const void *data_, size_t size)
{
	LOG_INF_CB("retro unserialize");
	return false;
}

void *retro_get_memory_data(unsigned id)
{
	LOG_INF_CB("retro get memory data");
	(void)id;
	return NULL;
}

size_t retro_get_memory_size(unsigned id)
{
	LOG_INF_CB("retro get memory size");
	(void)id;
	return 0;
}

void retro_cheat_reset(void)
{
	LOG_INF_CB("retro cheat reset");
}

void retro_cheat_set(unsigned index, bool enabled, const char *code)
{
	LOG_INF_CB("retro cheat set");
	(void)index;
	(void)enabled;
	(void)code;
}
