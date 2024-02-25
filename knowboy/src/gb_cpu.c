/**
 * @file gb_cpu.c
 * @brief Gameboy CPU Functionality.
 *
 * This file emulates all functionality of the Gameboy CPU.
 *
 * @author Rami Saad
 * @date 2021-03-28
 */

#include "gb_cpu_priv.h"
#include "gb_memory.h"
#include "logging.h"

#include <stdint.h>

/* Private variables ---------------------------------------------------------*/
uint8_t stopped = 0;
uint8_t halted = 0;
uint8_t interrupt_master_enable = 0;
uint8_t one_cycle_interrupt_delay = 0;
uint8_t op_remaining = 0;
int interupt_dur = 0;
uint8_t opcode = 0;
uint8_t current_cycle = 0;

extern registers_t reg;

/* Game Boy CPU instruction set */
gb_instr_t instructions[256] = {{gb_cpu_NOP, {1, 1, 1}},
				{gb_cpu_LOAD_BC_d16, {3, 3, 1}},
				{gb_cpu_LOAD_BC_A, {1, 2, 1}},
				{gb_cpu_INC_BC, {1, 2, 1}},
				{gb_cpu_INC_B, {1, 1, 1}},
				{gb_cpu_DEC_B, {1, 1, 1}},
				{gb_cpu_LOAD_B_d8, {2, 2, 1}},
				{gb_cpu_RLCA, {1, 1, 1}},
				{gb_cpu_LOAD_a16_SP, {3, 5, 1}},
				{gb_cpu_ADD_HL_BC, {1, 2, 1}},
				{gb_cpu_LOAD_A_BC, {1, 2, 1}},
				{gb_cpu_DEC_BC, {1, 2, 1}},
				{gb_cpu_INC_C, {1, 1, 1}},
				{gb_cpu_DEC_C, {1, 1, 1}},
				{gb_cpu_LOAD_C_d8, {2, 2, 1}},
				{gb_cpu_RRCA, {1, 1, 1}},
				{gb_cpu_STOP, {2, 1, 1}},
				{gb_cpu_LOAD_DE_d16, {3, 3, 1}},
				{gb_cpu_LOAD_DE_A, {1, 2, 1}},
				{gb_cpu_INC_DE, {1, 2, 1}},
				{gb_cpu_INC_D, {1, 1, 1}},
				{gb_cpu_DEC_D, {1, 1, 1}},
				{gb_cpu_LOAD_D_d8, {2, 2, 1}},
				{gb_cpu_RLA, {1, 1, 1}},
				{gb_cpu_JR_r8, {2, 3, 1}},
				{gb_cpu_ADD_HL_DE, {1, 2, 1}},
				{gb_cpu_LOAD_A_DE, {1, 2, 1}},
				{gb_cpu_DEC_DE, {1, 2, 1}},
				{gb_cpu_INC_E, {1, 1, 1}},
				{gb_cpu_DEC_E, {1, 1, 1}},
				{gb_cpu_LOAD_E_d8, {2, 2, 1}},
				{gb_cpu_RRA, {1, 1, 1}},
				{gb_cpu_JR_NZ_r8, {2, CUSTOM_CYCLES, 1}},
				{gb_cpu_LOAD_HL_d16, {3, 3, 1}},
				{gb_cpu_LOAD_HLI_A, {1, 2, 1}},
				{gb_cpu_INC_HL, {1, 2, 1}},
				{gb_cpu_INC_H, {1, 1, 1}},
				{gb_cpu_DEC_H, {1, 1, 1}},
				{gb_cpu_LOAD_H_d8, {2, 2, 1}},
				{gb_cpu_DAA, {1, 1, 1}},
				{gb_cpu_JR_Z_r8, {2, CUSTOM_CYCLES, 1}},
				{gb_cpu_ADD_HL_HL, {1, 2, 1}},
				{gb_cpu_LOAD_A_HLI, {1, 2, 1}},
				{gb_cpu_DEC_HL, {1, 2, 1}},
				{gb_cpu_INC_L, {1, 1, 1}},
				{gb_cpu_DEC_L, {1, 1, 1}},
				{gb_cpu_LOAD_L_d8, {2, 2, 1}},
				{gb_cpu_CPL, {1, 1, 1}},
				{gb_cpu_JR_NC_r8, {2, CUSTOM_CYCLES, 1}},
				{gb_cpu_LOAD_SP_d16, {3, 3, 1}},
				{gb_cpu_LOAD_HLD_A, {1, 2, 1}},
				{gb_cpu_INC_SP, {1, 2, 1}},
				{gb_cpu_INC_HL_ADDR, {1, 3, CUSTOM_TIMING}},
				{gb_cpu_DEC_HL_ADDR, {1, 3, CUSTOM_TIMING}},
				{gb_cpu_LOAD_HL_d8, {2, 3, 2}},
				{gb_cpu_SCF, {1, 1, 1}},
				{gb_cpu_JR_C_r8, {2, CUSTOM_CYCLES, 1}},
				{gb_cpu_ADD_HL_SP, {1, 2, 1}},
				{gb_cpu_LOAD_A_HLD, {1, 2, 1}},
				{gb_cpu_DEC_SP, {1, 2, 1}},
				{gb_cpu_INC_A, {1, 1, 1}},
				{gb_cpu_DEC_A, {1, 1, 1}},
				{gb_cpu_LOAD_A_d8, {2, 2, 1}},
				{gb_cpu_CCF, {1, 1, 1}},
				{gb_cpu_LOAD_B_B, {1, 1, 1}},
				{gb_cpu_LOAD_B_C, {1, 1, 1}},
				{gb_cpu_LOAD_B_D, {1, 1, 1}},
				{gb_cpu_LOAD_B_E, {1, 1, 1}},
				{gb_cpu_LOAD_B_H, {1, 1, 1}},
				{gb_cpu_LOAD_B_L, {1, 1, 1}},
				{gb_cpu_LOAD_B_HL, {1, 2, 1}},
				{gb_cpu_LOAD_B_A, {1, 1, 1}},
				{gb_cpu_LOAD_C_B, {1, 1, 1}},
				{gb_cpu_LOAD_C_C, {1, 1, 1}},
				{gb_cpu_LOAD_C_D, {1, 1, 1}},
				{gb_cpu_LOAD_C_E, {1, 1, 1}},
				{gb_cpu_LOAD_C_H, {1, 1, 1}},
				{gb_cpu_LOAD_C_L, {1, 1, 1}},
				{gb_cpu_LOAD_C_HL, {1, 2, 1}},
				{gb_cpu_LOAD_C_A, {1, 1, 1}},
				{gb_cpu_LOAD_D_B, {1, 1, 1}},
				{gb_cpu_LOAD_D_C, {1, 1, 1}},
				{gb_cpu_LOAD_D_D, {1, 1, 1}},
				{gb_cpu_LOAD_D_E, {1, 1, 1}},
				{gb_cpu_LOAD_D_H, {1, 1, 1}},
				{gb_cpu_LOAD_D_L, {1, 1, 1}},
				{gb_cpu_LOAD_D_HL, {1, 2, 1}},
				{gb_cpu_LOAD_D_A, {1, 1, 1}},
				{gb_cpu_LOAD_E_B, {1, 1, 1}},
				{gb_cpu_LOAD_E_C, {1, 1, 1}},
				{gb_cpu_LOAD_E_D, {1, 1, 1}},
				{gb_cpu_LOAD_E_E, {1, 1, 1}},
				{gb_cpu_LOAD_E_H, {1, 1, 1}},
				{gb_cpu_LOAD_E_L, {1, 1, 1}},
				{gb_cpu_LOAD_E_HL, {1, 2, 1}},
				{gb_cpu_LOAD_E_A, {1, 1, 1}},
				{gb_cpu_LOAD_H_B, {1, 1, 1}},
				{gb_cpu_LOAD_H_C, {1, 1, 1}},
				{gb_cpu_LOAD_H_D, {1, 1, 1}},
				{gb_cpu_LOAD_H_E, {1, 1, 1}},
				{gb_cpu_LOAD_H_H, {1, 1, 1}},
				{gb_cpu_LOAD_H_L, {1, 1, 1}},
				{gb_cpu_LOAD_H_HL, {1, 2, 1}},
				{gb_cpu_LOAD_H_A, {1, 1, 1}},
				{gb_cpu_LOAD_L_B, {1, 1, 1}},
				{gb_cpu_LOAD_L_C, {1, 1, 1}},
				{gb_cpu_LOAD_L_D, {1, 1, 1}},
				{gb_cpu_LOAD_L_E, {1, 1, 1}},
				{gb_cpu_LOAD_L_H, {1, 1, 1}},
				{gb_cpu_LOAD_L_L, {1, 1, 1}},
				{gb_cpu_LOAD_L_HL, {1, 2, 1}},
				{gb_cpu_LOAD_L_A, {1, 1, 1}},
				{gb_cpu_LOAD_HL_B, {1, 2, 1}},
				{gb_cpu_LOAD_HL_C, {1, 2, 1}},
				{gb_cpu_LOAD_HL_D, {1, 2, 1}},
				{gb_cpu_LOAD_HL_E, {1, 2, 1}},
				{gb_cpu_LOAD_HL_H, {1, 2, 1}},
				{gb_cpu_LOAD_HL_L, {1, 2, 1}},
				{gb_cpu_HALT, {1, 1, 1}},
				{gb_cpu_LOAD_HL_A, {1, 2, 1}},
				{gb_cpu_LOAD_A_B, {1, 1, 1}},
				{gb_cpu_LOAD_A_C, {1, 1, 1}},
				{gb_cpu_LOAD_A_D, {1, 1, 1}},
				{gb_cpu_LOAD_A_E, {1, 1, 1}},
				{gb_cpu_LOAD_A_H, {1, 1, 1}},
				{gb_cpu_LOAD_A_L, {1, 1, 1}},
				{gb_cpu_LOAD_A_HL, {1, 2, 1}},
				{gb_cpu_LOAD_A_A, {1, 1, 1}},
				{gb_cpu_ADD_A_B, {1, 1, 1}},
				{gb_cpu_ADD_A_C, {1, 1, 1}},
				{gb_cpu_ADD_A_D, {1, 1, 1}},
				{gb_cpu_ADD_A_E, {1, 1, 1}},
				{gb_cpu_ADD_A_H, {1, 1, 1}},
				{gb_cpu_ADD_A_L, {1, 1, 1}},
				{gb_cpu_ADD_A_HL, {1, 2, 1}},
				{gb_cpu_ADD_A_A, {1, 1, 1}},
				{gb_cpu_ADC_A_B, {1, 1, 1}},
				{gb_cpu_ADC_A_C, {1, 1, 1}},
				{gb_cpu_ADC_A_D, {1, 1, 1}},
				{gb_cpu_ADC_A_E, {1, 1, 1}},
				{gb_cpu_ADC_A_H, {1, 1, 1}},
				{gb_cpu_ADC_A_L, {1, 1, 1}},
				{gb_cpu_ADC_A_HL, {1, 2, 1}},
				{gb_cpu_ADC_A_A, {1, 1, 1}},
				{gb_cpu_SUB_B, {1, 1, 1}},
				{gb_cpu_SUB_C, {1, 1, 1}},
				{gb_cpu_SUB_D, {1, 1, 1}},
				{gb_cpu_SUB_E, {1, 1, 1}},
				{gb_cpu_SUB_H, {1, 1, 1}},
				{gb_cpu_SUB_L, {1, 1, 1}},
				{gb_cpu_SUB_HL, {1, 2, 1}},
				{gb_cpu_SUB_A, {1, 1, 1}},
				{gb_cpu_SBC_A_B, {1, 1, 1}},
				{gb_cpu_SBC_A_C, {1, 1, 1}},
				{gb_cpu_SBC_A_D, {1, 1, 1}},
				{gb_cpu_SBC_A_E, {1, 1, 1}},
				{gb_cpu_SBC_A_H, {1, 1, 1}},
				{gb_cpu_SBC_A_L, {1, 1, 1}},
				{gb_cpu_SBC_A_HL, {1, 2, 1}},
				{gb_cpu_SBC_A_A, {1, 1, 1}},
				{gb_cpu_AND_B, {1, 1, 1}},
				{gb_cpu_AND_C, {1, 1, 1}},
				{gb_cpu_AND_D, {1, 1, 1}},
				{gb_cpu_AND_E, {1, 1, 1}},
				{gb_cpu_AND_H, {1, 1, 1}},
				{gb_cpu_AND_L, {1, 1, 1}},
				{gb_cpu_AND_HL, {1, 2, 1}},
				{gb_cpu_AND_A, {1, 1, 1}},
				{gb_cpu_XOR_B, {1, 1, 1}},
				{gb_cpu_XOR_C, {1, 1, 1}},
				{gb_cpu_XOR_D, {1, 1, 1}},
				{gb_cpu_XOR_E, {1, 1, 1}},
				{gb_cpu_XOR_H, {1, 1, 1}},
				{gb_cpu_XOR_L, {1, 1, 1}},
				{gb_cpu_XOR_HL, {1, 2, 1}},
				{gb_cpu_XOR_A, {1, 1, 1}},
				{gb_cpu_OR_B, {1, 1, 1}},
				{gb_cpu_OR_C, {1, 1, 1}},
				{gb_cpu_OR_D, {1, 1, 1}},
				{gb_cpu_OR_E, {1, 1, 1}},
				{gb_cpu_OR_H, {1, 1, 1}},
				{gb_cpu_OR_L, {1, 1, 1}},
				{gb_cpu_OR_HL, {1, 2, 1}},
				{gb_cpu_OR_A, {1, 1, 1}},
				{gb_cpu_CP_B, {1, 1, 1}},
				{gb_cpu_CP_C, {1, 1, 1}},
				{gb_cpu_CP_D, {1, 1, 1}},
				{gb_cpu_CP_E, {1, 1, 1}},
				{gb_cpu_CP_H, {1, 1, 1}},
				{gb_cpu_CP_L, {1, 1, 1}},
				{gb_cpu_CP_HL, {1, 2, 1}},
				{gb_cpu_CP_A, {1, 1, 1}},
				{gb_cpu_RET_NZ, {1, CUSTOM_CYCLES, 1}},
				{gb_cpu_POP_BC, {1, 3, 1}},
				{gb_cpu_JP_NZ_a16, {3, CUSTOM_CYCLES, 1}},
				{gb_cpu_JP_a16, {3, 4, 1}},
				{gb_cpu_CALL_NZ_a16, {3, CUSTOM_CYCLES, 1}},
				{gb_cpu_PUSH_BC, {1, 4, 1}},
				{gb_cpu_ADD_A_d8, {2, 2, 1}},
				{gb_cpu_RST_00H, {1, 4, 1}},
				{gb_cpu_RET_Z, {1, CUSTOM_CYCLES, 1}},
				{gb_cpu_RET, {1, 4, 1}},
				{gb_cpu_JP_Z_a16, {3, CUSTOM_CYCLES, 1}},
				{gb_cpu_PREFIX, {1, 1, CUSTOM_TIMING}},
				{gb_cpu_CALL_Z_a16, {3, CUSTOM_CYCLES, 1}},
				{gb_cpu_CALL_a16, {3, 6, 1}},
				{gb_cpu_ADC_A_d8, {2, 2, 1}},
				{gb_cpu_RST_08H, {1, 4, 1}},
				{gb_cpu_RET_NC, {1, CUSTOM_CYCLES, 1}},
				{gb_cpu_POP_DE, {1, 3, 1}},
				{gb_cpu_JP_NC_a16, {3, CUSTOM_CYCLES, 1}},
				{NULL, {0, 0, 0}},
				{gb_cpu_CALL_NC_a16, {3, CUSTOM_CYCLES, 1}},
				{gb_cpu_PUSH_DE, {1, 4, 1}},
				{gb_cpu_SUB_d8, {2, 2, 1}},
				{gb_cpu_RST_10H, {1, 4, 1}},
				{gb_cpu_RET_C, {1, CUSTOM_CYCLES, 1}},
				{gb_cpu_RETI, {1, 4, 1}},
				{gb_cpu_JP_C_a16, {3, CUSTOM_CYCLES, 1}},
				{NULL, {0, 0, 0}},
				{gb_cpu_CALL_C_a16, {3, CUSTOM_CYCLES, 1}},
				{NULL, {0, 0, 0}},
				{gb_cpu_SBC_A_d8, {2, 2, 1}},
				{gb_cpu_RST_18H, {1, 4, 1}},
				{gb_cpu_LOAD_a8_A, {2, 3, 2}},
				{gb_cpu_POP_HL, {1, 3, 1}},
				{gb_cpu_LOAD_fC_A, {1, 2, 1}},
				{NULL, {0, 0, 0}},
				{NULL, {0, 0, 0}},
				{gb_cpu_PUSH_HL, {1, 4, 1}},
				{gb_cpu_AND_d8, {2, 2, 1}},
				{gb_cpu_RST20H, {1, 4, 1}},
				{gb_cpu_ADD_SP_r8, {2, 4, 1}},
				{gb_cpu_JP_HL, {1, 1, 1}},
				{gb_cpu_LOAD_a16_A, {3, 4, 3}},
				{NULL, {0, 0, 0}},
				{NULL, {0, 0, 0}},
				{NULL, {0, 0, 0}},
				{gb_cpu_XOR_d8, {2, 2, 1}},
				{gb_cpu_RST_28H, {1, 4, 1}},
				{gb_cpu_LOAD_A_a8, {2, 3, 2}},
				{gb_cpu_POP_AF, {1, 3, 1}},
				{gb_cpu_LOAD_A_fC, {1, 2, 1}},
				{gb_cpu_DI, {1, 1, 1}},
				{NULL, {0, 0, 0}},
				{gb_cpu_PUSH_AF, {1, 4, 1}},
				{gb_cpu_OR_d8, {2, 2, 1}},
				{gb_cpu_RST_30H, {1, 4, 1}},
				{gb_cpu_LOAD_HL_SP_r8, {2, 3, 1}},
				{gb_cpu_LOAD_SP_HL, {1, 2, 1}},
				{gb_cpu_LOAD_A_a16, {3, 4, 3}},
				{gb_cpu_EI, {1, 1, 1}},
				{NULL, {0, 0, 0}},
				{NULL, {0, 0, 0}},
				{gb_cpu_CP_d8, {2, 2, 1}},
				{gb_cpu_RST_38H, {1, 4, 1}}};

