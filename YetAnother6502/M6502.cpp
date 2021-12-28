#include "M6502.h"
#include <stdio.h>
#include <stdlib.h>
#include <memory>

class M6502::Status_Flags {
public:
	byte_t carry_f : 1;
	byte_t zero_f : 1;
	byte_t interrupt_f : 1;
	byte_t decimal_m : 1;
	byte_t break_c : 1;
	byte_t overflow_f : 1;
	byte_t negative_f : 1;

	void reset() {
		carry_f = zero_f = interrupt_f = decimal_m = break_c = overflow_f = negative_f = 0;
	}
};

class M6502::Memory {
public:
	~Memory() { delete[] mem; } // Deleting dynamic array
	Memory() = default;

	static constexpr u32_t MAXIMUM_MEMORY_SIZE = 64 * 1024;
	byte_t* mem{ new byte_t[MAXIMUM_MEMORY_SIZE] };

	byte_t read_byte(u32_t address) {
		if (address <= MAXIMUM_MEMORY_SIZE)
			return mem[address];
		else return -1;
	}

	byte_t write_byte(u32_t address, byte_t data) {
		//printf("[ address: %d rewritten with value %d ]\n", address, data);
		if (address <= MAXIMUM_MEMORY_SIZE) {
			mem[address] = data;
			return mem[address];
		}
		else return -1;
	}

	void init() {
		for (u32_t i{ 0 }; i < MAXIMUM_MEMORY_SIZE; i++) {
			write_byte(i, 0);
		}
	}
};

class M6502::CPU {
public:
	word_t pc; // Program Counter
	byte_t sp; // Stack Pointer 
	byte_t acc, x, y; // Accumulator, Index X, Index Y registers

	Status_Flags sf;

	static constexpr byte_t   //Opcodes
		INST_LDA_IM{ 0xA9 },
		INST_LDA_ZP{ 0xA5 },
		INST_LDA_ZPX{ 0xB5 },
		INST_LDA_ABS{ 0xAD },
		INST_LDA_ABSX{ 0xBD },
		INST_LDA_ABSY{ 0xB9 },
		INST_LDA_INDX{ 0xA1 },
		INST_LDA_INDY{ 0xB1 },

		INST_LDX_IM{ 0xA2 },
		INST_LDX_ZP{ 0xA6 },
		INST_LDX_ZPY{ 0xB6 },
		INST_LDX_ABS{ 0xAE },
		INST_LDX_ABSY{ 0xBE },

		INST_LDY_IM{ 0xA0 },
		INST_LDY_ZP{ 0xA4 },
		INST_LDY_ZPX{ 0xB4 },
		INST_LDY_ABS{ 0xAC },
		INST_LDY_ABSX{ 0xBC },

		INST_STA_ZP{ 0x85 },
		INST_STA_ZPX{ 0x95 },
		INST_STA_ABS{ 0x8D },
		INST_STA_ABSX{ 0x9D },
		INST_STA_ABSY{ 0x99 },
		INST_STA_INDX{ 0x81 },
		INST_STA_INDY{ 0x91 };

	void reset(Memory& mem) {
		pc = 0xFFC;
		sf.reset();
		sp = 0xFF;
		acc = x = y = 0;
		mem.init();
	}

	byte_t fetch_byte(s32_t& cycles, Memory& mem) {
		byte_t fetched_byte = mem.read_byte(pc);
		pc++;
		cycles--;
		return fetched_byte;
	}

	word_t fetch_word(s32_t& cycles, Memory& mem) {
		word_t fetched_word = mem.read_byte(pc);
		pc++;
		fetched_word |= (mem.read_byte(pc) << 8);
		cycles -= 2;
		return fetched_word;
	}

	byte_t read_byte_from_addr(s32_t& cycles, u32_t addr, Memory& mem) {
		byte_t read_byte = mem.read_byte(addr);
		cycles--;
		return read_byte;
	}

	word_t read_word_from_addr(s32_t& cycles, u32_t addr, Memory& mem) {
		byte_t low_byte = read_byte_from_addr(cycles, addr, mem);
		byte_t high_byte = read_byte_from_addr(cycles, addr + 1, mem);
		return low_byte | (high_byte << 8);
	}

	bool write_byte_to_addr(s32_t& cycles, u32_t addr, byte_t value, Memory& mem) {
		byte_t written_byte = mem.write_byte(addr, value);
		if (written_byte != -1) {
			cycles--;
			return true;
		}
		else return false;
	}

	/*bool write_word_to_addr(s32_t& cycles, u32_t addr, word_t value, Memory& mem) {
		byte_t low_byte = read_byte_from_addr(cycles, addr, mem);
		byte_t high_byte = read_byte_from_addr(cycles, addr + 1, mem);
		return low_byte | (high_byte << 8);
	}*/

