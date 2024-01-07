/**
 * @file gb_cpu_priv.h
 * @brief Private API and MACROs for the gameboy CPU.
 *
 * @author Rami Saad
 * @date 2021-06-06
 */

#ifndef SRC_GB_CPU_PRIV_H_
#define SRC_GB_CPU_PRIV_H_

#include "gb_common.h"
#include "gb_memory.h"

#include <stdint.h>
#include <stdio.h>

#define VBLANK_INTERRUPT  (1 << 0)
#define LCDSTAT_INTERRUPT (1 << 1)
#define TIMER_INTERRUPT	  (1 << 2)
#define SERIAL_INTERRUPT  (1 << 3)
#define JOYPAD_INTERRUPT  (1 << 4)

#define VBLANK_VECTOR  0x0040
#define LCDSTAT_VECTOR 0x0048
#define TIMER_VECTOR   0x0050
#define SERIAL_VECTOR  0x0058
#define JOYPAD_VECTOR  0x0060

#define Z_FLAG_BIT 7
#define N_FLAG_BIT 6
#define H_FLAG_BIT 5
#define C_FLAG_BIT 4

#define Z_FLAG_VAL (0x1 << Z_FLAG_BIT)
#define N_FLAG_VAL (0x1 << N_FLAG_BIT)
#define H_FLAG_VAL (0x1 << H_FLAG_BIT)
#define C_FLAG_VAL (0x1 << C_FLAG_BIT)

#define CUSTOM_CYCLES 255

/* Increment 8 bit register */
static inline void gb_cpu_inc_register(uint8_t *reg, uint8_t *flag_reg)
{
	(((*reg & 0x0F) == 0x0F) ? SET_BIT(*flag_reg, H_FLAG_BIT) : RST_BIT(*flag_reg, H_FLAG_BIT));
	++(*reg);
	RST_BIT(*flag_reg, N_FLAG_BIT);
	(*reg != 0) ? RST_BIT(*flag_reg, Z_FLAG_BIT) : SET_BIT(*flag_reg, Z_FLAG_BIT);
}

/* Decrement 8 bit register */
static inline void gb_cpu_dec_register(uint8_t *reg, uint8_t *flag_reg)
{
	((*reg & 0x0F) != 0) ? RST_BIT(*flag_reg, H_FLAG_BIT) : SET_BIT(*flag_reg, H_FLAG_BIT);
	--(*reg);
	SET_BIT(*flag_reg, N_FLAG_BIT);
	(*reg != 0) ? RST_BIT(*flag_reg, Z_FLAG_BIT) : SET_BIT(*flag_reg, Z_FLAG_BIT);
}

/* 16 bit addition to the HL register */
static inline void gb_cpu_addition_HL_register(uint16_t *reg_hl, uint16_t *reg_16,
					       uint8_t *flag_reg)
{
	uint32_t temp_res = *reg_hl + *reg_16;
	(temp_res & 0xFFFF0000) ? SET_BIT(*flag_reg, C_FLAG_BIT) : RST_BIT(*flag_reg, C_FLAG_BIT);
	(((temp_res & 0x07FF) < (*reg_hl & 0x07FF))) ? SET_BIT(*flag_reg, H_FLAG_BIT)
						     : RST_BIT(*flag_reg, H_FLAG_BIT);
	*reg_hl = (uint16_t)(temp_res & 0xffff);
	RST_BIT(*flag_reg, N_FLAG_BIT);
}

/* Adds the value of a register/ memory location to the A register */
static inline void gb_cpu_addition_A_register(uint8_t *reg_a, uint8_t *flag_reg, uint8_t *reg_x)
{
	uint32_t temp_res = *reg_a + *reg_x;
	((*reg_a & 0xF) + (*reg_x & 0xF) > 0xF) ? SET_BIT(*flag_reg, H_FLAG_BIT)
						: RST_BIT(*flag_reg, H_FLAG_BIT);
	RST_BIT(*flag_reg, N_FLAG_BIT);
	(temp_res > 0xFF) ? SET_BIT(*flag_reg, C_FLAG_BIT) : RST_BIT(*flag_reg, C_FLAG_BIT);
	*reg_a = (uint8_t)temp_res;
	(*reg_a != 0) ? RST_BIT(*flag_reg, Z_FLAG_BIT) : SET_BIT(*flag_reg, Z_FLAG_BIT);
}

/* Adds the value of a register/ memory location + carry to the A register */
static inline void gb_cpu_addition_carry_A_register(uint8_t *reg_a, uint8_t *flag_reg,
						    uint8_t *reg_x)
{
	uint32_t temp_res = *reg_a + *reg_x + CHK_BIT(*flag_reg, C_FLAG_BIT);
	((*reg_a & 0xF) + ((*reg_x & 0xF) + CHK_BIT(*flag_reg, C_FLAG_BIT)) > 0xF)
		? SET_BIT(*flag_reg, H_FLAG_BIT)
		: RST_BIT(*flag_reg, H_FLAG_BIT);
	RST_BIT(*flag_reg, N_FLAG_BIT);
	(temp_res > 0xFF) ? SET_BIT(*flag_reg, C_FLAG_BIT) : RST_BIT(*flag_reg, C_FLAG_BIT);
	*reg_a = (uint8_t)temp_res;
	(*reg_a != 0) ? RST_BIT(*flag_reg, Z_FLAG_BIT) : SET_BIT(*flag_reg, Z_FLAG_BIT);
}

/* Subtracts the value of a register/ memory location from the A register */
static inline void gb_cpu_subtraction_A_register(uint8_t *reg_a, uint8_t *flag_reg, uint8_t *reg_x)
{
	(*reg_x > *reg_a) ? SET_BIT(*flag_reg, C_FLAG_BIT) : RST_BIT(*flag_reg, C_FLAG_BIT);
	((*reg_x & 0x0F) > (*reg_a & 0x0F)) ? SET_BIT(*flag_reg, H_FLAG_BIT)
					    : RST_BIT(*flag_reg, H_FLAG_BIT);
	*reg_a -= *reg_x;
	(*reg_a != 0) ? RST_BIT(*flag_reg, Z_FLAG_BIT) : SET_BIT(*flag_reg, Z_FLAG_BIT);
	SET_BIT(*flag_reg, N_FLAG_BIT);
}

