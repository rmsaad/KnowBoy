/**
 * @file gb_ppu.c
 * @brief Gameboy PPU Functionality.
 *
 * This file emulates all functionality of the gameboy Pixel/Picture Processing Unit (PPU). This
 * includes drawing the background, window, and object layers to the frame buffer to be displayed
 * ~60 times a second.
 *
 * @author Rami Saad
 * @date 2021-06-11
 */

#include "gb_common.h"
#include "gb_memory.h"
#include "gb_ppu.h"
#include "logging.h"

#include <string.h>

// Gameboy Memory struct
extern memory_t mem;

// dot counter
static uint32_t ppu_dot_counter;

// Palettes
static uint32_t bgp_color_to_palette[4];
static uint32_t obp0_color_to_palette[4];
static uint32_t obp1_color_to_palette[4];

// Frame Buffer variables
static uint32_t ucGBLine[GAMEBOY_SCREEN_WIDTH * 3 * 2000];
static uint8_t ucBGWINline[GAMEBOY_SCREEN_WIDTH];

// Function Pointer pointing to external function in display.c
static gb_ppu_display_frame_buffer_t gb_ppu_display_frame_buffer;

// LCDC register
static bool ppu_enable = false;
static uint16_t wn_display_addr = TILE_MAP_LOCATION_LOW;
static bool wn_enable = false;
static uint16_t tile_data_addr = TILE_DATA_SIGNED_ADDR;
static uint16_t bg_display_addr = TILE_MAP_LOCATION_LOW;
static bool obj_size = false;
static bool obj_enable = false;
static bool bg_wn_enable = false;

// stat register
static uint8_t stat_mode;
static bool lyc_int_sel = false;
static bool mode_2_sel = false;
static bool mode_1_sel = false;
static bool mode_0_sel = false;

// scy register
static uint8_t scy = 0;

// scx register
static uint8_t scx = 0;

// ly register
static uint8_t ly = 0;

// wy register
static uint8_t wy = 0;

// wx register
static uint8_t wx = 0;

/*Function Prototypes*/
static void inline gb_ppu_check_lyc(void);
static void gb_ppu_set_stat_mode(uint8_t mode);
static void gb_ppu_update_frame_buffer(uint32_t data, int pixel_pos);
static void gb_ppu_draw_line_background(void);
static void gb_ppu_draw_line_window(void);
static void gb_ppu_draw_line_objects(void);
static void gb_ppu_draw_line(void);
static uint16_t gb_ppu_get_tile_line_data(uint16_t tile_offset, uint8_t line_offset,
					  uint16_t display_addr);

/**
 * @brief Sets function used in gb_ppu_draw_line() without needing to include
 * control.h
 * @param display_buffer function pointer holding address of
 * displayFrameBuffer() function in display.c
 * @return None
 */
void gb_ppu_set_display_frame_buffer(gb_ppu_display_frame_buffer_t display_frame_buffer)
{
	gb_ppu_display_frame_buffer = display_frame_buffer;
}

/**
 * @brief Zeros All Memory in the Line Buffer
 * @return Nothing
 */
void gb_ppu_init(void)
{
	memset(ucGBLine, 0, GAMEBOY_SCREEN_WIDTH * GAMEBOY_SCREEN_HEIGHT * 4);
	ppu_dot_counter = 0;
	stat_mode = 0;

	ppu_enable = false;
	wn_display_addr = TILE_MAP_LOCATION_LOW;
	wn_enable = false;
	tile_data_addr = TILE_DATA_SIGNED_ADDR;
	bg_display_addr = TILE_MAP_LOCATION_LOW;
	obj_size = false;
	obj_enable = false;
	bg_wn_enable = false;

	lyc_int_sel = false;
	mode_2_sel = false;
	mode_1_sel = false;
	mode_0_sel = false;

	scy = 0;
	scx = 0;
	ly = 0;
	wy = 0;
	wx = 0;
}

