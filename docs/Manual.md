# BandCHIP Assembler Manual (Version 0.4)

## Getting Started
Run the program, using the following syntax:
```
bandchip_assembler <input> -o <output>
```
Be aware that this version of the BandCHIP Assembler only supports one input file.  As long the input
file contains valid CHIP-8 assembly language instructions, it should work fine.

## Extension Support
|Extension |Description |Memory Limit |
|----------|------------|-------------|
|CHIP8|Enables the original CHIP-8 instructions (excluding machine language instructions.|4KB|
|SCHIP10|Enables SuperCHIP V1.0 instructions|4KB|
|SCHIP11|Enables SuperCHIP V1.1 instructions.  Builds upon SuperCHIP V1.0|4KB|
|HCHIP64|Enables HyperCHIP-64 instructions.  Builds upon SuperCHIP V1.1 with upgrades to the original CHIP-8 instructions.|64KB|

## Option Support
|Option Type |Description |Options |
|------------|------------|--------|
|ALIGN|Sets the memory alignment.|On (Word-Aligned/2 Byte), Off|
|EXTENSION|Sets the extension to use.|CHIP8, SCHIP10, SCHIP11, HCHIP64|

## Instructions
|Opcode |Instruction |Description |Supported Extensions |
|-------|------------|------------|---------------------|
|00CN|SCD|Scrolls the screen down by N pixels.|SuperCHIP V1.1, HyperCHIP-64|
|00DN|SCU|Scrolls the screen up by N pixels.|HyperCHIP-64|
|00E0|CLS|Clears the screen.|CHIP-8, SuperCHIP V1.0/V1.1, HyperCHIP-64|
|00EE|RET|Returns from the subroutine.|CHIP-8, SuperCHIP V1.0/V1.1, HyperCHIP-64|
|00FB|SCR|Scrolls the screen to the right by 4 pixels.|SuperCHIP V1.1, HyperCHIP-64|
|00FC|SCL|Scrolls the screen to the left by 4 pixels.|SuperCHIP V1.1, HyperCHIP-64|
|00FD|EXIT|Exits the interpreter.|SuperCHIP V1.0/V1.1, HyperCHIP-64|
|00FE|LOW|Enters Low Resolution Mode.|SuperCHIP V1.0/V1.1, HyperCHIP-64|
|00FF|HIGH|Enters High Resolution Mode.|SuperCHIP V1.0/V1.1, HyperCHIP-64|
|1NNN|JP NNN|Jumps to the absolute address.|CHIP-8, SuperCHIP V1.0/V1.1, HyperCHIP-64|
|2NNN|CALL NNN|Calls the subroutine at that absolute address.|CHIP-8, SuperCHIP V1.0/V1.1, HyperCHIP-64|
|3XNN|SE VX, NN|Skips the following instruction if VX == NN|CHIP-8, SuperCHIP V1.0/V1.1, HyperCHIP-64|
|4XNN|SNE VX, NN|Skips the following instruction if VX != NN|CHIP-8, SuperCHIP V1.0/V1.1, HyperCHIP-64|
|5XY0|SE VX, VY|Skips the following instruction if VX == VY|CHIP-8, SuperCHIP V1.0/V1.1, HyperCHIP-64|
|5XY2|LD [I], VX, VY|Stores registers VX to VY in memory, starting at I.  I does not increment.|HyperCHIP-64|
|5XY3|LD VX, VY, [I]|Loads registers VX to VY from memory, starting at I.  I does not increment.|HyperCHIP-64|
|6XNN|LD VX, NN|Sets the VX register to NN|CHIP-8, SuperCHIP V1.0/V1.1, HyperCHIP-64|
|7XNN|ADD VX, NN|Add NN to the VX register.|CHIP-8, SuperCHIP V1.0/V1.1, HyperCHIP-64|
|8XY0|LD VX, VY|Sets the VX register to the VY register.|CHIP-8, SuperCHIP V1.0/V1.1, HyperCHIP-64|
|8XY1|OR VX, VY|Sets the VX register to VX OR VY.|CHIP-8, SuperCHIP V1.0/V1.1, HyperCHIP-64|
|8XY2|AND VX, VY|Sets the VX register to VX AND VY.|CHIP-8, SuperCHIP V1.0/V1.1, HyperCHIP-64|
|8XY3|XOR VX, VY|Sets the VX register to VX XOR VY.|CHIP-8, SuperCHIP V1.0/V1.1, HyperCHIP-64|
|8XY4|ADD VX, VY|Add the VY register to the VX register.  Sets the VF register to 01 if carried, otherwise 00.|CHIP-8, SuperCHIP V1.0/V1.1, HyperCHIP-64|
|8XY5|SUB VX, VY|Subtracts the VY register from the VX register and stores the result in the VX register.  Sets the VF register to 00 if borrowed, otherwise 01.|CHIP-8, SuperCHIP V1.0/V1.1, HyperCHIP-64|
|8XY6|SHR VX, VY|Stores the VY register shifted one bit to the right into the VX register.  Before the shift, the least significant bit is stored in the VF register.|CHIP-8, SuperCHIP V1.0/V1.1, HyperCHIP-64|
|8XY7|SUBN VX, VY|Subtracts the VX register from the VY register and stores the result in the VX register.  Sets the VF register to 00 if borrowed, otherwise 01.|CHIP-8, SuperCHIP V1.0/V1.1, HyperCHIP-64|
|8XY8|ROR VX, VY|Stores the VY register rotated one bit to the right into the VX register.|HyperCHIP-64|
|8XY9|ROL VX, VY|Stores the VY register rotated one bit to the left into the VX register.|HyperCHIP-64|
|8XYA|TEST VX, VY|Tests VX AND VY without storing the result.  Sets the VF register to 01 if the result is non-zero, otherwise 00.|HyperCHIP-64|
|8XYB|NOT VX, VY|Sets the VX register to NOT VY.|HyperCHIP-64|
|8XYE|SHL VX, VY|Stores the VY register shifted one bit to the left into the VX register.  Before the shift, the most significant bit is stored in the VF register.|CHIP-8, SuperCHIP V1.0/V1.1, HyperCHIP-64|
|9XY0|SNE VX, VY|Skips the following instruction if VX != VY|CHIP-8, SuperCHIP V1.0/V1.1, HyperCHIP-64|
|ANNN|LD I, NNN|Loads the address into the I register.|CHIP-8, SuperCHIP V1.0/V1.1, HyperCHIP-64|
|BNNN|JP V0, NNN|Jumps to the absolute address + V0 register.|CHIP-8, SuperCHIP V1.0/V1.1, HyperCHIP-64|
|CXNN|RND VX, NN|Generates a random number based on the NN mask and stores the result in the VX register.|CHIP-8, SuperCHIP V1.0/V1.1, HyperCHIP-64|
|DXYN|DRW VX, VY, N|Draws the sprite stored in the I register with the height of N (if N == 0, draws a 16x16 in SuperCHIP HiRes modes) located at (VX, VY).|CHIP-8, SuperCHIP V1.0/V1.1, HyperCHIP-64|
|EX9E|SKP VX|Skips the following instruction if the key stored in the VX register was pressed.|CHIP-8, SuperCHIP V1.0/V1.1, HyperCHIP-64|
|EXA1|SKNP VX|Skips the following instruction if the key stored in the VX register was not pressed.|CHIP-8, SuperCHIP V1.0/V1.1, HyperCHIP-64|
|FX07|LD VX, DT|Sets the VX register to the delay timer register.|CHIP-8, SuperCHIP V1.0/V1.1, HyperCHIP-64|
|FX0A|LD VX, K|Waits for a keypress and then stores it in the VX register.|CHIP-8, SuperCHIP V1.0/V1.1, HyperCHIP-64|
|FX15|LD DT, VX|Sets the delay timer register to the VX register.|CHIP-8, SuperCHIP V1.0/V1.1, HyperCHIP-64|
|FX18|LD ST, VX|Sets the sound timer register to the VX register.|CHIP-8, SuperCHIP V1.0/V1.1, HyperCHIP-64|
|FX1E|ADD I, VX|Add the VX register to the I register.|CHIP-8, SuperCHIP V1.0/V1.1/HyperCHIP-64|
|FX20|JP [I + VX]|Jumps indirectly to the address stored in memory at I + VX.|HyperCHIP-64|
|FX21|CALL [I + VX]|Calls the subroutine indirectly at the address stored in memory at I + VX.|HyperCHIP-64|
|FX29|LD F, VX|Sets the I register to the 4x5 font sprite digit stored in the VX register.|CHIP-8, SuperCHIP V1.0/V1.1, HyperCHIP-64|
|FX30|LD HF, VX|Sets the I register to the 8x10 font sprite digit stored in the VX register.|SuperCHIP V1.1, HyperCHIP-64|
|FX33|LD B, VX|Stores the value in the VX register in 3-digit unpacked BCD form at I, I+1, and I+2.|CHIP-8, SuperCHIP V1.0/V1.1, HyperCHIP-64|
|FX55|LD [I], VX|Stores registers V0 to VX in memory, starting at I.  The I register is incremented in this form 'I = I + X + 1'.|CHIP-8, SuperCHIP V1.0/V1.1, HyperCHIP-64|
|FX65|LD VX, [I]|Loads registers V0 to VX from memory, starting at I.  The I register is incremented in this form 'I = I + X + 1'.|CHIP-8, SuperCHIP V1.0/V1.1, HyperCHIP-64|
|FX75|LD R, VX|Stores registers V0 to VX in User RPL Flags. (X <= 7)|SuperCHIP V1.0/V1.1, HyperCHIP-64|
|FX85|LD VX, R|Loads registers V0 to VX from User RPL Flags. (X <= 7)|SuperCHIP V1.0/V1.1, HyperCHIP-64|
|FXA2|LD I, [I + VX]|Loads the address stored in memory at I + VX into the I register.|HyperCHIP-64|

## Instruction Prefixes (HyperCHIP-64 Extension)
|Prefix |Description |Affected Instructions |
|-------|------------|----------------------|
|FNB0|4-bit Absolute Address Extend Prefix that extends addresses.|JP NNNN; CALL NNNN; LD I, NNNN; JP V0, NNNN|
|FXB1|V Register Offset Override Prefix that replaces the default register.|JP VX, NNN|

## Supported Notations
|Notation |Description |
|---------|------------|
|0x00|Hexadecimal notation, which is supported in both instructions and certain keywords.|
|0b00000000|Binary notation, which is supported only in data keywords.|

## Keywords
|Keyword |Description |
|--------|------------|
|ORG|Sets the address at the current line of code.  Should not be less than 0x200 (reserved) and the current address.|
|INCBIN|Includes binary data from the specified file.  Must be a string and file must exist.|
|DB|Data byte, which can be used to specify byte data.  Commas are used to add additional data in a single line.  Strings in double quotes can be used to define data.|
|DW|Data word, which can be used to specify word data.  Commas are used to add additional data in a single line.  You can use labels as values as they're already word-sized.  It is in big-endian form.|

## Comment Support
Comments are supported by the use of semicolons.

## Label Support
This assembler has support for global labels.  Support for local labels may get added in the future.  Global labels are in the following form:
```
GlobalLabel:
```
Primary uses for labels is for various instructions that happen to support addresses.  HyperCHIP-64 extension can actually access labels outside the 4KB range and into the 64KB range.

Here's an example demonstrating the use of labels:
```
LD V0, 1
MainLoop:
	SKP V0 ; Skip the next instruction if the '1' key was pressed.
	JP MainLoop
	JP EndLoop
EndLoop:
	JP EndLoop
```
