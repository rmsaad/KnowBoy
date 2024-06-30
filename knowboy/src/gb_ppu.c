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

// Line buffer start address

// PPU Registers
static uint8_t ly_value = 0;
static uint8_t stat_mode;

// State Ticks
static uint32_t line_cycle_counter;

// Palettes
static uint32_t bgp_color_to_palette[4];
static uint32_t obp0_color_to_palette[4];
static uint32_t obp1_color_to_palette[4];

// Frame Buffer variables
static uint32_t ucGBLine[160 * 3 * 2000];
static uint8_t ucBGWINline[160];
static uint32_t current_line;
static uint8_t ulScaleAmount = 1;
static uint32_t ulLineAdd;

// Function Pointer pointing to external function in display.c
static gb_ppu_display_frame_buffer_t gb_ppu_display_frame_buffer;

/*Function Prototypes*/
static void gb_ppu_check_bgp();
static void gb_ppu_check_obp0();
static void gb_ppu_check_obp1();
static void gb_ppu_check_lyc(uint8_t ly);
static void gb_ppu_set_stat_mode(uint8_t mode);
static void gb_ppu_update_frame_buffer(uint32_t data, int pixel_pos);
static void gb_ppu_draw_line_background(uint8_t ly, uint8_t scx, uint8_t scy,
					uint16_t tile_data_addr, uint16_t display_addr);
static void gb_ppu_draw_line_window(uint8_t ly, uint8_t wx, uint8_t wy, uint16_t tile_data_addr,
				    uint16_t display_addr);
static void gb_ppu_draw_line_objects(uint8_t ly);
static void gb_ppu_draw_line(uint8_t ly, uint8_t scx, uint8_t scy);
static uint16_t gb_ppu_get_tile_line_data(uint16_t tile_offset, uint8_t line_offset,
					  uint16_t tile_data_addr, uint16_t display_addr);
static uint16_t gb_ppu_get_background_window_tile_data_select();
static uint16_t gb_ppu_get_background_tile_display_select();
static uint16_t gb_ppu_get_window_tile_display_select();

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
	memset(ucGBLine, 0, 160 * 144 * 4 * ulScaleAmount);
	ly_value = 0;
	line_cycle_counter = 0;
	stat_mode = 0;
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
	static uint8_t fps_limiter = 0;

	if (gb_memory_read(LCDC_ADDR) & 0x80) { // check MSB of LCDC for screen en
		line_cycle_counter += 4;

		if (line_cycle_counter > 456) { // end of hblank or vblank
			ly_value++;
			gb_ppu_check_lyc(ly_value);
			if (ly_value > 153) { // end of vblank
				fps_limiter++;
				gb_ppu_set_stat_mode(STAT_MODE_2);
				ly_value = 0;

				if (CHK_BIT(gb_memory_read(STAT_ADDR), 5))
					gb_memory_set_bit(IF_ADDR, 1);
			}

			gb_memory_write(LY_ADDR, ly_value); // update LY register
			line_cycle_counter -= 456;
		}

		if (ly_value > 143) { // vblank region
			if (stat_mode != STAT_MODE_1) {
				gb_ppu_set_stat_mode(STAT_MODE_1);
				if (CHK_BIT(gb_memory_read(STAT_ADDR), 4))
					gb_memory_set_bit(IF_ADDR, 1);
				if (ly_value == 0x90) {
					gb_memory_set_bit(IF_ADDR, 0);
				}
			}
		} else {
			if (line_cycle_counter <= 80 && stat_mode != STAT_MODE_2) // oam region
				gb_ppu_set_stat_mode(STAT_MODE_2);
			else if (line_cycle_counter > 80 && line_cycle_counter <= 252 &&
				 stat_mode != STAT_MODE_3) { // vram region

				if (fps_limiter % 1 == 0) {
					gb_ppu_draw_line(ly_value, gb_memory_read(SCX_ADDR),
							 gb_memory_read(SCY_ADDR));
				}

				gb_ppu_set_stat_mode(STAT_MODE_3);
			} else if (line_cycle_counter > 252 && line_cycle_counter <= 456 &&
				   stat_mode != STAT_MODE_0) { // hblank region
				gb_ppu_set_stat_mode(STAT_MODE_0);
				if (CHK_BIT(gb_memory_read(STAT_ADDR), 3))
					gb_memory_set_bit(IF_ADDR, 1);
			}
		}
	} else { // screen is not enabled
		ly_value = 0;
		gb_memory_write(LY_ADDR, ly_value);
	}
}