gb_instr_t prefix_instructions[256] = {{gb_cpu_RLC_B, {2, 2, 1}},
				       {gb_cpu_RLC_C, {2, 2, 1}},
				       {gb_cpu_RLC_D, {2, 2, 1}},
				       {gb_cpu_RLC_E, {2, 2, 1}},
				       {gb_cpu_RLC_H, {2, 2, 1}},
				       {gb_cpu_RLC_L, {2, 2, 1}},
				       {gb_cpu_RLC_HL, {2, 4, CUSTOM_TIMING}},
				       {gb_cpu_RLC_A, {2, 2, 1}},
				       {gb_cpu_RRC_B, {2, 2, 1}},
				       {gb_cpu_RRC_C, {2, 2, 1}},
				       {gb_cpu_RRC_D, {2, 2, 1}},
				       {gb_cpu_RRC_E, {2, 2, 1}},
				       {gb_cpu_RRC_H, {2, 2, 1}},
				       {gb_cpu_RRC_L, {2, 2, 1}},
				       {gb_cpu_RRC_HL, {2, 4, CUSTOM_TIMING}},
				       {gb_cpu_RRC_A, {2, 2, 1}},
				       {gb_cpu_RL_B, {2, 2, 1}},
				       {gb_cpu_RL_C, {2, 2, 1}},
				       {gb_cpu_RL_D, {2, 2, 1}},
				       {gb_cpu_RL_E, {2, 2, 1}},
				       {gb_cpu_RL_H, {2, 2, 1}},
				       {gb_cpu_RL_L, {2, 2, 1}},
				       {gb_cpu_RL_HL, {2, 4, CUSTOM_TIMING}},
				       {gb_cpu_RL_A, {2, 2, 1}},
				       {gb_cpu_RR_B, {2, 2, 1}},
				       {gb_cpu_RR_C, {2, 2, 1}},
				       {gb_cpu_RR_D, {2, 2, 1}},
				       {gb_cpu_RR_E, {2, 2, 1}},
				       {gb_cpu_RR_H, {2, 2, 1}},
				       {gb_cpu_RR_L, {2, 2, 1}},
				       {gb_cpu_RR_HL, {2, 4, CUSTOM_TIMING}},
				       {gb_cpu_RR_A, {2, 2, 1}},
				       {gb_cpu_SLA_B, {2, 2, 1}},
				       {gb_cpu_SLA_C, {2, 2, 1}},
				       {gb_cpu_SLA_D, {2, 2, 1}},
				       {gb_cpu_SLA_E, {2, 2, 1}},
				       {gb_cpu_SLA_H, {2, 2, 1}},
				       {gb_cpu_SLA_L, {2, 2, 1}},
				       {gb_cpu_SLA_HL, {2, 4, CUSTOM_TIMING}},
				       {gb_cpu_SLA_A, {2, 2, 1}},
				       {gb_cpu_SRA_B, {2, 2, 1}},
				       {gb_cpu_SRA_C, {2, 2, 1}},
				       {gb_cpu_SRA_D, {2, 2, 1}},
				       {gb_cpu_SRA_E, {2, 2, 1}},
				       {gb_cpu_SRA_H, {2, 2, 1}},
				       {gb_cpu_SRA_L, {2, 2, 1}},
				       {gb_cpu_SRA_HL, {2, 4, CUSTOM_TIMING}},
				       {gb_cpu_SRA_A, {2, 2, 1}},
				       {gb_cpu_SWAP_B, {2, 2, 1}},
				       {gb_cpu_SWAP_C, {2, 2, 1}},
				       {gb_cpu_SWAP_D, {2, 2, 1}},
				       {gb_cpu_SWAP_E, {2, 2, 1}},
				       {gb_cpu_SWAP_H, {2, 2, 1}},
				       {gb_cpu_SWAP_L, {2, 2, 1}},
				       {gb_cpu_SWAP_HL, {2, 4, CUSTOM_TIMING}},
				       {gb_cpu_SWAP_A, {2, 2, 1}},
				       {gb_cpu_SRL_B, {2, 2, 1}},
				       {gb_cpu_SRL_C, {2, 2, 1}},
				       {gb_cpu_SRL_D, {2, 2, 1}},
				       {gb_cpu_SRL_E, {2, 2, 1}},
				       {gb_cpu_SRL_H, {2, 2, 1}},
				       {gb_cpu_SRL_L, {2, 2, 1}},
				       {gb_cpu_SRL_HL, {2, 4, CUSTOM_TIMING}},
				       {gb_cpu_SRL_A, {2, 2, 1}},
				       {gb_cpu_BIT_0_B, {2, 2, 1}},
				       {gb_cpu_BIT_0_C, {2, 2, 1}},
				       {gb_cpu_BIT_0_D, {2, 2, 1}},
				       {gb_cpu_BIT_0_E, {2, 2, 1}},
				       {gb_cpu_BIT_0_H, {2, 2, 1}},
				       {gb_cpu_BIT_0_L, {2, 2, 1}},
				       {gb_cpu_BIT_0_HL, {2, 3, 2}},
				       {gb_cpu_BIT_0_A, {2, 2, 1}},
				       {gb_cpu_BIT_1_B, {2, 2, 1}},
				       {gb_cpu_BIT_1_C, {2, 2, 1}},
				       {gb_cpu_BIT_1_D, {2, 2, 1}},
				       {gb_cpu_BIT_1_E, {2, 2, 1}},
				       {gb_cpu_BIT_1_H, {2, 2, 1}},
				       {gb_cpu_BIT_1_L, {2, 2, 1}},
				       {gb_cpu_BIT_1_HL, {2, 3, 2}},
				       {gb_cpu_BIT_1_A, {2, 2, 1}},
				       {gb_cpu_BIT_2_B, {2, 2, 1}},
				       {gb_cpu_BIT_2_C, {2, 2, 1}},
				       {gb_cpu_BIT_2_D, {2, 2, 1}},
				       {gb_cpu_BIT_2_E, {2, 2, 1}},
				       {gb_cpu_BIT_2_H, {2, 2, 1}},
				       {gb_cpu_BIT_2_L, {2, 2, 1}},
				       {gb_cpu_BIT_2_HL, {2, 3, 2}},
				       {gb_cpu_BIT_2_A, {2, 2, 1}},
				       {gb_cpu_BIT_3_B, {2, 2, 1}},
				       {gb_cpu_BIT_3_C, {2, 2, 1}},
				       {gb_cpu_BIT_3_D, {2, 2, 1}},
				       {gb_cpu_BIT_3_E, {2, 2, 1}},
				       {gb_cpu_BIT_3_H, {2, 2, 1}},
				       {gb_cpu_BIT_3_L, {2, 2, 1}},
				       {gb_cpu_BIT_3_HL, {2, 3, 2}},
				       {gb_cpu_BIT_3_A, {2, 2, 1}},
				       {gb_cpu_BIT_4_B, {2, 2, 1}},
				       {gb_cpu_BIT_4_C, {2, 2, 1}},
				       {gb_cpu_BIT_4_D, {2, 2, 1}},
				       {gb_cpu_BIT_4_E, {2, 2, 1}},
				       {gb_cpu_BIT_4_H, {2, 2, 1}},
				       {gb_cpu_BIT_4_L, {2, 2, 1}},
				       {gb_cpu_BIT_4_HL, {2, 3, 2}},
				       {gb_cpu_BIT_4_A, {2, 2, 1}},
				       {gb_cpu_BIT_5_B, {2, 2, 1}},
				       {gb_cpu_BIT_5_C, {2, 2, 1}},
				       {gb_cpu_BIT_5_D, {2, 2, 1}},
				       {gb_cpu_BIT_5_E, {2, 2, 1}},
				       {gb_cpu_BIT_5_H, {2, 2, 1}},
				       {gb_cpu_BIT_5_L, {2, 2, 1}},
				       {gb_cpu_BIT_5_HL, {2, 3, 2}},
				       {gb_cpu_BIT_5_A, {2, 2, 1}},
				       {gb_cpu_BIT_6_B, {2, 2, 1}},
				       {gb_cpu_BIT_6_C, {2, 2, 1}},
				       {gb_cpu_BIT_6_D, {2, 2, 1}},
				       {gb_cpu_BIT_6_E, {2, 2, 1}},
				       {gb_cpu_BIT_6_H, {2, 2, 1}},
				       {gb_cpu_BIT_6_L, {2, 2, 1}},
				       {gb_cpu_BIT_6_HL, {2, 3, 2}},
				       {gb_cpu_BIT_6_A, {2, 2, 1}},
				       {gb_cpu_BIT_7_B, {2, 2, 1}},
				       {gb_cpu_BIT_7_C, {2, 2, 1}},
				       {gb_cpu_BIT_7_D, {2, 2, 1}},
				       {gb_cpu_BIT_7_E, {2, 2, 1}},
				       {gb_cpu_BIT_7_H, {2, 2, 1}},
				       {gb_cpu_BIT_7_L, {2, 2, 1}},
				       {gb_cpu_BIT_7_HL, {2, 3, 2}},
				       {gb_cpu_BIT_7_A, {2, 2, 1}},
				       {gb_cpu_RES_0_B, {2, 2, 1}},
				       {gb_cpu_RES_0_C, {2, 2, 1}},
				       {gb_cpu_RES_0_D, {2, 2, 1}},
				       {gb_cpu_RES_0_E, {2, 2, 1}},
				       {gb_cpu_RES_0_H, {2, 2, 1}},
				       {gb_cpu_RES_0_L, {2, 2, 1}},
				       {gb_cpu_RES_0_HL, {2, 4, CUSTOM_TIMING}},
				       {gb_cpu_RES_0_A, {2, 2, 1}},
				       {gb_cpu_RES_1_B, {2, 2, 1}},
				       {gb_cpu_RES_1_C, {2, 2, 1}},
				       {gb_cpu_RES_1_D, {2, 2, 1}},
				       {gb_cpu_RES_1_E, {2, 2, 1}},
				       {gb_cpu_RES_1_H, {2, 2, 1}},
				       {gb_cpu_RES_1_L, {2, 2, 1}},
				       {gb_cpu_RES_1_HL, {2, 4, CUSTOM_TIMING}},
				       {gb_cpu_RES_1_A, {2, 2, 1}},
				       {gb_cpu_RES_2_B, {2, 2, 1}},
				       {gb_cpu_RES_2_C, {2, 2, 1}},
				       {gb_cpu_RES_2_D, {2, 2, 1}},
				       {gb_cpu_RES_2_E, {2, 2, 1}},
				       {gb_cpu_RES_2_H, {2, 2, 1}},
				       {gb_cpu_RES_2_L, {2, 2, 1}},
				       {gb_cpu_RES_2_HL, {2, 4, CUSTOM_TIMING}},
				       {gb_cpu_RES_2_A, {2, 2, 1}},
				       {gb_cpu_RES_3_B, {2, 2, 1}},
				       {gb_cpu_RES_3_C, {2, 2, 1}},
				       {gb_cpu_RES_3_D, {2, 2, 1}},
				       {gb_cpu_RES_3_E, {2, 2, 1}},
				       {gb_cpu_RES_3_H, {2, 2, 1}},
				       {gb_cpu_RES_3_L, {2, 2, 1}},
				       {gb_cpu_RES_3_HL, {2, 4, CUSTOM_TIMING}},
				       {gb_cpu_RES_3_A, {2, 2, 1}},
				       {gb_cpu_RES_4_B, {2, 2, 1}},
				       {gb_cpu_RES_4_C, {2, 2, 1}},
				       {gb_cpu_RES_4_D, {2, 2, 1}},
				       {gb_cpu_RES_4_E, {2, 2, 1}},
				       {gb_cpu_RES_4_H, {2, 2, 1}},
				       {gb_cpu_RES_4_L, {2, 2, 1}},
				       {gb_cpu_RES_4_HL, {2, 4, CUSTOM_TIMING}},
				       {gb_cpu_RES_4_A, {2, 2, 1}},
				       {gb_cpu_RES_5_B, {2, 2, 1}},
				       {gb_cpu_RES_5_C, {2, 2, 1}},
				       {gb_cpu_RES_5_D, {2, 2, 1}},
				       {gb_cpu_RES_5_E, {2, 2, 1}},
				       {gb_cpu_RES_5_H, {2, 2, 1}},
				       {gb_cpu_RES_5_L, {2, 2, 1}},
				       {gb_cpu_RES_5_HL, {2, 4, CUSTOM_TIMING}},
				       {gb_cpu_RES_5_A, {2, 2, 1}},
				       {gb_cpu_RES_6_B, {2, 2, 1}},
				       {gb_cpu_RES_6_C, {2, 2, 1}},
				       {gb_cpu_RES_6_D, {2, 2, 1}},
				       {gb_cpu_RES_6_E, {2, 2, 1}},
				       {gb_cpu_RES_6_H, {2, 2, 1}},
				       {gb_cpu_RES_6_L, {2, 2, 1}},
				       {gb_cpu_RES_6_HL, {2, 4, CUSTOM_TIMING}},
				       {gb_cpu_RES_6_A, {2, 2, 1}},
				       {gb_cpu_RES_7_B, {2, 2, 1}},
				       {gb_cpu_RES_7_C, {2, 2, 1}},
				       {gb_cpu_RES_7_D, {2, 2, 1}},
				       {gb_cpu_RES_7_E, {2, 2, 1}},
				       {gb_cpu_RES_7_H, {2, 2, 1}},
				       {gb_cpu_RES_7_L, {2, 2, 1}},
				       {gb_cpu_RES_7_HL, {2, 4, CUSTOM_TIMING}},
				       {gb_cpu_RES_7_A, {2, 2, 1}},
				       {gb_cpu_SET_0_B, {2, 2, 1}},
				       {gb_cpu_SET_0_C, {2, 2, 1}},
				       {gb_cpu_SET_0_D, {2, 2, 1}},
				       {gb_cpu_SET_0_E, {2, 2, 1}},
				       {gb_cpu_SET_0_H, {2, 2, 1}},
				       {gb_cpu_SET_0_L, {2, 2, 1}},
				       {gb_cpu_SET_0_HL, {2, 4, CUSTOM_TIMING}},
				       {gb_cpu_SET_0_A, {2, 2, 1}},
				       {gb_cpu_SET_1_B, {2, 2, 1}},
				       {gb_cpu_SET_1_C, {2, 2, 1}},
				       {gb_cpu_SET_1_D, {2, 2, 1}},
				       {gb_cpu_SET_1_E, {2, 2, 1}},
				       {gb_cpu_SET_1_H, {2, 2, 1}},
				       {gb_cpu_SET_1_L, {2, 2, 1}},
				       {gb_cpu_SET_1_HL, {2, 4, CUSTOM_TIMING}},
				       {gb_cpu_SET_1_A, {2, 2, 1}},
				       {gb_cpu_SET_2_B, {2, 2, 1}},
				       {gb_cpu_SET_2_C, {2, 2, 1}},
				       {gb_cpu_SET_2_D, {2, 2, 1}},
				       {gb_cpu_SET_2_E, {2, 2, 1}},
				       {gb_cpu_SET_2_H, {2, 2, 1}},
				       {gb_cpu_SET_2_L, {2, 2, 1}},
				       {gb_cpu_SET_2_HL, {2, 4, CUSTOM_TIMING}},
				       {gb_cpu_SET_2_A, {2, 2, 1}},
				       {gb_cpu_SET_3_B, {2, 2, 1}},
				       {gb_cpu_SET_3_C, {2, 2, 1}},
				       {gb_cpu_SET_3_D, {2, 2, 1}},
				       {gb_cpu_SET_3_E, {2, 2, 1}},
				       {gb_cpu_SET_3_H, {2, 2, 1}},
				       {gb_cpu_SET_3_L, {2, 2, 1}},
				       {gb_cpu_SET_3_HL, {2, 4, CUSTOM_TIMING}},
				       {gb_cpu_SET_3_A, {2, 2, 1}},
				       {gb_cpu_SET_4_B, {2, 2, 1}},
				       {gb_cpu_SET_4_C, {2, 2, 1}},
				       {gb_cpu_SET_4_D, {2, 2, 1}},
				       {gb_cpu_SET_4_E, {2, 2, 1}},
				       {gb_cpu_SET_4_H, {2, 2, 1}},
				       {gb_cpu_SET_4_L, {2, 2, 1}},
				       {gb_cpu_SET_4_HL, {2, 4, CUSTOM_TIMING}},
				       {gb_cpu_SET_4_A, {2, 2, 1}},
				       {gb_cpu_SET_5_B, {2, 2, 1}},
				       {gb_cpu_SET_5_C, {2, 2, 1}},
				       {gb_cpu_SET_5_D, {2, 2, 1}},
				       {gb_cpu_SET_5_E, {2, 2, 1}},
				       {gb_cpu_SET_5_H, {2, 2, 1}},
				       {gb_cpu_SET_5_L, {2, 2, 1}},
				       {gb_cpu_SET_5_HL, {2, 4, CUSTOM_TIMING}},
				       {gb_cpu_SET_5_A, {2, 2, 1}},
				       {gb_cpu_SET_6_B, {2, 2, 1}},
				       {gb_cpu_SET_6_C, {2, 2, 1}},
				       {gb_cpu_SET_6_D, {2, 2, 1}},
				       {gb_cpu_SET_6_E, {2, 2, 1}},
				       {gb_cpu_SET_6_H, {2, 2, 1}},
				       {gb_cpu_SET_6_L, {2, 2, 1}},
				       {gb_cpu_SET_6_HL, {2, 4, CUSTOM_TIMING}},
				       {gb_cpu_SET_6_A, {2, 2, 1}},
				       {gb_cpu_SET_7_B, {2, 2, 1}},
				       {gb_cpu_SET_7_C, {2, 2, 1}},
				       {gb_cpu_SET_7_D, {2, 2, 1}},
				       {gb_cpu_SET_7_E, {2, 2, 1}},
				       {gb_cpu_SET_7_H, {2, 2, 1}},
				       {gb_cpu_SET_7_L, {2, 2, 1}},
				       {gb_cpu_SET_7_HL, {2, 4, CUSTOM_TIMING}},
				       {gb_cpu_SET_7_A, {2, 2, 1}}};