/**
 * @brief Steps the PPU by tStates
 * @details This function steps the PPU by the tStates variable if the screen
 * enable (7th) bit of LCDC Register is high, the function sets the mode in the
 * STAT Register using the gb_ppu_set_stat_mode() function depending on how many tStates
 * have elapsed. after a full line is has passed the LY register is updated
 * gb_ppu_draw_line() is called to place data in the Line buffer.
 * @return Nothing
 * @note The function and the Gameboy both mimic CRT Displays in that their are
 * both a horizontal H-Blank after a Line is "drawn" and there is a vertical
 * V-Blank after a full frame is drawn
 * @attention The placement of the functions vCheckBackWinTileDataSel() and
 * vCheckBackTileDisplaySel() are just educated guesses at the current moment,
 * more research into when and how often these Registers are updated must be
 * conducted.
 */
void gb_ppu_step(void)
{
	if (!ppu_enable) {
		ly = 0;
		mem.map[LY_ADDR] = ly;
		return;
	}

	ppu_dot_counter += 4;

	if (ppu_dot_counter > 456) { // end of hblank or vblank
		ly++;
		gb_ppu_check_lyc();
		if (ly > 153) { // end of vblank
			gb_ppu_set_stat_mode(STAT_MODE_2);
			ly = 0;

			if (mode_2_sel)
				SET_BIT(mem.map[IF_ADDR], 1);
		}

		mem.map[LY_ADDR] = ly;
		ppu_dot_counter -= 456;
	}

	if (ly > 143) { // vblank region
		if (stat_mode != STAT_MODE_1) {
			gb_ppu_set_stat_mode(STAT_MODE_1);
			if (mode_1_sel)
				SET_BIT(mem.map[IF_ADDR], 1);
			if (ly == 0x90) {
				SET_BIT(mem.map[IF_ADDR], 0);
			}
		}
	} else {
		if (ppu_dot_counter <= 80 && stat_mode != STAT_MODE_2) // oam region
			gb_ppu_set_stat_mode(STAT_MODE_2);
		else if (ppu_dot_counter > 80 && ppu_dot_counter <= 252 &&
			 stat_mode != STAT_MODE_3) { // vram region
			gb_ppu_draw_line();

			gb_ppu_set_stat_mode(STAT_MODE_3);
		} else if (ppu_dot_counter > 252 && ppu_dot_counter <= 456 &&
			   stat_mode != STAT_MODE_0) { // hblank region
			gb_ppu_set_stat_mode(STAT_MODE_0);
			if (mode_0_sel)
				SET_BIT(mem.map[IF_ADDR], 1);
		}
	}
}

/**
 * @brief Finds specific line data of a tile using the tile_offset and
 * line_offset
 * @details Called by gb_ppu_draw_line() and returns color information of a
 * specific tile in the Gameboy's VRAM located by using the tile_offset and
 * line_offset parameters.
 * @param tile_offset gives the address offset in the tile map in Gameboy's VRAM
 * @param line_offset gives the line offset in the tile
 * @return uint16_t data containing the color information of each pixel of a
 * particular line belonging to a tile in the Gameboy's VRAM
 */
static uint16_t gb_ppu_get_tile_line_data(uint16_t tile_offset, uint8_t line_offset,
					  uint16_t display_addr)
{
	uint16_t address = tile_data_addr;
	if (tile_data_addr == TILE_DATA_UNSIGNED_ADDR) {
		address += (mem.map[display_addr + tile_offset] * 0x10);
	} else {
		int8_t signed_conv = (int8_t)(mem.map[display_addr + tile_offset]);
		address += (signed_conv + 128) * 0x10;
	}
	address += line_offset;
	return CAT_BYTES(mem.map[address], mem.map[address + 1]);
}

/**
 * @brief Compares LY Register to LYC
 * @details LYC Register compares itself to LY Register and Sets the appropriate
 * flag (2nd bit) in the STAT Register if they hold the same value
 * @param ly value of the LY Register
 * @return Nothing
 */
static inline void gb_ppu_check_lyc(void)
{
	if (mem.map[LYC_ADDR] == ly) {
		SET_BIT(mem.map[LYC_ADDR], 2);
		if (lyc_int_sel)
			SET_BIT(mem.map[IF_ADDR], 1);
	} else {
		RST_BIT(mem.map[STAT_ADDR], 2);
	}
}