/**
 * @brief Background and window color to palette conversion
 * @details Updates a color to its appropriate palette based on value of the
 * Background Palette Register. each number assigned corresponds to a color
 * referenced in STM32's L8 color mode.
 * @return Nothing
 */
static void gb_ppu_check_bgp()
{
	uint8_t BGP = gb_memory_read(BGP_ADDR);
	for (int i = 0; i < 4; i++) {
		switch ((BGP >> (i * 2)) & 0x03) {
		case 0:
			bgp_color_to_palette[i] = 0XFF9BBC0F;
			break;
		case 1:
			bgp_color_to_palette[i] = 0XFF8BAC0F;
			break;
		case 2:
			bgp_color_to_palette[i] = 0XFF306230;
			break;
		case 3:
			bgp_color_to_palette[i] = 0XFF0F380F;
			break;
		default:
			break;
		}
	}
}

/**
 * @brief OBP0 register color to palette conversion
 * @details Updates a color to its appropriate palette based on value of the
 * OBP0 Register. each number assigned corresponds to a color referenced in
 * STM32's L8 color mode.
 * @return Nothing
 */
static void gb_ppu_check_obp0()
{
	uint8_t BGP = gb_memory_read(OBP0_ADDR);
	for (int i = 0; i < 4; i++) {
		switch ((BGP >> (i * 2)) & 0x03) {
		case 0:
			obp0_color_to_palette[i] = 0XFF9BBC0F;
			break;
		case 1:
			obp0_color_to_palette[i] = 0XFF8BAC0F;
			break;
		case 2:
			obp0_color_to_palette[i] = 0XFF306230;
			break;
		case 3:
			obp0_color_to_palette[i] = 0XFF0F380F;
			break;
		default:
			break;
		}
	}
}

/**
 * @brief OBP1 register color to palette conversion
 * @details Updates a color to its appropriate palette based on value of the
 * OBP1 Register. each number assigned corresponds to a color referenced in
 * STM32's L8 color mode.
 * @return Nothing
 */
static void gb_ppu_check_obp1()
{
	uint8_t BGP = gb_memory_read(OBP1_ADDR);
	for (int i = 0; i < 4; i++) {
		switch ((BGP >> (i * 2)) & 0x03) {
		case 0:
			obp1_color_to_palette[i] = 0XFF9BBC0F;
			break;
		case 1:
			obp1_color_to_palette[i] = 0XFF8BAC0F;
			break;
		case 2:
			obp1_color_to_palette[i] = 0XFF306230;
			break;
		case 3:
			obp1_color_to_palette[i] = 0XFF0F380F;
			break;
		default:
			break;
		}
	}
}

/**
 * @brief Read LCDC Register to find appropriate Background & Tile Data Address
 * @details This function Reads from the LCDC Register and depending on the
 * value of the 4th bit, it will change the Background & Window Tile Data Select
 * Address to 0x8000 on high or to 0x8800 on low.
 * @return Background & Window Tile Data Select Address
 * @note Background & Window Tile Data Select should be 0x9000 on a low 4th bit
 * in a real Gameboy, but it is much simpler to implement this way with
 * emulation, the  0x400 offset is accounted for in gb_ppu_get_tile_line_data()
 */
static uint16_t gb_ppu_get_background_window_tile_data_select()
{
	return (gb_memory_read(LCDC_ADDR) & 0x10) ? TILE_DATA_UNSIGNED_ADDR : TILE_DATA_SIGNED_ADDR;
}

/**
 * @brief Read LCDC Register to find appropriate Background Tile Display Select
 * Address
 * @details This function Reads from the LCDC Register and depending on the
 * value of the 3rd bit, it will change the Background Tile Display Address to
 * 0X9C00 on high or to 0x9800 on low.
 * @return Background Tile Display Select Address
 */
static uint16_t gb_ppu_get_background_tile_display_select()
{
	return (gb_memory_read(LCDC_ADDR) & 0x08) ? TILE_MAP_LOCATION_HIGH : TILE_MAP_LOCATION_LOW;
}