/* Subtracts the value of a register/ memory location + carry from the A register */
static inline void gb_cpu_subtraction_carry_A_register(uint8_t *reg_a, uint8_t *flag_reg,
						       uint8_t *reg_x)
{
	uint32_t temp_res = *reg_x + CHK_BIT(*flag_reg, C_FLAG_BIT);
	((*reg_a & 0xF) - ((*reg_x & 0xF) + CHK_BIT(*flag_reg, C_FLAG_BIT)) < 0x0)
		? SET_BIT(*flag_reg, H_FLAG_BIT)
		: RST_BIT(*flag_reg, H_FLAG_BIT);
	(*reg_a - *reg_x - CHK_BIT(*flag_reg, C_FLAG_BIT) < 0) ? SET_BIT(*flag_reg, C_FLAG_BIT)
							       : RST_BIT(*flag_reg, C_FLAG_BIT);
	*reg_a -= (uint8_t)temp_res;
	(*reg_a != 0) ? RST_BIT(*flag_reg, Z_FLAG_BIT) : SET_BIT(*flag_reg, Z_FLAG_BIT);
	SET_BIT(*flag_reg, N_FLAG_BIT);
}

/* Compare register A with another register/memory location */
static inline void gb_cpu_compare_A_register(uint8_t *reg_a, uint8_t *flag_reg, uint8_t *reg_x)
{
	(*reg_x > *reg_a) ? SET_BIT(*flag_reg, C_FLAG_BIT) : RST_BIT(*flag_reg, C_FLAG_BIT);
	((*reg_x & 0x0F) > (*reg_a & 0x0F)) ? SET_BIT(*flag_reg, H_FLAG_BIT)
					    : RST_BIT(*flag_reg, H_FLAG_BIT);
	(*reg_a == *reg_x) ? SET_BIT(*flag_reg, Z_FLAG_BIT) : RST_BIT(*flag_reg, Z_FLAG_BIT);
	SET_BIT(*flag_reg, N_FLAG_BIT);
}

/* Pop two bytes from the stack and jump to that address */
static inline void gb_cpu_return_from_stack(uint16_t *reg_sp, uint16_t *reg_pc)
{
	*reg_pc = CAT_BYTES(gb_memory_read(*reg_sp), gb_memory_read(*reg_sp + 1));
	*reg_sp += 2;
}

/*Pop two bytes off of the stack and into 16 bit register, increments the stack pointer by 2 */
static inline void gb_cpu_pop_from_stack(uint16_t *reg_sp, uint16_t *reg_16)
{
	*reg_16 = CAT_BYTES(gb_memory_read(*reg_sp), gb_memory_read(*reg_sp + 1));
	*reg_sp += 2;
}

/*Pushes 16 bit register onto stack */
static inline void gb_cpu_push_to_stack(uint16_t *reg_sp, uint16_t *reg_16)
{
	gb_memory_write(*reg_sp - 1, (uint8_t)((*reg_16 & 0xFF00) >> 8));
	gb_memory_write(*reg_sp - 2, (uint8_t)(*reg_16 & 0x00FF));
	*reg_sp -= 2;
}

/* Rotate register/memory left, bit 7 becomes carry flag */
static inline void gb_cpu_rotate_left_with_carry(uint8_t *reg_x, uint8_t *flag_reg)
{
	uint8_t temp_carry = CHK_BIT(*reg_x, 7);
	*reg_x <<= 1;
	*reg_x += temp_carry;
	(*reg_x == 0) ? SET_BIT(*flag_reg, Z_FLAG_BIT) : RST_BIT(*flag_reg, Z_FLAG_BIT);
	RST_BIT(*flag_reg, N_FLAG_BIT);
	RST_BIT(*flag_reg, H_FLAG_BIT);
	(temp_carry != 0) ? SET_BIT(*flag_reg, C_FLAG_BIT) : RST_BIT(*flag_reg, C_FLAG_BIT);
}

/* Rotate register/memory right, bit 0 becomes carry flag */
static inline void gb_cpu_rotate_right_with_carry(uint8_t *reg_x, uint8_t *flag_reg)
{
	uint8_t temp_carry = CHK_BIT(*reg_x, 0);
	*reg_x >>= 1;
	if (temp_carry)
		SET_BIT(*reg_x, 7);
	(*reg_x == 0) ? SET_BIT(*flag_reg, Z_FLAG_BIT) : RST_BIT(*flag_reg, Z_FLAG_BIT);
	RST_BIT(*flag_reg, N_FLAG_BIT);
	RST_BIT(*flag_reg, H_FLAG_BIT);
	(temp_carry != 0) ? SET_BIT(*flag_reg, C_FLAG_BIT) : RST_BIT(*flag_reg, C_FLAG_BIT);
}

/* Rotate register/memory left through carry */
static inline void gb_cpu_rotate_left_through_carry(uint8_t *reg_x, uint8_t *flag_reg)
{
	uint8_t prev_carry = CHK_BIT(*flag_reg, C_FLAG_BIT);
	((*reg_x & 0x80) != 0) ? SET_BIT(*flag_reg, C_FLAG_BIT) : RST_BIT(*flag_reg, C_FLAG_BIT);
	*reg_x <<= 1;
	*reg_x += prev_carry;
	RST_BIT(*flag_reg, N_FLAG_BIT);
	RST_BIT(*flag_reg, H_FLAG_BIT);
	(*reg_x == 0) ? SET_BIT(*flag_reg, Z_FLAG_BIT) : RST_BIT(*flag_reg, Z_FLAG_BIT);
}

/* Rotate register/memory right through carry */
static inline void gb_cpu_rotate_right_through_carry(uint8_t *reg_x, uint8_t *flag_reg)
{
	uint8_t prev_carry = CHK_BIT(*flag_reg, C_FLAG_BIT);
	((*reg_x & 0x01) != 0) ? SET_BIT(*flag_reg, C_FLAG_BIT) : RST_BIT(*flag_reg, C_FLAG_BIT);
	*reg_x >>= 1;
	*reg_x += (prev_carry << 7);
	RST_BIT(*flag_reg, N_FLAG_BIT);
	RST_BIT(*flag_reg, H_FLAG_BIT);
	(*reg_x == 0) ? SET_BIT(*flag_reg, Z_FLAG_BIT) : RST_BIT(*flag_reg, Z_FLAG_BIT);
}

/* Shift register/memory left into the carry, The least significant bit is set to zero */
static inline void gb_cpu_shift_left_into_carry(uint8_t *reg_x, uint8_t *flag_reg)
{
	((*reg_x & 0x80) != 0) ? SET_BIT(*flag_reg, C_FLAG_BIT) : RST_BIT(*flag_reg, C_FLAG_BIT);
	*reg_x <<= 1;
	RST_BIT(*flag_reg, N_FLAG_BIT);
	RST_BIT(*flag_reg, H_FLAG_BIT);
	(*reg_x == 0) ? SET_BIT(*flag_reg, Z_FLAG_BIT) : RST_BIT(*flag_reg, Z_FLAG_BIT);
}

