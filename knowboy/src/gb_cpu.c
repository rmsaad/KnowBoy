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
static uint8_t stopped = 0;
static uint8_t halted = 0;
static uint8_t interrupt_master_enable = 0;
static uint8_t one_cycle_interrupt_delay = 0;
static uint8_t op_remaining = 0;
static int interupt_dur = 0;
static uint8_t opcode = 0;
static uint8_t current_cycle = 0;
static bool dont_update_pc = false;
extern memory_t mem;

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
				{gb_cpu_JR_r8, {2, 3, 3}},
				{gb_cpu_ADD_HL_DE, {1, 2, 1}},
				{gb_cpu_LOAD_A_DE, {1, 2, 1}},
				{gb_cpu_DEC_DE, {1, 2, 1}},
				{gb_cpu_INC_E, {1, 1, 1}},
				{gb_cpu_DEC_E, {1, 1, 1}},
				{gb_cpu_LOAD_E_d8, {2, 2, 1}},
				{gb_cpu_RRA, {1, 1, 1}},
				{gb_cpu_JR_NZ_r8, {2, CUSTOM_TIMING, 1}},
				{gb_cpu_LOAD_HL_d16, {3, 3, 1}},
				{gb_cpu_LOAD_HLI_A, {1, 2, 1}},
				{gb_cpu_INC_HL, {1, 2, 1}},
				{gb_cpu_INC_H, {1, 1, 1}},
				{gb_cpu_DEC_H, {1, 1, 1}},
				{gb_cpu_LOAD_H_d8, {2, 2, 1}},
				{gb_cpu_DAA, {1, 1, 1}},
				{gb_cpu_JR_Z_r8, {2, CUSTOM_TIMING, 1}},
				{gb_cpu_ADD_HL_HL, {1, 2, 1}},
				{gb_cpu_LOAD_A_HLI, {1, 2, 1}},
				{gb_cpu_DEC_HL, {1, 2, 1}},
				{gb_cpu_INC_L, {1, 1, 1}},
				{gb_cpu_DEC_L, {1, 1, 1}},
				{gb_cpu_LOAD_L_d8, {2, 2, 1}},
				{gb_cpu_CPL, {1, 1, 1}},
				{gb_cpu_JR_NC_r8, {2, CUSTOM_TIMING, 1}},
				{gb_cpu_LOAD_SP_d16, {3, 3, 1}},
				{gb_cpu_LOAD_HLD_A, {1, 2, 1}},
				{gb_cpu_INC_SP, {1, 2, 1}},
				{gb_cpu_INC_HL_ADDR, {1, 3, CUSTOM_TIMING}},
				{gb_cpu_DEC_HL_ADDR, {1, 3, CUSTOM_TIMING}},
				{gb_cpu_LOAD_HL_d8, {2, 3, 2}},
				{gb_cpu_SCF, {1, 1, 1}},
				{gb_cpu_JR_C_r8, {2, CUSTOM_TIMING, 1}},
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
				{gb_cpu_RET_NZ, {1, CUSTOM_TIMING, 1}},
				{gb_cpu_POP_BC, {1, 3, 1}},
				{gb_cpu_JP_NZ_a16, {3, CUSTOM_TIMING, 1}},
				{gb_cpu_JP_a16, {3, 4, 4}},
				{gb_cpu_CALL_NZ_a16, {3, CUSTOM_TIMING, 1}},
				{gb_cpu_PUSH_BC, {1, 4, 1}},
				{gb_cpu_ADD_A_d8, {2, 2, 1}},
				{gb_cpu_RST_00H, {1, 4, 1}},
				{gb_cpu_RET_Z, {1, CUSTOM_TIMING, 1}},
				{gb_cpu_RET, {1, 4, 1}},
				{gb_cpu_JP_Z_a16, {3, CUSTOM_TIMING, 1}},
				{gb_cpu_PREFIX, {1, 1, CUSTOM_TIMING}},
				{gb_cpu_CALL_Z_a16, {3, CUSTOM_TIMING, 1}},
				{gb_cpu_CALL_a16, {3, 6, 1}},
				{gb_cpu_ADC_A_d8, {2, 2, 1}},
				{gb_cpu_RST_08H, {1, 4, 1}},
				{gb_cpu_RET_NC, {1, CUSTOM_TIMING, 1}},
				{gb_cpu_POP_DE, {1, 3, 1}},
				{gb_cpu_JP_NC_a16, {3, CUSTOM_TIMING, 1}},
				{NULL, {0, 0, 0}},
				{gb_cpu_CALL_NC_a16, {3, CUSTOM_TIMING, 1}},
				{gb_cpu_PUSH_DE, {1, 4, 1}},
				{gb_cpu_SUB_d8, {2, 2, 1}},
				{gb_cpu_RST_10H, {1, 4, 1}},
				{gb_cpu_RET_C, {1, CUSTOM_TIMING, 1}},
				{gb_cpu_RETI, {1, 4, 1}},
				{gb_cpu_JP_C_a16, {3, CUSTOM_TIMING, 1}},
				{NULL, {0, 0, 0}},
				{gb_cpu_CALL_C_a16, {3, CUSTOM_TIMING, 1}},
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
static void gb_cpu_NOP(gb_instr_info_t *info)
{
	(void)info;
}

static void gb_cpu_LOAD_BC_d16(gb_instr_info_t *info)
{
	(void)info;
	mem.reg.BC = CAT_BYTES(gb_memory_read(mem.reg.PC + 1), gb_memory_read(mem.reg.PC + 2));
}

static void gb_cpu_LOAD_BC_A(gb_instr_info_t *info)
{
	(void)info;
	gb_memory_write(mem.reg.BC, mem.reg.A);
}

static void gb_cpu_INC_BC(gb_instr_info_t *info)
{
	(void)info;
	mem.reg.BC++;
}

static void gb_cpu_INC_B(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_inc_register(&mem.reg.B, &mem.reg.F);
}

static void gb_cpu_DEC_B(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_dec_register(&mem.reg.B, &mem.reg.F);
}

static void gb_cpu_LOAD_B_d8(gb_instr_info_t *info)
{
	(void)info;
	mem.reg.B = gb_memory_read(mem.reg.PC + 1);
}

static void gb_cpu_RLCA(gb_instr_info_t *info)
{
	(void)info;
	uint8_t temp_carry = CHK_BIT(mem.reg.A, 7);
	mem.reg.F = (temp_carry != 0) ? C_FLAG_VAL : 0x00;
	mem.reg.A <<= 1;
	mem.reg.A += temp_carry;
}

static void gb_cpu_LOAD_a16_SP(gb_instr_info_t *info)
{
	(void)info;
	gb_memory_write_short(
		CAT_BYTES(gb_memory_read(mem.reg.PC + 1), gb_memory_read(mem.reg.PC + 2)),
		mem.reg.SP);
}

static void gb_cpu_ADD_HL_BC(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_addition_HL_register(&mem.reg.HL, &mem.reg.BC, &mem.reg.F);
}

static void gb_cpu_LOAD_A_BC(gb_instr_info_t *info)
{
	(void)info;
	mem.reg.A = gb_memory_read(mem.reg.BC);
}

static void gb_cpu_DEC_BC(gb_instr_info_t *info)
{
	(void)info;
	mem.reg.BC--;
}

static void gb_cpu_INC_C(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_inc_register(&mem.reg.C, &mem.reg.F);
}

static void gb_cpu_DEC_C(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_dec_register(&mem.reg.C, &mem.reg.F);
}

static void gb_cpu_LOAD_C_d8(gb_instr_info_t *info)
{
	(void)info;
	mem.reg.C = gb_memory_read(mem.reg.PC + 1);
}

static void gb_cpu_RRCA(gb_instr_info_t *info)
{
	(void)info;
	uint8_t temp_carry = CHK_BIT(mem.reg.A, 0);
	mem.reg.F = (temp_carry != 0) ? C_FLAG_VAL : 0x00;
	mem.reg.A >>= 1;
	if (temp_carry != 0) {
		SET_BIT(mem.reg.A, 7);
	}
}

/*********************0x1X*/
static void gb_cpu_STOP(gb_instr_info_t *info)
{
	(void)info;
	stopped = 1;
} // MORE NEEDED TO IMPLEMENT LATER

static void gb_cpu_LOAD_DE_d16(gb_instr_info_t *info)
{
	(void)info;
	mem.reg.DE = CAT_BYTES(gb_memory_read(mem.reg.PC + 1), gb_memory_read(mem.reg.PC + 2));
}

static void gb_cpu_LOAD_DE_A(gb_instr_info_t *info)
{
	(void)info;
	gb_memory_write(mem.reg.DE, mem.reg.A);
}

static void gb_cpu_INC_DE(gb_instr_info_t *info)
{
	(void)info;
	mem.reg.DE++;
}

static void gb_cpu_INC_D(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_inc_register(&mem.reg.D, &mem.reg.F);
}

static void gb_cpu_DEC_D(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_dec_register(&mem.reg.D, &mem.reg.F);
}

static void gb_cpu_LOAD_D_d8(gb_instr_info_t *info)
{
	(void)info;
	mem.reg.D = gb_memory_read(mem.reg.PC + 1);
}

static void gb_cpu_RLA(gb_instr_info_t *info)
{
	(void)info;
	uint8_t prev_carry = CHK_BIT(mem.reg.F, C_FLAG_BIT);
	mem.reg.F = ((mem.reg.A & 0x80) != 0) ? C_FLAG_VAL : 0x00;
	mem.reg.A <<= 1;
	mem.reg.A += prev_carry;
}

static void gb_cpu_JR_r8(gb_instr_info_t *info)
{
	(void)info;
	mem.reg.PC += (int8_t)gb_memory_read(mem.reg.PC + 1);
}

static void gb_cpu_ADD_HL_DE(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_addition_HL_register(&mem.reg.HL, &mem.reg.DE, &mem.reg.F);
}

static void gb_cpu_LOAD_A_DE(gb_instr_info_t *info)
{
	(void)info;
	mem.reg.A = gb_memory_read(mem.reg.DE);
}

static void gb_cpu_DEC_DE(gb_instr_info_t *info)
{
	(void)info;
	mem.reg.DE--;
}

static void gb_cpu_INC_E(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_inc_register(&mem.reg.E, &mem.reg.F);
}

static void gb_cpu_DEC_E(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_dec_register(&mem.reg.E, &mem.reg.F);
}

static void gb_cpu_LOAD_E_d8(gb_instr_info_t *info)
{
	(void)info;
	mem.reg.E = gb_memory_read(mem.reg.PC + 1);
}

static void gb_cpu_RRA(gb_instr_info_t *info)
{
	(void)info;
	uint8_t prev_carry = CHK_BIT(mem.reg.F, C_FLAG_BIT);
	mem.reg.F = ((mem.reg.A & 0x01) != 0) ? C_FLAG_VAL : 0x00;
	mem.reg.A >>= 1;
	mem.reg.A += (prev_carry << 7);
}

/*********************0x2X*/
static void gb_cpu_JR_NZ_r8(gb_instr_info_t *info)
{
	if (info->current_cycle == 1) {
		op_remaining = (CHK_BIT(mem.reg.F, Z_FLAG_BIT) != 0) ? 2 : 3;
	} else if (info->current_cycle == 3) {
		int8_t r8_val = (int8_t)gb_memory_read(mem.reg.PC + 1);
		mem.reg.PC += r8_val;
	}
	info->current_cycle = CUSTOM_TIMING;
}

static void gb_cpu_LOAD_HL_d16(gb_instr_info_t *info)
{
	(void)info;
	mem.reg.HL = CAT_BYTES(gb_memory_read(mem.reg.PC + 1), gb_memory_read(mem.reg.PC + 2));
}

static void gb_cpu_LOAD_HLI_A(gb_instr_info_t *info)
{
	(void)info;
	gb_memory_write(mem.reg.HL, mem.reg.A);
	mem.reg.HL++;
}

static void gb_cpu_INC_HL(gb_instr_info_t *info)
{
	(void)info;
	mem.reg.HL++;
}

static void gb_cpu_INC_H(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_inc_register(&mem.reg.H, &mem.reg.F);
}

static void gb_cpu_DEC_H(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_dec_register(&mem.reg.H, &mem.reg.F);
}

static void gb_cpu_LOAD_H_d8(gb_instr_info_t *info)
{
	(void)info;
	mem.reg.H = gb_memory_read(mem.reg.PC + 1);
}

static void gb_cpu_DAA(gb_instr_info_t *info)
{
	(void)info;
	uint16_t tempShort = mem.reg.A;
	if (CHK_BIT(mem.reg.F, N_FLAG_BIT) != 0) {
		if (CHK_BIT(mem.reg.F, H_FLAG_BIT) != 0)
			tempShort += 0xFA;
		if (CHK_BIT(mem.reg.F, C_FLAG_BIT) != 0)
			tempShort += 0xA0;
	} else {
		if (CHK_BIT(mem.reg.F, H_FLAG_BIT) || (tempShort & 0xF) > 9)
			tempShort += 0x06;
		if (CHK_BIT(mem.reg.F, C_FLAG_BIT) || ((tempShort & 0x1F0) > 0x90)) {
			tempShort += 0x60;
			SET_BIT(mem.reg.F, C_FLAG_BIT);
		} else {
			RST_BIT(mem.reg.F, C_FLAG_BIT);
		}
	}
	mem.reg.A = (uint8_t)tempShort;
	RST_BIT(mem.reg.F, H_FLAG_BIT);
	(mem.reg.A != 0) ? RST_BIT(mem.reg.F, Z_FLAG_BIT) : SET_BIT(mem.reg.F, Z_FLAG_BIT);
}

