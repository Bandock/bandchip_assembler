# BandCHIP Assembler Manual (Version 0.1)

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

## Instructions
|Opcode |Instruction |Description |Supported Extensions |
|-------|------------|------------|---------------------|
|00E0|CLS|Clears the screen.|CHIP-8, SuperCHIP V1.0/V1.1, HyperCHIP-64|
|00EE|RET|Returns from the subroutine.|CHIP-8, SuperCHIP V1.0/V1.1, HyperCHIP-64|
|1NNN|JP NNN|Jumps to the absolute address.|CHIP-8, SuperCHIP V1.0/V1.1, HyperCHIP-64|
|2NNN|CALL NNN|Calls the subroutine at that absolute address.|CHIP-8, SuperCHIP V1.0/V1.1, HyperCHIP-64|
|3XNN|SE VX,NN|Skips the following instruction if VX == NN|CHIP-8, SuperCHIP V1.0/V1.1, HyperCHIP-64|

## Instruction Prefixes (HyperCHIP-64 Extension)
|Prefix |Description |Affected Instructions |
|-------|------------|----------------------|
|FNB0|4-bit Absolute Address Extend Prefix that extends addresses.|JP NNN; CALL NNN;|
|FXB1|V Register Offset Override Prefix that replaces the default register.|JP V0,NNN|

Work In Progress