/* Shift register/memory right with carry */
static inline void gb_cpu_shift_right_with_carry(uint8_t *reg_x, uint8_t *flag_reg)
{
	uint8_t tempMSB = *reg_x & 0x80;
	((*reg_x & 0x01) != 0) ? SET_BIT(*flag_reg, C_FLAG_BIT) : RST_BIT(*flag_reg, C_FLAG_BIT);
	*reg_x >>= 1;
	*reg_x += tempMSB;
	RST_BIT(*flag_reg, N_FLAG_BIT);
	RST_BIT(*flag_reg, H_FLAG_BIT);
	(*reg_x == 0) ? SET_BIT(*flag_reg, Z_FLAG_BIT) : RST_BIT(*flag_reg, Z_FLAG_BIT);
}

/* Swap upper and lower nibbles of a register or memory location */
static inline void gb_cpu_swap_nibbles(uint8_t *reg_x, uint8_t *flag_reg)
{
	*reg_x = ((*reg_x & 0x0F) << 4) | ((*reg_x & 0xF0) >> 4);
	(*reg_x == 0) ? SET_BIT(*flag_reg, Z_FLAG_BIT) : RST_BIT(*flag_reg, Z_FLAG_BIT);
	RST_BIT(*flag_reg, N_FLAG_BIT);
	RST_BIT(*flag_reg, H_FLAG_BIT);
	RST_BIT(*flag_reg, C_FLAG_BIT);
}

/* Shift register/memory right into the carry, The most significant bit is set to zero */
static inline void gb_cpu_shift_right_into_carry(uint8_t *reg_x, uint8_t *flag_reg)
{
	((*reg_x & 0x01) != 0) ? SET_BIT(*flag_reg, C_FLAG_BIT) : RST_BIT(*flag_reg, C_FLAG_BIT);
	*reg_x >>= 1;
	RST_BIT(*flag_reg, N_FLAG_BIT);
	RST_BIT(*flag_reg, H_FLAG_BIT);
	(*reg_x == 0) ? SET_BIT(*flag_reg, Z_FLAG_BIT) : RST_BIT(*flag_reg, Z_FLAG_BIT);
}

/* Test the value of a bit in a register/memory and sets the appropriate flags in the flag register
 */
static inline void gb_cpu_bit_check(uint8_t *reg_x, uint8_t bit, uint8_t *flag_reg)
{
	(CHK_BIT(*reg_x, bit) != 0) ? RST_BIT(*flag_reg, Z_FLAG_BIT)
				    : SET_BIT(*flag_reg, Z_FLAG_BIT);
	RST_BIT(*flag_reg, N_FLAG_BIT);
	SET_BIT(*flag_reg, H_FLAG_BIT);
}

/* Resets a bit in the memory address that is held in the HL register */
static inline void gb_cpu_reset_HL_address_bit(uint16_t *reg_hl, uint8_t bit)
{
	uint8_t temp_res = gb_memory_read(*reg_hl);
	RST_BIT(temp_res, bit);
	gb_memory_write(*reg_hl, temp_res);
}

/* Sets a bit in the memory address that is held in the HL register */
static inline void gb_cpu_set_HL_address_bit(uint16_t *reg_hl, uint8_t bit)
{
	uint8_t temp_res = gb_memory_read(*reg_hl);
	SET_BIT(temp_res, bit);
	gb_memory_write(*reg_hl, temp_res);
}

/* type defs */
typedef struct {
	uint8_t bytes;
	uint8_t cycles;
	uint8_t current_cycle;
} gb_instr_info_t;

typedef void (*gb_instr_func_t)(gb_instr_info_t *);

typedef struct {
	gb_instr_func_t instr;
	gb_instr_info_t info;
} gb_instr_t;

/* function prototypes -----------------------------------------------*/
void gb_cpu_NOP(gb_instr_info_t *info);
void gb_cpu_LOAD_BC_d16(gb_instr_info_t *info);
void gb_cpu_LOAD_BC_A(gb_instr_info_t *info);
void gb_cpu_INC_BC(gb_instr_info_t *info);
void gb_cpu_INC_B(gb_instr_info_t *info);
void gb_cpu_DEC_B(gb_instr_info_t *info);
void gb_cpu_LOAD_B_d8(gb_instr_info_t *info);
void gb_cpu_RLCA(gb_instr_info_t *info);
void gb_cpu_LOAD_a16_SP(gb_instr_info_t *info);
void gb_cpu_ADD_HL_BC(gb_instr_info_t *info);
void gb_cpu_LOAD_A_BC(gb_instr_info_t *info);
void gb_cpu_DEC_BC(gb_instr_info_t *info);
void gb_cpu_INC_C(gb_instr_info_t *info);
void gb_cpu_DEC_C(gb_instr_info_t *info);
void gb_cpu_LOAD_C_d8(gb_instr_info_t *info);
void gb_cpu_RRCA(gb_instr_info_t *info);

void gb_cpu_STOP(gb_instr_info_t *info);
void gb_cpu_LOAD_DE_d16(gb_instr_info_t *info);
void gb_cpu_LOAD_DE_A(gb_instr_info_t *info);
void gb_cpu_INC_DE(gb_instr_info_t *info);
void gb_cpu_INC_D(gb_instr_info_t *info);
void gb_cpu_DEC_D(gb_instr_info_t *info);
void gb_cpu_LOAD_D_d8(gb_instr_info_t *info);
void gb_cpu_RLA(gb_instr_info_t *info);
void gb_cpu_JR_r8(gb_instr_info_t *info);
void gb_cpu_ADD_HL_DE(gb_instr_info_t *info);
void gb_cpu_LOAD_A_DE(gb_instr_info_t *info);
void gb_cpu_DEC_DE(gb_instr_info_t *info);
void gb_cpu_INC_E(gb_instr_info_t *info);
void gb_cpu_DEC_E(gb_instr_info_t *info);
void gb_cpu_LOAD_E_d8(gb_instr_info_t *info);
void gb_cpu_RRA(gb_instr_info_t *info);