/**
 * @brief Sets the mode of the STAT Register
 * @param mode current status of the LCD controller
 * @return Nothing
 */
static void gb_ppu_set_stat_mode(uint8_t mode)
{
	stat_mode = mode;
	switch (mode) {
	case STAT_MODE_0:
		RST_BIT(mem.map[STAT_ADDR], 1);
		RST_BIT(mem.map[STAT_ADDR], 0);
		break; // 00
	case STAT_MODE_1:
		RST_BIT(mem.map[STAT_ADDR], 1);
		SET_BIT(mem.map[STAT_ADDR], 0);
		break; // 01
	case STAT_MODE_2:
		SET_BIT(mem.map[STAT_ADDR], 1);
		RST_BIT(mem.map[STAT_ADDR], 0);
		break; // 10
	case STAT_MODE_3:
		SET_BIT(mem.map[STAT_ADDR], 1);
		SET_BIT(mem.map[STAT_ADDR], 0);
		break; // 11
	default:
		break;
	}
}

/**
 * @brief Updates the line buffer with pixel information for 1 specified pixel
 * @details Updates and applies a pixel perfect image scaling algorithm on 1
 * pixel of the line buffer when called.
 * @param data Color information for the current pixel
 * @param pixel_pos X position for the current pixel
 * @returns Nothing
 */
static void gb_ppu_update_frame_buffer(uint32_t data, int pixel_pos)
{
	ucGBLine[(ly * GAMEBOY_SCREEN_WIDTH) + pixel_pos] = data;
}

/**
 * @brief Update line buffer with background information
 * @details Populates the line buffer with Background information on the line ly
 * displayed on Screen
 * @returns Nothing
 */
static void gb_ppu_draw_line_background(void)
{
	uint16_t tile_offset = (((uint8_t)(scy + ly) / 8) * 32) +
			       (scx / 8); // gives the address offset in the tile map
	uint8_t line_offset = (((scy % 8) + ly) % 8) * 2; // gives the line offset in the tile
	uint8_t pixl_offset = scx % 8;			  // gives current pixel offset

	uint16_t first_tile = tile_offset % 32;
	uint16_t tile_data =
		gb_ppu_get_tile_line_data(tile_offset, line_offset,
					  bg_display_addr); // tile data holds tile line information

	for (int j = 0; j < GAMEBOY_SCREEN_WIDTH; j++) {

		uint32_t pixel_data = 0;

		switch (((tile_data << pixl_offset) & 0x8080)) {
		case 0x0000:
			pixel_data = bgp_color_to_palette[0];
			ucBGWINline[j] = 0;
			break;
		case 0x0080:
			pixel_data = bgp_color_to_palette[1];
			ucBGWINline[j] = 1;
			break;
		case 0x8000:
			pixel_data = bgp_color_to_palette[2];
			ucBGWINline[j] = 2;
			break;
		case 0x8080:
			pixel_data = bgp_color_to_palette[3];
			ucBGWINline[j] = 3;
			break;
		}

		gb_ppu_update_frame_buffer(pixel_data, j);
		pixl_offset++;

		if (pixl_offset == 8) {
			tile_offset++;
			pixl_offset = 0;
			if (first_tile + (tile_offset % 32) >= 12 &&
			    (tile_offset % 32) < first_tile)
				tile_data = gb_ppu_get_tile_line_data(tile_offset - 32, line_offset,
								      bg_display_addr);
			else
				tile_data = gb_ppu_get_tile_line_data(tile_offset, line_offset,
								      bg_display_addr);
		}
	}
}

/**
 * @brief Update line buffer with window information
 * @details Populates the line buffer with window information if it is currently
 * displayed on the line ly
 * displayed on Screen
 * @returns Nothing
 */