static void gb_cpu_JR_Z_r8(gb_instr_info_t *info)
{
	if (info->current_cycle == 1) {
		op_remaining = (CHK_BIT(mem.reg.F, Z_FLAG_BIT) != 0) ? 3 : 2;
	} else if (info->current_cycle == 3) {
		int8_t r8_val = (int8_t)gb_memory_read(mem.reg.PC + 1);
		mem.reg.PC += r8_val;
	}
	info->current_cycle = CUSTOM_TIMING;
}

static void gb_cpu_ADD_HL_HL(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_addition_HL_register(&mem.reg.HL, &mem.reg.HL, &mem.reg.F);
}

static void gb_cpu_LOAD_A_HLI(gb_instr_info_t *info)
{
	(void)info;
	mem.reg.A = gb_memory_read(mem.reg.HL);
	mem.reg.HL++;
}

static void gb_cpu_DEC_HL(gb_instr_info_t *info)
{
	(void)info;
	mem.reg.HL--;
}

static void gb_cpu_INC_L(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_inc_register(&mem.reg.L, &mem.reg.F);
}

static void gb_cpu_DEC_L(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_dec_register(&mem.reg.L, &mem.reg.F);
}

static void gb_cpu_LOAD_L_d8(gb_instr_info_t *info)
{
	(void)info;
	mem.reg.L = gb_memory_read(mem.reg.PC + 1);
}

static void gb_cpu_CPL(gb_instr_info_t *info)
{
	(void)info;
	mem.reg.A = ~(mem.reg.A);
	SET_BIT(mem.reg.F, N_FLAG_BIT);
	SET_BIT(mem.reg.F, H_FLAG_BIT);
}

/*********************0x3X*/
static void gb_cpu_JR_NC_r8(gb_instr_info_t *info)
{

	if (info->current_cycle == 1) {
		op_remaining = (CHK_BIT(mem.reg.F, C_FLAG_BIT) != 0) ? 2 : 3;
	} else if (info->current_cycle == 3) {
		int8_t r8_val = (int8_t)gb_memory_read(mem.reg.PC + 1);
		mem.reg.PC += r8_val;
	}
	info->current_cycle = CUSTOM_TIMING;
}

static void gb_cpu_LOAD_SP_d16(gb_instr_info_t *info)
{
	(void)info;
	mem.reg.SP = CAT_BYTES(gb_memory_read(mem.reg.PC + 1), gb_memory_read(mem.reg.PC + 2));
}

static void gb_cpu_LOAD_HLD_A(gb_instr_info_t *info)
{
	(void)info;
	gb_memory_write(mem.reg.HL, mem.reg.A);
	mem.reg.HL--;
}

static void gb_cpu_INC_SP(gb_instr_info_t *info)
{
	(void)info;
	mem.reg.SP++;
}

static void gb_cpu_INC_HL_ADDR(gb_instr_info_t *info)
{
	static uint8_t temp_res = 0;
	if (info->current_cycle == 1) {
		temp_res = gb_memory_read(mem.reg.HL);
		((temp_res & 0x0F) == 0x0F) ? SET_BIT(mem.reg.F, H_FLAG_BIT)
					    : RST_BIT(mem.reg.F, H_FLAG_BIT);
		RST_BIT(mem.reg.F, N_FLAG_BIT);

	} else if (info->current_cycle == 2) {
		gb_memory_write(mem.reg.HL, temp_res + 1);
		temp_res = gb_memory_read(mem.reg.HL);
		(temp_res != 0) ? RST_BIT(mem.reg.F, Z_FLAG_BIT) : SET_BIT(mem.reg.F, Z_FLAG_BIT);
	}
	info->current_cycle = CUSTOM_TIMING;
}

static void gb_cpu_DEC_HL_ADDR(gb_instr_info_t *info)
{
	static uint8_t temp_res = 0;
	if (info->current_cycle == 1) {
		temp_res = gb_memory_read(mem.reg.HL);
		((temp_res & 0x0F) != 0) ? RST_BIT(mem.reg.F, H_FLAG_BIT)
					 : SET_BIT(mem.reg.F, H_FLAG_BIT);
		SET_BIT(mem.reg.F, N_FLAG_BIT);

	} else if (info->current_cycle == 2) {
		gb_memory_write(mem.reg.HL, temp_res - 1);
		temp_res = gb_memory_read(mem.reg.HL);
		(temp_res != 0) ? RST_BIT(mem.reg.F, Z_FLAG_BIT) : SET_BIT(mem.reg.F, Z_FLAG_BIT);
	}
	info->current_cycle = CUSTOM_TIMING;
}

static void gb_cpu_LOAD_HL_d8(gb_instr_info_t *info)
{
	(void)info;
	gb_memory_write(mem.reg.HL, gb_memory_read(mem.reg.PC + 1));
}

static void gb_cpu_SCF(gb_instr_info_t *info)
{
	(void)info;
	RST_BIT(mem.reg.F, N_FLAG_BIT);
	RST_BIT(mem.reg.F, H_FLAG_BIT);
	SET_BIT(mem.reg.F, C_FLAG_BIT);
}

static void gb_cpu_JR_C_r8(gb_instr_info_t *info)
{
	if (info->current_cycle == 1) {
		op_remaining = (CHK_BIT(mem.reg.F, C_FLAG_BIT) != 0) ? 3 : 2;
	} else if (info->current_cycle == 3) {
		int8_t r8_val = (int8_t)gb_memory_read(mem.reg.PC + 1);
		mem.reg.PC += r8_val;
	}
	info->current_cycle = CUSTOM_TIMING;
}

static void gb_cpu_ADD_HL_SP(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_addition_HL_register(&mem.reg.HL, &mem.reg.SP, &mem.reg.F);
}

static void gb_cpu_LOAD_A_HLD(gb_instr_info_t *info)
{
	(void)info;
	mem.reg.A = gb_memory_read(mem.reg.HL);
	mem.reg.HL--;
}

static void gb_cpu_DEC_SP(gb_instr_info_t *info)
{
	(void)info;
	mem.reg.SP--;
}

static void gb_cpu_INC_A(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_inc_register(&mem.reg.A, &mem.reg.F);
}

static void gb_cpu_DEC_A(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_dec_register(&mem.reg.A, &mem.reg.F);
}

static void gb_cpu_LOAD_A_d8(gb_instr_info_t *info)
{
	(void)info;
	mem.reg.A = gb_memory_read(mem.reg.PC + 1);
}

static void gb_cpu_CCF(gb_instr_info_t *info)
{
	(void)info;
	RST_BIT(mem.reg.F, N_FLAG_BIT);
	RST_BIT(mem.reg.F, H_FLAG_BIT);
	(CHK_BIT(mem.reg.F, C_FLAG_BIT)) ? RST_BIT(mem.reg.F, C_FLAG_BIT)
					 : SET_BIT(mem.reg.F, C_FLAG_BIT);
}

/*********************0x4X*/
static void gb_cpu_LOAD_B_B(gb_instr_info_t *info)
{
	(void)info;
	mem.reg.B = mem.reg.B;
}

static void gb_cpu_LOAD_B_C(gb_instr_info_t *info)
{
	(void)info;
	mem.reg.B = mem.reg.C;
}

static void gb_cpu_LOAD_B_D(gb_instr_info_t *info)
{
	(void)info;
	mem.reg.B = mem.reg.D;
}

static void gb_cpu_LOAD_B_E(gb_instr_info_t *info)
{
	(void)info;
	mem.reg.B = mem.reg.E;
}

static void gb_cpu_LOAD_B_H(gb_instr_info_t *info)
{
	(void)info;
	mem.reg.B = mem.reg.H;
}

static void gb_cpu_LOAD_B_L(gb_instr_info_t *info)
{
	(void)info;
	mem.reg.B = mem.reg.L;
}

static void gb_cpu_LOAD_B_HL(gb_instr_info_t *info)
{
	(void)info;
	mem.reg.B = gb_memory_read(mem.reg.HL);
}

static void gb_cpu_LOAD_B_A(gb_instr_info_t *info)
{
	(void)info;
	mem.reg.B = mem.reg.A;
}

static void gb_cpu_LOAD_C_B(gb_instr_info_t *info)
{
	(void)info;
	mem.reg.C = mem.reg.B;
}

static void gb_cpu_LOAD_C_C(gb_instr_info_t *info)
{
	(void)info;
	mem.reg.C = mem.reg.C;
}

static void gb_cpu_LOAD_C_D(gb_instr_info_t *info)
{
	(void)info;
	mem.reg.C = mem.reg.D;
}

static void gb_cpu_LOAD_C_E(gb_instr_info_t *info)
{
	(void)info;
	mem.reg.C = mem.reg.E;
}

static void gb_cpu_LOAD_C_H(gb_instr_info_t *info)
{
	(void)info;
	mem.reg.C = mem.reg.H;
}

static void gb_cpu_LOAD_C_L(gb_instr_info_t *info)
{
	(void)info;
	mem.reg.C = mem.reg.L;
}

static void gb_cpu_LOAD_C_HL(gb_instr_info_t *info)
{
	(void)info;
	mem.reg.C = gb_memory_read(mem.reg.HL);
}

static void gb_cpu_LOAD_C_A(gb_instr_info_t *info)
{
	(void)info;
	mem.reg.C = mem.reg.A;
}

/*********************0x5X*/
static void gb_cpu_LOAD_D_B(gb_instr_info_t *info)
{
	(void)info;
	mem.reg.D = mem.reg.B;
}

static void gb_cpu_LOAD_D_C(gb_instr_info_t *info)
{
	(void)info;
	mem.reg.D = mem.reg.C;
}

static void gb_cpu_LOAD_D_D(gb_instr_info_t *info)
{
	(void)info;
	mem.reg.D = mem.reg.D;
}

static void gb_cpu_LOAD_D_E(gb_instr_info_t *info)
{
	(void)info;
	mem.reg.D = mem.reg.E;
}

static void gb_cpu_LOAD_D_H(gb_instr_info_t *info)
{
	(void)info;
	mem.reg.D = mem.reg.H;
}

static void gb_cpu_LOAD_D_L(gb_instr_info_t *info)
{
	(void)info;
	mem.reg.D = mem.reg.L;
}

static void gb_cpu_LOAD_D_HL(gb_instr_info_t *info)
{
	(void)info;
	mem.reg.D = gb_memory_read(mem.reg.HL);
}

static void gb_cpu_LOAD_D_A(gb_instr_info_t *info)
{
	(void)info;
	mem.reg.D = mem.reg.A;
}

static void gb_cpu_LOAD_E_B(gb_instr_info_t *info)
{
	(void)info;
	mem.reg.E = mem.reg.B;
}

static void gb_cpu_LOAD_E_C(gb_instr_info_t *info)
{
	(void)info;
	mem.reg.E = mem.reg.C;
}

static void gb_cpu_LOAD_E_D(gb_instr_info_t *info)
{
	(void)info;
	mem.reg.E = mem.reg.D;
}

static void gb_cpu_LOAD_E_E(gb_instr_info_t *info)
{
	(void)info;
	mem.reg.E = mem.reg.E;
}

static void gb_cpu_LOAD_E_H(gb_instr_info_t *info)
{
	(void)info;
	mem.reg.E = mem.reg.H;
}

static void gb_cpu_LOAD_E_L(gb_instr_info_t *info)
{
	(void)info;
	mem.reg.E = mem.reg.L;
}

static void gb_cpu_LOAD_E_HL(gb_instr_info_t *info)
{
	(void)info;
	mem.reg.E = gb_memory_read(mem.reg.HL);
}

static void gb_cpu_LOAD_E_A(gb_instr_info_t *info)
{
	(void)info;
	mem.reg.E = mem.reg.A;
}

/*********************0x6X*/
static void gb_cpu_LOAD_H_B(gb_instr_info_t *info)
{
	(void)info;
	mem.reg.H = mem.reg.B;
}

static void gb_cpu_LOAD_H_C(gb_instr_info_t *info)
{
	(void)info;
	mem.reg.H = mem.reg.C;
}

static void gb_cpu_LOAD_H_D(gb_instr_info_t *info)
{
	(void)info;
	mem.reg.H = mem.reg.D;
}

static void gb_cpu_LOAD_H_E(gb_instr_info_t *info)
{
	(void)info;
	mem.reg.H = mem.reg.E;
}

static void gb_cpu_LOAD_H_H(gb_instr_info_t *info)
{
	(void)info;
	mem.reg.H = mem.reg.H;
}

static void gb_cpu_LOAD_H_L(gb_instr_info_t *info)
{
	(void)info;
	mem.reg.H = mem.reg.L;
}

static void gb_cpu_LOAD_H_HL(gb_instr_info_t *info)
{
	(void)info;
	mem.reg.H = gb_memory_read(mem.reg.HL);
}

