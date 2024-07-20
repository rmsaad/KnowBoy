/**
 * @file gb_mbc.c
 * @brief Gameboy debugger Functionality.
 *
 * This file implements debugger functionality for the gameboy.
 *
 * @author Rami Saad
 * @date 2024-06-30
 */

#include "gb_debug.h"
#include "gb_memory.h"
#include "logging.h"

#include <ctype.h>
#include <stdint.h>
#include <string.h>

#define MAX_BREAKPOINTS 5

typedef struct {
	uint16_t address[MAX_BREAKPOINTS];
	bool activated[MAX_BREAKPOINTS];
} breakpoints_t;

static bool debugger_stopped;
static breakpoints_t breakpoints;
static gb_debug_check_msg_queue_t gb_debug_check_queue;
static gb_debug_flush_t gb_debug_flush;
static void *gb_debug_queue_ctx;
static bool proceed;
static uint16_t prev_PC;
extern memory_t mem;

static bool gb_debug_parse_address(char *str, int len, uint16_t *result)
{
	int num = 0;
	int i = 0;
	bool is_valid = true;
	bool is_hex = false;

	if (str[0] == '-') {
		is_valid = false;
	}

	if (len > 2 && str[0] == '0' && str[1] == 'x') {
		is_hex = true;
		i = 2;
	}

	while (i < len) {
		if (isdigit(str[i])) {
			num = num * (is_hex ? 16 : 10) + (str[i] - '0');
		} else if (is_hex && str[i] >= 'a' && str[i] <= 'f') {
			num = num * 16 + (str[i] - 'a' + 10);
		} else if (is_hex && str[i] >= 'A' && str[i] <= 'F') {
			num = num * 16 + (str[i] - 'A' + 10);
		} else if (str[i] == '\0') {
			break;
		} else {
			is_valid = false;
			break;
		}
		i++;
	}

	if (is_valid) {
		*result = num;
	}

	return is_valid;
}

static int gb_debug_get_free_breakpoint(breakpoints_t *bps)
{
	for (int i = 0; i < MAX_BREAKPOINTS; i++) {
		if (!bps->activated[i]) {
			return i;
		}
	}
	return -1;
}

static bool gb_debug_deactivate_breakpoint(breakpoints_t *bps, uint16_t address)
{
	for (int i = 0; i < MAX_BREAKPOINTS; i++) {
		if (bps->address[i] == address && bps->activated[i]) {
			bps->activated[i] = false;
			return true;
		}
	}
	return false;
}

static bool gb_debug_is_breakpoint_activated(breakpoints_t *bps, uint16_t address)
{
	for (int i = 0; i < MAX_BREAKPOINTS; i++) {
		if (bps->address[i] == address && bps->activated[i]) {
			return true;
		}
	}
	return false;
}

static void gb_debug_deactivate_all_breakpoints(breakpoints_t *bps)
{
	for (int i = 0; i < MAX_BREAKPOINTS; i++) {
		bps->activated[i] = false;
	}
}

void gb_debug_init(gb_debug_check_msg_queue_t check_msg_queue, gb_debug_flush_t flush,
		   void *queue_ctx)
{
	debugger_stopped = false;
	proceed = false;
	prev_PC = 0xFFFF;
	gb_debug_check_queue = check_msg_queue;
	gb_debug_queue_ctx = queue_ctx;
	gb_debug_flush = flush;
	gb_debug_deactivate_all_breakpoints(&breakpoints);
}

void gb_debug_check_msg_queue(void)
{
	char message[QUEUE_MSG_LEN];
	if (gb_debug_check_queue(gb_debug_queue_ctx, message)) {
		if (strncmp(message, "stop", QUEUE_MSG_LEN) == 0) {
			debugger_stopped = true;
		} else if (strncmp(message, "continue", QUEUE_MSG_LEN) == 0) {
			debugger_stopped = false;
		} else if (strncmp(message, "step", QUEUE_MSG_LEN) == 0) {
			proceed = true;
			prev_PC = mem.reg.PC;
		} else if (strncmp(message, "registers", QUEUE_MSG_LEN) == 0) {
			LOG_DBG("opcode: %x, PC: %x, AF: %x, BC: %x, DE: %x, HL: %x, SP: %x",
				mem.map[mem.reg.PC], mem.reg.PC, mem.reg.AF, mem.reg.BC, mem.reg.DE,
				mem.reg.HL, mem.reg.SP);
		} else if (strncmp(message, "break", sizeof("break") - 1) == 0) {
			uint16_t address;
			bool valid =
				gb_debug_parse_address(&message[sizeof("break")],
						       QUEUE_MSG_LEN - (sizeof("break")), &address);
			if (valid && !gb_debug_is_breakpoint_activated(&breakpoints, address)) {
				int index = gb_debug_get_free_breakpoint(&breakpoints);
				if (index < 0) {
					LOG_ERR("No more breakpoints availible");
					return;
				}
				breakpoints.address[index] = address;
				breakpoints.activated[index] = true;
			}
		} else if (strncmp(message, "delete", sizeof("delete") - 1) == 0) {
			uint16_t address;
			bool valid = gb_debug_parse_address(&message[sizeof("delete")],
							    QUEUE_MSG_LEN - (sizeof("delete")),
							    &address);
			if (valid && gb_debug_is_breakpoint_activated(&breakpoints, address)) {
				gb_debug_deactivate_breakpoint(&breakpoints, address);
			}
		} else if (strncmp(message, "print", sizeof("print") - 1) == 0) {
			uint16_t address;
			bool valid =
				gb_debug_parse_address(&message[sizeof("print")],
						       QUEUE_MSG_LEN - (sizeof("print")), &address);
			if (valid) {
				LOG_DBG("address %x: %x", address, mem.map[address]);
			}
		}
		gb_debug_flush();
	}
}

bool gb_debug_step(void)
{
	if (proceed == true) {
		if (mem.reg.PC == prev_PC) {
			return false;
		} else {
			proceed = false;
		}
	}

	for (int i = 0; i < MAX_BREAKPOINTS; i++) {
		if (breakpoints.activated[i] == true) {
			if (mem.reg.PC == breakpoints.address[i]) {
				debugger_stopped = true;
			}
		}
	}

	if (debugger_stopped) {
		gb_debug_check_msg_queue();
	}

	return debugger_stopped && !proceed;
}