/*********************0x0X*/
void gb_cpu_NOP(gb_instr_info_t *info)
{
	(void)info;
}

void gb_cpu_LOAD_BC_d16(gb_instr_info_t *info)
{
	(void)info;
	reg.BC = CAT_BYTES(gb_memory_read(reg.PC - 2), gb_memory_read(reg.PC - 1));
}

void gb_cpu_LOAD_BC_A(gb_instr_info_t *info)
{
	(void)info;
	gb_memory_write(reg.BC, reg.A);
}

void gb_cpu_INC_BC(gb_instr_info_t *info)
{
	(void)info;
	reg.BC++;
}

void gb_cpu_INC_B(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_inc_register(&reg.B, &reg.F);
}

void gb_cpu_DEC_B(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_dec_register(&reg.B, &reg.F);
}

void gb_cpu_LOAD_B_d8(gb_instr_info_t *info)
{
	(void)info;
	reg.B = gb_memory_read(reg.PC - 1);
}

void gb_cpu_RLCA(gb_instr_info_t *info)
{
	(void)info;
	uint8_t temp_carry = CHK_BIT(reg.A, 7);
	reg.F = (temp_carry != 0) ? C_FLAG_VAL : 0x00;
	reg.A <<= 1;
	reg.A += temp_carry;
}

void gb_cpu_LOAD_a16_SP(gb_instr_info_t *info)
{
	(void)info;
	gb_memory_write_short(CAT_BYTES(gb_memory_read(reg.PC - 2), gb_memory_read(reg.PC - 1)),
			      reg.SP);
}

void gb_cpu_ADD_HL_BC(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_addition_HL_register(&reg.HL, &reg.BC, &reg.F);
}

void gb_cpu_LOAD_A_BC(gb_instr_info_t *info)
{
	(void)info;
	reg.A = gb_memory_read(reg.BC);
}

void gb_cpu_DEC_BC(gb_instr_info_t *info)
{
	(void)info;
	reg.BC--;
}

void gb_cpu_INC_C(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_inc_register(&reg.C, &reg.F);
}

void gb_cpu_DEC_C(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_dec_register(&reg.C, &reg.F);
}

void gb_cpu_LOAD_C_d8(gb_instr_info_t *info)
{
	(void)info;
	reg.C = gb_memory_read(reg.PC - 1);
}

void gb_cpu_RRCA(gb_instr_info_t *info)
{
	(void)info;
	uint8_t temp_carry = CHK_BIT(reg.A, 0);
	reg.F = (temp_carry != 0) ? C_FLAG_VAL : 0x00;
	reg.A >>= 1;
	if (temp_carry != 0) {
		SET_BIT(reg.A, 7);
	}
}

/*********************0x1X*/
void gb_cpu_STOP(gb_instr_info_t *info)
{
	(void)info;
	stopped = 1;
} // MORE NEEDED TO IMPLEMENT LATER

void gb_cpu_LOAD_DE_d16(gb_instr_info_t *info)
{
	(void)info;
	reg.DE = CAT_BYTES(gb_memory_read(reg.PC - 2), gb_memory_read(reg.PC - 1));
}

void gb_cpu_LOAD_DE_A(gb_instr_info_t *info)
{
	(void)info;
	gb_memory_write(reg.DE, reg.A);
}

void gb_cpu_INC_DE(gb_instr_info_t *info)
{
	(void)info;
	reg.DE++;
}

void gb_cpu_INC_D(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_inc_register(&reg.D, &reg.F);
}

void gb_cpu_DEC_D(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_dec_register(&reg.D, &reg.F);
}

void gb_cpu_LOAD_D_d8(gb_instr_info_t *info)
{
	(void)info;
	reg.D = gb_memory_read(reg.PC - 1);
}

void gb_cpu_RLA(gb_instr_info_t *info)
{
	(void)info;
	uint8_t prev_carry = CHK_BIT(reg.F, C_FLAG_BIT);
	reg.F = ((reg.A & 0x80) != 0) ? C_FLAG_VAL : 0x00;
	reg.A <<= 1;
	reg.A += prev_carry;
}

void gb_cpu_JR_r8(gb_instr_info_t *info)
{
	(void)info;
	reg.PC += (int8_t)gb_memory_read(reg.PC - 1);
}

void gb_cpu_ADD_HL_DE(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_addition_HL_register(&reg.HL, &reg.DE, &reg.F);
}

void gb_cpu_LOAD_A_DE(gb_instr_info_t *info)
{
	(void)info;
	reg.A = gb_memory_read(reg.DE);
}

void gb_cpu_DEC_DE(gb_instr_info_t *info)
{
	(void)info;
	reg.DE--;
}

void gb_cpu_INC_E(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_inc_register(&reg.E, &reg.F);
}

void gb_cpu_DEC_E(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_dec_register(&reg.E, &reg.F);
}

void gb_cpu_LOAD_E_d8(gb_instr_info_t *info)
{
	(void)info;
	reg.E = gb_memory_read(reg.PC - 1);
}

void gb_cpu_RRA(gb_instr_info_t *info)
{
	(void)info;
	uint8_t prev_carry = CHK_BIT(reg.F, C_FLAG_BIT);
	reg.F = ((reg.A & 0x01) != 0) ? C_FLAG_VAL : 0x00;
	reg.A >>= 1;
	reg.A += (prev_carry << 7);
}

/*********************0x2X*/
void gb_cpu_JR_NZ_r8(gb_instr_info_t *info)
{
	if (info->cycles == 3) {
		int8_t r8_val = (int8_t)gb_memory_read(reg.PC - 1);
		reg.PC += r8_val;
	}
	info->cycles = CUSTOM_CYCLES;
}

void gb_cpu_LOAD_HL_d16(gb_instr_info_t *info)
{
	(void)info;
	reg.HL = CAT_BYTES(gb_memory_read(reg.PC - 2), gb_memory_read(reg.PC - 1));
}

void gb_cpu_LOAD_HLI_A(gb_instr_info_t *info)
{
	(void)info;
	gb_memory_write(reg.HL, reg.A);
	reg.HL++;
}

void gb_cpu_INC_HL(gb_instr_info_t *info)
{
	(void)info;
	reg.HL++;
}

void gb_cpu_INC_H(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_inc_register(&reg.H, &reg.F);
}

void gb_cpu_DEC_H(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_dec_register(&reg.H, &reg.F);
}

void gb_cpu_LOAD_H_d8(gb_instr_info_t *info)
{
	(void)info;
	reg.H = gb_memory_read(reg.PC - 1);
}

void gb_cpu_DAA(gb_instr_info_t *info)
{
	(void)info;
	uint16_t tempShort = reg.A;
	if (CHK_BIT(reg.F, N_FLAG_BIT) != 0) {
		if (CHK_BIT(reg.F, H_FLAG_BIT) != 0)
			tempShort += 0xFA;
		if (CHK_BIT(reg.F, C_FLAG_BIT) != 0)
			tempShort += 0xA0;
	} else {
		if (CHK_BIT(reg.F, H_FLAG_BIT) || (tempShort & 0xF) > 9)
			tempShort += 0x06;
		if (CHK_BIT(reg.F, C_FLAG_BIT) || ((tempShort & 0x1F0) > 0x90)) {
			tempShort += 0x60;
			SET_BIT(reg.F, C_FLAG_BIT);
		} else {
			RST_BIT(reg.F, C_FLAG_BIT);
		}
	}
	reg.A = (uint8_t)tempShort;
	RST_BIT(reg.F, H_FLAG_BIT);
	(reg.A != 0) ? RST_BIT(reg.F, Z_FLAG_BIT) : SET_BIT(reg.F, Z_FLAG_BIT);
}

void gb_cpu_JR_Z_r8(gb_instr_info_t *info)
{
	if (info->cycles == 3) {
		int8_t r8_val = (int8_t)gb_memory_read(reg.PC - 1);
		reg.PC += r8_val;
	}
	info->cycles = CUSTOM_CYCLES;
}

void gb_cpu_ADD_HL_HL(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_addition_HL_register(&reg.HL, &reg.HL, &reg.F);
}

void gb_cpu_LOAD_A_HLI(gb_instr_info_t *info)
{
	(void)info;
	reg.A = gb_memory_read(reg.HL);
	reg.HL++;
}

void gb_cpu_DEC_HL(gb_instr_info_t *info)
{
	(void)info;
	reg.HL--;
}

void gb_cpu_INC_L(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_inc_register(&reg.L, &reg.F);
}

void gb_cpu_DEC_L(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_dec_register(&reg.L, &reg.F);
}

void gb_cpu_LOAD_L_d8(gb_instr_info_t *info)
{
	(void)info;
	reg.L = gb_memory_read(reg.PC - 1);
}

void gb_cpu_CPL(gb_instr_info_t *info)
{
	(void)info;
	reg.A = ~(reg.A);
	SET_BIT(reg.F, N_FLAG_BIT);
	SET_BIT(reg.F, H_FLAG_BIT);
}

/*********************0x3X*/
void gb_cpu_JR_NC_r8(gb_instr_info_t *info)
{
	if (info->cycles == 3) {
		int8_t r8_val = (int8_t)gb_memory_read(reg.PC - 1);
		reg.PC += r8_val;
	}
	info->cycles = CUSTOM_CYCLES;
}

void gb_cpu_LOAD_SP_d16(gb_instr_info_t *info)
{
	(void)info;
	reg.SP = CAT_BYTES(gb_memory_read(reg.PC - 2), gb_memory_read(reg.PC - 1));
}

void gb_cpu_LOAD_HLD_A(gb_instr_info_t *info)
{
	(void)info;
	gb_memory_write(reg.HL, reg.A);
	reg.HL--;
}

void gb_cpu_INC_SP(gb_instr_info_t *info)
{
	(void)info;
	reg.SP++;
}

void gb_cpu_INC_HL_ADDR(gb_instr_info_t *info)
{
	static uint8_t temp_res = 0;
	if (info->current_cycle == 1) {
		temp_res = gb_memory_read(reg.HL);
		((temp_res & 0x0F) == 0x0F) ? SET_BIT(reg.F, H_FLAG_BIT)
					    : RST_BIT(reg.F, H_FLAG_BIT);
		RST_BIT(reg.F, N_FLAG_BIT);

	} else if (info->current_cycle == 2) {
		gb_memory_write(reg.HL, temp_res + 1);
		temp_res = gb_memory_read(reg.HL);
		(temp_res != 0) ? RST_BIT(reg.F, Z_FLAG_BIT) : SET_BIT(reg.F, Z_FLAG_BIT);
	}
	info->current_cycle = CUSTOM_TIMING;
}

void gb_cpu_DEC_HL_ADDR(gb_instr_info_t *info)
{
	static uint8_t temp_res = 0;
	if (info->current_cycle == 1) {
		temp_res = gb_memory_read(reg.HL);
		((temp_res & 0x0F) != 0) ? RST_BIT(reg.F, H_FLAG_BIT) : SET_BIT(reg.F, H_FLAG_BIT);
		SET_BIT(reg.F, N_FLAG_BIT);

	} else if (info->current_cycle == 2) {
		gb_memory_write(reg.HL, temp_res - 1);
		temp_res = gb_memory_read(reg.HL);
		(temp_res != 0) ? RST_BIT(reg.F, Z_FLAG_BIT) : SET_BIT(reg.F, Z_FLAG_BIT);
	}
	info->current_cycle = CUSTOM_TIMING;
}

void gb_cpu_LOAD_HL_d8(gb_instr_info_t *info)
{
	(void)info;
	gb_memory_write(reg.HL, gb_memory_read(reg.PC - 1));
}