void gb_cpu_JR_NZ_r8(gb_instr_info_t *info);
void gb_cpu_LOAD_HL_d16(gb_instr_info_t *info);
void gb_cpu_LOAD_HLI_A(gb_instr_info_t *info);
void gb_cpu_INC_HL(gb_instr_info_t *info);
void gb_cpu_INC_H(gb_instr_info_t *info);
void gb_cpu_DEC_H(gb_instr_info_t *info);
void gb_cpu_LOAD_H_d8(gb_instr_info_t *info);
void gb_cpu_DAA(gb_instr_info_t *info);
void gb_cpu_JR_Z_r8(gb_instr_info_t *info);
void gb_cpu_ADD_HL_HL(gb_instr_info_t *info);
void gb_cpu_LOAD_A_HLI(gb_instr_info_t *info);
void gb_cpu_DEC_HL(gb_instr_info_t *info);
void gb_cpu_INC_L(gb_instr_info_t *info);
void gb_cpu_DEC_L(gb_instr_info_t *info);
void gb_cpu_LOAD_L_d8(gb_instr_info_t *info);
void gb_cpu_CPL(gb_instr_info_t *info);

void gb_cpu_JR_NC_r8(gb_instr_info_t *info);
void gb_cpu_LOAD_SP_d16(gb_instr_info_t *info);
void gb_cpu_LOAD_HLD_A(gb_instr_info_t *info);
void gb_cpu_INC_SP(gb_instr_info_t *info);
void gb_cpu_INC_HL_ADDR(gb_instr_info_t *info);
void gb_cpu_DEC_HL_ADDR(gb_instr_info_t *info);
void gb_cpu_LOAD_HL_d8(gb_instr_info_t *info);
void gb_cpu_SCF(gb_instr_info_t *info);
void gb_cpu_JR_C_r8(gb_instr_info_t *info);
void gb_cpu_ADD_HL_SP(gb_instr_info_t *info);
void gb_cpu_LOAD_A_HLD(gb_instr_info_t *info);
void gb_cpu_DEC_SP(gb_instr_info_t *info);
void gb_cpu_INC_A(gb_instr_info_t *info);
void gb_cpu_DEC_A(gb_instr_info_t *info);
void gb_cpu_LOAD_A_d8(gb_instr_info_t *info);
void gb_cpu_CCF(gb_instr_info_t *info);

void gb_cpu_LOAD_B_B(gb_instr_info_t *info);
void gb_cpu_LOAD_B_C(gb_instr_info_t *info);
void gb_cpu_LOAD_B_D(gb_instr_info_t *info);
void gb_cpu_LOAD_B_E(gb_instr_info_t *info);
void gb_cpu_LOAD_B_H(gb_instr_info_t *info);
void gb_cpu_LOAD_B_L(gb_instr_info_t *info);
void gb_cpu_LOAD_B_HL(gb_instr_info_t *info);
void gb_cpu_LOAD_B_A(gb_instr_info_t *info);
void gb_cpu_LOAD_C_B(gb_instr_info_t *info);
void gb_cpu_LOAD_C_C(gb_instr_info_t *info);
void gb_cpu_LOAD_C_D(gb_instr_info_t *info);
void gb_cpu_LOAD_C_E(gb_instr_info_t *info);
void gb_cpu_LOAD_C_H(gb_instr_info_t *info);
void gb_cpu_LOAD_C_L(gb_instr_info_t *info);
void gb_cpu_LOAD_C_HL(gb_instr_info_t *info);
void gb_cpu_LOAD_C_A(gb_instr_info_t *info);

void gb_cpu_LOAD_D_B(gb_instr_info_t *info);
void gb_cpu_LOAD_D_C(gb_instr_info_t *info);
void gb_cpu_LOAD_D_D(gb_instr_info_t *info);
void gb_cpu_LOAD_D_E(gb_instr_info_t *info);
void gb_cpu_LOAD_D_H(gb_instr_info_t *info);
void gb_cpu_LOAD_D_L(gb_instr_info_t *info);
void gb_cpu_LOAD_D_HL(gb_instr_info_t *info);
void gb_cpu_LOAD_D_A(gb_instr_info_t *info);
void gb_cpu_LOAD_E_B(gb_instr_info_t *info);
void gb_cpu_LOAD_E_C(gb_instr_info_t *info);
void gb_cpu_LOAD_E_D(gb_instr_info_t *info);
void gb_cpu_LOAD_E_E(gb_instr_info_t *info);
void gb_cpu_LOAD_E_H(gb_instr_info_t *info);
void gb_cpu_LOAD_E_L(gb_instr_info_t *info);
void gb_cpu_LOAD_E_HL(gb_instr_info_t *info);
void gb_cpu_LOAD_E_A(gb_instr_info_t *info);

void gb_cpu_LOAD_H_B(gb_instr_info_t *info);
void gb_cpu_LOAD_H_C(gb_instr_info_t *info);
void gb_cpu_LOAD_H_D(gb_instr_info_t *info);
void gb_cpu_LOAD_H_E(gb_instr_info_t *info);
void gb_cpu_LOAD_H_H(gb_instr_info_t *info);
void gb_cpu_LOAD_H_L(gb_instr_info_t *info);
void gb_cpu_LOAD_H_HL(gb_instr_info_t *info);
void gb_cpu_LOAD_H_A(gb_instr_info_t *info);
void gb_cpu_LOAD_L_B(gb_instr_info_t *info);
void gb_cpu_LOAD_L_C(gb_instr_info_t *info);
void gb_cpu_LOAD_L_D(gb_instr_info_t *info);
void gb_cpu_LOAD_L_E(gb_instr_info_t *info);
void gb_cpu_LOAD_L_H(gb_instr_info_t *info);
void gb_cpu_LOAD_L_L(gb_instr_info_t *info);
void gb_cpu_LOAD_L_HL(gb_instr_info_t *info);
void gb_cpu_LOAD_L_A(gb_instr_info_t *info);

void gb_cpu_LOAD_HL_B(gb_instr_info_t *info);
void gb_cpu_LOAD_HL_C(gb_instr_info_t *info);
void gb_cpu_LOAD_HL_D(gb_instr_info_t *info);
void gb_cpu_LOAD_HL_E(gb_instr_info_t *info);
void gb_cpu_LOAD_HL_H(gb_instr_info_t *info);
void gb_cpu_LOAD_HL_L(gb_instr_info_t *info);
void gb_cpu_HALT(gb_instr_info_t *info);
void gb_cpu_LOAD_HL_A(gb_instr_info_t *info);
void gb_cpu_LOAD_A_B(gb_instr_info_t *info);
void gb_cpu_LOAD_A_C(gb_instr_info_t *info);
void gb_cpu_LOAD_A_D(gb_instr_info_t *info);
void gb_cpu_LOAD_A_E(gb_instr_info_t *info);
void gb_cpu_LOAD_A_H(gb_instr_info_t *info);
void gb_cpu_LOAD_A_L(gb_instr_info_t *info);
void gb_cpu_LOAD_A_HL(gb_instr_info_t *info);
void gb_cpu_LOAD_A_A(gb_instr_info_t *info);