static void gb_cpu_LOAD_H_A(gb_instr_info_t *info)
{
	(void)info;
	mem.reg.H = mem.reg.A;
}

static void gb_cpu_LOAD_L_B(gb_instr_info_t *info)
{
	(void)info;
	mem.reg.L = mem.reg.B;
}

static void gb_cpu_LOAD_L_C(gb_instr_info_t *info)
{
	(void)info;
	mem.reg.L = mem.reg.C;
}

static void gb_cpu_LOAD_L_D(gb_instr_info_t *info)
{
	(void)info;
	mem.reg.L = mem.reg.D;
}

static void gb_cpu_LOAD_L_E(gb_instr_info_t *info)
{
	(void)info;
	mem.reg.L = mem.reg.E;
}

static void gb_cpu_LOAD_L_H(gb_instr_info_t *info)
{
	(void)info;
	mem.reg.L = mem.reg.H;
}

static void gb_cpu_LOAD_L_L(gb_instr_info_t *info)
{
	(void)info;
	mem.reg.L = mem.reg.L;
}

static void gb_cpu_LOAD_L_HL(gb_instr_info_t *info)
{
	(void)info;
	mem.reg.L = gb_memory_read(mem.reg.HL);
}

static void gb_cpu_LOAD_L_A(gb_instr_info_t *info)
{
	(void)info;
	mem.reg.L = mem.reg.A;
}

/*********************0x7X*/
static void gb_cpu_LOAD_HL_B(gb_instr_info_t *info)
{
	(void)info;
	gb_memory_write(mem.reg.HL, mem.reg.B);
}

static void gb_cpu_LOAD_HL_C(gb_instr_info_t *info)
{
	(void)info;
	gb_memory_write(mem.reg.HL, mem.reg.C);
}

static void gb_cpu_LOAD_HL_D(gb_instr_info_t *info)
{
	(void)info;
	gb_memory_write(mem.reg.HL, mem.reg.D);
}

static void gb_cpu_LOAD_HL_E(gb_instr_info_t *info)
{
	(void)info;
	gb_memory_write(mem.reg.HL, mem.reg.E);
}

static void gb_cpu_LOAD_HL_H(gb_instr_info_t *info)
{
	(void)info;
	gb_memory_write(mem.reg.HL, mem.reg.H);
}

static void gb_cpu_LOAD_HL_L(gb_instr_info_t *info)
{
	(void)info;
	gb_memory_write(mem.reg.HL, mem.reg.L);
}

static void gb_cpu_HALT(gb_instr_info_t *info)
{
	(void)info;
	halted = 1;
}

static void gb_cpu_LOAD_HL_A(gb_instr_info_t *info)
{
	(void)info;
	gb_memory_write(mem.reg.HL, mem.reg.A);
}

static void gb_cpu_LOAD_A_B(gb_instr_info_t *info)
{
	(void)info;
	mem.reg.A = mem.reg.B;
}

static void gb_cpu_LOAD_A_C(gb_instr_info_t *info)
{
	(void)info;
	mem.reg.A = mem.reg.C;
}

static void gb_cpu_LOAD_A_D(gb_instr_info_t *info)
{
	(void)info;
	mem.reg.A = mem.reg.D;
}

static void gb_cpu_LOAD_A_E(gb_instr_info_t *info)
{
	(void)info;
	mem.reg.A = mem.reg.E;
}

static void gb_cpu_LOAD_A_H(gb_instr_info_t *info)
{
	(void)info;
	mem.reg.A = mem.reg.H;
}

static void gb_cpu_LOAD_A_L(gb_instr_info_t *info)
{
	(void)info;
	mem.reg.A = mem.reg.L;
}

static void gb_cpu_LOAD_A_HL(gb_instr_info_t *info)
{
	(void)info;
	mem.reg.A = gb_memory_read(mem.reg.HL);
}

static void gb_cpu_LOAD_A_A(gb_instr_info_t *info)
{
	(void)info;
	mem.reg.A = mem.reg.A;
}

/*********************0x8X*/
static void gb_cpu_ADD_A_B(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_addition_A_register(&mem.reg.A, &mem.reg.F, &mem.reg.B);
}

static void gb_cpu_ADD_A_C(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_addition_A_register(&mem.reg.A, &mem.reg.F, &mem.reg.C);
}

static void gb_cpu_ADD_A_D(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_addition_A_register(&mem.reg.A, &mem.reg.F, &mem.reg.D);
}

static void gb_cpu_ADD_A_E(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_addition_A_register(&mem.reg.A, &mem.reg.F, &mem.reg.E);
}

static void gb_cpu_ADD_A_H(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_addition_A_register(&mem.reg.A, &mem.reg.F, &mem.reg.H);
}

static void gb_cpu_ADD_A_L(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_addition_A_register(&mem.reg.A, &mem.reg.F, &mem.reg.L);
}

static void gb_cpu_ADD_A_HL(gb_instr_info_t *info)
{
	(void)info;
	uint8_t temp_res = gb_memory_read(mem.reg.HL);
	gb_cpu_addition_A_register(&mem.reg.A, &mem.reg.F, &temp_res);
}

static void gb_cpu_ADD_A_A(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_addition_A_register(&mem.reg.A, &mem.reg.F, &mem.reg.A);
}

static void gb_cpu_ADC_A_B(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_addition_carry_A_register(&mem.reg.A, &mem.reg.F, &mem.reg.B);
}

static void gb_cpu_ADC_A_C(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_addition_carry_A_register(&mem.reg.A, &mem.reg.F, &mem.reg.C);
}

static void gb_cpu_ADC_A_D(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_addition_carry_A_register(&mem.reg.A, &mem.reg.F, &mem.reg.D);
}

static void gb_cpu_ADC_A_E(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_addition_carry_A_register(&mem.reg.A, &mem.reg.F, &mem.reg.E);
}

static void gb_cpu_ADC_A_H(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_addition_carry_A_register(&mem.reg.A, &mem.reg.F, &mem.reg.H);
}

static void gb_cpu_ADC_A_L(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_addition_carry_A_register(&mem.reg.A, &mem.reg.F, &mem.reg.L);
}

static void gb_cpu_ADC_A_HL(gb_instr_info_t *info)
{
	(void)info;
	uint8_t temp_res = gb_memory_read(mem.reg.HL);
	gb_cpu_addition_carry_A_register(&mem.reg.A, &mem.reg.F, &temp_res);
}

static void gb_cpu_ADC_A_A(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_addition_carry_A_register(&mem.reg.A, &mem.reg.F, &mem.reg.A);
}

/*********************0x9X*/
static void gb_cpu_SUB_B(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_subtraction_A_register(&mem.reg.A, &mem.reg.F, &mem.reg.B);
}

static void gb_cpu_SUB_C(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_subtraction_A_register(&mem.reg.A, &mem.reg.F, &mem.reg.C);
}

static void gb_cpu_SUB_D(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_subtraction_A_register(&mem.reg.A, &mem.reg.F, &mem.reg.D);
}

static void gb_cpu_SUB_E(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_subtraction_A_register(&mem.reg.A, &mem.reg.F, &mem.reg.E);
}

static void gb_cpu_SUB_H(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_subtraction_A_register(&mem.reg.A, &mem.reg.F, &mem.reg.H);
}

static void gb_cpu_SUB_L(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_subtraction_A_register(&mem.reg.A, &mem.reg.F, &mem.reg.L);
}

static void gb_cpu_SUB_HL(gb_instr_info_t *info)
{
	(void)info;
	uint8_t temp_res = gb_memory_read(mem.reg.HL);
	gb_cpu_subtraction_A_register(&mem.reg.A, &mem.reg.F, &temp_res);
}

static void gb_cpu_SUB_A(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_subtraction_A_register(&mem.reg.A, &mem.reg.F, &mem.reg.A);
}

static void gb_cpu_SBC_A_B(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_subtraction_carry_A_register(&mem.reg.A, &mem.reg.F, &mem.reg.B);
}

static void gb_cpu_SBC_A_C(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_subtraction_carry_A_register(&mem.reg.A, &mem.reg.F, &mem.reg.C);
}

static void gb_cpu_SBC_A_D(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_subtraction_carry_A_register(&mem.reg.A, &mem.reg.F, &mem.reg.D);
}

static void gb_cpu_SBC_A_E(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_subtraction_carry_A_register(&mem.reg.A, &mem.reg.F, &mem.reg.E);
}

static void gb_cpu_SBC_A_H(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_subtraction_carry_A_register(&mem.reg.A, &mem.reg.F, &mem.reg.H);
}

static void gb_cpu_SBC_A_L(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_subtraction_carry_A_register(&mem.reg.A, &mem.reg.F, &mem.reg.L);
}

static void gb_cpu_SBC_A_HL(gb_instr_info_t *info)
{
	(void)info;
	uint8_t temp_res = gb_memory_read(mem.reg.HL);
	gb_cpu_subtraction_carry_A_register(&mem.reg.A, &mem.reg.F, &temp_res);
}

static void gb_cpu_SBC_A_A(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_subtraction_carry_A_register(&mem.reg.A, &mem.reg.F, &mem.reg.A);
}

/*********************0xAX*/
static void gb_cpu_AND_B(gb_instr_info_t *info)
{
	(void)info;
	mem.reg.A &= mem.reg.B;
	mem.reg.F = (mem.reg.A == 0) ? 0xA0 : 0x20;
}

static void gb_cpu_AND_C(gb_instr_info_t *info)
{
	(void)info;
	mem.reg.A &= mem.reg.C;
	mem.reg.F = (mem.reg.A == 0) ? 0xA0 : 0x20;
}

static void gb_cpu_AND_D(gb_instr_info_t *info)
{
	(void)info;
	mem.reg.A &= mem.reg.D;
	mem.reg.F = (mem.reg.A == 0) ? 0xA0 : 0x20;
}

static void gb_cpu_AND_E(gb_instr_info_t *info)
{
	(void)info;
	mem.reg.A &= mem.reg.E;
	mem.reg.F = (mem.reg.A == 0) ? 0xA0 : 0x20;
}

static void gb_cpu_AND_H(gb_instr_info_t *info)
{
	(void)info;
	mem.reg.A &= mem.reg.H;
	mem.reg.F = (mem.reg.A == 0) ? 0xA0 : 0x20;
}

static void gb_cpu_AND_L(gb_instr_info_t *info)
{
	(void)info;
	mem.reg.A &= mem.reg.L;
	mem.reg.F = (mem.reg.A == 0) ? 0xA0 : 0x20;
}

static void gb_cpu_AND_HL(gb_instr_info_t *info)
{
	(void)info;
	mem.reg.A &= gb_memory_read(mem.reg.HL);
	mem.reg.F = (mem.reg.A == 0) ? 0xA0 : 0x20;
}

static void gb_cpu_AND_A(gb_instr_info_t *info)
{
	(void)info;
	mem.reg.A &= mem.reg.A;
	mem.reg.F = (mem.reg.A == 0) ? 0xA0 : 0x20;
}

static void gb_cpu_XOR_B(gb_instr_info_t *info)
{
	(void)info;
	mem.reg.A ^= mem.reg.B;
	mem.reg.F = (mem.reg.A == 0) ? 0x80 : 0x00;
}

static void gb_cpu_XOR_C(gb_instr_info_t *info)
{
	(void)info;
	mem.reg.A ^= mem.reg.C;
	mem.reg.F = (mem.reg.A == 0) ? 0x80 : 0x00;
}

static void gb_cpu_XOR_D(gb_instr_info_t *info)
{
	(void)info;
	mem.reg.A ^= mem.reg.D;
	mem.reg.F = (mem.reg.A == 0) ? 0x80 : 0x00;
}

static void gb_cpu_XOR_E(gb_instr_info_t *info)
{
	(void)info;
	mem.reg.A ^= mem.reg.E;
	mem.reg.F = (mem.reg.A == 0) ? 0x80 : 0x00;
}

static void gb_cpu_XOR_H(gb_instr_info_t *info)
{
	(void)info;
	mem.reg.A ^= mem.reg.H;
	mem.reg.F = (mem.reg.A == 0) ? 0x80 : 0x00;
}

static void gb_cpu_XOR_L(gb_instr_info_t *info)
{
	(void)info;
	mem.reg.A ^= mem.reg.L;
	mem.reg.F = (mem.reg.A == 0) ? 0x80 : 0x00;
}

static void gb_cpu_XOR_HL(gb_instr_info_t *info)
{
	(void)info;
	mem.reg.A ^= gb_memory_read(mem.reg.HL);
	mem.reg.F = (mem.reg.A == 0) ? 0x80 : 0x00;
}

static void gb_cpu_XOR_A(gb_instr_info_t *info)
{
	(void)info;
	mem.reg.A ^= mem.reg.A;
	mem.reg.F = (mem.reg.A == 0) ? 0x80 : 0x00;
}

/*********************0xBX*/
static void gb_cpu_OR_B(gb_instr_info_t *info)
{
	(void)info;
	mem.reg.A |= mem.reg.B;
	mem.reg.F = (mem.reg.A == 0) ? 0x80 : 0x00;
}

static void gb_cpu_OR_C(gb_instr_info_t *info)
{
	(void)info;
	mem.reg.A |= mem.reg.C;
	mem.reg.F = (mem.reg.A == 0) ? 0x80 : 0x00;
}