void gb_cpu_SCF(gb_instr_info_t *info)
{
	(void)info;
	RST_BIT(reg.F, N_FLAG_BIT);
	RST_BIT(reg.F, H_FLAG_BIT);
	SET_BIT(reg.F, C_FLAG_BIT);
}

void gb_cpu_JR_C_r8(gb_instr_info_t *info)
{
	if (info->cycles == 3) {
		int8_t r8_val = (int8_t)gb_memory_read(reg.PC - 1);
		reg.PC += r8_val;
	}
	info->cycles = CUSTOM_CYCLES;
}

void gb_cpu_ADD_HL_SP(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_addition_HL_register(&reg.HL, &reg.SP, &reg.F);
}

void gb_cpu_LOAD_A_HLD(gb_instr_info_t *info)
{
	(void)info;
	reg.A = gb_memory_read(reg.HL);
	reg.HL--;
}

void gb_cpu_DEC_SP(gb_instr_info_t *info)
{
	(void)info;
	reg.SP--;
}

void gb_cpu_INC_A(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_inc_register(&reg.A, &reg.F);
}

void gb_cpu_DEC_A(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_dec_register(&reg.A, &reg.F);
}

void gb_cpu_LOAD_A_d8(gb_instr_info_t *info)
{
	(void)info;
	reg.A = gb_memory_read(reg.PC - 1);
}

void gb_cpu_CCF(gb_instr_info_t *info)
{
	(void)info;
	RST_BIT(reg.F, N_FLAG_BIT);
	RST_BIT(reg.F, H_FLAG_BIT);
	(CHK_BIT(reg.F, C_FLAG_BIT)) ? RST_BIT(reg.F, C_FLAG_BIT) : SET_BIT(reg.F, C_FLAG_BIT);
}

/*********************0x4X*/
void gb_cpu_LOAD_B_B(gb_instr_info_t *info)
{
	(void)info;
	reg.B = reg.B;
}

void gb_cpu_LOAD_B_C(gb_instr_info_t *info)
{
	(void)info;
	reg.B = reg.C;
}

void gb_cpu_LOAD_B_D(gb_instr_info_t *info)
{
	(void)info;
	reg.B = reg.D;
}

void gb_cpu_LOAD_B_E(gb_instr_info_t *info)
{
	(void)info;
	reg.B = reg.E;
}

void gb_cpu_LOAD_B_H(gb_instr_info_t *info)
{
	(void)info;
	reg.B = reg.H;
}

void gb_cpu_LOAD_B_L(gb_instr_info_t *info)
{
	(void)info;
	reg.B = reg.L;
}

void gb_cpu_LOAD_B_HL(gb_instr_info_t *info)
{
	(void)info;
	reg.B = gb_memory_read(reg.HL);
}

void gb_cpu_LOAD_B_A(gb_instr_info_t *info)
{
	(void)info;
	reg.B = reg.A;
}

void gb_cpu_LOAD_C_B(gb_instr_info_t *info)
{
	(void)info;
	reg.C = reg.B;
}

void gb_cpu_LOAD_C_C(gb_instr_info_t *info)
{
	(void)info;
	reg.C = reg.C;
}

void gb_cpu_LOAD_C_D(gb_instr_info_t *info)
{
	(void)info;
	reg.C = reg.D;
}

void gb_cpu_LOAD_C_E(gb_instr_info_t *info)
{
	(void)info;
	reg.C = reg.E;
}

void gb_cpu_LOAD_C_H(gb_instr_info_t *info)
{
	(void)info;
	reg.C = reg.H;
}

void gb_cpu_LOAD_C_L(gb_instr_info_t *info)
{
	(void)info;
	reg.C = reg.L;
}

void gb_cpu_LOAD_C_HL(gb_instr_info_t *info)
{
	(void)info;
	reg.C = gb_memory_read(reg.HL);
}

void gb_cpu_LOAD_C_A(gb_instr_info_t *info)
{
	(void)info;
	reg.C = reg.A;
}

/*********************0x5X*/
void gb_cpu_LOAD_D_B(gb_instr_info_t *info)
{
	(void)info;
	reg.D = reg.B;
}

void gb_cpu_LOAD_D_C(gb_instr_info_t *info)
{
	(void)info;
	reg.D = reg.C;
}

void gb_cpu_LOAD_D_D(gb_instr_info_t *info)
{
	(void)info;
	reg.D = reg.D;
}

void gb_cpu_LOAD_D_E(gb_instr_info_t *info)
{
	(void)info;
	reg.D = reg.E;
}

void gb_cpu_LOAD_D_H(gb_instr_info_t *info)
{
	(void)info;
	reg.D = reg.H;
}

void gb_cpu_LOAD_D_L(gb_instr_info_t *info)
{
	(void)info;
	reg.D = reg.L;
}

void gb_cpu_LOAD_D_HL(gb_instr_info_t *info)
{
	(void)info;
	reg.D = gb_memory_read(reg.HL);
}

void gb_cpu_LOAD_D_A(gb_instr_info_t *info)
{
	(void)info;
	reg.D = reg.A;
}

void gb_cpu_LOAD_E_B(gb_instr_info_t *info)
{
	(void)info;
	reg.E = reg.B;
}

void gb_cpu_LOAD_E_C(gb_instr_info_t *info)
{
	(void)info;
	reg.E = reg.C;
}

void gb_cpu_LOAD_E_D(gb_instr_info_t *info)
{
	(void)info;
	reg.E = reg.D;
}

void gb_cpu_LOAD_E_E(gb_instr_info_t *info)
{
	(void)info;
	reg.E = reg.E;
}

void gb_cpu_LOAD_E_H(gb_instr_info_t *info)
{
	(void)info;
	reg.E = reg.H;
}

void gb_cpu_LOAD_E_L(gb_instr_info_t *info)
{
	(void)info;
	reg.E = reg.L;
}

void gb_cpu_LOAD_E_HL(gb_instr_info_t *info)
{
	(void)info;
	reg.E = gb_memory_read(reg.HL);
}

void gb_cpu_LOAD_E_A(gb_instr_info_t *info)
{
	(void)info;
	reg.E = reg.A;
}

/*********************0x6X*/
void gb_cpu_LOAD_H_B(gb_instr_info_t *info)
{
	(void)info;
	reg.H = reg.B;
}

void gb_cpu_LOAD_H_C(gb_instr_info_t *info)
{
	(void)info;
	reg.H = reg.C;
}

void gb_cpu_LOAD_H_D(gb_instr_info_t *info)
{
	(void)info;
	reg.H = reg.D;
}

void gb_cpu_LOAD_H_E(gb_instr_info_t *info)
{
	(void)info;
	reg.H = reg.E;
}

void gb_cpu_LOAD_H_H(gb_instr_info_t *info)
{
	(void)info;
	reg.H = reg.H;
}

void gb_cpu_LOAD_H_L(gb_instr_info_t *info)
{
	(void)info;
	reg.H = reg.L;
}

void gb_cpu_LOAD_H_HL(gb_instr_info_t *info)
{
	(void)info;
	reg.H = gb_memory_read(reg.HL);
}

void gb_cpu_LOAD_H_A(gb_instr_info_t *info)
{
	(void)info;
	reg.H = reg.A;
}

void gb_cpu_LOAD_L_B(gb_instr_info_t *info)
{
	(void)info;
	reg.L = reg.B;
}

void gb_cpu_LOAD_L_C(gb_instr_info_t *info)
{
	(void)info;
	reg.L = reg.C;
}

void gb_cpu_LOAD_L_D(gb_instr_info_t *info)
{
	(void)info;
	reg.L = reg.D;
}

void gb_cpu_LOAD_L_E(gb_instr_info_t *info)
{
	(void)info;
	reg.L = reg.E;
}

void gb_cpu_LOAD_L_H(gb_instr_info_t *info)
{
	(void)info;
	reg.L = reg.H;
}

void gb_cpu_LOAD_L_L(gb_instr_info_t *info)
{
	(void)info;
	reg.L = reg.L;
}

void gb_cpu_LOAD_L_HL(gb_instr_info_t *info)
{
	(void)info;
	reg.L = gb_memory_read(reg.HL);
}

void gb_cpu_LOAD_L_A(gb_instr_info_t *info)
{
	(void)info;
	reg.L = reg.A;
}

/*********************0x7X*/
void gb_cpu_LOAD_HL_B(gb_instr_info_t *info)
{
	(void)info;
	gb_memory_write(reg.HL, reg.B);
}

void gb_cpu_LOAD_HL_C(gb_instr_info_t *info)
{
	(void)info;
	gb_memory_write(reg.HL, reg.C);
}

void gb_cpu_LOAD_HL_D(gb_instr_info_t *info)
{
	(void)info;
	gb_memory_write(reg.HL, reg.D);
}

void gb_cpu_LOAD_HL_E(gb_instr_info_t *info)
{
	(void)info;
	gb_memory_write(reg.HL, reg.E);
}

void gb_cpu_LOAD_HL_H(gb_instr_info_t *info)
{
	(void)info;
	gb_memory_write(reg.HL, reg.H);
}

void gb_cpu_LOAD_HL_L(gb_instr_info_t *info)
{
	(void)info;
	gb_memory_write(reg.HL, reg.L);
}

void gb_cpu_HALT(gb_instr_info_t *info)
{
	(void)info;
	halted = 1;
}

void gb_cpu_LOAD_HL_A(gb_instr_info_t *info)
{
	(void)info;
	gb_memory_write(reg.HL, reg.A);
}

void gb_cpu_LOAD_A_B(gb_instr_info_t *info)
{
	(void)info;
	reg.A = reg.B;
}

void gb_cpu_LOAD_A_C(gb_instr_info_t *info)
{
	(void)info;
	reg.A = reg.C;
}

void gb_cpu_LOAD_A_D(gb_instr_info_t *info)
{
	(void)info;
	reg.A = reg.D;
}

void gb_cpu_LOAD_A_E(gb_instr_info_t *info)
{
	(void)info;
	reg.A = reg.E;
}

void gb_cpu_LOAD_A_H(gb_instr_info_t *info)
{
	(void)info;
	reg.A = reg.H;
}

void gb_cpu_LOAD_A_L(gb_instr_info_t *info)
{
	(void)info;
	reg.A = reg.L;
}

void gb_cpu_LOAD_A_HL(gb_instr_info_t *info)
{
	(void)info;
	reg.A = gb_memory_read(reg.HL);
}

void gb_cpu_LOAD_A_A(gb_instr_info_t *info)
{
	(void)info;
	reg.A = reg.A;
}

/*********************0x8X*/
void gb_cpu_ADD_A_B(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_addition_A_register(&reg.A, &reg.F, &reg.B);
}

void gb_cpu_ADD_A_C(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_addition_A_register(&reg.A, &reg.F, &reg.C);
}

void gb_cpu_ADD_A_D(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_addition_A_register(&reg.A, &reg.F, &reg.D);
}

void gb_cpu_ADD_A_E(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_addition_A_register(&reg.A, &reg.F, &reg.E);
}

void gb_cpu_ADD_A_H(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_addition_A_register(&reg.A, &reg.F, &reg.H);
}

void gb_cpu_ADD_A_L(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_addition_A_register(&reg.A, &reg.F, &reg.L);
}

void gb_cpu_ADD_A_HL(gb_instr_info_t *info)
{
	(void)info;
	uint8_t temp_res = gb_memory_read(reg.HL);
	gb_cpu_addition_A_register(&reg.A, &reg.F, &temp_res);
}

void gb_cpu_ADD_A_A(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_addition_A_register(&reg.A, &reg.F, &reg.A);
}

void gb_cpu_ADC_A_B(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_addition_carry_A_register(&reg.A, &reg.F, &reg.B);
}

void gb_cpu_ADC_A_C(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_addition_carry_A_register(&reg.A, &reg.F, &reg.C);
}

void gb_cpu_ADC_A_D(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_addition_carry_A_register(&reg.A, &reg.F, &reg.D);
}

void gb_cpu_ADC_A_E(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_addition_carry_A_register(&reg.A, &reg.F, &reg.E);
}

void gb_cpu_ADC_A_H(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_addition_carry_A_register(&reg.A, &reg.F, &reg.H);
}

void gb_cpu_ADC_A_L(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_addition_carry_A_register(&reg.A, &reg.F, &reg.L);
}

void gb_cpu_ADC_A_HL(gb_instr_info_t *info)
{
	(void)info;
	uint8_t temp_res = gb_memory_read(reg.HL);
	gb_cpu_addition_carry_A_register(&reg.A, &reg.F, &temp_res);
}

void gb_cpu_ADC_A_A(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_addition_carry_A_register(&reg.A, &reg.F, &reg.A);
}

/*********************0x9X*/
void gb_cpu_SUB_B(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_subtraction_A_register(&reg.A, &reg.F, &reg.B);
}

void gb_cpu_SUB_C(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_subtraction_A_register(&reg.A, &reg.F, &reg.C);
}

void gb_cpu_SUB_D(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_subtraction_A_register(&reg.A, &reg.F, &reg.D);
}

void gb_cpu_SUB_E(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_subtraction_A_register(&reg.A, &reg.F, &reg.E);
}

void gb_cpu_SUB_H(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_subtraction_A_register(&reg.A, &reg.F, &reg.H);
}

void gb_cpu_SUB_L(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_subtraction_A_register(&reg.A, &reg.F, &reg.L);
}

void gb_cpu_SUB_HL(gb_instr_info_t *info)
{
	(void)info;
	uint8_t temp_res = gb_memory_read(reg.HL);
	gb_cpu_subtraction_A_register(&reg.A, &reg.F, &temp_res);
}

void gb_cpu_SUB_A(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_subtraction_A_register(&reg.A, &reg.F, &reg.A);
}

void gb_cpu_SBC_A_B(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_subtraction_carry_A_register(&reg.A, &reg.F, &reg.B);
}

void gb_cpu_SBC_A_C(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_subtraction_carry_A_register(&reg.A, &reg.F, &reg.C);
}

void gb_cpu_SBC_A_D(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_subtraction_carry_A_register(&reg.A, &reg.F, &reg.D);
}

void gb_cpu_SBC_A_E(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_subtraction_carry_A_register(&reg.A, &reg.F, &reg.E);
}

void gb_cpu_SBC_A_H(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_subtraction_carry_A_register(&reg.A, &reg.F, &reg.H);
}

void gb_cpu_SBC_A_L(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_subtraction_carry_A_register(&reg.A, &reg.F, &reg.L);
}

void gb_cpu_SBC_A_HL(gb_instr_info_t *info)
{
	(void)info;
	uint8_t temp_res = gb_memory_read(reg.HL);
	gb_cpu_subtraction_carry_A_register(&reg.A, &reg.F, &temp_res);
}

void gb_cpu_SBC_A_A(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_subtraction_carry_A_register(&reg.A, &reg.F, &reg.A);
}

/*********************0xAX*/
void gb_cpu_AND_B(gb_instr_info_t *info)
{
	(void)info;
	reg.A &= reg.B;
	reg.F = (reg.A == 0) ? 0xA0 : 0x20;
}

void gb_cpu_AND_C(gb_instr_info_t *info)
{
	(void)info;
	reg.A &= reg.C;
	reg.F = (reg.A == 0) ? 0xA0 : 0x20;
}

void gb_cpu_AND_D(gb_instr_info_t *info)
{
	(void)info;
	reg.A &= reg.D;
	reg.F = (reg.A == 0) ? 0xA0 : 0x20;
}

void gb_cpu_AND_E(gb_instr_info_t *info)
{
	(void)info;
	reg.A &= reg.E;
	reg.F = (reg.A == 0) ? 0xA0 : 0x20;
}

void gb_cpu_AND_H(gb_instr_info_t *info)
{
	(void)info;
	reg.A &= reg.H;
	reg.F = (reg.A == 0) ? 0xA0 : 0x20;
}

void gb_cpu_AND_L(gb_instr_info_t *info)
{
	(void)info;
	reg.A &= reg.L;
	reg.F = (reg.A == 0) ? 0xA0 : 0x20;
}