void gb_cpu_ADD_A_B(gb_instr_info_t *info);
void gb_cpu_ADD_A_C(gb_instr_info_t *info);
void gb_cpu_ADD_A_D(gb_instr_info_t *info);
void gb_cpu_ADD_A_E(gb_instr_info_t *info);
void gb_cpu_ADD_A_H(gb_instr_info_t *info);
void gb_cpu_ADD_A_L(gb_instr_info_t *info);
void gb_cpu_ADD_A_HL(gb_instr_info_t *info);
void gb_cpu_ADD_A_A(gb_instr_info_t *info);
void gb_cpu_ADC_A_B(gb_instr_info_t *info);
void gb_cpu_ADC_A_C(gb_instr_info_t *info);
void gb_cpu_ADC_A_D(gb_instr_info_t *info);
void gb_cpu_ADC_A_E(gb_instr_info_t *info);
void gb_cpu_ADC_A_H(gb_instr_info_t *info);
void gb_cpu_ADC_A_L(gb_instr_info_t *info);
void gb_cpu_ADC_A_HL(gb_instr_info_t *info);
void gb_cpu_ADC_A_A(gb_instr_info_t *info);

void gb_cpu_SUB_B(gb_instr_info_t *info);
void gb_cpu_SUB_C(gb_instr_info_t *info);
void gb_cpu_SUB_D(gb_instr_info_t *info);
void gb_cpu_SUB_E(gb_instr_info_t *info);
void gb_cpu_SUB_H(gb_instr_info_t *info);
void gb_cpu_SUB_L(gb_instr_info_t *info);
void gb_cpu_SUB_HL(gb_instr_info_t *info);
void gb_cpu_SUB_A(gb_instr_info_t *info);
void gb_cpu_SBC_A_B(gb_instr_info_t *info);
void gb_cpu_SBC_A_C(gb_instr_info_t *info);
void gb_cpu_SBC_A_D(gb_instr_info_t *info);
void gb_cpu_SBC_A_E(gb_instr_info_t *info);
void gb_cpu_SBC_A_H(gb_instr_info_t *info);
void gb_cpu_SBC_A_L(gb_instr_info_t *info);
void gb_cpu_SBC_A_HL(gb_instr_info_t *info);
void gb_cpu_SBC_A_A(gb_instr_info_t *info);

void gb_cpu_AND_B(gb_instr_info_t *info);
void gb_cpu_AND_C(gb_instr_info_t *info);
void gb_cpu_AND_D(gb_instr_info_t *info);
void gb_cpu_AND_E(gb_instr_info_t *info);
void gb_cpu_AND_H(gb_instr_info_t *info);
void gb_cpu_AND_L(gb_instr_info_t *info);
void gb_cpu_AND_HL(gb_instr_info_t *info);
void gb_cpu_AND_A(gb_instr_info_t *info);
void gb_cpu_XOR_B(gb_instr_info_t *info);
void gb_cpu_XOR_C(gb_instr_info_t *info);
void gb_cpu_XOR_D(gb_instr_info_t *info);
void gb_cpu_XOR_E(gb_instr_info_t *info);
void gb_cpu_XOR_H(gb_instr_info_t *info);
void gb_cpu_XOR_L(gb_instr_info_t *info);
void gb_cpu_XOR_HL(gb_instr_info_t *info);
void gb_cpu_XOR_A(gb_instr_info_t *info);

void gb_cpu_OR_B(gb_instr_info_t *info);
void gb_cpu_OR_C(gb_instr_info_t *info);
void gb_cpu_OR_D(gb_instr_info_t *info);
void gb_cpu_OR_E(gb_instr_info_t *info);
void gb_cpu_OR_H(gb_instr_info_t *info);
void gb_cpu_OR_L(gb_instr_info_t *info);
void gb_cpu_OR_HL(gb_instr_info_t *info);
void gb_cpu_OR_A(gb_instr_info_t *info);
void gb_cpu_CP_B(gb_instr_info_t *info);
void gb_cpu_CP_C(gb_instr_info_t *info);
void gb_cpu_CP_D(gb_instr_info_t *info);
void gb_cpu_CP_E(gb_instr_info_t *info);
void gb_cpu_CP_H(gb_instr_info_t *info);
void gb_cpu_CP_L(gb_instr_info_t *info);
void gb_cpu_CP_HL(gb_instr_info_t *info);
void gb_cpu_CP_A(gb_instr_info_t *info);

void gb_cpu_RET_NZ(gb_instr_info_t *info);
void gb_cpu_POP_BC(gb_instr_info_t *info);
void gb_cpu_JP_NZ_a16(gb_instr_info_t *info);
void gb_cpu_JP_a16(gb_instr_info_t *info);
void gb_cpu_CALL_NZ_a16(gb_instr_info_t *info);
void gb_cpu_PUSH_BC(gb_instr_info_t *info);
void gb_cpu_ADD_A_d8(gb_instr_info_t *info);
void gb_cpu_RST_00H(gb_instr_info_t *info);
void gb_cpu_RET_Z(gb_instr_info_t *info);
void gb_cpu_RET(gb_instr_info_t *info);
void gb_cpu_JP_Z_a16(gb_instr_info_t *info);
void gb_cpu_PREFIX(gb_instr_info_t *info);
void gb_cpu_CALL_Z_a16(gb_instr_info_t *info);
void gb_cpu_CALL_a16(gb_instr_info_t *info);
void gb_cpu_ADC_A_d8(gb_instr_info_t *info);
void gb_cpu_RST_08H(gb_instr_info_t *info);

void gb_cpu_RET_NC(gb_instr_info_t *info);
void gb_cpu_POP_DE(gb_instr_info_t *info);
void gb_cpu_JP_NC_a16(gb_instr_info_t *info);
// -----------
void gb_cpu_CALL_NC_a16(gb_instr_info_t *info);
void gb_cpu_PUSH_DE(gb_instr_info_t *info);
void gb_cpu_SUB_d8(gb_instr_info_t *info);
void gb_cpu_RST_10H(gb_instr_info_t *info);
void gb_cpu_RET_C(gb_instr_info_t *info);
void gb_cpu_RETI(gb_instr_info_t *info);
void gb_cpu_JP_C_a16(gb_instr_info_t *info);
// -----------
void gb_cpu_CALL_C_a16(gb_instr_info_t *info);
// -----------
void gb_cpu_SBC_A_d8(gb_instr_info_t *info);
void gb_cpu_RST_18H(gb_instr_info_t *info);

