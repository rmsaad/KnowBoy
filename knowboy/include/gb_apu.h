/**
 * @file gb_apu.h
 * @brief API for the gameboy audio.
 *
 * @author Rami Saad
 * @date 2021-07-11
 */

#ifndef INCLUDE_GB_APU_H_
#define INCLUDE_GB_APU_H_

#include <stdint.h>

void gb_apu_init(uint16_t *buf, uint16_t *buf_pos, uint16_t buf_size);
void gb_apu_step(void);
uint8_t gb_apu_memory_read(uint16_t address);
void gb_apu_memory_write(uint16_t address, uint8_t data);

#endif /* INCLUDE_GB_APU_H_ */