void gb_cpu_AND_HL(gb_instr_info_t *info)
{
	(void)info;
	reg.A &= gb_memory_read(reg.HL);
	reg.F = (reg.A == 0) ? 0xA0 : 0x20;
}

void gb_cpu_AND_A(gb_instr_info_t *info)
{
	(void)info;
	reg.A &= reg.A;
	reg.F = (reg.A == 0) ? 0xA0 : 0x20;
}

void gb_cpu_XOR_B(gb_instr_info_t *info)
{
	(void)info;
	reg.A ^= reg.B;
	reg.F = (reg.A == 0) ? 0x80 : 0x00;
}

void gb_cpu_XOR_C(gb_instr_info_t *info)
{
	(void)info;
	reg.A ^= reg.C;
	reg.F = (reg.A == 0) ? 0x80 : 0x00;
}

void gb_cpu_XOR_D(gb_instr_info_t *info)
{
	(void)info;
	reg.A ^= reg.D;
	reg.F = (reg.A == 0) ? 0x80 : 0x00;
}

void gb_cpu_XOR_E(gb_instr_info_t *info)
{
	(void)info;
	reg.A ^= reg.E;
	reg.F = (reg.A == 0) ? 0x80 : 0x00;
}

void gb_cpu_XOR_H(gb_instr_info_t *info)
{
	(void)info;
	reg.A ^= reg.H;
	reg.F = (reg.A == 0) ? 0x80 : 0x00;
}

void gb_cpu_XOR_L(gb_instr_info_t *info)
{
	(void)info;
	reg.A ^= reg.L;
	reg.F = (reg.A == 0) ? 0x80 : 0x00;
}

void gb_cpu_XOR_HL(gb_instr_info_t *info)
{
	(void)info;
	reg.A ^= gb_memory_read(reg.HL);
	reg.F = (reg.A == 0) ? 0x80 : 0x00;
}

void gb_cpu_XOR_A(gb_instr_info_t *info)
{
	(void)info;
	reg.A ^= reg.A;
	reg.F = (reg.A == 0) ? 0x80 : 0x00;
}

/*********************0xBX*/
void gb_cpu_OR_B(gb_instr_info_t *info)
{
	(void)info;
	reg.A |= reg.B;
	reg.F = (reg.A == 0) ? 0x80 : 0x00;
}

void gb_cpu_OR_C(gb_instr_info_t *info)
{
	(void)info;
	reg.A |= reg.C;
	reg.F = (reg.A == 0) ? 0x80 : 0x00;
}

void gb_cpu_OR_D(gb_instr_info_t *info)
{
	(void)info;
	reg.A |= reg.D;
	reg.F = (reg.A == 0) ? 0x80 : 0x00;
}

void gb_cpu_OR_E(gb_instr_info_t *info)
{
	(void)info;
	reg.A |= reg.E;
	reg.F = (reg.A == 0) ? 0x80 : 0x00;
}

void gb_cpu_OR_H(gb_instr_info_t *info)
{
	(void)info;
	reg.A |= reg.H;
	reg.F = (reg.A == 0) ? 0x80 : 0x00;
}

void gb_cpu_OR_L(gb_instr_info_t *info)
{
	(void)info;
	reg.A |= reg.L;
	reg.F = (reg.A == 0) ? 0x80 : 0x00;
}

void gb_cpu_OR_HL(gb_instr_info_t *info)
{
	(void)info;
	reg.A |= gb_memory_read(reg.HL);
	reg.F = (reg.A == 0) ? 0x80 : 0x00;
}

void gb_cpu_OR_A(gb_instr_info_t *info)
{
	(void)info;
	reg.A |= reg.A;
	reg.F = (reg.A == 0) ? 0x80 : 0x00;
}

void gb_cpu_CP_B(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_compare_A_register(&reg.A, &reg.F, &reg.B);
}

void gb_cpu_CP_C(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_compare_A_register(&reg.A, &reg.F, &reg.C);
}

void gb_cpu_CP_D(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_compare_A_register(&reg.A, &reg.F, &reg.D);
}

void gb_cpu_CP_E(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_compare_A_register(&reg.A, &reg.F, &reg.E);
}

void gb_cpu_CP_H(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_compare_A_register(&reg.A, &reg.F, &reg.H);
}

void gb_cpu_CP_L(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_compare_A_register(&reg.A, &reg.F, &reg.L);
}

void gb_cpu_CP_HL(gb_instr_info_t *info)
{
	(void)info;
	uint8_t temp_res = gb_memory_read(reg.HL);
	gb_cpu_compare_A_register(&reg.A, &reg.F, &temp_res);
}

void gb_cpu_CP_A(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_compare_A_register(&reg.A, &reg.F, &reg.A);
}

/*********************0xCX*/
void gb_cpu_RET_NZ(gb_instr_info_t *info)
{
	if (info->cycles == 5) {
		gb_cpu_return_from_stack(&reg.SP, &reg.PC);
	}
	info->cycles = CUSTOM_CYCLES;
}

void gb_cpu_POP_BC(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_pop_from_stack(&reg.SP, &reg.BC);
}

void gb_cpu_JP_NZ_a16(gb_instr_info_t *info)
{
	if (info->cycles == 4) {
		reg.PC = CAT_BYTES(gb_memory_read(reg.PC - 2), gb_memory_read(reg.PC - 1));
	}
	info->cycles = CUSTOM_CYCLES;
}

void gb_cpu_JP_a16(gb_instr_info_t *info)
{
	(void)info;
	reg.PC = CAT_BYTES(gb_memory_read(reg.PC - 2), gb_memory_read(reg.PC - 1));
}

void gb_cpu_CALL_NZ_a16(gb_instr_info_t *info)
{
	if (info->cycles == 6) {
		gb_cpu_push_to_stack(&reg.SP, &reg.PC);
		reg.PC = CAT_BYTES(gb_memory_read(reg.PC - 2), gb_memory_read(reg.PC - 1));
	}
	info->cycles = CUSTOM_CYCLES;
}

void gb_cpu_PUSH_BC(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_push_to_stack(&reg.SP, &reg.BC);
}

void gb_cpu_ADD_A_d8(gb_instr_info_t *info)
{
	(void)info;
	uint8_t temp_res = gb_memory_read(reg.PC - 1);
	gb_cpu_addition_A_register(&reg.A, &reg.F, &temp_res);
}

void gb_cpu_RST_00H(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_push_to_stack(&reg.SP, &reg.PC);
	reg.PC = 0x0000;
}

void gb_cpu_RET_Z(gb_instr_info_t *info)
{
	if (info->cycles == 5) {
		gb_cpu_return_from_stack(&reg.SP, &reg.PC);
	}
	info->cycles = CUSTOM_CYCLES;
}

void gb_cpu_RET(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_return_from_stack(&reg.SP, &reg.PC);
}

void gb_cpu_JP_Z_a16(gb_instr_info_t *info)
{
	if (info->cycles == 4) {
		reg.PC = CAT_BYTES(gb_memory_read(reg.PC - 2), gb_memory_read(reg.PC - 1));
	}
	info->cycles = CUSTOM_CYCLES;
}

void gb_cpu_PREFIX(gb_instr_info_t *info)
{
	((void (*)(gb_instr_info_t *))prefix_instructions[gb_memory_read(reg.PC - 1)].instr)(info);
}

void gb_cpu_CALL_Z_a16(gb_instr_info_t *info)
{
	if (info->cycles == 6) {
		gb_cpu_push_to_stack(&reg.SP, &reg.PC);
		reg.PC = CAT_BYTES(gb_memory_read(reg.PC - 2), gb_memory_read(reg.PC - 1));
	}
	info->cycles = CUSTOM_CYCLES;
}

void gb_cpu_CALL_a16(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_push_to_stack(&reg.SP, &reg.PC);
	reg.PC = CAT_BYTES(gb_memory_read(reg.PC - 2), gb_memory_read(reg.PC - 1));
}

void gb_cpu_ADC_A_d8(gb_instr_info_t *info)
{
	(void)info;
	uint8_t temp_res = gb_memory_read(reg.PC - 1);
	gb_cpu_addition_carry_A_register(&reg.A, &reg.F, &temp_res);
}

void gb_cpu_RST_08H(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_push_to_stack(&reg.SP, &reg.PC);
	reg.PC = 0x0008;
}

/*********************0xDX*/
void gb_cpu_RET_NC(gb_instr_info_t *info)
{

	if (info->cycles == 5) {
		gb_cpu_return_from_stack(&reg.SP, &reg.PC);
	}
	info->cycles = CUSTOM_CYCLES;
}

void gb_cpu_POP_DE(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_pop_from_stack(&reg.SP, &reg.DE);
}

void gb_cpu_JP_NC_a16(gb_instr_info_t *info)
{
	if (info->cycles == 4) {
		reg.PC = CAT_BYTES(gb_memory_read(reg.PC - 2), gb_memory_read(reg.PC - 1));
	}
	info->cycles = CUSTOM_CYCLES;
}

// -----------
void gb_cpu_CALL_NC_a16(gb_instr_info_t *info)
{
	if (info->cycles == 6) {
		gb_cpu_push_to_stack(&reg.SP, &reg.PC);
		reg.PC = CAT_BYTES(gb_memory_read(reg.PC - 2), gb_memory_read(reg.PC - 1));
	}
	info->cycles = CUSTOM_CYCLES;
}

void gb_cpu_PUSH_DE(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_push_to_stack(&reg.SP, &reg.DE);
}

void gb_cpu_SUB_d8(gb_instr_info_t *info)
{
	(void)info;
	uint8_t temp_res = gb_memory_read(reg.PC - 1);
	gb_cpu_subtraction_A_register(&reg.A, &reg.F, &temp_res);
}

void gb_cpu_RST_10H(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_push_to_stack(&reg.SP, &reg.PC);
	reg.PC = 0x0010;
}

void gb_cpu_RET_C(gb_instr_info_t *info)
{
	if (info->cycles == 5) {
		gb_cpu_return_from_stack(&reg.SP, &reg.PC);
	}
	info->cycles = CUSTOM_CYCLES;
}

void gb_cpu_RETI(gb_instr_info_t *info)
{
	(void)info;
	interrupt_master_enable = 1;
	gb_cpu_return_from_stack(&reg.SP, &reg.PC);
}

void gb_cpu_JP_C_a16(gb_instr_info_t *info)
{
	if (info->cycles == 4) {
		reg.PC = CAT_BYTES(gb_memory_read(reg.PC - 2), gb_memory_read(reg.PC - 1));
	}
	info->cycles = CUSTOM_CYCLES;
}

// -----------
void gb_cpu_CALL_C_a16(gb_instr_info_t *info)
{
	if (info->cycles == 6) {
		gb_cpu_push_to_stack(&reg.SP, &reg.PC);
		reg.PC = CAT_BYTES(gb_memory_read(reg.PC - 2), gb_memory_read(reg.PC - 1));
	}
	info->cycles = CUSTOM_CYCLES;
}

// -----------
void gb_cpu_SBC_A_d8(gb_instr_info_t *info)
{
	(void)info;
	uint8_t temp_res = gb_memory_read(reg.PC - 1);
	gb_cpu_subtraction_carry_A_register(&reg.A, &reg.F, &temp_res);
}

void gb_cpu_RST_18H(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_push_to_stack(&reg.SP, &reg.PC);
	reg.PC = 0x0018;
}

/*********************0xEX*/
void gb_cpu_LOAD_a8_A(gb_instr_info_t *info)
{
	(void)info;
	gb_memory_write(0xFF00 + gb_memory_read(reg.PC - 1), reg.A);
}

void gb_cpu_POP_HL(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_pop_from_stack(&reg.SP, &reg.HL);
}

void gb_cpu_LOAD_fC_A(gb_instr_info_t *info)
{
	(void)info;
	gb_memory_write(0xFF00 + reg.C, reg.A);
}

// -----------
// -----------
void gb_cpu_PUSH_HL(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_push_to_stack(&reg.SP, &reg.HL);
}

void gb_cpu_AND_d8(gb_instr_info_t *info)
{
	(void)info;
	reg.A &= gb_memory_read(reg.PC - 1);
	reg.F = (reg.A == 0) ? 0xA0 : 0x20;
}

void gb_cpu_RST20H(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_push_to_stack(&reg.SP, &reg.PC);
	reg.PC = 0x0020;
}

void gb_cpu_ADD_SP_r8(gb_instr_info_t *info)
{
	(void)info;
	uint8_t r8_val = gb_memory_read(reg.PC - 1);
	uint32_t temp_res = reg.SP + r8_val;
	((temp_res & 0xFF) < (reg.SP & 0xFF)) ? SET_BIT(reg.F, C_FLAG_BIT)
					      : RST_BIT(reg.F, C_FLAG_BIT);
	((temp_res & 0xF) < (reg.SP & 0xF)) ? SET_BIT(reg.F, H_FLAG_BIT)
					    : RST_BIT(reg.F, H_FLAG_BIT);
	RST_BIT(reg.F, Z_FLAG_BIT);
	RST_BIT(reg.F, N_FLAG_BIT);
	reg.SP += (int8_t)r8_val;
}

void gb_cpu_JP_HL(gb_instr_info_t *info)
{
	(void)info;
	reg.PC = reg.HL;
}

void gb_cpu_LOAD_a16_A(gb_instr_info_t *info)
{
	(void)info;
	gb_memory_write(CAT_BYTES(gb_memory_read(reg.PC - 2), gb_memory_read(reg.PC - 1)), reg.A);
}

// -----------
// -----------
// -----------
void gb_cpu_XOR_d8(gb_instr_info_t *info)
{
	(void)info;
	reg.A ^= gb_memory_read(reg.PC - 1);
	reg.F = (reg.A == 0) ? 0x80 : 0x00;
}

void gb_cpu_RST_28H(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_push_to_stack(&reg.SP, &reg.PC);
	reg.PC = 0x0028;
}

/*********************0xFX*/
void gb_cpu_LOAD_A_a8(gb_instr_info_t *info)
{
	(void)info;
	reg.A = gb_memory_read(0xFF00 + gb_memory_read(reg.PC - 1));
}

void gb_cpu_POP_AF(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_pop_from_stack(&reg.SP, &reg.AF);
	reg.AF &= 0xFFF0;
}

void gb_cpu_LOAD_A_fC(gb_instr_info_t *info)
{
	(void)info;
	reg.A = gb_memory_read(0xFF00 + reg.C);
}

void gb_cpu_DI(gb_instr_info_t *info)
{
	(void)info;
	interrupt_master_enable = 0;
}

// -----------
void gb_cpu_PUSH_AF(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_push_to_stack(&reg.SP, &reg.AF);
}

void gb_cpu_OR_d8(gb_instr_info_t *info)
{
	(void)info;
	reg.A |= gb_memory_read(reg.PC - 1);
	reg.F = (reg.A == 0) ? 0x80 : 0x00;
}

void gb_cpu_RST_30H(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_push_to_stack(&reg.SP, &reg.PC);
	reg.PC = 0x0030;
}

void gb_cpu_LOAD_HL_SP_r8(gb_instr_info_t *info)
{
	(void)info;
	uint8_t r8_val = gb_memory_read(reg.PC - 1);
	uint32_t temp_res = reg.SP + r8_val;
	((temp_res & 0xFF) < (reg.SP & 0xFF)) ? SET_BIT(reg.F, C_FLAG_BIT)
					      : RST_BIT(reg.F, C_FLAG_BIT);
	((temp_res & 0xF) < (reg.SP & 0xF)) ? SET_BIT(reg.F, H_FLAG_BIT)
					    : RST_BIT(reg.F, H_FLAG_BIT);
	RST_BIT(reg.F, Z_FLAG_BIT);
	RST_BIT(reg.F, N_FLAG_BIT);
	reg.HL = reg.SP + (int8_t)r8_val;
}

void gb_cpu_LOAD_SP_HL(gb_instr_info_t *info)
{
	(void)info;
	reg.SP = reg.HL;
}

void gb_cpu_LOAD_A_a16(gb_instr_info_t *info)
{
	(void)info;
	reg.A = gb_memory_read(CAT_BYTES(gb_memory_read(reg.PC - 2), gb_memory_read(reg.PC - 1)));
}