/**
 * @brief Read LCDC Register to find appropriate Window Tile Display Select
 * Address
 * @details This function Reads from the LCDC Register and depending on the
 * value of the 4th bit, it will change the Window Tile Display Address to
 * 0X9C00 on high or to 0x9800 on low.
 * @return Window Tile Display Select Address
 */
static uint16_t gb_ppu_get_window_tile_display_select()
{
	return (gb_memory_read(LCDC_ADDR) & 0x40) ? TILE_MAP_LOCATION_HIGH : TILE_MAP_LOCATION_LOW;
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
					  uint16_t tile_data_addr, uint16_t display_addr)
{
	if (tile_data_addr == 0x8000) {
		return gb_memory_read_short(tile_data_addr +
					    (gb_memory_read(display_addr + tile_offset) * 0x10) +
					    line_offset);
	} else {
		int8_t temp = (int8_t)(gb_memory_read(display_addr + tile_offset));
		uint16_t temp2 = (temp + 128) * 0x10;
		return gb_memory_read_short(tile_data_addr + temp2 + line_offset);
	}
}

/**
 * @brief Compares LY Register to LYC
 * @details LYC Register compares itself to LY Register and Sets the appropriate
 * flag (2nd bit) in the STAT Register if they hold the same value
 * @param ly value of the LY Register
 * @return Nothing
 */
static void gb_ppu_check_lyc(uint8_t ly)
{
	if (ly == gb_memory_read(LYC_ADDR)) {
		gb_memory_set_bit(STAT_ADDR, 2);
		if (CHK_BIT(gb_memory_read(STAT_ADDR), 6))
			gb_memory_set_bit(IF_ADDR, 1);
	} else {
		gb_memory_reset_bit(STAT_ADDR, 2);
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
		gb_memory_reset_bit(STAT_ADDR, 1);
		gb_memory_reset_bit(STAT_ADDR, 0);
		break; // 00
	case STAT_MODE_1:
		gb_memory_reset_bit(STAT_ADDR, 1);
		gb_memory_set_bit(STAT_ADDR, 0);
		break; // 01
	case STAT_MODE_2:
		gb_memory_set_bit(STAT_ADDR, 1);
		gb_memory_reset_bit(STAT_ADDR, 0);
		break; // 10
	case STAT_MODE_3:
		gb_memory_set_bit(STAT_ADDR, 1);
		gb_memory_set_bit(STAT_ADDR, 0);
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
	pixel_pos *= ulScaleAmount;
	for (int xStretch = 0; xStretch < ulScaleAmount; xStretch++) {
		for (int yStretch = 0; yStretch < ulScaleAmount; yStretch++)
			ucGBLine[pixel_pos + xStretch + (current_line) + (ulLineAdd * yStretch)] =
				data;
	}
}

/**
 * @brief Update line buffer with background information
 * @details Populates the line buffer with Background information on the line ly
 * @param ly LY Register Value
 * @param scx X location of the left most position on the background tile map
 * @param scy Y location of the top most position on the background tile map
 * @param tile_data_addr Start Address of location in VRAM of Background Tiles
 * @param display_addr  Start Address holding the offset in VRAM for each Tile
 * displayed on Screen
 * @returns Nothing
 */
static void gb_ppu_draw_line_background(uint8_t ly, uint8_t scx, uint8_t scy,
					uint16_t tile_data_addr, uint16_t display_addr)
{
	uint16_t tile_offset = (((uint8_t)(scy + ly) / 8) * 32) +
			       (scx / 8); // gives the address offset in the tile map
	uint8_t line_offset = (((scy % 8) + ly) % 8) * 2; // gives the line offset in the tile
	uint8_t pixl_offset = scx % 8;			  // gives current pixel offset

	uint16_t first_tile = tile_offset % 32;
	uint16_t tile_data =
		gb_ppu_get_tile_line_data(tile_offset, line_offset, tile_data_addr,
					  display_addr); // tile data holds tile line information

	for (int j = 0; j < 160; j++) {

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
								      tile_data_addr, display_addr);
			else
				tile_data = gb_ppu_get_tile_line_data(tile_offset, line_offset,
								      tile_data_addr, display_addr);
		}
	}
}

/**
 * @brief Update line buffer with window information
 * @details Populates the line buffer with window information if it is currently
 * displayed on the line ly
 * @param ly LY Register Value
 * @param wx Window X position
 * @param wy Window Y position
 * @param tile_data_addr Start Address of location in VRAM of Window Tiles
 * @param display_addr  Start Address holding the offset in VRAM for each Tile
 * displayed on Screen
 * @returns Nothing
 */