static void gb_cpu_OR_D(gb_instr_info_t *info)
{
	(void)info;
	mem.reg.A |= mem.reg.D;
	mem.reg.F = (mem.reg.A == 0) ? 0x80 : 0x00;
}

static void gb_cpu_OR_E(gb_instr_info_t *info)
{
	(void)info;
	mem.reg.A |= mem.reg.E;
	mem.reg.F = (mem.reg.A == 0) ? 0x80 : 0x00;
}

static void gb_cpu_OR_H(gb_instr_info_t *info)
{
	(void)info;
	mem.reg.A |= mem.reg.H;
	mem.reg.F = (mem.reg.A == 0) ? 0x80 : 0x00;
}

static void gb_cpu_OR_L(gb_instr_info_t *info)
{
	(void)info;
	mem.reg.A |= mem.reg.L;
	mem.reg.F = (mem.reg.A == 0) ? 0x80 : 0x00;
}

static void gb_cpu_OR_HL(gb_instr_info_t *info)
{
	(void)info;
	mem.reg.A |= gb_memory_read(mem.reg.HL);
	mem.reg.F = (mem.reg.A == 0) ? 0x80 : 0x00;
}

static void gb_cpu_OR_A(gb_instr_info_t *info)
{
	(void)info;
	mem.reg.A |= mem.reg.A;
	mem.reg.F = (mem.reg.A == 0) ? 0x80 : 0x00;
}

static void gb_cpu_CP_B(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_compare_A_register(&mem.reg.A, &mem.reg.F, &mem.reg.B);
}

static void gb_cpu_CP_C(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_compare_A_register(&mem.reg.A, &mem.reg.F, &mem.reg.C);
}

static void gb_cpu_CP_D(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_compare_A_register(&mem.reg.A, &mem.reg.F, &mem.reg.D);
}

static void gb_cpu_CP_E(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_compare_A_register(&mem.reg.A, &mem.reg.F, &mem.reg.E);
}

static void gb_cpu_CP_H(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_compare_A_register(&mem.reg.A, &mem.reg.F, &mem.reg.H);
}

static void gb_cpu_CP_L(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_compare_A_register(&mem.reg.A, &mem.reg.F, &mem.reg.L);
}

static void gb_cpu_CP_HL(gb_instr_info_t *info)
{
	(void)info;
	uint8_t temp_res = gb_memory_read(mem.reg.HL);
	gb_cpu_compare_A_register(&mem.reg.A, &mem.reg.F, &temp_res);
}

static void gb_cpu_CP_A(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_compare_A_register(&mem.reg.A, &mem.reg.F, &mem.reg.A);
}

/*********************0xCX*/
static void gb_cpu_RET_NZ(gb_instr_info_t *info)
{
	if (info->current_cycle == 1) {
		op_remaining = (CHK_BIT(mem.reg.F, Z_FLAG_BIT)) ? 2 : 5;
	} else if (info->current_cycle == 5) {
		dont_update_pc = true;
		gb_cpu_return_from_stack(&mem.reg.SP, &mem.reg.PC);
	}
	info->current_cycle = CUSTOM_TIMING;
}

static void gb_cpu_POP_BC(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_pop_from_stack(&mem.reg.SP, &mem.reg.BC);
}

static void gb_cpu_JP_NZ_a16(gb_instr_info_t *info)
{
	if (info->current_cycle == 1) {
		op_remaining = (CHK_BIT(mem.reg.F, Z_FLAG_BIT) != 0) ? 3 : 4;
	} else if (info->current_cycle == 4) {
		dont_update_pc = true;
		mem.reg.PC =
			CAT_BYTES(gb_memory_read(mem.reg.PC + 1), gb_memory_read(mem.reg.PC + 2));
	}
	info->current_cycle = CUSTOM_TIMING;
}

static void gb_cpu_JP_a16(gb_instr_info_t *info)
{
	(void)info;
	dont_update_pc = true;
	mem.reg.PC = CAT_BYTES(gb_memory_read(mem.reg.PC + 1), gb_memory_read(mem.reg.PC + 2));
}

static void gb_cpu_CALL_NZ_a16(gb_instr_info_t *info)
{
	if (info->current_cycle == 1) {
		op_remaining = (CHK_BIT(mem.reg.F, Z_FLAG_BIT) != 0) ? 3 : 6;
	} else if (info->current_cycle == 6) {
		dont_update_pc = true;
		gb_cpu_push_to_stack(&mem.reg.SP, mem.reg.PC + info->bytes);
		mem.reg.PC =
			CAT_BYTES(gb_memory_read(mem.reg.PC + 1), gb_memory_read(mem.reg.PC + 2));
	}
	info->current_cycle = CUSTOM_TIMING;
}

static void gb_cpu_PUSH_BC(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_push_to_stack(&mem.reg.SP, mem.reg.BC);
}

static void gb_cpu_ADD_A_d8(gb_instr_info_t *info)
{
	(void)info;
	uint8_t temp_res = gb_memory_read(mem.reg.PC + 1);
	gb_cpu_addition_A_register(&mem.reg.A, &mem.reg.F, &temp_res);
}

static void gb_cpu_RST_00H(gb_instr_info_t *info)
{
	(void)info;
	dont_update_pc = true;
	gb_cpu_push_to_stack(&mem.reg.SP, mem.reg.PC + info->bytes);
	mem.reg.PC = 0x0000;
}

static void gb_cpu_RET_Z(gb_instr_info_t *info)
{
	if (info->current_cycle == 1) {
		op_remaining = (CHK_BIT(mem.reg.F, Z_FLAG_BIT)) ? 5 : 2;
	} else if (info->current_cycle == 5) {
		dont_update_pc = true;
		gb_cpu_return_from_stack(&mem.reg.SP, &mem.reg.PC);
	}
	info->current_cycle = CUSTOM_TIMING;
}

static void gb_cpu_RET(gb_instr_info_t *info)
{
	(void)info;
	dont_update_pc = true;
	gb_cpu_return_from_stack(&mem.reg.SP, &mem.reg.PC);
}

static void gb_cpu_JP_Z_a16(gb_instr_info_t *info)
{
	if (info->current_cycle == 1) {
		op_remaining = (CHK_BIT(mem.reg.F, Z_FLAG_BIT) != 0) ? 4 : 3;
	} else if (info->current_cycle == 4) {
		dont_update_pc = true;
		mem.reg.PC =
			CAT_BYTES(gb_memory_read(mem.reg.PC + 1), gb_memory_read(mem.reg.PC + 2));
	}
	info->current_cycle = CUSTOM_TIMING;
}

static void gb_cpu_PREFIX(gb_instr_info_t *info)
{
	((void (*)(gb_instr_info_t *))prefix_instructions[gb_memory_read(mem.reg.PC + 1)].instr)(
		info);
}

static void gb_cpu_CALL_Z_a16(gb_instr_info_t *info)
{
	if (info->current_cycle == 1) {
		op_remaining = (CHK_BIT(mem.reg.F, Z_FLAG_BIT) != 0) ? 6 : 3;
	} else if (info->current_cycle == 6) {
		dont_update_pc = true;
		gb_cpu_push_to_stack(&mem.reg.SP, mem.reg.PC + info->bytes);
		mem.reg.PC =
			CAT_BYTES(gb_memory_read(mem.reg.PC + 1), gb_memory_read(mem.reg.PC + 2));
	}
	info->current_cycle = CUSTOM_TIMING;
}

static void gb_cpu_CALL_a16(gb_instr_info_t *info)
{
	(void)info;
	dont_update_pc = true;
	gb_cpu_push_to_stack(&mem.reg.SP, mem.reg.PC + info->bytes);
	mem.reg.PC = CAT_BYTES(gb_memory_read(mem.reg.PC + 1), gb_memory_read(mem.reg.PC + 2));
}

static void gb_cpu_ADC_A_d8(gb_instr_info_t *info)
{
	(void)info;
	uint8_t temp_res = gb_memory_read(mem.reg.PC + 1);
	gb_cpu_addition_carry_A_register(&mem.reg.A, &mem.reg.F, &temp_res);
}

static void gb_cpu_RST_08H(gb_instr_info_t *info)
{
	(void)info;
	dont_update_pc = true;
	gb_cpu_push_to_stack(&mem.reg.SP, mem.reg.PC + info->bytes);
	mem.reg.PC = 0x0008;
}

/*********************0xDX*/
static void gb_cpu_RET_NC(gb_instr_info_t *info)
{

	if (info->current_cycle == 1) {
		op_remaining = (CHK_BIT(mem.reg.F, C_FLAG_BIT)) ? 2 : 5;
	} else if (info->current_cycle == 5) {
		dont_update_pc = true;
		gb_cpu_return_from_stack(&mem.reg.SP, &mem.reg.PC);
	}
	info->current_cycle = CUSTOM_TIMING;
}

static void gb_cpu_POP_DE(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_pop_from_stack(&mem.reg.SP, &mem.reg.DE);
}

static void gb_cpu_JP_NC_a16(gb_instr_info_t *info)
{
	if (info->current_cycle == 1) {
		op_remaining = (CHK_BIT(mem.reg.F, C_FLAG_BIT) != 0) ? 3 : 4;
	} else if (info->current_cycle == 4) {
		dont_update_pc = true;
		mem.reg.PC =
			CAT_BYTES(gb_memory_read(mem.reg.PC + 1), gb_memory_read(mem.reg.PC + 2));
	}
	info->current_cycle = CUSTOM_TIMING;
}

// -----------
static void gb_cpu_CALL_NC_a16(gb_instr_info_t *info)
{
	if (info->current_cycle == 1) {
		op_remaining = (CHK_BIT(mem.reg.F, C_FLAG_BIT) != 0) ? 3 : 6;
	} else if (info->current_cycle == 6) {
		dont_update_pc = true;
		gb_cpu_push_to_stack(&mem.reg.SP, mem.reg.PC + info->bytes);
		mem.reg.PC =
			CAT_BYTES(gb_memory_read(mem.reg.PC + 1), gb_memory_read(mem.reg.PC + 2));
	}
	info->current_cycle = CUSTOM_TIMING;
}

static void gb_cpu_PUSH_DE(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_push_to_stack(&mem.reg.SP, mem.reg.DE);
}

static void gb_cpu_SUB_d8(gb_instr_info_t *info)
{
	(void)info;
	uint8_t temp_res = gb_memory_read(mem.reg.PC + 1);
	gb_cpu_subtraction_A_register(&mem.reg.A, &mem.reg.F, &temp_res);
}

static void gb_cpu_RST_10H(gb_instr_info_t *info)
{
	(void)info;
	dont_update_pc = true;
	gb_cpu_push_to_stack(&mem.reg.SP, mem.reg.PC + info->bytes);
	mem.reg.PC = 0x0010;
}

static void gb_cpu_RET_C(gb_instr_info_t *info)
{
	if (info->current_cycle == 1) {
		op_remaining = (CHK_BIT(mem.reg.F, C_FLAG_BIT)) ? 5 : 2;
	} else if (info->current_cycle == 5) {
		dont_update_pc = true;
		gb_cpu_return_from_stack(&mem.reg.SP, &mem.reg.PC);
	}
	info->current_cycle = CUSTOM_TIMING;
}

static void gb_cpu_RETI(gb_instr_info_t *info)
{
	(void)info;
	interrupt_master_enable = 1;
	dont_update_pc = true;
	gb_cpu_return_from_stack(&mem.reg.SP, &mem.reg.PC);
}

static void gb_cpu_JP_C_a16(gb_instr_info_t *info)
{
	if (info->current_cycle == 1) {
		op_remaining = (CHK_BIT(mem.reg.F, C_FLAG_BIT) != 0) ? 4 : 3;
	} else if (info->current_cycle == 4) {
		dont_update_pc = true;
		mem.reg.PC =
			CAT_BYTES(gb_memory_read(mem.reg.PC + 1), gb_memory_read(mem.reg.PC + 2));
	}
	info->current_cycle = CUSTOM_TIMING;
}

// -----------
static void gb_cpu_CALL_C_a16(gb_instr_info_t *info)
{
	if (info->current_cycle == 1) {
		op_remaining = (CHK_BIT(mem.reg.F, C_FLAG_BIT) != 0) ? 6 : 3;
	} else if (info->current_cycle == 6) {
		dont_update_pc = true;
		gb_cpu_push_to_stack(&mem.reg.SP, mem.reg.PC + info->bytes);
		mem.reg.PC =
			CAT_BYTES(gb_memory_read(mem.reg.PC + 1), gb_memory_read(mem.reg.PC + 2));
	}
	info->current_cycle = CUSTOM_TIMING;
}

// -----------
static void gb_cpu_SBC_A_d8(gb_instr_info_t *info)
{
	(void)info;
	uint8_t temp_res = gb_memory_read(mem.reg.PC + 1);
	gb_cpu_subtraction_carry_A_register(&mem.reg.A, &mem.reg.F, &temp_res);
}

static void gb_cpu_RST_18H(gb_instr_info_t *info)
{
	(void)info;
	dont_update_pc = true;
	gb_cpu_push_to_stack(&mem.reg.SP, mem.reg.PC + info->bytes);
	mem.reg.PC = 0x0018;
}