static void gb_ppu_draw_line_window(void)
{
	if (wy > ly || wy > 143 || wx > 166)
		return;

	uint16_t tile_offset =
		(((uint8_t)(ly - wy) / 8) * 32);     // gives the address offset in the tile map
	uint8_t line_offset = (((ly - wy) % 8)) * 2; // gives the line offset in the tile
	uint8_t pixl_offset = (wx - 7) % 8;	     // gives current pixel offset

	uint16_t tile_data = gb_ppu_get_tile_line_data(
		tile_offset, line_offset, wn_display_addr); // tile data holds tile line information

	for (int j = (wx - 7); j < GAMEBOY_SCREEN_WIDTH; j++) {
		uint32_t pixel_data = 0;

		switch (((tile_data << pixl_offset) & 0x8080)) {
		case 0x0000:
			pixel_data = bgp_color_to_palette[0];
			ucBGWINline[j] = 0;
			break;
		case 0x0080:
			pixel_data = bgp_color_to_palette[1];
			ucBGWINline[j] = 1;
			break;
		case 0x8000:
			pixel_data = bgp_color_to_palette[2];
			ucBGWINline[j] = 2;
			break;
		case 0x8080:
			pixel_data = bgp_color_to_palette[3];
			ucBGWINline[j] = 3;
			break;
		}

		gb_ppu_update_frame_buffer(pixel_data, j);
		pixl_offset++;

		if (pixl_offset == 8) {
			tile_offset++;
			pixl_offset = 0;
			tile_data = gb_ppu_get_tile_line_data(tile_offset, line_offset,
							      wn_display_addr);
		}
	}
}

/**
 * @brief  Update line buffer with object information
 * @details Populates the line buffer with object sprites on line ly
 * @returns Nothing
 */
static void gb_ppu_draw_line_objects(void)
{
	for (int obj = 0; obj < 40; obj++) {
		// must be signed for logic to work
		int16_t y_coordinate = mem.map[OAM_BASE + (obj * 4)] - 16;
		// "" "" "" same here
		int16_t x_coordinate = mem.map[OAM_BASE + (obj * 4) + 1] - 8;
		uint8_t data_tile = mem.map[OAM_BASE + (obj * 4) + 2];
		uint8_t obj_prio = CHK_BIT(mem.map[OAM_BASE + (obj * 4) + 3], 7);
		uint8_t obj_y_flip = CHK_BIT(mem.map[OAM_BASE + (obj * 4) + 3], 6);
		uint8_t obj_x_flip = CHK_BIT(mem.map[OAM_BASE + (obj * 4) + 3], 5);
		uint8_t obj_palette = CHK_BIT(mem.map[OAM_BASE + (obj * 4) + 3], 4);
		uint8_t obj_height = (obj_size == 0) ? 8 : 16;

		if (y_coordinate <= ly && (y_coordinate + obj_height) > ly) {

			uint8_t line_offset = obj_y_flip
						      ? ((obj_height - 1) - (ly - y_coordinate)) * 2
						      : (ly - y_coordinate) * 2;
			uint16_t address =
				TILE_DATA_UNSIGNED_ADDR + (data_tile * 0x10) + line_offset;
			uint16_t tile_data = CAT_BYTES(mem.map[address], mem.map[address + 1]);
			uint32_t *palette = (obj_palette) ? &obp1_color_to_palette[0]
							  : &obp0_color_to_palette[0];

			for (int pixel_num = 0; pixel_num < 8; pixel_num++) {

				uint16_t color_info =
					(obj_x_flip) ? (((tile_data >> pixel_num) & 0x0101) << 7)
						     : ((tile_data << pixel_num) & 0x8080);
				uint32_t pixel_data = 0;

				switch (color_info) {
				case 0x0000:
					pixel_data = 0;
					break;
				case 0x0080:
					pixel_data = palette[1];
					break;
				case 0x8000:
					pixel_data = palette[2];
					break;
				case 0x8080:
					pixel_data = palette[3];
					break;
				}

				if (pixel_data != 0 && x_coordinate + pixel_num >= 0 &&
				    (x_coordinate + pixel_num) < GAMEBOY_SCREEN_WIDTH) {
					if ((obj_prio) && ucBGWINline[x_coordinate + pixel_num]) {
						// do nothing this circumstance
					} else {
						gb_ppu_update_frame_buffer(
							pixel_data, x_coordinate + pixel_num);
					}
				}
			}
		}
	}
}

