/**
 * @file gb_ppu.h
 * @brief API for the pixel/picture processing unit.
 *
 * @author Rami Saad
 * @date 2021-06-11
 */

#ifndef INCLUDE_GB_PPU_H_
#define INCLUDE_GB_PPU_H_

#define GAMEBOY_SCREEN_WIDTH  160
#define GAMEBOY_SCREEN_HEIGHT 144

typedef void (*gb_ppu_display_frame_buffer_t)(uint32_t *);

void gb_ppu_set_display_frame_buffer(gb_ppu_display_frame_buffer_t display_frame_buffer);
void gb_ppu_step(void);
void gb_ppu_init(void);

#endif /* INCLUDE_GB_PPU_H_ */