	byte_t read_zero_page(s32_t& cycles, Memory& mem, s32_t offset = 0) {
		byte_t zero_page_addr = fetch_byte(cycles, mem);
		return read_byte_from_addr(cycles, zero_page_addr + offset, mem);
	}

	byte_t read_absolute(s32_t& cycles, Memory& mem, s32_t offset = 0) {
		word_t absolute_addr = fetch_word(cycles, mem);
		if (static_cast<bool>((absolute_addr ^ (absolute_addr + offset)) >> 8)) cycles--; // if crossed the page boundary
		return read_byte_from_addr(cycles, absolute_addr + offset, mem);
	}

	byte_t read_indirect_x(s32_t& cycles, Memory& mem, byte_t& x_reg) {
		byte_t zero_page_addr = fetch_byte(cycles, mem);
		zero_page_addr += x_reg;
		word_t eff_addr = read_word_from_addr(cycles, zero_page_addr, mem);
		cycles--;
		return read_byte_from_addr(cycles, eff_addr, mem);
	}

	byte_t read_indirect_y(s32_t& cycles, Memory& mem, byte_t& y_reg) {
		byte_t zero_page_addr = fetch_byte(cycles, mem);
		word_t eff_addr = read_word_from_addr(cycles, zero_page_addr, mem);
		word_t eff_addr_add_y = eff_addr + y_reg;
		if (static_cast<bool>((eff_addr ^ eff_addr_add_y) >> 8)) cycles--; // if it crossed the page boundary
		return read_byte_from_addr(cycles, eff_addr_add_y, mem);
	}

	void print_registers() {
		printf("[ a: %x | x: %x | y: %x | pc: %x | sp: %x ]\n", 
			acc, x, y, pc, sp);
	}
	void print_status_flags() {
		printf("[ c: %d | z: %d | i: %d | d: %d | b: %d | v: %d | n: %d ]\n", 
			sf.carry_f, sf.zero_f, sf.interrupt_f, sf.decimal_m, sf.break_c, sf.overflow_f, sf.negative_f);
	}

	void LD_flags(byte_t& cpu_register) {
		if ((cpu_register & 0b1000000) > 0) sf.negative_f = 1;
		if (cpu_register == 0) sf.zero_f = 1;
	}

	void execute(s32_t cycles, Memory& mem) {
		while (cycles > 0) {
			byte_t instruction = fetch_byte(cycles, mem);
			printf("[ fetched byte: %x ]\n", instruction);
			switch (instruction)
			{
				// LDA
			case INST_LDA_IM: {
				acc = fetch_byte(cycles, mem);
				LD_flags(acc);
			} break;
			case INST_LDA_ZP: {
				acc = read_zero_page(cycles, mem);
				LD_flags(acc);
			} break;
			case INST_LDA_ZPX: {
				acc = read_zero_page(cycles, mem, x);
				LD_flags(acc);
			} break;
			case INST_LDA_ABS: {
				acc = read_absolute(cycles, mem);
				LD_flags(acc);
			} break;
			case INST_LDA_ABSX: {
				acc = read_absolute(cycles, mem, x);
				LD_flags(acc);
			} break;
			case INST_LDA_ABSY: {
				acc = read_absolute(cycles, mem, y);
				LD_flags(acc);
			} break;
			case INST_LDA_INDX: {
				acc = read_indirect_x(cycles, mem, x);
				LD_flags(acc);
			} break;
			case INST_LDA_INDY: {
				acc = read_indirect_y(cycles, mem, y);
				LD_flags(acc);
			} break;
				// LDX
			case INST_LDX_IM: {
				x = fetch_byte(cycles, mem);
				LD_flags(x);
			} break;
			case INST_LDX_ZP: {
				x = read_zero_page(cycles, mem);
				LD_flags(x);
			} break;
			case INST_LDX_ZPY: {
				x = read_zero_page(cycles, mem, y);
				LD_flags(x);
			} break;
			case INST_LDX_ABS: {
				x = read_absolute(cycles, mem);
				LD_flags(x);
			} break;
			case INST_LDX_ABSY: {
				x = read_absolute(cycles, mem, y);
				LD_flags(x);
			} break;
				// LDY
			case INST_LDY_IM: {
				y = fetch_byte(cycles, mem);
				LD_flags(y);
			} break;
			case INST_LDY_ZP: {
				y = read_zero_page(cycles, mem);
				LD_flags(y);
			} break;
			case INST_LDY_ZPX: {
				y = read_zero_page(cycles, mem, x);
				LD_flags(y);
			} break;
			case INST_LDY_ABS: {
				y = read_absolute(cycles, mem);
				LD_flags(y);
			} break;
			case INST_LDY_ABSX: {
				y = read_absolute(cycles, mem, x);
				LD_flags(y);
			} break;
				// STA
			case INST_STA_ZP: {

			} break;
			default: {
				printf("[ ! instruction: %x is undefined ]\n", instruction);
			} break;
			}
		}
		// Debug Stuff
		print_registers();
		print_status_flags();
	}

};