void gb_cpu_EI(gb_instr_info_t *info)
{
	(void)info;
	interrupt_master_enable = 1;
}

// -----------
// -----------
void gb_cpu_CP_d8(gb_instr_info_t *info)
{
	(void)info;
	uint8_t temp_res = gb_memory_read(reg.PC - 1);
	gb_cpu_compare_A_register(&reg.A, &reg.F, &temp_res);
}
void gb_cpu_RST_38H(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_push_to_stack(&reg.SP, &reg.PC);
	reg.PC = 0x0038;
}

/*Prefix implementation*/

/*********************0x0X*/
void gb_cpu_RLC_B(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_rotate_left_with_carry(&reg.B, &reg.F);
}

void gb_cpu_RLC_C(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_rotate_left_with_carry(&reg.C, &reg.F);
}

void gb_cpu_RLC_D(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_rotate_left_with_carry(&reg.D, &reg.F);
}

void gb_cpu_RLC_E(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_rotate_left_with_carry(&reg.E, &reg.F);
}

void gb_cpu_RLC_H(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_rotate_left_with_carry(&reg.H, &reg.F);
}

void gb_cpu_RLC_L(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_rotate_left_with_carry(&reg.L, &reg.F);
}

void gb_cpu_RLC_HL(gb_instr_info_t *info)
{
	static uint8_t temp_val = 0;
	if (info->current_cycle == 2) {
		temp_val = gb_memory_read(reg.HL);

	} else if (info->current_cycle == 3) {
		gb_cpu_rotate_left_with_carry(&temp_val, &reg.F);
		gb_memory_write(reg.HL, temp_val);
	}
	info->current_cycle = CUSTOM_TIMING;
}

void gb_cpu_RLC_A(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_rotate_left_with_carry(&reg.A, &reg.F);
}

void gb_cpu_RRC_B(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_rotate_right_with_carry(&reg.B, &reg.F);
}

void gb_cpu_RRC_C(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_rotate_right_with_carry(&reg.C, &reg.F);
}

void gb_cpu_RRC_D(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_rotate_right_with_carry(&reg.D, &reg.F);
}

void gb_cpu_RRC_E(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_rotate_right_with_carry(&reg.E, &reg.F);
}

void gb_cpu_RRC_H(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_rotate_right_with_carry(&reg.H, &reg.F);
}

void gb_cpu_RRC_L(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_rotate_right_with_carry(&reg.L, &reg.F);
}

void gb_cpu_RRC_HL(gb_instr_info_t *info)
{
	static uint8_t temp_val = 0;
	if (info->current_cycle == 2) {
		temp_val = gb_memory_read(reg.HL);

	} else if (info->current_cycle == 3) {
		gb_cpu_rotate_right_with_carry(&temp_val, &reg.F);
		gb_memory_write(reg.HL, temp_val);
	}
	info->current_cycle = CUSTOM_TIMING;
}

void gb_cpu_RRC_A(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_rotate_right_with_carry(&reg.A, &reg.F);
}

/*********************0x1X*/
void gb_cpu_RL_B(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_rotate_left_through_carry(&reg.B, &reg.F);
}

void gb_cpu_RL_C(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_rotate_left_through_carry(&reg.C, &reg.F);
}

void gb_cpu_RL_D(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_rotate_left_through_carry(&reg.D, &reg.F);
}

void gb_cpu_RL_E(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_rotate_left_through_carry(&reg.E, &reg.F);
}

void gb_cpu_RL_H(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_rotate_left_through_carry(&reg.H, &reg.F);
}

void gb_cpu_RL_L(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_rotate_left_through_carry(&reg.L, &reg.F);
}

void gb_cpu_RL_HL(gb_instr_info_t *info)
{
	static uint8_t temp_val = 0;
	if (info->current_cycle == 2) {
		temp_val = gb_memory_read(reg.HL);

	} else if (info->current_cycle == 3) {
		gb_cpu_rotate_left_through_carry(&temp_val, &reg.F);
		gb_memory_write(reg.HL, temp_val);
	}
	info->current_cycle = CUSTOM_TIMING;
}

void gb_cpu_RL_A(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_rotate_left_through_carry(&reg.A, &reg.F);
}

void gb_cpu_RR_B(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_rotate_right_through_carry(&reg.B, &reg.F);
}

void gb_cpu_RR_C(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_rotate_right_through_carry(&reg.C, &reg.F);
}

void gb_cpu_RR_D(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_rotate_right_through_carry(&reg.D, &reg.F);
}

void gb_cpu_RR_E(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_rotate_right_through_carry(&reg.E, &reg.F);
}

void gb_cpu_RR_H(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_rotate_right_through_carry(&reg.H, &reg.F);
}

void gb_cpu_RR_L(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_rotate_right_through_carry(&reg.L, &reg.F);
}

void gb_cpu_RR_HL(gb_instr_info_t *info)
{
	static uint8_t temp_val = 0;
	if (info->current_cycle == 2) {
		temp_val = gb_memory_read(reg.HL);

	} else if (info->current_cycle == 3) {
		gb_cpu_rotate_right_through_carry(&temp_val, &reg.F);
		gb_memory_write(reg.HL, temp_val);
	}
	info->current_cycle = CUSTOM_TIMING;
}

void gb_cpu_RR_A(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_rotate_right_through_carry(&reg.A, &reg.F);
}

/*********************0x2X*/
void gb_cpu_SLA_B(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_shift_left_into_carry(&reg.B, &reg.F);
}

void gb_cpu_SLA_C(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_shift_left_into_carry(&reg.C, &reg.F);
}

void gb_cpu_SLA_D(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_shift_left_into_carry(&reg.D, &reg.F);
}

void gb_cpu_SLA_E(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_shift_left_into_carry(&reg.E, &reg.F);
}

void gb_cpu_SLA_H(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_shift_left_into_carry(&reg.H, &reg.F);
}

void gb_cpu_SLA_L(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_shift_left_into_carry(&reg.L, &reg.F);
}

void gb_cpu_SLA_HL(gb_instr_info_t *info)
{
	static uint8_t temp_val = 0;
	if (info->current_cycle == 2) {
		temp_val = gb_memory_read(reg.HL);

	} else if (info->current_cycle == 3) {
		gb_cpu_shift_left_into_carry(&temp_val, &reg.F);
		gb_memory_write(reg.HL, temp_val);
	}
	info->current_cycle = CUSTOM_TIMING;
}

void gb_cpu_SLA_A(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_shift_left_into_carry(&reg.A, &reg.F);
}

void gb_cpu_SRA_B(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_shift_right_with_carry(&reg.B, &reg.F);
}

void gb_cpu_SRA_C(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_shift_right_with_carry(&reg.C, &reg.F);
}

void gb_cpu_SRA_D(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_shift_right_with_carry(&reg.D, &reg.F);
}

void gb_cpu_SRA_E(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_shift_right_with_carry(&reg.E, &reg.F);
}

void gb_cpu_SRA_H(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_shift_right_with_carry(&reg.H, &reg.F);
}

void gb_cpu_SRA_L(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_shift_right_with_carry(&reg.L, &reg.F);
}

void gb_cpu_SRA_HL(gb_instr_info_t *info)
{
	static uint8_t temp_val = 0;
	if (info->current_cycle == 2) {
		temp_val = gb_memory_read(reg.HL);

	} else if (info->current_cycle == 3) {
		gb_cpu_shift_right_with_carry(&temp_val, &reg.F);
		gb_memory_write(reg.HL, temp_val);
	}
	info->current_cycle = CUSTOM_TIMING;
}

void gb_cpu_SRA_A(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_shift_right_with_carry(&reg.A, &reg.F);
}

/*********************0x3X*/
void gb_cpu_SWAP_B(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_swap_nibbles(&reg.B, &reg.F);
}

void gb_cpu_SWAP_C(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_swap_nibbles(&reg.C, &reg.F);
}

void gb_cpu_SWAP_D(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_swap_nibbles(&reg.D, &reg.F);
}

void gb_cpu_SWAP_E(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_swap_nibbles(&reg.E, &reg.F);
}

void gb_cpu_SWAP_H(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_swap_nibbles(&reg.H, &reg.F);
}

void gb_cpu_SWAP_L(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_swap_nibbles(&reg.L, &reg.F);
}

void gb_cpu_SWAP_HL(gb_instr_info_t *info)
{
	static uint8_t temp_val = 0;
	if (info->current_cycle == 2) {
		temp_val = gb_memory_read(reg.HL);

	} else if (info->current_cycle == 3) {
		gb_cpu_swap_nibbles(&temp_val, &reg.F);
		gb_memory_write(reg.HL, temp_val);
	}
	info->current_cycle = CUSTOM_TIMING;
}

void gb_cpu_SWAP_A(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_swap_nibbles(&reg.A, &reg.F);
}

void gb_cpu_SRL_B(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_shift_right_into_carry(&reg.B, &reg.F);
}

void gb_cpu_SRL_C(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_shift_right_into_carry(&reg.C, &reg.F);
}

void gb_cpu_SRL_D(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_shift_right_into_carry(&reg.D, &reg.F);
}

void gb_cpu_SRL_E(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_shift_right_into_carry(&reg.E, &reg.F);
}

void gb_cpu_SRL_H(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_shift_right_into_carry(&reg.H, &reg.F);
}

void gb_cpu_SRL_L(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_shift_right_into_carry(&reg.L, &reg.F);
}

void gb_cpu_SRL_HL(gb_instr_info_t *info)
{
	static uint8_t temp_val = 0;
	if (info->current_cycle == 2) {
		temp_val = gb_memory_read(reg.HL);

	} else if (info->current_cycle == 3) {
		gb_cpu_shift_right_into_carry(&temp_val, &reg.F);
		gb_memory_write(reg.HL, temp_val);
	}
	info->current_cycle = CUSTOM_TIMING;
}

void gb_cpu_SRL_A(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_shift_right_into_carry(&reg.A, &reg.F);
}

/*********************0x4X*/
void gb_cpu_BIT_0_B(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_bit_check(&reg.B, 0, &reg.F);
}

void gb_cpu_BIT_0_C(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_bit_check(&reg.C, 0, &reg.F);
}

void gb_cpu_BIT_0_D(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_bit_check(&reg.D, 0, &reg.F);
}

void gb_cpu_BIT_0_E(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_bit_check(&reg.E, 0, &reg.F);
}

void gb_cpu_BIT_0_H(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_bit_check(&reg.H, 0, &reg.F);
}

void gb_cpu_BIT_0_L(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_bit_check(&reg.L, 0, &reg.F);
}

void gb_cpu_BIT_0_HL(gb_instr_info_t *info)
{
	(void)info;
	uint8_t temp_res = gb_memory_read(reg.HL);
	gb_cpu_bit_check(&temp_res, 0, &reg.F);
}

void gb_cpu_BIT_0_A(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_bit_check(&reg.A, 0, &reg.F);
}

void gb_cpu_BIT_1_B(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_bit_check(&reg.B, 1, &reg.F);
}

void gb_cpu_BIT_1_C(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_bit_check(&reg.C, 1, &reg.F);
}

void gb_cpu_BIT_1_D(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_bit_check(&reg.D, 1, &reg.F);
}

void gb_cpu_BIT_1_E(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_bit_check(&reg.E, 1, &reg.F);
}

void gb_cpu_BIT_1_H(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_bit_check(&reg.H, 1, &reg.F);
}

void gb_cpu_BIT_1_L(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_bit_check(&reg.L, 1, &reg.F);
}

void gb_cpu_BIT_1_HL(gb_instr_info_t *info)
{
	(void)info;
	uint8_t temp_res = gb_memory_read(reg.HL);
	gb_cpu_bit_check(&temp_res, 1, &reg.F);
}

void gb_cpu_BIT_1_A(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_bit_check(&reg.A, 1, &reg.F);
}

/*********************0x5X*/
void gb_cpu_BIT_2_B(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_bit_check(&reg.B, 2, &reg.F);
}

void gb_cpu_BIT_2_C(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_bit_check(&reg.C, 2, &reg.F);
}

void gb_cpu_BIT_2_D(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_bit_check(&reg.D, 2, &reg.F);
}

void gb_cpu_BIT_2_E(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_bit_check(&reg.E, 2, &reg.F);
}

void gb_cpu_BIT_2_H(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_bit_check(&reg.H, 2, &reg.F);
}

void gb_cpu_BIT_2_L(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_bit_check(&reg.L, 2, &reg.F);
}

void gb_cpu_BIT_2_HL(gb_instr_info_t *info)
{
	(void)info;
	uint8_t temp_res = gb_memory_read(reg.HL);
	gb_cpu_bit_check(&temp_res, 2, &reg.F);
}
void gb_cpu_BIT_2_A(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_bit_check(&reg.A, 2, &reg.F);
}

void gb_cpu_BIT_3_B(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_bit_check(&reg.B, 3, &reg.F);
}

void gb_cpu_BIT_3_C(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_bit_check(&reg.C, 3, &reg.F);
}

void gb_cpu_BIT_3_D(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_bit_check(&reg.D, 3, &reg.F);
}

void gb_cpu_BIT_3_E(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_bit_check(&reg.E, 3, &reg.F);
}

void gb_cpu_BIT_3_H(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_bit_check(&reg.H, 3, &reg.F);
}

void gb_cpu_BIT_3_L(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_bit_check(&reg.L, 3, &reg.F);
}

void gb_cpu_BIT_3_HL(gb_instr_info_t *info)
{
	(void)info;
	uint8_t temp_res = gb_memory_read(reg.HL);
	gb_cpu_bit_check(&temp_res, 3, &reg.F);
}

void gb_cpu_BIT_3_A(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_bit_check(&reg.A, 3, &reg.F);
}

/*********************0x6X*/
void gb_cpu_BIT_4_B(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_bit_check(&reg.B, 4, &reg.F);
}

void gb_cpu_BIT_4_C(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_bit_check(&reg.C, 4, &reg.F);
}

void gb_cpu_BIT_4_D(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_bit_check(&reg.D, 4, &reg.F);
}

void gb_cpu_BIT_4_E(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_bit_check(&reg.E, 4, &reg.F);
}

void gb_cpu_BIT_4_H(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_bit_check(&reg.H, 4, &reg.F);
}

void gb_cpu_BIT_4_L(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_bit_check(&reg.L, 4, &reg.F);
}

void gb_cpu_BIT_4_HL(gb_instr_info_t *info)
{
	(void)info;
	uint8_t temp_res = gb_memory_read(reg.HL);
	gb_cpu_bit_check(&temp_res, 4, &reg.F);
}

void gb_cpu_BIT_4_A(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_bit_check(&reg.A, 4, &reg.F);
}

void gb_cpu_BIT_5_B(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_bit_check(&reg.B, 5, &reg.F);
}

void gb_cpu_BIT_5_C(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_bit_check(&reg.C, 5, &reg.F);
}

void gb_cpu_BIT_5_D(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_bit_check(&reg.D, 5, &reg.F);
}

void gb_cpu_BIT_5_E(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_bit_check(&reg.E, 5, &reg.F);
}

void gb_cpu_BIT_5_H(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_bit_check(&reg.H, 5, &reg.F);
}

void gb_cpu_BIT_5_L(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_bit_check(&reg.L, 5, &reg.F);
}

void gb_cpu_BIT_5_HL(gb_instr_info_t *info)
{
	(void)info;
	uint8_t temp_res = gb_memory_read(reg.HL);
	gb_cpu_bit_check(&temp_res, 5, &reg.F);
}

void gb_cpu_BIT_5_A(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_bit_check(&reg.A, 5, &reg.F);
}

/*********************0x7X*/
void gb_cpu_BIT_6_B(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_bit_check(&reg.B, 6, &reg.F);
}

void gb_cpu_BIT_6_C(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_bit_check(&reg.C, 6, &reg.F);
}

void gb_cpu_BIT_6_D(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_bit_check(&reg.D, 6, &reg.F);
}

void gb_cpu_BIT_6_E(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_bit_check(&reg.E, 6, &reg.F);
}

void gb_cpu_BIT_6_H(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_bit_check(&reg.H, 6, &reg.F);
}