/*********************0xEX*/
static void gb_cpu_LOAD_a8_A(gb_instr_info_t *info)
{
	(void)info;
	gb_memory_write(0xFF00 + gb_memory_read(mem.reg.PC + 1), mem.reg.A);
}

static void gb_cpu_POP_HL(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_pop_from_stack(&mem.reg.SP, &mem.reg.HL);
}

static void gb_cpu_LOAD_fC_A(gb_instr_info_t *info)
{
	(void)info;
	gb_memory_write(0xFF00 + mem.reg.C, mem.reg.A);
}

// -----------
// -----------
static void gb_cpu_PUSH_HL(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_push_to_stack(&mem.reg.SP, mem.reg.HL);
}

static void gb_cpu_AND_d8(gb_instr_info_t *info)
{
	(void)info;
	mem.reg.A &= gb_memory_read(mem.reg.PC + 1);
	mem.reg.F = (mem.reg.A == 0) ? 0xA0 : 0x20;
}

static void gb_cpu_RST20H(gb_instr_info_t *info)
{
	(void)info;
	dont_update_pc = true;
	gb_cpu_push_to_stack(&mem.reg.SP, mem.reg.PC + info->bytes);
	mem.reg.PC = 0x0020;
}

static void gb_cpu_ADD_SP_r8(gb_instr_info_t *info)
{
	(void)info;
	uint8_t r8_val = gb_memory_read(mem.reg.PC + 1);
	uint32_t temp_res = mem.reg.SP + r8_val;
	((temp_res & 0xFF) < (mem.reg.SP & 0xFF)) ? SET_BIT(mem.reg.F, C_FLAG_BIT)
						  : RST_BIT(mem.reg.F, C_FLAG_BIT);
	((temp_res & 0xF) < (mem.reg.SP & 0xF)) ? SET_BIT(mem.reg.F, H_FLAG_BIT)
						: RST_BIT(mem.reg.F, H_FLAG_BIT);
	RST_BIT(mem.reg.F, Z_FLAG_BIT);
	RST_BIT(mem.reg.F, N_FLAG_BIT);
	mem.reg.SP += (int8_t)r8_val;
}

static void gb_cpu_JP_HL(gb_instr_info_t *info)
{
	(void)info;
	dont_update_pc = true;
	mem.reg.PC = mem.reg.HL;
}

static void gb_cpu_LOAD_a16_A(gb_instr_info_t *info)
{
	(void)info;
	gb_memory_write(CAT_BYTES(gb_memory_read(mem.reg.PC + 1), gb_memory_read(mem.reg.PC + 2)),
			mem.reg.A);
}

// -----------
// -----------
// -----------
static void gb_cpu_XOR_d8(gb_instr_info_t *info)
{
	(void)info;
	mem.reg.A ^= gb_memory_read(mem.reg.PC + 1);
	mem.reg.F = (mem.reg.A == 0) ? 0x80 : 0x00;
}

static void gb_cpu_RST_28H(gb_instr_info_t *info)
{
	(void)info;
	dont_update_pc = true;
	gb_cpu_push_to_stack(&mem.reg.SP, mem.reg.PC + info->bytes);
	mem.reg.PC = 0x0028;
}

/*********************0xFX*/
static void gb_cpu_LOAD_A_a8(gb_instr_info_t *info)
{
	(void)info;
	mem.reg.A = gb_memory_read(0xFF00 + gb_memory_read(mem.reg.PC + 1));
}

static void gb_cpu_POP_AF(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_pop_from_stack(&mem.reg.SP, &mem.reg.AF);
	mem.reg.AF &= 0xFFF0;
}

static void gb_cpu_LOAD_A_fC(gb_instr_info_t *info)
{
	(void)info;
	mem.reg.A = gb_memory_read(0xFF00 + mem.reg.C);
}

static void gb_cpu_DI(gb_instr_info_t *info)
{
	(void)info;
	interrupt_master_enable = 0;
}

// -----------
static void gb_cpu_PUSH_AF(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_push_to_stack(&mem.reg.SP, mem.reg.AF);
}

static void gb_cpu_OR_d8(gb_instr_info_t *info)
{
	(void)info;
	mem.reg.A |= gb_memory_read(mem.reg.PC + 1);
	mem.reg.F = (mem.reg.A == 0) ? 0x80 : 0x00;
}

static void gb_cpu_RST_30H(gb_instr_info_t *info)
{
	(void)info;
	dont_update_pc = true;
	gb_cpu_push_to_stack(&mem.reg.SP, mem.reg.PC + info->bytes);
	mem.reg.PC = 0x0030;
}

static void gb_cpu_LOAD_HL_SP_r8(gb_instr_info_t *info)
{
	(void)info;
	uint8_t r8_val = gb_memory_read(mem.reg.PC + 1);
	uint32_t temp_res = mem.reg.SP + r8_val;
	((temp_res & 0xFF) < (mem.reg.SP & 0xFF)) ? SET_BIT(mem.reg.F, C_FLAG_BIT)
						  : RST_BIT(mem.reg.F, C_FLAG_BIT);
	((temp_res & 0xF) < (mem.reg.SP & 0xF)) ? SET_BIT(mem.reg.F, H_FLAG_BIT)
						: RST_BIT(mem.reg.F, H_FLAG_BIT);
	RST_BIT(mem.reg.F, Z_FLAG_BIT);
	RST_BIT(mem.reg.F, N_FLAG_BIT);
	mem.reg.HL = mem.reg.SP + (int8_t)r8_val;
}

static void gb_cpu_LOAD_SP_HL(gb_instr_info_t *info)
{
	(void)info;
	mem.reg.SP = mem.reg.HL;
}

static void gb_cpu_LOAD_A_a16(gb_instr_info_t *info)
{
	(void)info;
	mem.reg.A = gb_memory_read(
		CAT_BYTES(gb_memory_read(mem.reg.PC + 1), gb_memory_read(mem.reg.PC + 2)));
}

static void gb_cpu_EI(gb_instr_info_t *info)
{
	(void)info;
	interrupt_master_enable = 1;
}

// -----------
// -----------
static void gb_cpu_CP_d8(gb_instr_info_t *info)
{
	(void)info;
	uint8_t temp_res = gb_memory_read(mem.reg.PC + 1);
	gb_cpu_compare_A_register(&mem.reg.A, &mem.reg.F, &temp_res);
}
static void gb_cpu_RST_38H(gb_instr_info_t *info)
{
	(void)info;
	dont_update_pc = true;
	gb_cpu_push_to_stack(&mem.reg.SP, mem.reg.PC + info->bytes);
	mem.reg.PC = 0x0038;
}

/*Prefix implementation*/

/*********************0x0X*/
static void gb_cpu_RLC_B(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_rotate_left_with_carry(&mem.reg.B, &mem.reg.F);
}

static void gb_cpu_RLC_C(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_rotate_left_with_carry(&mem.reg.C, &mem.reg.F);
}

static void gb_cpu_RLC_D(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_rotate_left_with_carry(&mem.reg.D, &mem.reg.F);
}

static void gb_cpu_RLC_E(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_rotate_left_with_carry(&mem.reg.E, &mem.reg.F);
}

static void gb_cpu_RLC_H(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_rotate_left_with_carry(&mem.reg.H, &mem.reg.F);
}

static void gb_cpu_RLC_L(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_rotate_left_with_carry(&mem.reg.L, &mem.reg.F);
}

static void gb_cpu_RLC_HL(gb_instr_info_t *info)
{
	static uint8_t temp_val = 0;
	if (info->current_cycle == 2) {
		temp_val = gb_memory_read(mem.reg.HL);

	} else if (info->current_cycle == 3) {
		gb_cpu_rotate_left_with_carry(&temp_val, &mem.reg.F);
		gb_memory_write(mem.reg.HL, temp_val);
	}
	info->current_cycle = CUSTOM_TIMING;
}

static void gb_cpu_RLC_A(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_rotate_left_with_carry(&mem.reg.A, &mem.reg.F);
}

static void gb_cpu_RRC_B(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_rotate_right_with_carry(&mem.reg.B, &mem.reg.F);
}

static void gb_cpu_RRC_C(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_rotate_right_with_carry(&mem.reg.C, &mem.reg.F);
}

static void gb_cpu_RRC_D(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_rotate_right_with_carry(&mem.reg.D, &mem.reg.F);
}

static void gb_cpu_RRC_E(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_rotate_right_with_carry(&mem.reg.E, &mem.reg.F);
}

static void gb_cpu_RRC_H(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_rotate_right_with_carry(&mem.reg.H, &mem.reg.F);
}

static void gb_cpu_RRC_L(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_rotate_right_with_carry(&mem.reg.L, &mem.reg.F);
}

static void gb_cpu_RRC_HL(gb_instr_info_t *info)
{
	static uint8_t temp_val = 0;
	if (info->current_cycle == 2) {
		temp_val = gb_memory_read(mem.reg.HL);

	} else if (info->current_cycle == 3) {
		gb_cpu_rotate_right_with_carry(&temp_val, &mem.reg.F);
		gb_memory_write(mem.reg.HL, temp_val);
	}
	info->current_cycle = CUSTOM_TIMING;
}

static void gb_cpu_RRC_A(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_rotate_right_with_carry(&mem.reg.A, &mem.reg.F);
}

/*********************0x1X*/
static void gb_cpu_RL_B(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_rotate_left_through_carry(&mem.reg.B, &mem.reg.F);
}

static void gb_cpu_RL_C(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_rotate_left_through_carry(&mem.reg.C, &mem.reg.F);
}

static void gb_cpu_RL_D(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_rotate_left_through_carry(&mem.reg.D, &mem.reg.F);
}

static void gb_cpu_RL_E(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_rotate_left_through_carry(&mem.reg.E, &mem.reg.F);
}

static void gb_cpu_RL_H(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_rotate_left_through_carry(&mem.reg.H, &mem.reg.F);
}

static void gb_cpu_RL_L(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_rotate_left_through_carry(&mem.reg.L, &mem.reg.F);
}

static void gb_cpu_RL_HL(gb_instr_info_t *info)
{
	static uint8_t temp_val = 0;
	if (info->current_cycle == 2) {
		temp_val = gb_memory_read(mem.reg.HL);

	} else if (info->current_cycle == 3) {
		gb_cpu_rotate_left_through_carry(&temp_val, &mem.reg.F);
		gb_memory_write(mem.reg.HL, temp_val);
	}
	info->current_cycle = CUSTOM_TIMING;
}

static void gb_cpu_RL_A(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_rotate_left_through_carry(&mem.reg.A, &mem.reg.F);
}

static void gb_cpu_RR_B(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_rotate_right_through_carry(&mem.reg.B, &mem.reg.F);
}

static void gb_cpu_RR_C(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_rotate_right_through_carry(&mem.reg.C, &mem.reg.F);
}

static void gb_cpu_RR_D(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_rotate_right_through_carry(&mem.reg.D, &mem.reg.F);
}

static void gb_cpu_RR_E(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_rotate_right_through_carry(&mem.reg.E, &mem.reg.F);
}

static void gb_cpu_RR_H(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_rotate_right_through_carry(&mem.reg.H, &mem.reg.F);
}

static void gb_cpu_RR_L(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_rotate_right_through_carry(&mem.reg.L, &mem.reg.F);
}

static void gb_cpu_RR_HL(gb_instr_info_t *info)
{
	static uint8_t temp_val = 0;
	if (info->current_cycle == 2) {
		temp_val = gb_memory_read(mem.reg.HL);

	} else if (info->current_cycle == 3) {
		gb_cpu_rotate_right_through_carry(&temp_val, &mem.reg.F);
		gb_memory_write(mem.reg.HL, temp_val);
	}
	info->current_cycle = CUSTOM_TIMING;
}

static void gb_cpu_RR_A(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_rotate_right_through_carry(&mem.reg.A, &mem.reg.F);
}

/*********************0x2X*/
static void gb_cpu_SLA_B(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_shift_left_into_carry(&mem.reg.B, &mem.reg.F);
}

static void gb_cpu_SLA_C(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_shift_left_into_carry(&mem.reg.C, &mem.reg.F);
}

static void gb_cpu_SLA_D(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_shift_left_into_carry(&mem.reg.D, &mem.reg.F);
}

static void gb_cpu_SLA_E(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_shift_left_into_carry(&mem.reg.E, &mem.reg.F);
}

static void gb_cpu_SLA_H(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_shift_left_into_carry(&mem.reg.H, &mem.reg.F);
}

static void gb_cpu_SLA_L(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_shift_left_into_carry(&mem.reg.L, &mem.reg.F);
}

static void gb_cpu_SLA_HL(gb_instr_info_t *info)
{
	static uint8_t temp_val = 0;
	if (info->current_cycle == 2) {
		temp_val = gb_memory_read(mem.reg.HL);

	} else if (info->current_cycle == 3) {
		gb_cpu_shift_left_into_carry(&temp_val, &mem.reg.F);
		gb_memory_write(mem.reg.HL, temp_val);
	}
	info->current_cycle = CUSTOM_TIMING;
}

static void gb_cpu_SLA_A(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_shift_left_into_carry(&mem.reg.A, &mem.reg.F);
}

static void gb_cpu_SRA_B(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_shift_right_with_carry(&mem.reg.B, &mem.reg.F);
}