void gb_cpu_LOAD_a8_A(gb_instr_info_t *info);
void gb_cpu_POP_HL(gb_instr_info_t *info);
void gb_cpu_LOAD_fC_A(gb_instr_info_t *info);
// -----------
// -----------
void gb_cpu_PUSH_HL(gb_instr_info_t *info);
void gb_cpu_AND_d8(gb_instr_info_t *info);
void gb_cpu_RST20H(gb_instr_info_t *info);
void gb_cpu_ADD_SP_r8(gb_instr_info_t *info);
void gb_cpu_JP_HL(gb_instr_info_t *info);
void gb_cpu_LOAD_a16_A(gb_instr_info_t *info);
// -----------
// -----------
// -----------
void gb_cpu_XOR_d8(gb_instr_info_t *info);
void gb_cpu_RST_28H(gb_instr_info_t *info);

void gb_cpu_LOAD_A_a8(gb_instr_info_t *info);
void gb_cpu_POP_AF(gb_instr_info_t *info);
void gb_cpu_LOAD_A_fC(gb_instr_info_t *info);
void gb_cpu_DI(gb_instr_info_t *info);
// -----------
void gb_cpu_PUSH_AF(gb_instr_info_t *info);
void gb_cpu_OR_d8(gb_instr_info_t *info);
void gb_cpu_RST_30H(gb_instr_info_t *info);
void gb_cpu_LOAD_HL_SP_r8(gb_instr_info_t *info);
void gb_cpu_LOAD_SP_HL(gb_instr_info_t *info);
void gb_cpu_LOAD_A_a16(gb_instr_info_t *info);
void gb_cpu_EI(gb_instr_info_t *info);
// -----------
// -----------
void gb_cpu_CP_d8(gb_instr_info_t *info);
void gb_cpu_RST_38H(gb_instr_info_t *info);

/* Prefix instruction */

void gb_cpu_RLC_B(gb_instr_info_t *info);
void gb_cpu_RLC_C(gb_instr_info_t *info);
void gb_cpu_RLC_D(gb_instr_info_t *info);
void gb_cpu_RLC_E(gb_instr_info_t *info);
void gb_cpu_RLC_H(gb_instr_info_t *info);
void gb_cpu_RLC_L(gb_instr_info_t *info);
void gb_cpu_RLC_HL(gb_instr_info_t *info);
void gb_cpu_RLC_A(gb_instr_info_t *info);
void gb_cpu_RRC_B(gb_instr_info_t *info);
void gb_cpu_RRC_C(gb_instr_info_t *info);
void gb_cpu_RRC_D(gb_instr_info_t *info);
void gb_cpu_RRC_E(gb_instr_info_t *info);
void gb_cpu_RRC_H(gb_instr_info_t *info);
void gb_cpu_RRC_L(gb_instr_info_t *info);
void gb_cpu_RRC_HL(gb_instr_info_t *info);
void gb_cpu_RRC_A(gb_instr_info_t *info);

void gb_cpu_RL_B(gb_instr_info_t *info);
void gb_cpu_RL_C(gb_instr_info_t *info);
void gb_cpu_RL_D(gb_instr_info_t *info);
void gb_cpu_RL_E(gb_instr_info_t *info);
void gb_cpu_RL_H(gb_instr_info_t *info);
void gb_cpu_RL_L(gb_instr_info_t *info);
void gb_cpu_RL_HL(gb_instr_info_t *info);
void gb_cpu_RL_A(gb_instr_info_t *info);
void gb_cpu_RR_B(gb_instr_info_t *info);
void gb_cpu_RR_C(gb_instr_info_t *info);
void gb_cpu_RR_D(gb_instr_info_t *info);
void gb_cpu_RR_E(gb_instr_info_t *info);
void gb_cpu_RR_H(gb_instr_info_t *info);
void gb_cpu_RR_L(gb_instr_info_t *info);
void gb_cpu_RR_HL(gb_instr_info_t *info);
void gb_cpu_RR_A(gb_instr_info_t *info);

void gb_cpu_SLA_B(gb_instr_info_t *info);
void gb_cpu_SLA_C(gb_instr_info_t *info);
void gb_cpu_SLA_D(gb_instr_info_t *info);
void gb_cpu_SLA_E(gb_instr_info_t *info);
void gb_cpu_SLA_H(gb_instr_info_t *info);
void gb_cpu_SLA_L(gb_instr_info_t *info);
void gb_cpu_SLA_HL(gb_instr_info_t *info);
void gb_cpu_SLA_A(gb_instr_info_t *info);
void gb_cpu_SRA_B(gb_instr_info_t *info);
void gb_cpu_SRA_C(gb_instr_info_t *info);
void gb_cpu_SRA_D(gb_instr_info_t *info);
void gb_cpu_SRA_E(gb_instr_info_t *info);
void gb_cpu_SRA_H(gb_instr_info_t *info);
void gb_cpu_SRA_L(gb_instr_info_t *info);
void gb_cpu_SRA_HL(gb_instr_info_t *info);
void gb_cpu_SRA_A(gb_instr_info_t *info);

void gb_cpu_SWAP_B(gb_instr_info_t *info);
void gb_cpu_SWAP_C(gb_instr_info_t *info);
void gb_cpu_SWAP_D(gb_instr_info_t *info);
void gb_cpu_SWAP_E(gb_instr_info_t *info);
void gb_cpu_SWAP_H(gb_instr_info_t *info);
void gb_cpu_SWAP_L(gb_instr_info_t *info);
void gb_cpu_SWAP_HL(gb_instr_info_t *info);
void gb_cpu_SWAP_A(gb_instr_info_t *info);
void gb_cpu_SRL_B(gb_instr_info_t *info);
void gb_cpu_SRL_C(gb_instr_info_t *info);
void gb_cpu_SRL_D(gb_instr_info_t *info);
void gb_cpu_SRL_E(gb_instr_info_t *info);
void gb_cpu_SRL_H(gb_instr_info_t *info);
void gb_cpu_SRL_L(gb_instr_info_t *info);
void gb_cpu_SRL_HL(gb_instr_info_t *info);
void gb_cpu_SRL_A(gb_instr_info_t *info);