void gb_cpu_BIT_6_L(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_bit_check(&reg.L, 6, &reg.F);
}

void gb_cpu_BIT_6_HL(gb_instr_info_t *info)
{
	(void)info;
	uint8_t temp_res = gb_memory_read(reg.HL);
	gb_cpu_bit_check(&temp_res, 6, &reg.F);
}

void gb_cpu_BIT_6_A(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_bit_check(&reg.A, 6, &reg.F);
}

void gb_cpu_BIT_7_B(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_bit_check(&reg.B, 7, &reg.F);
}

void gb_cpu_BIT_7_C(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_bit_check(&reg.C, 7, &reg.F);
}

void gb_cpu_BIT_7_D(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_bit_check(&reg.D, 7, &reg.F);
}

void gb_cpu_BIT_7_E(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_bit_check(&reg.E, 7, &reg.F);
}

void gb_cpu_BIT_7_H(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_bit_check(&reg.H, 7, &reg.F);
}

void gb_cpu_BIT_7_L(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_bit_check(&reg.L, 7, &reg.F);
}

void gb_cpu_BIT_7_HL(gb_instr_info_t *info)
{
	(void)info;
	uint8_t temp_res = gb_memory_read(reg.HL);
	gb_cpu_bit_check(&temp_res, 7, &reg.F);
}

void gb_cpu_BIT_7_A(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_bit_check(&reg.A, 7, &reg.F);
}

/*********************0x8X*/
void gb_cpu_RES_0_B(gb_instr_info_t *info)
{
	(void)info;
	RST_BIT(reg.B, 0);
}

void gb_cpu_RES_0_C(gb_instr_info_t *info)
{
	(void)info;
	RST_BIT(reg.C, 0);
}

void gb_cpu_RES_0_D(gb_instr_info_t *info)
{
	(void)info;
	RST_BIT(reg.D, 0);
}

void gb_cpu_RES_0_E(gb_instr_info_t *info)
{
	(void)info;
	RST_BIT(reg.E, 0);
}

void gb_cpu_RES_0_H(gb_instr_info_t *info)
{
	(void)info;
	RST_BIT(reg.H, 0);
}

void gb_cpu_RES_0_L(gb_instr_info_t *info)
{
	(void)info;
	RST_BIT(reg.L, 0);
}

void gb_cpu_RES_0_HL(gb_instr_info_t *info)
{
	static uint8_t temp_res = 0;
	if (info->current_cycle == 2) {
		temp_res = gb_memory_read(reg.HL);
		RST_BIT(temp_res, 0);

	} else if (info->current_cycle == 3) {
		gb_memory_write(reg.HL, temp_res);
	}
	info->current_cycle = CUSTOM_TIMING;
}

void gb_cpu_RES_0_A(gb_instr_info_t *info)
{
	(void)info;
	RST_BIT(reg.A, 0);
}

void gb_cpu_RES_1_B(gb_instr_info_t *info)
{
	(void)info;
	RST_BIT(reg.B, 1);
}

void gb_cpu_RES_1_C(gb_instr_info_t *info)
{
	(void)info;
	RST_BIT(reg.C, 1);
}

void gb_cpu_RES_1_D(gb_instr_info_t *info)
{
	(void)info;
	RST_BIT(reg.D, 1);
}

void gb_cpu_RES_1_E(gb_instr_info_t *info)
{
	(void)info;
	RST_BIT(reg.E, 1);
}

void gb_cpu_RES_1_H(gb_instr_info_t *info)
{
	(void)info;
	RST_BIT(reg.H, 1);
}

void gb_cpu_RES_1_L(gb_instr_info_t *info)
{
	(void)info;
	RST_BIT(reg.L, 1);
}

void gb_cpu_RES_1_HL(gb_instr_info_t *info)
{
	static uint8_t temp_res = 0;
	if (info->current_cycle == 2) {
		temp_res = gb_memory_read(reg.HL);
		RST_BIT(temp_res, 1);

	} else if (info->current_cycle == 3) {
		gb_memory_write(reg.HL, temp_res);
	}
	info->current_cycle = CUSTOM_TIMING;
}

void gb_cpu_RES_1_A(gb_instr_info_t *info)
{
	(void)info;
	RST_BIT(reg.A, 1);
}

/*********************0x9X*/
void gb_cpu_RES_2_B(gb_instr_info_t *info)
{
	(void)info;
	RST_BIT(reg.B, 2);
}

void gb_cpu_RES_2_C(gb_instr_info_t *info)
{
	(void)info;
	RST_BIT(reg.C, 2);
}

void gb_cpu_RES_2_D(gb_instr_info_t *info)
{
	(void)info;
	RST_BIT(reg.D, 2);
}

void gb_cpu_RES_2_E(gb_instr_info_t *info)
{
	(void)info;
	RST_BIT(reg.E, 2);
}

void gb_cpu_RES_2_H(gb_instr_info_t *info)
{
	(void)info;
	RST_BIT(reg.H, 2);
}

void gb_cpu_RES_2_L(gb_instr_info_t *info)
{
	(void)info;
	RST_BIT(reg.L, 2);
}

void gb_cpu_RES_2_HL(gb_instr_info_t *info)
{
	static uint8_t temp_res = 0;
	if (info->current_cycle == 2) {
		temp_res = gb_memory_read(reg.HL);
		RST_BIT(temp_res, 2);

	} else if (info->current_cycle == 3) {
		gb_memory_write(reg.HL, temp_res);
	}
	info->current_cycle = CUSTOM_TIMING;
}

void gb_cpu_RES_2_A(gb_instr_info_t *info)
{
	(void)info;
	RST_BIT(reg.A, 2);
}

void gb_cpu_RES_3_B(gb_instr_info_t *info)
{
	(void)info;
	RST_BIT(reg.B, 3);
}

void gb_cpu_RES_3_C(gb_instr_info_t *info)
{
	(void)info;
	RST_BIT(reg.C, 3);
}

void gb_cpu_RES_3_D(gb_instr_info_t *info)
{
	(void)info;
	RST_BIT(reg.D, 3);
}

void gb_cpu_RES_3_E(gb_instr_info_t *info)
{
	(void)info;
	RST_BIT(reg.E, 3);
}

void gb_cpu_RES_3_H(gb_instr_info_t *info)
{
	(void)info;
	RST_BIT(reg.H, 3);
}

void gb_cpu_RES_3_L(gb_instr_info_t *info)
{
	(void)info;
	RST_BIT(reg.L, 3);
}

void gb_cpu_RES_3_HL(gb_instr_info_t *info)
{
	static uint8_t temp_res = 0;
	if (info->current_cycle == 2) {
		temp_res = gb_memory_read(reg.HL);
		RST_BIT(temp_res, 3);

	} else if (info->current_cycle == 3) {
		gb_memory_write(reg.HL, temp_res);
	}
	info->current_cycle = CUSTOM_TIMING;
}

void gb_cpu_RES_3_A(gb_instr_info_t *info)
{
	(void)info;
	RST_BIT(reg.A, 3);
}

/*********************0xAX*/
void gb_cpu_RES_4_B(gb_instr_info_t *info)
{
	(void)info;
	RST_BIT(reg.B, 4);
}

void gb_cpu_RES_4_C(gb_instr_info_t *info)
{
	(void)info;
	RST_BIT(reg.C, 4);
}

void gb_cpu_RES_4_D(gb_instr_info_t *info)
{
	(void)info;
	RST_BIT(reg.D, 4);
}

void gb_cpu_RES_4_E(gb_instr_info_t *info)
{
	(void)info;
	RST_BIT(reg.E, 4);
}

void gb_cpu_RES_4_H(gb_instr_info_t *info)
{
	(void)info;
	RST_BIT(reg.H, 4);
}

void gb_cpu_RES_4_L(gb_instr_info_t *info)
{
	(void)info;
	RST_BIT(reg.L, 4);
}

void gb_cpu_RES_4_HL(gb_instr_info_t *info)
{
	static uint8_t temp_res = 0;
	if (info->current_cycle == 2) {
		temp_res = gb_memory_read(reg.HL);
		RST_BIT(temp_res, 4);

	} else if (info->current_cycle == 3) {
		gb_memory_write(reg.HL, temp_res);
	}
	info->current_cycle = CUSTOM_TIMING;
}

void gb_cpu_RES_4_A(gb_instr_info_t *info)
{
	(void)info;
	RST_BIT(reg.A, 4);
}

void gb_cpu_RES_5_B(gb_instr_info_t *info)
{
	(void)info;
	RST_BIT(reg.B, 5);
}

void gb_cpu_RES_5_C(gb_instr_info_t *info)
{
	(void)info;
	RST_BIT(reg.C, 5);
}

void gb_cpu_RES_5_D(gb_instr_info_t *info)
{
	(void)info;
	RST_BIT(reg.D, 5);
}

void gb_cpu_RES_5_E(gb_instr_info_t *info)
{
	(void)info;
	RST_BIT(reg.E, 5);
}

void gb_cpu_RES_5_H(gb_instr_info_t *info)
{
	(void)info;
	RST_BIT(reg.H, 5);
}

void gb_cpu_RES_5_L(gb_instr_info_t *info)
{
	(void)info;
	RST_BIT(reg.L, 5);
}

void gb_cpu_RES_5_HL(gb_instr_info_t *info)
{
	static uint8_t temp_res = 0;
	if (info->current_cycle == 2) {
		temp_res = gb_memory_read(reg.HL);
		RST_BIT(temp_res, 5);

	} else if (info->current_cycle == 3) {
		gb_memory_write(reg.HL, temp_res);
	}
	info->current_cycle = CUSTOM_TIMING;
}

void gb_cpu_RES_5_A(gb_instr_info_t *info)
{
	(void)info;
	RST_BIT(reg.A, 5);
}

/*********************0xBX*/
void gb_cpu_RES_6_B(gb_instr_info_t *info)
{
	(void)info;
	RST_BIT(reg.B, 6);
}

void gb_cpu_RES_6_C(gb_instr_info_t *info)
{
	(void)info;
	RST_BIT(reg.C, 6);
}

void gb_cpu_RES_6_D(gb_instr_info_t *info)
{
	(void)info;
	RST_BIT(reg.D, 6);
}

void gb_cpu_RES_6_E(gb_instr_info_t *info)
{
	(void)info;
	RST_BIT(reg.E, 6);
}

void gb_cpu_RES_6_H(gb_instr_info_t *info)
{
	(void)info;
	RST_BIT(reg.H, 6);
}

void gb_cpu_RES_6_L(gb_instr_info_t *info)
{
	(void)info;
	RST_BIT(reg.L, 6);
}

void gb_cpu_RES_6_HL(gb_instr_info_t *info)
{
	static uint8_t temp_res = 0;
	if (info->current_cycle == 2) {
		temp_res = gb_memory_read(reg.HL);
		RST_BIT(temp_res, 6);

	} else if (info->current_cycle == 3) {
		gb_memory_write(reg.HL, temp_res);
	}
	info->current_cycle = CUSTOM_TIMING;
}

void gb_cpu_RES_6_A(gb_instr_info_t *info)
{
	(void)info;
	RST_BIT(reg.A, 6);
}

void gb_cpu_RES_7_B(gb_instr_info_t *info)
{
	(void)info;
	RST_BIT(reg.B, 7);
}

void gb_cpu_RES_7_C(gb_instr_info_t *info)
{
	(void)info;
	RST_BIT(reg.C, 7);
}

void gb_cpu_RES_7_D(gb_instr_info_t *info)
{
	(void)info;
	RST_BIT(reg.D, 7);
}

void gb_cpu_RES_7_E(gb_instr_info_t *info)
{
	(void)info;
	RST_BIT(reg.E, 7);
}

void gb_cpu_RES_7_H(gb_instr_info_t *info)
{
	(void)info;
	RST_BIT(reg.H, 7);
}

void gb_cpu_RES_7_L(gb_instr_info_t *info)
{
	(void)info;
	RST_BIT(reg.L, 7);
}

void gb_cpu_RES_7_HL(gb_instr_info_t *info)
{
	static uint8_t temp_res = 0;
	if (info->current_cycle == 2) {
		temp_res = gb_memory_read(reg.HL);
		RST_BIT(temp_res, 7);

	} else if (info->current_cycle == 3) {
		gb_memory_write(reg.HL, temp_res);
	}
	info->current_cycle = CUSTOM_TIMING;
}

void gb_cpu_RES_7_A(gb_instr_info_t *info)
{
	(void)info;
	RST_BIT(reg.A, 7);
}

/*********************0xCX*/
void gb_cpu_SET_0_B(gb_instr_info_t *info)
{
	(void)info;
	SET_BIT(reg.B, 0);
}

void gb_cpu_SET_0_C(gb_instr_info_t *info)
{
	(void)info;
	SET_BIT(reg.C, 0);
}

void gb_cpu_SET_0_D(gb_instr_info_t *info)
{
	(void)info;
	SET_BIT(reg.D, 0);
}

void gb_cpu_SET_0_E(gb_instr_info_t *info)
{
	(void)info;
	SET_BIT(reg.E, 0);
}

void gb_cpu_SET_0_H(gb_instr_info_t *info)
{
	(void)info;
	SET_BIT(reg.H, 0);
}

void gb_cpu_SET_0_L(gb_instr_info_t *info)
{
	(void)info;
	SET_BIT(reg.L, 0);
}

void gb_cpu_SET_0_HL(gb_instr_info_t *info)
{
	static uint8_t temp_res = 0;
	if (info->current_cycle == 2) {
		temp_res = gb_memory_read(reg.HL);
		SET_BIT(temp_res, 0);

	} else if (info->current_cycle == 3) {
		gb_memory_write(reg.HL, temp_res);
	}
	info->current_cycle = CUSTOM_TIMING;
}

void gb_cpu_SET_0_A(gb_instr_info_t *info)
{
	(void)info;
	SET_BIT(reg.A, 0);
}

void gb_cpu_SET_1_B(gb_instr_info_t *info)
{
	(void)info;
	SET_BIT(reg.B, 1);
}

void gb_cpu_SET_1_C(gb_instr_info_t *info)
{
	(void)info;
	SET_BIT(reg.C, 1);
}

void gb_cpu_SET_1_D(gb_instr_info_t *info)
{
	(void)info;
	SET_BIT(reg.D, 1);
}

void gb_cpu_SET_1_E(gb_instr_info_t *info)
{
	(void)info;
	SET_BIT(reg.E, 1);
}

void gb_cpu_SET_1_H(gb_instr_info_t *info)
{
	(void)info;
	SET_BIT(reg.H, 1);
}

void gb_cpu_SET_1_L(gb_instr_info_t *info)
{
	(void)info;
	SET_BIT(reg.L, 1);
}

void gb_cpu_SET_1_HL(gb_instr_info_t *info)
{
	static uint8_t temp_res = 0;
	if (info->current_cycle == 2) {
		temp_res = gb_memory_read(reg.HL);
		SET_BIT(temp_res, 1);

	} else if (info->current_cycle == 3) {
		gb_memory_write(reg.HL, temp_res);
	}
	info->current_cycle = CUSTOM_TIMING;
}

void gb_cpu_SET_1_A(gb_instr_info_t *info)
{
	(void)info;
	SET_BIT(reg.A, 1);
}

/*********************0xDX*/
void gb_cpu_SET_2_B(gb_instr_info_t *info)
{
	(void)info;
	SET_BIT(reg.B, 2);
}

void gb_cpu_SET_2_C(gb_instr_info_t *info)
{
	(void)info;
	SET_BIT(reg.C, 2);
}

void gb_cpu_SET_2_D(gb_instr_info_t *info)
{
	(void)info;
	SET_BIT(reg.D, 2);
}

void gb_cpu_SET_2_E(gb_instr_info_t *info)
{
	(void)info;
	SET_BIT(reg.E, 2);
}

void gb_cpu_SET_2_H(gb_instr_info_t *info)
{
	(void)info;
	SET_BIT(reg.H, 2);
}

void gb_cpu_SET_2_L(gb_instr_info_t *info)
{
	(void)info;
	SET_BIT(reg.L, 2);
}

