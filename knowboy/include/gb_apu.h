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
void gb_apu_set_dac_ch1(uint8_t dac_mask);
void gb_apu_set_dac_ch2(uint8_t dac_mask);
void gb_apu_set_dac_ch3(uint8_t dac_mask);
void gb_apu_set_dac_ch4(uint8_t dac_mask);
void gb_apu_set_length_ch1(void);
void gb_apu_set_length_ch2(void);
void gb_apu_set_length_ch3(void);
void gb_apu_set_length_ch4(void);
void gb_apu_update_ch1_counter(void);
void gb_apu_update_ch2_counter(void);
void gb_apu_update_ch3_counter(void);
void gb_apu_update_ch4_counter(void);
void gb_apu_trigger_ch1(void);
void gb_apu_trigger_ch2(void);
void gb_apu_trigger_ch3(void);
void gb_apu_trigger_ch4(void);
void gb_apu_check_negate_ch1(void);
uint8_t gb_apu_memory_read(uint16_t address);
void gb_apu_memory_write(uint16_t address, uint8_t data);

#endif /* INCLUDE_GB_APU_H_ */