void gb_cpu_BIT_0_B(gb_instr_info_t *info);
void gb_cpu_BIT_0_C(gb_instr_info_t *info);
void gb_cpu_BIT_0_D(gb_instr_info_t *info);
void gb_cpu_BIT_0_E(gb_instr_info_t *info);
void gb_cpu_BIT_0_H(gb_instr_info_t *info);
void gb_cpu_BIT_0_L(gb_instr_info_t *info);
void gb_cpu_BIT_0_HL(gb_instr_info_t *info);
void gb_cpu_BIT_0_A(gb_instr_info_t *info);
void gb_cpu_BIT_1_B(gb_instr_info_t *info);
void gb_cpu_BIT_1_C(gb_instr_info_t *info);
void gb_cpu_BIT_1_D(gb_instr_info_t *info);
void gb_cpu_BIT_1_E(gb_instr_info_t *info);
void gb_cpu_BIT_1_H(gb_instr_info_t *info);
void gb_cpu_BIT_1_L(gb_instr_info_t *info);
void gb_cpu_BIT_1_HL(gb_instr_info_t *info);
void gb_cpu_BIT_1_A(gb_instr_info_t *info);

void gb_cpu_BIT_2_B(gb_instr_info_t *info);
void gb_cpu_BIT_2_C(gb_instr_info_t *info);
void gb_cpu_BIT_2_D(gb_instr_info_t *info);
void gb_cpu_BIT_2_E(gb_instr_info_t *info);
void gb_cpu_BIT_2_H(gb_instr_info_t *info);
void gb_cpu_BIT_2_L(gb_instr_info_t *info);
void gb_cpu_BIT_2_HL(gb_instr_info_t *info);
void gb_cpu_BIT_2_A(gb_instr_info_t *info);
void gb_cpu_BIT_3_B(gb_instr_info_t *info);
void gb_cpu_BIT_3_C(gb_instr_info_t *info);
void gb_cpu_BIT_3_D(gb_instr_info_t *info);
void gb_cpu_BIT_3_E(gb_instr_info_t *info);
void gb_cpu_BIT_3_H(gb_instr_info_t *info);
void gb_cpu_BIT_3_L(gb_instr_info_t *info);
void gb_cpu_BIT_3_HL(gb_instr_info_t *info);
void gb_cpu_BIT_3_A(gb_instr_info_t *info);

void gb_cpu_BIT_4_B(gb_instr_info_t *info);
void gb_cpu_BIT_4_C(gb_instr_info_t *info);
void gb_cpu_BIT_4_D(gb_instr_info_t *info);
void gb_cpu_BIT_4_E(gb_instr_info_t *info);
void gb_cpu_BIT_4_H(gb_instr_info_t *info);
void gb_cpu_BIT_4_L(gb_instr_info_t *info);
void gb_cpu_BIT_4_HL(gb_instr_info_t *info);
void gb_cpu_BIT_4_A(gb_instr_info_t *info);
void gb_cpu_BIT_5_B(gb_instr_info_t *info);
void gb_cpu_BIT_5_C(gb_instr_info_t *info);
void gb_cpu_BIT_5_D(gb_instr_info_t *info);
void gb_cpu_BIT_5_E(gb_instr_info_t *info);
void gb_cpu_BIT_5_H(gb_instr_info_t *info);
void gb_cpu_BIT_5_L(gb_instr_info_t *info);
void gb_cpu_BIT_5_HL(gb_instr_info_t *info);
void gb_cpu_BIT_5_A(gb_instr_info_t *info);

void gb_cpu_BIT_6_B(gb_instr_info_t *info);
void gb_cpu_BIT_6_C(gb_instr_info_t *info);
void gb_cpu_BIT_6_D(gb_instr_info_t *info);
void gb_cpu_BIT_6_E(gb_instr_info_t *info);
void gb_cpu_BIT_6_H(gb_instr_info_t *info);
void gb_cpu_BIT_6_L(gb_instr_info_t *info);
void gb_cpu_BIT_6_HL(gb_instr_info_t *info);
void gb_cpu_BIT_6_A(gb_instr_info_t *info);
void gb_cpu_BIT_7_B(gb_instr_info_t *info);
void gb_cpu_BIT_7_C(gb_instr_info_t *info);
void gb_cpu_BIT_7_D(gb_instr_info_t *info);
void gb_cpu_BIT_7_E(gb_instr_info_t *info);
void gb_cpu_BIT_7_H(gb_instr_info_t *info);
void gb_cpu_BIT_7_L(gb_instr_info_t *info);
void gb_cpu_BIT_7_HL(gb_instr_info_t *info);
void gb_cpu_BIT_7_A(gb_instr_info_t *info);

void gb_cpu_RES_0_B(gb_instr_info_t *info);
void gb_cpu_RES_0_C(gb_instr_info_t *info);
void gb_cpu_RES_0_D(gb_instr_info_t *info);
void gb_cpu_RES_0_E(gb_instr_info_t *info);
void gb_cpu_RES_0_H(gb_instr_info_t *info);
void gb_cpu_RES_0_L(gb_instr_info_t *info);
void gb_cpu_RES_0_HL(gb_instr_info_t *info);
void gb_cpu_RES_0_A(gb_instr_info_t *info);
void gb_cpu_RES_1_B(gb_instr_info_t *info);
void gb_cpu_RES_1_C(gb_instr_info_t *info);
void gb_cpu_RES_1_D(gb_instr_info_t *info);
void gb_cpu_RES_1_E(gb_instr_info_t *info);
void gb_cpu_RES_1_H(gb_instr_info_t *info);
void gb_cpu_RES_1_L(gb_instr_info_t *info);
void gb_cpu_RES_1_HL(gb_instr_info_t *info);
void gb_cpu_RES_1_A(gb_instr_info_t *info);

void gb_cpu_RES_2_B(gb_instr_info_t *info);
void gb_cpu_RES_2_C(gb_instr_info_t *info);
void gb_cpu_RES_2_D(gb_instr_info_t *info);
void gb_cpu_RES_2_E(gb_instr_info_t *info);
void gb_cpu_RES_2_H(gb_instr_info_t *info);
void gb_cpu_RES_2_L(gb_instr_info_t *info);
void gb_cpu_RES_2_HL(gb_instr_info_t *info);
void gb_cpu_RES_2_A(gb_instr_info_t *info);
void gb_cpu_RES_3_B(gb_instr_info_t *info);
void gb_cpu_RES_3_C(gb_instr_info_t *info);
void gb_cpu_RES_3_D(gb_instr_info_t *info);
void gb_cpu_RES_3_E(gb_instr_info_t *info);
void gb_cpu_RES_3_H(gb_instr_info_t *info);
void gb_cpu_RES_3_L(gb_instr_info_t *info);
void gb_cpu_RES_3_HL(gb_instr_info_t *info);
void gb_cpu_RES_3_A(gb_instr_info_t *info);