static void gb_cpu_SRA_C(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_shift_right_with_carry(&mem.reg.C, &mem.reg.F);
}

static void gb_cpu_SRA_D(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_shift_right_with_carry(&mem.reg.D, &mem.reg.F);
}

static void gb_cpu_SRA_E(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_shift_right_with_carry(&mem.reg.E, &mem.reg.F);
}

static void gb_cpu_SRA_H(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_shift_right_with_carry(&mem.reg.H, &mem.reg.F);
}

static void gb_cpu_SRA_L(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_shift_right_with_carry(&mem.reg.L, &mem.reg.F);
}

static void gb_cpu_SRA_HL(gb_instr_info_t *info)
{
	static uint8_t temp_val = 0;
	if (info->current_cycle == 2) {
		temp_val = gb_memory_read(mem.reg.HL);

	} else if (info->current_cycle == 3) {
		gb_cpu_shift_right_with_carry(&temp_val, &mem.reg.F);
		gb_memory_write(mem.reg.HL, temp_val);
	}
	info->current_cycle = CUSTOM_TIMING;
}

static void gb_cpu_SRA_A(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_shift_right_with_carry(&mem.reg.A, &mem.reg.F);
}

/*********************0x3X*/
static void gb_cpu_SWAP_B(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_swap_nibbles(&mem.reg.B, &mem.reg.F);
}

static void gb_cpu_SWAP_C(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_swap_nibbles(&mem.reg.C, &mem.reg.F);
}

static void gb_cpu_SWAP_D(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_swap_nibbles(&mem.reg.D, &mem.reg.F);
}

static void gb_cpu_SWAP_E(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_swap_nibbles(&mem.reg.E, &mem.reg.F);
}

static void gb_cpu_SWAP_H(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_swap_nibbles(&mem.reg.H, &mem.reg.F);
}

static void gb_cpu_SWAP_L(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_swap_nibbles(&mem.reg.L, &mem.reg.F);
}

static void gb_cpu_SWAP_HL(gb_instr_info_t *info)
{
	static uint8_t temp_val = 0;
	if (info->current_cycle == 2) {
		temp_val = gb_memory_read(mem.reg.HL);

	} else if (info->current_cycle == 3) {
		gb_cpu_swap_nibbles(&temp_val, &mem.reg.F);
		gb_memory_write(mem.reg.HL, temp_val);
	}
	info->current_cycle = CUSTOM_TIMING;
}

static void gb_cpu_SWAP_A(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_swap_nibbles(&mem.reg.A, &mem.reg.F);
}

static void gb_cpu_SRL_B(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_shift_right_into_carry(&mem.reg.B, &mem.reg.F);
}

static void gb_cpu_SRL_C(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_shift_right_into_carry(&mem.reg.C, &mem.reg.F);
}

static void gb_cpu_SRL_D(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_shift_right_into_carry(&mem.reg.D, &mem.reg.F);
}

static void gb_cpu_SRL_E(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_shift_right_into_carry(&mem.reg.E, &mem.reg.F);
}

static void gb_cpu_SRL_H(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_shift_right_into_carry(&mem.reg.H, &mem.reg.F);
}

static void gb_cpu_SRL_L(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_shift_right_into_carry(&mem.reg.L, &mem.reg.F);
}

static void gb_cpu_SRL_HL(gb_instr_info_t *info)
{
	static uint8_t temp_val = 0;
	if (info->current_cycle == 2) {
		temp_val = gb_memory_read(mem.reg.HL);

	} else if (info->current_cycle == 3) {
		gb_cpu_shift_right_into_carry(&temp_val, &mem.reg.F);
		gb_memory_write(mem.reg.HL, temp_val);
	}
	info->current_cycle = CUSTOM_TIMING;
}

static void gb_cpu_SRL_A(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_shift_right_into_carry(&mem.reg.A, &mem.reg.F);
}

/*********************0x4X*/
static void gb_cpu_BIT_0_B(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_bit_check(&mem.reg.B, 0, &mem.reg.F);
}

static void gb_cpu_BIT_0_C(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_bit_check(&mem.reg.C, 0, &mem.reg.F);
}

static void gb_cpu_BIT_0_D(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_bit_check(&mem.reg.D, 0, &mem.reg.F);
}

static void gb_cpu_BIT_0_E(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_bit_check(&mem.reg.E, 0, &mem.reg.F);
}

static void gb_cpu_BIT_0_H(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_bit_check(&mem.reg.H, 0, &mem.reg.F);
}

static void gb_cpu_BIT_0_L(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_bit_check(&mem.reg.L, 0, &mem.reg.F);
}

static void gb_cpu_BIT_0_HL(gb_instr_info_t *info)
{
	(void)info;
	uint8_t temp_res = gb_memory_read(mem.reg.HL);
	gb_cpu_bit_check(&temp_res, 0, &mem.reg.F);
}

static void gb_cpu_BIT_0_A(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_bit_check(&mem.reg.A, 0, &mem.reg.F);
}

static void gb_cpu_BIT_1_B(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_bit_check(&mem.reg.B, 1, &mem.reg.F);
}

static void gb_cpu_BIT_1_C(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_bit_check(&mem.reg.C, 1, &mem.reg.F);
}

static void gb_cpu_BIT_1_D(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_bit_check(&mem.reg.D, 1, &mem.reg.F);
}

static void gb_cpu_BIT_1_E(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_bit_check(&mem.reg.E, 1, &mem.reg.F);
}

static void gb_cpu_BIT_1_H(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_bit_check(&mem.reg.H, 1, &mem.reg.F);
}

static void gb_cpu_BIT_1_L(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_bit_check(&mem.reg.L, 1, &mem.reg.F);
}

static void gb_cpu_BIT_1_HL(gb_instr_info_t *info)
{
	(void)info;
	uint8_t temp_res = gb_memory_read(mem.reg.HL);
	gb_cpu_bit_check(&temp_res, 1, &mem.reg.F);
}

static void gb_cpu_BIT_1_A(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_bit_check(&mem.reg.A, 1, &mem.reg.F);
}

/*********************0x5X*/
static void gb_cpu_BIT_2_B(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_bit_check(&mem.reg.B, 2, &mem.reg.F);
}

static void gb_cpu_BIT_2_C(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_bit_check(&mem.reg.C, 2, &mem.reg.F);
}

static void gb_cpu_BIT_2_D(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_bit_check(&mem.reg.D, 2, &mem.reg.F);
}

static void gb_cpu_BIT_2_E(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_bit_check(&mem.reg.E, 2, &mem.reg.F);
}

static void gb_cpu_BIT_2_H(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_bit_check(&mem.reg.H, 2, &mem.reg.F);
}

static void gb_cpu_BIT_2_L(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_bit_check(&mem.reg.L, 2, &mem.reg.F);
}

static void gb_cpu_BIT_2_HL(gb_instr_info_t *info)
{
	(void)info;
	uint8_t temp_res = gb_memory_read(mem.reg.HL);
	gb_cpu_bit_check(&temp_res, 2, &mem.reg.F);
}
static void gb_cpu_BIT_2_A(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_bit_check(&mem.reg.A, 2, &mem.reg.F);
}

static void gb_cpu_BIT_3_B(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_bit_check(&mem.reg.B, 3, &mem.reg.F);
}

static void gb_cpu_BIT_3_C(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_bit_check(&mem.reg.C, 3, &mem.reg.F);
}

static void gb_cpu_BIT_3_D(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_bit_check(&mem.reg.D, 3, &mem.reg.F);
}

static void gb_cpu_BIT_3_E(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_bit_check(&mem.reg.E, 3, &mem.reg.F);
}

static void gb_cpu_BIT_3_H(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_bit_check(&mem.reg.H, 3, &mem.reg.F);
}

static void gb_cpu_BIT_3_L(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_bit_check(&mem.reg.L, 3, &mem.reg.F);
}

static void gb_cpu_BIT_3_HL(gb_instr_info_t *info)
{
	(void)info;
	uint8_t temp_res = gb_memory_read(mem.reg.HL);
	gb_cpu_bit_check(&temp_res, 3, &mem.reg.F);
}

static void gb_cpu_BIT_3_A(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_bit_check(&mem.reg.A, 3, &mem.reg.F);
}

/*********************0x6X*/
static void gb_cpu_BIT_4_B(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_bit_check(&mem.reg.B, 4, &mem.reg.F);
}

static void gb_cpu_BIT_4_C(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_bit_check(&mem.reg.C, 4, &mem.reg.F);
}

static void gb_cpu_BIT_4_D(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_bit_check(&mem.reg.D, 4, &mem.reg.F);
}

static void gb_cpu_BIT_4_E(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_bit_check(&mem.reg.E, 4, &mem.reg.F);
}

static void gb_cpu_BIT_4_H(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_bit_check(&mem.reg.H, 4, &mem.reg.F);
}

static void gb_cpu_BIT_4_L(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_bit_check(&mem.reg.L, 4, &mem.reg.F);
}

static void gb_cpu_BIT_4_HL(gb_instr_info_t *info)
{
	(void)info;
	uint8_t temp_res = gb_memory_read(mem.reg.HL);
	gb_cpu_bit_check(&temp_res, 4, &mem.reg.F);
}

static void gb_cpu_BIT_4_A(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_bit_check(&mem.reg.A, 4, &mem.reg.F);
}

static void gb_cpu_BIT_5_B(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_bit_check(&mem.reg.B, 5, &mem.reg.F);
}

static void gb_cpu_BIT_5_C(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_bit_check(&mem.reg.C, 5, &mem.reg.F);
}

static void gb_cpu_BIT_5_D(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_bit_check(&mem.reg.D, 5, &mem.reg.F);
}

static void gb_cpu_BIT_5_E(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_bit_check(&mem.reg.E, 5, &mem.reg.F);
}

static void gb_cpu_BIT_5_H(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_bit_check(&mem.reg.H, 5, &mem.reg.F);
}

static void gb_cpu_BIT_5_L(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_bit_check(&mem.reg.L, 5, &mem.reg.F);
}

static void gb_cpu_BIT_5_HL(gb_instr_info_t *info)
{
	(void)info;
	uint8_t temp_res = gb_memory_read(mem.reg.HL);
	gb_cpu_bit_check(&temp_res, 5, &mem.reg.F);
}

static void gb_cpu_BIT_5_A(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_bit_check(&mem.reg.A, 5, &mem.reg.F);
}

/*********************0x7X*/
static void gb_cpu_BIT_6_B(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_bit_check(&mem.reg.B, 6, &mem.reg.F);
}

static void gb_cpu_BIT_6_C(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_bit_check(&mem.reg.C, 6, &mem.reg.F);
}

static void gb_cpu_BIT_6_D(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_bit_check(&mem.reg.D, 6, &mem.reg.F);
}

static void gb_cpu_BIT_6_E(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_bit_check(&mem.reg.E, 6, &mem.reg.F);
}

static void gb_cpu_BIT_6_H(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_bit_check(&mem.reg.H, 6, &mem.reg.F);
}

static void gb_cpu_BIT_6_L(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_bit_check(&mem.reg.L, 6, &mem.reg.F);
}

static void gb_cpu_BIT_6_HL(gb_instr_info_t *info)
{
	(void)info;
	uint8_t temp_res = gb_memory_read(mem.reg.HL);
	gb_cpu_bit_check(&temp_res, 6, &mem.reg.F);
}

static void gb_cpu_BIT_6_A(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_bit_check(&mem.reg.A, 6, &mem.reg.F);
}

static void gb_cpu_BIT_7_B(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_bit_check(&mem.reg.B, 7, &mem.reg.F);
}

static void gb_cpu_BIT_7_C(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_bit_check(&mem.reg.C, 7, &mem.reg.F);
}

static void gb_cpu_BIT_7_D(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_bit_check(&mem.reg.D, 7, &mem.reg.F);
}

static void gb_cpu_BIT_7_E(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_bit_check(&mem.reg.E, 7, &mem.reg.F);
}

static void gb_cpu_BIT_7_H(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_bit_check(&mem.reg.H, 7, &mem.reg.F);
}

static void gb_cpu_BIT_7_L(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_bit_check(&mem.reg.L, 7, &mem.reg.F);
}

static void gb_cpu_BIT_7_HL(gb_instr_info_t *info)
{
	(void)info;
	uint8_t temp_res = gb_memory_read(mem.reg.HL);
	gb_cpu_bit_check(&temp_res, 7, &mem.reg.F);
}

static void gb_cpu_BIT_7_A(gb_instr_info_t *info)
{
	(void)info;
	gb_cpu_bit_check(&mem.reg.A, 7, &mem.reg.F);
}

/*********************0x8X*/
static void gb_cpu_RES_0_B(gb_instr_info_t *info)
{
	(void)info;
	RST_BIT(mem.reg.B, 0);
}

static void gb_cpu_RES_0_C(gb_instr_info_t *info)
{
	(void)info;
	RST_BIT(mem.reg.C, 0);
}

static void gb_cpu_RES_0_D(gb_instr_info_t *info)
{
	(void)info;
	RST_BIT(mem.reg.D, 0);
}

static void gb_cpu_RES_0_E(gb_instr_info_t *info)
{
	(void)info;
	RST_BIT(mem.reg.E, 0);
}