/**
 * @brief Update data in the line buffer for 1 line of the Gameboy
 * @details Populates the line buffer with data related to a line ly, copies
 * line buffer to appropriate location in frame buffer
 * @param ly LY Register Value
 * @returns Nothing
 */
static void gb_ppu_draw_line(void)
{
	if (bg_wn_enable) {
		gb_ppu_draw_line_background();
		if (wn_enable) {
			gb_ppu_draw_line_window();
		}
	} else {
		for (int j = 0; j < GAMEBOY_SCREEN_WIDTH; j++) {
			gb_ppu_update_frame_buffer(1, j);
		}
	}

	if (obj_enable) {
		gb_ppu_draw_line_objects();
	}

	if (ly == 143) {
		gb_ppu_display_frame_buffer(&ucGBLine[(0)]);
	}
}

uint8_t gb_ppu_memory_read(uint16_t address)
{
	return mem.map[address];
}

void gb_ppu_memory_write(uint16_t address, uint8_t data)
{
	uint32_t *palette_sel = NULL;

	switch (address) {
	case LCDC_ADDR:
		ppu_enable = CHK_BIT(data, 7) ? true : false;
		wn_display_addr = CHK_BIT(data, 6) ? TILE_MAP_LOCATION_HIGH : TILE_MAP_LOCATION_LOW;
		wn_enable = CHK_BIT(data, 5) ? true : false;
		tile_data_addr = CHK_BIT(data, 4) ? TILE_DATA_UNSIGNED_ADDR : TILE_DATA_SIGNED_ADDR;
		bg_display_addr = CHK_BIT(data, 3) ? TILE_MAP_LOCATION_HIGH : TILE_MAP_LOCATION_LOW;
		obj_size = CHK_BIT(data, 2) ? true : false;
		obj_enable = CHK_BIT(data, 1) ? true : false;
		bg_wn_enable = CHK_BIT(data, 0) ? true : false;
		mem.map[address] = data;
		return;

	case STAT_ADDR:
		lyc_int_sel = CHK_BIT(data, 6) ? true : false;
		mode_2_sel = CHK_BIT(data, 5) ? true : false;
		mode_1_sel = CHK_BIT(data, 4) ? true : false;
		mode_0_sel = CHK_BIT(data, 3) ? true : false;
		mem.map[address] = (data & ~0x7) | mem.map[address] & 0x7;
		return;

	case SCY_ADDR:
		scy = data;
		mem.map[address] = data;
		return;

	case SCX_ADDR:
		scx = data;
		mem.map[address] = data;
		return;

	case LY_ADDR:
		return;

	case LYC_ADDR:
		mem.map[address] = data;
		return;

	case DMA_ADDR:
		for (uint16_t i = 0; i < 40 * 4; i++)
			mem.map[OAM_BASE + i] = gb_memory_read((data << 8) + i);
		return;

	case BGP_ADDR:
		palette_sel = (palette_sel == NULL) ? bgp_color_to_palette : palette_sel;
	case OBP0_ADDR:
		palette_sel = (palette_sel == NULL) ? obp0_color_to_palette : palette_sel;
	case OBP1_ADDR:
		palette_sel = (palette_sel == NULL) ? obp1_color_to_palette : palette_sel;

		for (int i = 0; i < 4; i++) {
			switch ((data >> (i * 2)) & 0x03) {
			case 0:
				palette_sel[i] = 0XFF9BBC0F;
				break;
			case 1:
				palette_sel[i] = 0XFF8BAC0F;
				break;
			case 2:
				palette_sel[i] = 0XFF306230;
				break;
			case 3:
				palette_sel[i] = 0XFF0F380F;
				break;
			default:
				break;
			}
		}
		mem.map[address] = data;
		return;

	case WY_ADDR:
		wy = data;
		mem.map[address] = data;
		return;

	case WX_ADDR:
		wx = data;
		mem.map[address] = data;
		return;

	default:
		mem.map[address] = data;
		return;
	}
}