void gb_cpu_SET_2_HL(gb_instr_info_t *info)
{
	static uint8_t temp_res = 0;
	if (info->current_cycle == 2) {
		temp_res = gb_memory_read(reg.HL);
		SET_BIT(temp_res, 2);

	} else if (info->current_cycle == 3) {
		gb_memory_write(reg.HL, temp_res);
	}
	info->current_cycle = CUSTOM_TIMING;
}

void gb_cpu_SET_2_A(gb_instr_info_t *info)
{
	(void)info;
	SET_BIT(reg.A, 2);
}

void gb_cpu_SET_3_B(gb_instr_info_t *info)
{
	(void)info;
	SET_BIT(reg.B, 3);
}

void gb_cpu_SET_3_C(gb_instr_info_t *info)
{
	(void)info;
	SET_BIT(reg.C, 3);
}

void gb_cpu_SET_3_D(gb_instr_info_t *info)
{
	(void)info;
	SET_BIT(reg.D, 3);
}

void gb_cpu_SET_3_E(gb_instr_info_t *info)
{
	(void)info;
	SET_BIT(reg.E, 3);
}

void gb_cpu_SET_3_H(gb_instr_info_t *info)
{
	(void)info;
	SET_BIT(reg.H, 3);
}

void gb_cpu_SET_3_L(gb_instr_info_t *info)
{
	(void)info;
	SET_BIT(reg.L, 3);
}

void gb_cpu_SET_3_HL(gb_instr_info_t *info)
{
	static uint8_t temp_res = 0;
	if (info->current_cycle == 2) {
		temp_res = gb_memory_read(reg.HL);
		SET_BIT(temp_res, 3);

	} else if (info->current_cycle == 3) {
		gb_memory_write(reg.HL, temp_res);
	}
	info->current_cycle = CUSTOM_TIMING;
}

void gb_cpu_SET_3_A(gb_instr_info_t *info)
{
	(void)info;
	SET_BIT(reg.A, 3);
}

/*********************0xEX*/
void gb_cpu_SET_4_B(gb_instr_info_t *info)
{
	(void)info;
	SET_BIT(reg.B, 4);
}

void gb_cpu_SET_4_C(gb_instr_info_t *info)
{
	(void)info;
	SET_BIT(reg.C, 4);
}

void gb_cpu_SET_4_D(gb_instr_info_t *info)
{
	(void)info;
	SET_BIT(reg.D, 4);
}

void gb_cpu_SET_4_E(gb_instr_info_t *info)
{
	(void)info;
	SET_BIT(reg.E, 4);
}

void gb_cpu_SET_4_H(gb_instr_info_t *info)
{
	(void)info;
	SET_BIT(reg.H, 4);
}

void gb_cpu_SET_4_L(gb_instr_info_t *info)
{
	(void)info;
	SET_BIT(reg.L, 4);
}

void gb_cpu_SET_4_HL(gb_instr_info_t *info)
{
	static uint8_t temp_res = 0;
	if (info->current_cycle == 2) {
		temp_res = gb_memory_read(reg.HL);
		SET_BIT(temp_res, 4);

	} else if (info->current_cycle == 3) {
		gb_memory_write(reg.HL, temp_res);
	}
	info->current_cycle = CUSTOM_TIMING;
}

void gb_cpu_SET_4_A(gb_instr_info_t *info)
{
	(void)info;
	SET_BIT(reg.A, 4);
}

void gb_cpu_SET_5_B(gb_instr_info_t *info)
{
	(void)info;
	SET_BIT(reg.B, 5);
}

void gb_cpu_SET_5_C(gb_instr_info_t *info)
{
	(void)info;
	SET_BIT(reg.C, 5);
}

void gb_cpu_SET_5_D(gb_instr_info_t *info)
{
	(void)info;
	SET_BIT(reg.D, 5);
}

void gb_cpu_SET_5_E(gb_instr_info_t *info)
{
	(void)info;
	SET_BIT(reg.E, 5);
}

void gb_cpu_SET_5_H(gb_instr_info_t *info)
{
	(void)info;
	SET_BIT(reg.H, 5);
}

void gb_cpu_SET_5_L(gb_instr_info_t *info)
{
	(void)info;
	SET_BIT(reg.L, 5);
}

void gb_cpu_SET_5_HL(gb_instr_info_t *info)
{
	static uint8_t temp_res = 0;
	if (info->current_cycle == 2) {
		temp_res = gb_memory_read(reg.HL);
		SET_BIT(temp_res, 5);

	} else if (info->current_cycle == 3) {
		gb_memory_write(reg.HL, temp_res);
	}
	info->current_cycle = CUSTOM_TIMING;
}

void gb_cpu_SET_5_A(gb_instr_info_t *info)
{
	(void)info;
	SET_BIT(reg.A, 5);
}

/*********************0xFX*/
void gb_cpu_SET_6_B(gb_instr_info_t *info)
{
	(void)info;
	SET_BIT(reg.B, 6);
}

void gb_cpu_SET_6_C(gb_instr_info_t *info)
{
	(void)info;
	SET_BIT(reg.C, 6);
}

void gb_cpu_SET_6_D(gb_instr_info_t *info)
{
	(void)info;
	SET_BIT(reg.D, 6);
}

void gb_cpu_SET_6_E(gb_instr_info_t *info)
{
	(void)info;
	SET_BIT(reg.E, 6);
}

void gb_cpu_SET_6_H(gb_instr_info_t *info)
{
	(void)info;
	SET_BIT(reg.H, 6);
}

void gb_cpu_SET_6_L(gb_instr_info_t *info)
{
	(void)info;
	SET_BIT(reg.L, 6);
}

void gb_cpu_SET_6_HL(gb_instr_info_t *info)
{
	static uint8_t temp_res = 0;
	if (info->current_cycle == 2) {
		temp_res = gb_memory_read(reg.HL);
		SET_BIT(temp_res, 6);

	} else if (info->current_cycle == 3) {
		gb_memory_write(reg.HL, temp_res);
	}
	info->current_cycle = CUSTOM_TIMING;
}

void gb_cpu_SET_6_A(gb_instr_info_t *info)
{
	(void)info;
	SET_BIT(reg.A, 6);
}

void gb_cpu_SET_7_B(gb_instr_info_t *info)
{
	(void)info;
	SET_BIT(reg.B, 7);
}

void gb_cpu_SET_7_C(gb_instr_info_t *info)
{
	(void)info;
	SET_BIT(reg.C, 7);
}

void gb_cpu_SET_7_D(gb_instr_info_t *info)
{
	(void)info;
	SET_BIT(reg.D, 7);
}

void gb_cpu_SET_7_E(gb_instr_info_t *info)
{
	(void)info;
	SET_BIT(reg.E, 7);
}

void gb_cpu_SET_7_H(gb_instr_info_t *info)
{
	(void)info;
	SET_BIT(reg.H, 7);
}

void gb_cpu_SET_7_L(gb_instr_info_t *info)
{
	(void)info;
	SET_BIT(reg.L, 7);
}

void gb_cpu_SET_7_HL(gb_instr_info_t *info)
{
	static uint8_t temp_res = 0;
	if (info->current_cycle == 2) {
		temp_res = gb_memory_read(reg.HL);
		SET_BIT(temp_res, 7);

	} else if (info->current_cycle == 3) {
		gb_memory_write(reg.HL, temp_res);
	}
	info->current_cycle = CUSTOM_TIMING;
}

void gb_cpu_SET_7_A(gb_instr_info_t *info)
{
	(void)info;
	SET_BIT(reg.A, 7);
}

uint8_t fetch_custom_duration(int opcode)
{
	switch (opcode) {
	case 0x20:
		return (CHK_BIT(reg.F, Z_FLAG_BIT) != 0) ? 2 : 3;
	case 0x28:
		return (CHK_BIT(reg.F, Z_FLAG_BIT) != 0) ? 3 : 2;
	case 0x30:
		return (CHK_BIT(reg.F, C_FLAG_BIT) != 0) ? 2 : 3;
	case 0x38:
		return (CHK_BIT(reg.F, C_FLAG_BIT) != 0) ? 3 : 2;
	case 0xC0:
		return (CHK_BIT(reg.F, Z_FLAG_BIT)) ? 2 : 5;
	case 0xC2:
		return (CHK_BIT(reg.F, Z_FLAG_BIT) != 0) ? 3 : 4;
	case 0xC4:
		return (CHK_BIT(reg.F, Z_FLAG_BIT) != 0) ? 3 : 6;
	case 0xC8:
		return (CHK_BIT(reg.F, Z_FLAG_BIT)) ? 5 : 2;
	case 0xCA:
		return (CHK_BIT(reg.F, Z_FLAG_BIT) != 0) ? 4 : 3;
	case 0xCC:
		return (CHK_BIT(reg.F, Z_FLAG_BIT) != 0) ? 6 : 3;
	case 0xD0:
		return (CHK_BIT(reg.F, C_FLAG_BIT)) ? 2 : 5;
	case 0xD2:
		return (CHK_BIT(reg.F, C_FLAG_BIT) != 0) ? 3 : 4;
	case 0xD4:
		return (CHK_BIT(reg.F, C_FLAG_BIT) != 0) ? 3 : 6;
	case 0xD8:
		return (CHK_BIT(reg.F, C_FLAG_BIT)) ? 5 : 2;
	case 0xDA:
		return (CHK_BIT(reg.F, C_FLAG_BIT) != 0) ? 4 : 3;
	case 0xDC:
		return (CHK_BIT(reg.F, C_FLAG_BIT) != 0) ? 6 : 3;
	default:
		LOG_ERR("BAD OPCODE %x", opcode);
		return 0;
	}
}

/**
 * @brief If the corresponding IE and IF are both set then jump to the
 * corresponding ISR
 * @returns Nothing
 */
void gb_cpu_interrupt_handler(int *interupt_dur)
{
	if (one_cycle_interrupt_delay == 1) {
		if (gb_memory_read(IE_ADDR) & gb_memory_read(IF_ADDR) & 0x1F) {
			interrupt_master_enable = 0;
			one_cycle_interrupt_delay = 0;
			halted = 0;
			uint8_t interrupt_triggered = 0;
			uint8_t interrupt_set_and_enabled =
				gb_memory_read(IE_ADDR) & gb_memory_read(IF_ADDR);
			if (interrupt_set_and_enabled & VBLANK_INTERRUPT) {
				gb_memory_reset_bit(IF_ADDR, 0);
				gb_cpu_push_to_stack(&reg.SP, &reg.PC);
				interrupt_triggered = 1;
				reg.PC = VBLANK_VECTOR;
			} else if (interrupt_set_and_enabled & LCDSTAT_INTERRUPT) {
				gb_memory_reset_bit(IF_ADDR, 1);
				gb_cpu_push_to_stack(&reg.SP, &reg.PC);
				interrupt_triggered = 1;
				reg.PC = LCDSTAT_VECTOR;
			} else if (interrupt_set_and_enabled & TIMER_INTERRUPT) {
				gb_memory_reset_bit(IF_ADDR, 2);
				gb_cpu_push_to_stack(&reg.SP, &reg.PC);
				interrupt_triggered = 1;
				reg.PC = TIMER_VECTOR;
			} else if (interrupt_set_and_enabled & SERIAL_INTERRUPT) {
				gb_memory_reset_bit(IF_ADDR, 3);
				gb_cpu_push_to_stack(&reg.SP, &reg.PC);
				interrupt_triggered = 1;
				reg.PC = SERIAL_VECTOR;
			} else if (interrupt_set_and_enabled & JOYPAD_INTERRUPT) {
				gb_memory_reset_bit(IF_ADDR, 4);
				gb_cpu_push_to_stack(&reg.SP, &reg.PC);
				interrupt_triggered = 1;
				reg.PC = JOYPAD_VECTOR;
			}

			if (interrupt_triggered == 1) {
				*interupt_dur += 5;
			}
		}

	} else {
		one_cycle_interrupt_delay++;
	}
}

/**
 * @brief If the CPU is halted and interrupt_master_enable is not set, then this
 * function will unhalt the CPU if both IE and IF flags are set without jumping
 * to the ISR
 * @returns Nothing
 */
void gb_cpu_halted_handler(void)
{
	if (one_cycle_interrupt_delay == 1) {
		if (gb_memory_read(IE_ADDR) & gb_memory_read(IF_ADDR) & 0x1F) {
			interrupt_master_enable = 0;
			one_cycle_interrupt_delay = 0;
			uint8_t interrupt_set_and_enabled =
				gb_memory_read(IE_ADDR) & gb_memory_read(IF_ADDR);
			if (interrupt_set_and_enabled & VBLANK_INTERRUPT) {
				halted = 0;
			} else if (interrupt_set_and_enabled & LCDSTAT_INTERRUPT) {
				halted = 0;
			} else if (interrupt_set_and_enabled & TIMER_INTERRUPT) {
				halted = 0;
			} else if (interrupt_set_and_enabled & SERIAL_INTERRUPT) {
				halted = 0;
			} else if (interrupt_set_and_enabled & JOYPAD_INTERRUPT) {
				halted = 0;
			}
		}
	} else {
		one_cycle_interrupt_delay++;
	}
}

void gb_cpu_init(void)
{
	stopped = 0;
	halted = 0;
	interrupt_master_enable = 0;
	one_cycle_interrupt_delay = 0;
	op_remaining = 0;
	interupt_dur = 0;
	opcode = 0;
	current_cycle = 0;
}

/**
 * @brief fetch, decode and execute 1 CPU instruction, increment timers and jump
 * to interrupt handler
 * @param opcode Opcode of instruction to be executed
 * @returns Nothing
 */
void gb_cpu_step(void)
{

	/* IF not halted */
	if (halted) {
		goto finally;
	}

	if (interupt_dur) {
		interupt_dur--;
		op_remaining++;
		goto finally;
	}

	/* if no remaining op */
	if (op_remaining <= 0) {

		opcode = gb_memory_read(reg.PC);

		/* Inc PC */
		reg.PC += (opcode != 0xCB)
				  ? instructions[opcode].info.bytes
				  : prefix_instructions[gb_memory_read(reg.PC + 1)].info.bytes;

		/* Find new cycle time */
		if (opcode == 0xCB) {
			op_remaining = prefix_instructions[gb_memory_read(reg.PC - 1)].info.cycles;
		} else if (instructions[opcode].info.cycles == CUSTOM_CYCLES) {
			op_remaining = fetch_custom_duration(opcode);
			instructions[opcode].info.cycles = op_remaining;
		} else {
			op_remaining = instructions[opcode].info.cycles;
		}

		/* First cylce */
		current_cycle = 1;
	}

	if (opcode == 0xCB) {
		uint8_t prefix_opcode = gb_memory_read(reg.PC - 1);

		if (prefix_instructions[prefix_opcode].info.current_cycle == current_cycle ||
		    prefix_instructions[prefix_opcode].info.current_cycle == CUSTOM_TIMING) {

			if (prefix_instructions[prefix_opcode].info.current_cycle ==
			    CUSTOM_TIMING) {
				prefix_instructions[prefix_opcode].info.current_cycle =
					current_cycle;
			}
			((void (*)(gb_instr_info_t *))instructions[opcode].instr)(
				&prefix_instructions[prefix_opcode].info);
		}
	}

	else if (instructions[opcode].info.current_cycle == current_cycle ||
		 instructions[opcode].info.current_cycle == CUSTOM_TIMING) {
		if (instructions[opcode].info.current_cycle == CUSTOM_TIMING) {
			instructions[opcode].info.current_cycle = current_cycle;
		}
		((void (*)(gb_instr_info_t *))instructions[opcode].instr)(
			&instructions[opcode].info);
	}
	current_cycle++;

finally:
	/* Inc timers */
	gb_memory_inc_timers(1);

	/* dec op remaining */
	op_remaining--;

	/* Handle interupts */
	if (interrupt_master_enable == 1 && op_remaining == 0) {
		gb_cpu_interrupt_handler(&interupt_dur);
	} else if (halted == 1 && op_remaining == 0) {
		gb_cpu_halted_handler();
	}
}