static void gb_cpu_RES_0_H(gb_instr_info_t *info)
{
	(void)info;
	RST_BIT(mem.reg.H, 0);
}

static void gb_cpu_RES_0_L(gb_instr_info_t *info)
{
	(void)info;
	RST_BIT(mem.reg.L, 0);
}

static void gb_cpu_RES_0_HL(gb_instr_info_t *info)
{
	static uint8_t temp_res = 0;
	if (info->current_cycle == 2) {
		temp_res = gb_memory_read(mem.reg.HL);
		RST_BIT(temp_res, 0);

	} else if (info->current_cycle == 3) {
		gb_memory_write(mem.reg.HL, temp_res);
	}
	info->current_cycle = CUSTOM_TIMING;
}

static void gb_cpu_RES_0_A(gb_instr_info_t *info)
{
	(void)info;
	RST_BIT(mem.reg.A, 0);
}

static void gb_cpu_RES_1_B(gb_instr_info_t *info)
{
	(void)info;
	RST_BIT(mem.reg.B, 1);
}

static void gb_cpu_RES_1_C(gb_instr_info_t *info)
{
	(void)info;
	RST_BIT(mem.reg.C, 1);
}

static void gb_cpu_RES_1_D(gb_instr_info_t *info)
{
	(void)info;
	RST_BIT(mem.reg.D, 1);
}

static void gb_cpu_RES_1_E(gb_instr_info_t *info)
{
	(void)info;
	RST_BIT(mem.reg.E, 1);
}

static void gb_cpu_RES_1_H(gb_instr_info_t *info)
{
	(void)info;
	RST_BIT(mem.reg.H, 1);
}

static void gb_cpu_RES_1_L(gb_instr_info_t *info)
{
	(void)info;
	RST_BIT(mem.reg.L, 1);
}

static void gb_cpu_RES_1_HL(gb_instr_info_t *info)
{
	static uint8_t temp_res = 0;
	if (info->current_cycle == 2) {
		temp_res = gb_memory_read(mem.reg.HL);
		RST_BIT(temp_res, 1);

	} else if (info->current_cycle == 3) {
		gb_memory_write(mem.reg.HL, temp_res);
	}
	info->current_cycle = CUSTOM_TIMING;
}

static void gb_cpu_RES_1_A(gb_instr_info_t *info)
{
	(void)info;
	RST_BIT(mem.reg.A, 1);
}

/*********************0x9X*/
static void gb_cpu_RES_2_B(gb_instr_info_t *info)
{
	(void)info;
	RST_BIT(mem.reg.B, 2);
}

static void gb_cpu_RES_2_C(gb_instr_info_t *info)
{
	(void)info;
	RST_BIT(mem.reg.C, 2);
}

static void gb_cpu_RES_2_D(gb_instr_info_t *info)
{
	(void)info;
	RST_BIT(mem.reg.D, 2);
}

static void gb_cpu_RES_2_E(gb_instr_info_t *info)
{
	(void)info;
	RST_BIT(mem.reg.E, 2);
}

static void gb_cpu_RES_2_H(gb_instr_info_t *info)
{
	(void)info;
	RST_BIT(mem.reg.H, 2);
}

static void gb_cpu_RES_2_L(gb_instr_info_t *info)
{
	(void)info;
	RST_BIT(mem.reg.L, 2);
}

static void gb_cpu_RES_2_HL(gb_instr_info_t *info)
{
	static uint8_t temp_res = 0;
	if (info->current_cycle == 2) {
		temp_res = gb_memory_read(mem.reg.HL);
		RST_BIT(temp_res, 2);

	} else if (info->current_cycle == 3) {
		gb_memory_write(mem.reg.HL, temp_res);
	}
	info->current_cycle = CUSTOM_TIMING;
}

static void gb_cpu_RES_2_A(gb_instr_info_t *info)
{
	(void)info;
	RST_BIT(mem.reg.A, 2);
}

static void gb_cpu_RES_3_B(gb_instr_info_t *info)
{
	(void)info;
	RST_BIT(mem.reg.B, 3);
}

static void gb_cpu_RES_3_C(gb_instr_info_t *info)
{
	(void)info;
	RST_BIT(mem.reg.C, 3);
}

static void gb_cpu_RES_3_D(gb_instr_info_t *info)
{
	(void)info;
	RST_BIT(mem.reg.D, 3);
}

static void gb_cpu_RES_3_E(gb_instr_info_t *info)
{
	(void)info;
	RST_BIT(mem.reg.E, 3);
}

static void gb_cpu_RES_3_H(gb_instr_info_t *info)
{
	(void)info;
	RST_BIT(mem.reg.H, 3);
}

static void gb_cpu_RES_3_L(gb_instr_info_t *info)
{
	(void)info;
	RST_BIT(mem.reg.L, 3);
}

static void gb_cpu_RES_3_HL(gb_instr_info_t *info)
{
	static uint8_t temp_res = 0;
	if (info->current_cycle == 2) {
		temp_res = gb_memory_read(mem.reg.HL);
		RST_BIT(temp_res, 3);

	} else if (info->current_cycle == 3) {
		gb_memory_write(mem.reg.HL, temp_res);
	}
	info->current_cycle = CUSTOM_TIMING;
}

static void gb_cpu_RES_3_A(gb_instr_info_t *info)
{
	(void)info;
	RST_BIT(mem.reg.A, 3);
}

/*********************0xAX*/
static void gb_cpu_RES_4_B(gb_instr_info_t *info)
{
	(void)info;
	RST_BIT(mem.reg.B, 4);
}

static void gb_cpu_RES_4_C(gb_instr_info_t *info)
{
	(void)info;
	RST_BIT(mem.reg.C, 4);
}

static void gb_cpu_RES_4_D(gb_instr_info_t *info)
{
	(void)info;
	RST_BIT(mem.reg.D, 4);
}

static void gb_cpu_RES_4_E(gb_instr_info_t *info)
{
	(void)info;
	RST_BIT(mem.reg.E, 4);
}

static void gb_cpu_RES_4_H(gb_instr_info_t *info)
{
	(void)info;
	RST_BIT(mem.reg.H, 4);
}

static void gb_cpu_RES_4_L(gb_instr_info_t *info)
{
	(void)info;
	RST_BIT(mem.reg.L, 4);
}

static void gb_cpu_RES_4_HL(gb_instr_info_t *info)
{
	static uint8_t temp_res = 0;
	if (info->current_cycle == 2) {
		temp_res = gb_memory_read(mem.reg.HL);
		RST_BIT(temp_res, 4);

	} else if (info->current_cycle == 3) {
		gb_memory_write(mem.reg.HL, temp_res);
	}
	info->current_cycle = CUSTOM_TIMING;
}

static void gb_cpu_RES_4_A(gb_instr_info_t *info)
{
	(void)info;
	RST_BIT(mem.reg.A, 4);
}

static void gb_cpu_RES_5_B(gb_instr_info_t *info)
{
	(void)info;
	RST_BIT(mem.reg.B, 5);
}

static void gb_cpu_RES_5_C(gb_instr_info_t *info)
{
	(void)info;
	RST_BIT(mem.reg.C, 5);
}

static void gb_cpu_RES_5_D(gb_instr_info_t *info)
{
	(void)info;
	RST_BIT(mem.reg.D, 5);
}

static void gb_cpu_RES_5_E(gb_instr_info_t *info)
{
	(void)info;
	RST_BIT(mem.reg.E, 5);
}

static void gb_cpu_RES_5_H(gb_instr_info_t *info)
{
	(void)info;
	RST_BIT(mem.reg.H, 5);
}

static void gb_cpu_RES_5_L(gb_instr_info_t *info)
{
	(void)info;
	RST_BIT(mem.reg.L, 5);
}

static void gb_cpu_RES_5_HL(gb_instr_info_t *info)
{
	static uint8_t temp_res = 0;
	if (info->current_cycle == 2) {
		temp_res = gb_memory_read(mem.reg.HL);
		RST_BIT(temp_res, 5);

	} else if (info->current_cycle == 3) {
		gb_memory_write(mem.reg.HL, temp_res);
	}
	info->current_cycle = CUSTOM_TIMING;
}

static void gb_cpu_RES_5_A(gb_instr_info_t *info)
{
	(void)info;
	RST_BIT(mem.reg.A, 5);
}

/*********************0xBX*/
static void gb_cpu_RES_6_B(gb_instr_info_t *info)
{
	(void)info;
	RST_BIT(mem.reg.B, 6);
}

static void gb_cpu_RES_6_C(gb_instr_info_t *info)
{
	(void)info;
	RST_BIT(mem.reg.C, 6);
}

static void gb_cpu_RES_6_D(gb_instr_info_t *info)
{
	(void)info;
	RST_BIT(mem.reg.D, 6);
}

static void gb_cpu_RES_6_E(gb_instr_info_t *info)
{
	(void)info;
	RST_BIT(mem.reg.E, 6);
}

static void gb_cpu_RES_6_H(gb_instr_info_t *info)
{
	(void)info;
	RST_BIT(mem.reg.H, 6);
}

static void gb_cpu_RES_6_L(gb_instr_info_t *info)
{
	(void)info;
	RST_BIT(mem.reg.L, 6);
}

static void gb_cpu_RES_6_HL(gb_instr_info_t *info)
{
	static uint8_t temp_res = 0;
	if (info->current_cycle == 2) {
		temp_res = gb_memory_read(mem.reg.HL);
		RST_BIT(temp_res, 6);

	} else if (info->current_cycle == 3) {
		gb_memory_write(mem.reg.HL, temp_res);
	}
	info->current_cycle = CUSTOM_TIMING;
}

static void gb_cpu_RES_6_A(gb_instr_info_t *info)
{
	(void)info;
	RST_BIT(mem.reg.A, 6);
}

static void gb_cpu_RES_7_B(gb_instr_info_t *info)
{
	(void)info;
	RST_BIT(mem.reg.B, 7);
}

static void gb_cpu_RES_7_C(gb_instr_info_t *info)
{
	(void)info;
	RST_BIT(mem.reg.C, 7);
}

static void gb_cpu_RES_7_D(gb_instr_info_t *info)
{
	(void)info;
	RST_BIT(mem.reg.D, 7);
}

static void gb_cpu_RES_7_E(gb_instr_info_t *info)
{
	(void)info;
	RST_BIT(mem.reg.E, 7);
}

static void gb_cpu_RES_7_H(gb_instr_info_t *info)
{
	(void)info;
	RST_BIT(mem.reg.H, 7);
}

static void gb_cpu_RES_7_L(gb_instr_info_t *info)
{
	(void)info;
	RST_BIT(mem.reg.L, 7);
}

static void gb_cpu_RES_7_HL(gb_instr_info_t *info)
{
	static uint8_t temp_res = 0;
	if (info->current_cycle == 2) {
		temp_res = gb_memory_read(mem.reg.HL);
		RST_BIT(temp_res, 7);

	} else if (info->current_cycle == 3) {
		gb_memory_write(mem.reg.HL, temp_res);
	}
	info->current_cycle = CUSTOM_TIMING;
}

static void gb_cpu_RES_7_A(gb_instr_info_t *info)
{
	(void)info;
	RST_BIT(mem.reg.A, 7);
}

/*********************0xCX*/
static void gb_cpu_SET_0_B(gb_instr_info_t *info)
{
	(void)info;
	SET_BIT(mem.reg.B, 0);
}

static void gb_cpu_SET_0_C(gb_instr_info_t *info)
{
	(void)info;
	SET_BIT(mem.reg.C, 0);
}

static void gb_cpu_SET_0_D(gb_instr_info_t *info)
{
	(void)info;
	SET_BIT(mem.reg.D, 0);
}

static void gb_cpu_SET_0_E(gb_instr_info_t *info)
{
	(void)info;
	SET_BIT(mem.reg.E, 0);
}

static void gb_cpu_SET_0_H(gb_instr_info_t *info)
{
	(void)info;
	SET_BIT(mem.reg.H, 0);
}

static void gb_cpu_SET_0_L(gb_instr_info_t *info)
{
	(void)info;
	SET_BIT(mem.reg.L, 0);
}

static void gb_cpu_SET_0_HL(gb_instr_info_t *info)
{
	static uint8_t temp_res = 0;
	if (info->current_cycle == 2) {
		temp_res = gb_memory_read(mem.reg.HL);
		SET_BIT(temp_res, 0);

	} else if (info->current_cycle == 3) {
		gb_memory_write(mem.reg.HL, temp_res);
	}
	info->current_cycle = CUSTOM_TIMING;
}

static void gb_cpu_SET_0_A(gb_instr_info_t *info)
{
	(void)info;
	SET_BIT(mem.reg.A, 0);
}

static void gb_cpu_SET_1_B(gb_instr_info_t *info)
{
	(void)info;
	SET_BIT(mem.reg.B, 1);
}

static void gb_cpu_SET_1_C(gb_instr_info_t *info)
{
	(void)info;
	SET_BIT(mem.reg.C, 1);
}

static void gb_cpu_SET_1_D(gb_instr_info_t *info)
{
	(void)info;
	SET_BIT(mem.reg.D, 1);
}

static void gb_cpu_SET_1_E(gb_instr_info_t *info)
{
	(void)info;
	SET_BIT(mem.reg.E, 1);
}

