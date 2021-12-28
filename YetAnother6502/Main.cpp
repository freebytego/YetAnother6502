#include "M6502.cpp"
#include <iostream>
int main() {

	M6502::CPU cpu;
	M6502::Memory memory;

	cpu.reset(memory);

	memory.write_byte(0xFFC, M6502::CPU::INST_LDX_IM);
	memory.write_byte(0xFFD, 0x86);
	cpu.execute(2, memory);

	return 0;
}