void gb_cpu_RES_4_B(gb_instr_info_t *info);
void gb_cpu_RES_4_C(gb_instr_info_t *info);
void gb_cpu_RES_4_D(gb_instr_info_t *info);
void gb_cpu_RES_4_E(gb_instr_info_t *info);
void gb_cpu_RES_4_H(gb_instr_info_t *info);
void gb_cpu_RES_4_L(gb_instr_info_t *info);
void gb_cpu_RES_4_HL(gb_instr_info_t *info);
void gb_cpu_RES_4_A(gb_instr_info_t *info);
void gb_cpu_RES_5_B(gb_instr_info_t *info);
void gb_cpu_RES_5_C(gb_instr_info_t *info);
void gb_cpu_RES_5_D(gb_instr_info_t *info);
void gb_cpu_RES_5_E(gb_instr_info_t *info);
void gb_cpu_RES_5_H(gb_instr_info_t *info);
void gb_cpu_RES_5_L(gb_instr_info_t *info);
void gb_cpu_RES_5_HL(gb_instr_info_t *info);
void gb_cpu_RES_5_A(gb_instr_info_t *info);

void gb_cpu_RES_6_B(gb_instr_info_t *info);
void gb_cpu_RES_6_C(gb_instr_info_t *info);
void gb_cpu_RES_6_D(gb_instr_info_t *info);
void gb_cpu_RES_6_E(gb_instr_info_t *info);
void gb_cpu_RES_6_H(gb_instr_info_t *info);
void gb_cpu_RES_6_L(gb_instr_info_t *info);
void gb_cpu_RES_6_HL(gb_instr_info_t *info);
void gb_cpu_RES_6_A(gb_instr_info_t *info);
void gb_cpu_RES_7_B(gb_instr_info_t *info);
void gb_cpu_RES_7_C(gb_instr_info_t *info);
void gb_cpu_RES_7_D(gb_instr_info_t *info);
void gb_cpu_RES_7_E(gb_instr_info_t *info);
void gb_cpu_RES_7_H(gb_instr_info_t *info);
void gb_cpu_RES_7_L(gb_instr_info_t *info);
void gb_cpu_RES_7_HL(gb_instr_info_t *info);
void gb_cpu_RES_7_A(gb_instr_info_t *info);

void gb_cpu_SET_0_B(gb_instr_info_t *info);
void gb_cpu_SET_0_C(gb_instr_info_t *info);
void gb_cpu_SET_0_D(gb_instr_info_t *info);
void gb_cpu_SET_0_E(gb_instr_info_t *info);
void gb_cpu_SET_0_H(gb_instr_info_t *info);
void gb_cpu_SET_0_L(gb_instr_info_t *info);
void gb_cpu_SET_0_HL(gb_instr_info_t *info);
void gb_cpu_SET_0_A(gb_instr_info_t *info);
void gb_cpu_SET_1_B(gb_instr_info_t *info);
void gb_cpu_SET_1_C(gb_instr_info_t *info);
void gb_cpu_SET_1_D(gb_instr_info_t *info);
void gb_cpu_SET_1_E(gb_instr_info_t *info);
void gb_cpu_SET_1_H(gb_instr_info_t *info);
void gb_cpu_SET_1_L(gb_instr_info_t *info);
void gb_cpu_SET_1_HL(gb_instr_info_t *info);
void gb_cpu_SET_1_A(gb_instr_info_t *info);

void gb_cpu_SET_2_B(gb_instr_info_t *info);
void gb_cpu_SET_2_C(gb_instr_info_t *info);
void gb_cpu_SET_2_D(gb_instr_info_t *info);
void gb_cpu_SET_2_E(gb_instr_info_t *info);
void gb_cpu_SET_2_H(gb_instr_info_t *info);
void gb_cpu_SET_2_L(gb_instr_info_t *info);
void gb_cpu_SET_2_HL(gb_instr_info_t *info);
void gb_cpu_SET_2_A(gb_instr_info_t *info);
void gb_cpu_SET_3_B(gb_instr_info_t *info);
void gb_cpu_SET_3_C(gb_instr_info_t *info);
void gb_cpu_SET_3_D(gb_instr_info_t *info);
void gb_cpu_SET_3_E(gb_instr_info_t *info);
void gb_cpu_SET_3_H(gb_instr_info_t *info);
void gb_cpu_SET_3_L(gb_instr_info_t *info);
void gb_cpu_SET_3_HL(gb_instr_info_t *info);
void gb_cpu_SET_3_A(gb_instr_info_t *info);

void gb_cpu_SET_4_B(gb_instr_info_t *info);
void gb_cpu_SET_4_C(gb_instr_info_t *info);
void gb_cpu_SET_4_D(gb_instr_info_t *info);
void gb_cpu_SET_4_E(gb_instr_info_t *info);
void gb_cpu_SET_4_H(gb_instr_info_t *info);
void gb_cpu_SET_4_L(gb_instr_info_t *info);
void gb_cpu_SET_4_HL(gb_instr_info_t *info);
void gb_cpu_SET_4_A(gb_instr_info_t *info);
void gb_cpu_SET_5_B(gb_instr_info_t *info);
void gb_cpu_SET_5_C(gb_instr_info_t *info);
void gb_cpu_SET_5_D(gb_instr_info_t *info);
void gb_cpu_SET_5_E(gb_instr_info_t *info);
void gb_cpu_SET_5_H(gb_instr_info_t *info);
void gb_cpu_SET_5_L(gb_instr_info_t *info);
void gb_cpu_SET_5_HL(gb_instr_info_t *info);
void gb_cpu_SET_5_A(gb_instr_info_t *info);

void gb_cpu_SET_6_B(gb_instr_info_t *info);
void gb_cpu_SET_6_C(gb_instr_info_t *info);
void gb_cpu_SET_6_D(gb_instr_info_t *info);
void gb_cpu_SET_6_E(gb_instr_info_t *info);
void gb_cpu_SET_6_H(gb_instr_info_t *info);
void gb_cpu_SET_6_L(gb_instr_info_t *info);
void gb_cpu_SET_6_HL(gb_instr_info_t *info);
void gb_cpu_SET_6_A(gb_instr_info_t *info);
void gb_cpu_SET_7_B(gb_instr_info_t *info);
void gb_cpu_SET_7_C(gb_instr_info_t *info);
void gb_cpu_SET_7_D(gb_instr_info_t *info);
void gb_cpu_SET_7_E(gb_instr_info_t *info);
void gb_cpu_SET_7_H(gb_instr_info_t *info);
void gb_cpu_SET_7_L(gb_instr_info_t *info);
void gb_cpu_SET_7_HL(gb_instr_info_t *info);
void gb_cpu_SET_7_A(gb_instr_info_t *info);

#endif /* SRC_GB_CPU_PRIV_H_ */