static void gb_cpu_SET_1_H(gb_instr_info_t *info)
{
	(void)info;
	SET_BIT(mem.reg.H, 1);
}

static void gb_cpu_SET_1_L(gb_instr_info_t *info)
{
	(void)info;
	SET_BIT(mem.reg.L, 1);
}

static void gb_cpu_SET_1_HL(gb_instr_info_t *info)
{
	static uint8_t temp_res = 0;
	if (info->current_cycle == 2) {
		temp_res = gb_memory_read(mem.reg.HL);
		SET_BIT(temp_res, 1);

	} else if (info->current_cycle == 3) {
		gb_memory_write(mem.reg.HL, temp_res);
	}
	info->current_cycle = CUSTOM_TIMING;
}

static void gb_cpu_SET_1_A(gb_instr_info_t *info)
{
	(void)info;
	SET_BIT(mem.reg.A, 1);
}

/*********************0xDX*/
static void gb_cpu_SET_2_B(gb_instr_info_t *info)
{
	(void)info;
	SET_BIT(mem.reg.B, 2);
}

static void gb_cpu_SET_2_C(gb_instr_info_t *info)
{
	(void)info;
	SET_BIT(mem.reg.C, 2);
}

static void gb_cpu_SET_2_D(gb_instr_info_t *info)
{
	(void)info;
	SET_BIT(mem.reg.D, 2);
}

static void gb_cpu_SET_2_E(gb_instr_info_t *info)
{
	(void)info;
	SET_BIT(mem.reg.E, 2);
}

static void gb_cpu_SET_2_H(gb_instr_info_t *info)
{
	(void)info;
	SET_BIT(mem.reg.H, 2);
}

static void gb_cpu_SET_2_L(gb_instr_info_t *info)
{
	(void)info;
	SET_BIT(mem.reg.L, 2);
}

static void gb_cpu_SET_2_HL(gb_instr_info_t *info)
{
	static uint8_t temp_res = 0;
	if (info->current_cycle == 2) {
		temp_res = gb_memory_read(mem.reg.HL);
		SET_BIT(temp_res, 2);

	} else if (info->current_cycle == 3) {
		gb_memory_write(mem.reg.HL, temp_res);
	}
	info->current_cycle = CUSTOM_TIMING;
}

static void gb_cpu_SET_2_A(gb_instr_info_t *info)
{
	(void)info;
	SET_BIT(mem.reg.A, 2);
}

static void gb_cpu_SET_3_B(gb_instr_info_t *info)
{
	(void)info;
	SET_BIT(mem.reg.B, 3);
}

static void gb_cpu_SET_3_C(gb_instr_info_t *info)
{
	(void)info;
	SET_BIT(mem.reg.C, 3);
}

static void gb_cpu_SET_3_D(gb_instr_info_t *info)
{
	(void)info;
	SET_BIT(mem.reg.D, 3);
}

static void gb_cpu_SET_3_E(gb_instr_info_t *info)
{
	(void)info;
	SET_BIT(mem.reg.E, 3);
}

static void gb_cpu_SET_3_H(gb_instr_info_t *info)
{
	(void)info;
	SET_BIT(mem.reg.H, 3);
}

static void gb_cpu_SET_3_L(gb_instr_info_t *info)
{
	(void)info;
	SET_BIT(mem.reg.L, 3);
}

static void gb_cpu_SET_3_HL(gb_instr_info_t *info)
{
	static uint8_t temp_res = 0;
	if (info->current_cycle == 2) {
		temp_res = gb_memory_read(mem.reg.HL);
		SET_BIT(temp_res, 3);

	} else if (info->current_cycle == 3) {
		gb_memory_write(mem.reg.HL, temp_res);
	}
	info->current_cycle = CUSTOM_TIMING;
}

static void gb_cpu_SET_3_A(gb_instr_info_t *info)
{
	(void)info;
	SET_BIT(mem.reg.A, 3);
}

/*********************0xEX*/
static void gb_cpu_SET_4_B(gb_instr_info_t *info)
{
	(void)info;
	SET_BIT(mem.reg.B, 4);
}

static void gb_cpu_SET_4_C(gb_instr_info_t *info)
{
	(void)info;
	SET_BIT(mem.reg.C, 4);
}

static void gb_cpu_SET_4_D(gb_instr_info_t *info)
{
	(void)info;
	SET_BIT(mem.reg.D, 4);
}

static void gb_cpu_SET_4_E(gb_instr_info_t *info)
{
	(void)info;
	SET_BIT(mem.reg.E, 4);
}

static void gb_cpu_SET_4_H(gb_instr_info_t *info)
{
	(void)info;
	SET_BIT(mem.reg.H, 4);
}

static void gb_cpu_SET_4_L(gb_instr_info_t *info)
{
	(void)info;
	SET_BIT(mem.reg.L, 4);
}

static void gb_cpu_SET_4_HL(gb_instr_info_t *info)
{
	static uint8_t temp_res = 0;
	if (info->current_cycle == 2) {
		temp_res = gb_memory_read(mem.reg.HL);
		SET_BIT(temp_res, 4);

	} else if (info->current_cycle == 3) {
		gb_memory_write(mem.reg.HL, temp_res);
	}
	info->current_cycle = CUSTOM_TIMING;
}

static void gb_cpu_SET_4_A(gb_instr_info_t *info)
{
	(void)info;
	SET_BIT(mem.reg.A, 4);
}

static void gb_cpu_SET_5_B(gb_instr_info_t *info)
{
	(void)info;
	SET_BIT(mem.reg.B, 5);
}

static void gb_cpu_SET_5_C(gb_instr_info_t *info)
{
	(void)info;
	SET_BIT(mem.reg.C, 5);
}

static void gb_cpu_SET_5_D(gb_instr_info_t *info)
{
	(void)info;
	SET_BIT(mem.reg.D, 5);
}

static void gb_cpu_SET_5_E(gb_instr_info_t *info)
{
	(void)info;
	SET_BIT(mem.reg.E, 5);
}

static void gb_cpu_SET_5_H(gb_instr_info_t *info)
{
	(void)info;
	SET_BIT(mem.reg.H, 5);
}

static void gb_cpu_SET_5_L(gb_instr_info_t *info)
{
	(void)info;
	SET_BIT(mem.reg.L, 5);
}

static void gb_cpu_SET_5_HL(gb_instr_info_t *info)
{
	static uint8_t temp_res = 0;
	if (info->current_cycle == 2) {
		temp_res = gb_memory_read(mem.reg.HL);
		SET_BIT(temp_res, 5);

	} else if (info->current_cycle == 3) {
		gb_memory_write(mem.reg.HL, temp_res);
	}
	info->current_cycle = CUSTOM_TIMING;
}

static void gb_cpu_SET_5_A(gb_instr_info_t *info)
{
	(void)info;
	SET_BIT(mem.reg.A, 5);
}

/*********************0xFX*/
static void gb_cpu_SET_6_B(gb_instr_info_t *info)
{
	(void)info;
	SET_BIT(mem.reg.B, 6);
}

static void gb_cpu_SET_6_C(gb_instr_info_t *info)
{
	(void)info;
	SET_BIT(mem.reg.C, 6);
}

static void gb_cpu_SET_6_D(gb_instr_info_t *info)
{
	(void)info;
	SET_BIT(mem.reg.D, 6);
}

static void gb_cpu_SET_6_E(gb_instr_info_t *info)
{
	(void)info;
	SET_BIT(mem.reg.E, 6);
}

static void gb_cpu_SET_6_H(gb_instr_info_t *info)
{
	(void)info;
	SET_BIT(mem.reg.H, 6);
}

static void gb_cpu_SET_6_L(gb_instr_info_t *info)
{
	(void)info;
	SET_BIT(mem.reg.L, 6);
}

static void gb_cpu_SET_6_HL(gb_instr_info_t *info)
{
	static uint8_t temp_res = 0;
	if (info->current_cycle == 2) {
		temp_res = gb_memory_read(mem.reg.HL);
		SET_BIT(temp_res, 6);

	} else if (info->current_cycle == 3) {
		gb_memory_write(mem.reg.HL, temp_res);
	}
	info->current_cycle = CUSTOM_TIMING;
}

static void gb_cpu_SET_6_A(gb_instr_info_t *info)
{
	(void)info;
	SET_BIT(mem.reg.A, 6);
}

static void gb_cpu_SET_7_B(gb_instr_info_t *info)
{
	(void)info;
	SET_BIT(mem.reg.B, 7);
}

static void gb_cpu_SET_7_C(gb_instr_info_t *info)
{
	(void)info;
	SET_BIT(mem.reg.C, 7);
}

static void gb_cpu_SET_7_D(gb_instr_info_t *info)
{
	(void)info;
	SET_BIT(mem.reg.D, 7);
}

static void gb_cpu_SET_7_E(gb_instr_info_t *info)
{
	(void)info;
	SET_BIT(mem.reg.E, 7);
}

static void gb_cpu_SET_7_H(gb_instr_info_t *info)
{
	(void)info;
	SET_BIT(mem.reg.H, 7);
}

static void gb_cpu_SET_7_L(gb_instr_info_t *info)
{
	(void)info;
	SET_BIT(mem.reg.L, 7);
}

static void gb_cpu_SET_7_HL(gb_instr_info_t *info)
{
	static uint8_t temp_res = 0;
	if (info->current_cycle == 2) {
		temp_res = gb_memory_read(mem.reg.HL);
		SET_BIT(temp_res, 7);

	} else if (info->current_cycle == 3) {
		gb_memory_write(mem.reg.HL, temp_res);
	}
	info->current_cycle = CUSTOM_TIMING;
}

static void gb_cpu_SET_7_A(gb_instr_info_t *info)
{
	(void)info;
	SET_BIT(mem.reg.A, 7);
}

/**
 * @brief If the corresponding IE and IF are both set then jump to the
 * corresponding ISR
 * @returns Nothing
 */
static void gb_cpu_interrupt_handler(int *interupt_dur)
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
				gb_cpu_push_to_stack(&mem.reg.SP, mem.reg.PC);
				interrupt_triggered = 1;
				mem.reg.PC = VBLANK_VECTOR;
			} else if (interrupt_set_and_enabled & LCDSTAT_INTERRUPT) {
				gb_memory_reset_bit(IF_ADDR, 1);
				gb_cpu_push_to_stack(&mem.reg.SP, mem.reg.PC);
				interrupt_triggered = 1;
				mem.reg.PC = LCDSTAT_VECTOR;
			} else if (interrupt_set_and_enabled & TIMER_INTERRUPT) {
				gb_memory_reset_bit(IF_ADDR, 2);
				gb_cpu_push_to_stack(&mem.reg.SP, mem.reg.PC);
				interrupt_triggered = 1;
				mem.reg.PC = TIMER_VECTOR;
			} else if (interrupt_set_and_enabled & SERIAL_INTERRUPT) {
				gb_memory_reset_bit(IF_ADDR, 3);
				gb_cpu_push_to_stack(&mem.reg.SP, mem.reg.PC);
				interrupt_triggered = 1;
				mem.reg.PC = SERIAL_VECTOR;
			} else if (interrupt_set_and_enabled & JOYPAD_INTERRUPT) {
				gb_memory_reset_bit(IF_ADDR, 4);
				gb_cpu_push_to_stack(&mem.reg.SP, mem.reg.PC);
				interrupt_triggered = 1;
				mem.reg.PC = JOYPAD_VECTOR;
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
static void gb_cpu_halted_handler(void)
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
	static uint8_t next_instruction = 0;
	static gb_instr_t *intstruction_table = NULL;

	/* if halted */
	if (halted) {
		goto finally;
	}

	if (interupt_dur) {
		interupt_dur--;
		goto finally;
	}

	/* if no remaining op */
	if (op_remaining <= 0) {
		dont_update_pc = false;
		next_instruction = gb_memory_read(mem.reg.PC);
		if (next_instruction != PREFIX_OPCODE) {
			opcode = next_instruction;
			intstruction_table = instructions;
		} else {
			opcode = gb_memory_read(mem.reg.PC + 1);
			intstruction_table = prefix_instructions;
		}
		op_remaining = intstruction_table[opcode].info.cycles;
		current_cycle = 1;
	}

	if (intstruction_table[opcode].info.current_cycle == current_cycle ||
	    intstruction_table[opcode].info.current_cycle == CUSTOM_TIMING) {

		if (intstruction_table[opcode].info.current_cycle == CUSTOM_TIMING) {
			intstruction_table[opcode].info.current_cycle = current_cycle;
		}

		((void (*)(gb_instr_info_t *))instructions[next_instruction].instr)(
			&intstruction_table[opcode].info);
	}
	current_cycle++;

	/* dec op remaining */
	op_remaining--;

	if (op_remaining == 0 && dont_update_pc == false) {
		mem.reg.PC += intstruction_table[opcode].info.bytes;
	}

finally:
	/* Inc timers */
	gb_memory_inc_timers(1);

	/* Handle interupts */
	if (interrupt_master_enable == 1 && op_remaining == 0) {
		gb_cpu_interrupt_handler(&interupt_dur);
	} else if (halted == 1 && op_remaining == 0) {
		gb_cpu_halted_handler();
	}
}
