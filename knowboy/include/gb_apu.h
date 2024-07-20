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
void gb_apu_trigger_ch1(void);
void gb_apu_trigger_ch2(void);
void gb_apu_trigger_ch3(void);
void gb_apu_trigger_ch4(void);

#endif /* INCLUDE_GB_APU_H_ */