static void gb_ppu_draw_line_window(uint8_t ly, uint8_t wx, uint8_t wy, uint16_t tile_data_addr,
				    uint16_t display_addr)
{
	if (wy > ly || wy > 143 || wx > 166)
		return;

	uint16_t tile_offset =
		(((uint8_t)(ly - wy) / 8) * 32);     // gives the address offset in the tile map
	uint8_t line_offset = (((ly - wy) % 8)) * 2; // gives the line offset in the tile
	uint8_t pixl_offset = (wx - 7) % 8;	     // gives current pixel offset

	uint16_t tile_data =
		gb_ppu_get_tile_line_data(tile_offset, line_offset, tile_data_addr,
					  display_addr); // tile data holds tile line information

	for (int j = (wx - 7); j < 160; j++) {
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
							      tile_data_addr, display_addr);
		}
	}
}

/**
 * @brief  Update line buffer with object information
 * @details Populates the line buffer with object sprites on line ly
 * @param ly LY Register Value
 * @returns Nothing
 */
static void gb_ppu_draw_line_objects(uint8_t ly)
{
	for (int obj = 0; obj < 40; obj++) {
		// must be signed for logic to work
		int16_t y_coordinate = gb_memory_read(OAM_BASE + (obj * 4)) - 16;
		// "" "" "" same here
		int16_t x_coordinate = gb_memory_read(OAM_BASE + (obj * 4) + 1) - 8;
		uint8_t data_tile = gb_memory_read(OAM_BASE + (obj * 4) + 2);
		uint8_t obj_prio = CHK_BIT(gb_memory_read(OAM_BASE + (obj * 4) + 3), 7);
		uint8_t obj_y_flip = CHK_BIT(gb_memory_read(OAM_BASE + (obj * 4) + 3), 6);
		uint8_t obj_x_flip = CHK_BIT(gb_memory_read(OAM_BASE + (obj * 4) + 3), 5);
		uint8_t obj_palette = CHK_BIT(gb_memory_read(OAM_BASE + (obj * 4) + 3), 4);
		uint8_t obj_size = CHK_BIT(gb_memory_read(LCDC_ADDR), 2);

		uint8_t obj_height = (obj_size == 0) ? 8 : 16;

		if (y_coordinate <= ly && (y_coordinate + obj_height) > ly) {

			uint8_t line_offset = obj_y_flip
						      ? ((obj_height - 1) - (ly - y_coordinate)) * 2
						      : (ly - y_coordinate) * 2;
			uint16_t tile_data = gb_memory_read_short(TILE_DATA_UNSIGNED_ADDR +
								  (data_tile * 0x10) + line_offset);
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
				    (x_coordinate + pixel_num) < 160) {
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
 * @param scx Scroll X Register Value
 * @param scy Scroll Y Register Value
 * @returns Nothing
 */
static void gb_ppu_draw_line(uint8_t ly, uint8_t scx, uint8_t scy)
{

	// update Palettes
	gb_ppu_check_bgp();
	gb_ppu_check_obp0();
	gb_ppu_check_obp1();

	uint16_t tile_data_addr = gb_ppu_get_background_window_tile_data_select();
	current_line = ly * ulScaleAmount * ulScaleAmount * 160;
	ulLineAdd = ulScaleAmount * 160;

	if (gb_memory_read(LCDC_ADDR) & 0x01) {
		gb_ppu_draw_line_background(ly, scx, scy, tile_data_addr,
					    gb_ppu_get_background_tile_display_select());
		if (gb_memory_read(LCDC_ADDR) & 0x20) {
			gb_ppu_draw_line_window(ly, gb_memory_read(WX_ADDR),
						gb_memory_read(WY_ADDR), tile_data_addr,
						gb_ppu_get_window_tile_display_select());
		}
	} else {
		for (int j = 0; j < 160; j++) {
			gb_ppu_update_frame_buffer(1, j);
		}
	}

	if (gb_memory_read(LCDC_ADDR) & 0x02) {
		gb_ppu_draw_line_objects(ly);
	}

	if (ly == 143) {
		gb_ppu_display_frame_buffer(&ucGBLine[(0)]);
	}
}
