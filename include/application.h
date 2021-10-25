#ifndef _APPLICATION_H_
#define _APPLICATION_H_

#include <iostream>
#include <string>
#include <array>
#include <vector>

namespace BandCHIP_Assembler
{
	enum class ExtensionType { CHIP8, SuperCHIP10, SuperCHIP11, HyperCHIP64 };
	enum class SymbolType { Label };
	enum class ErrorType { 
		NoError, ReservedToken, InvalidToken, NoOperandsSupported, TooFewOperands, TooManyOperands,
		InvalidValue, InvalidRegister, Only4KBSupported, SuperCHIP10Required, SuperCHIP11Required,
		HyperCHIP64Required
       	};
	enum class TokenType { None, Instruction, Extension, Align, DataByte, DataWord };
	enum class InstructionType {
		None, ClearScreen, Return, Jump, Call, SkipEqual, SkipNotEqual, Load, Add, Or, And, Xor,
		Subtract, ShiftRight, SubtractN, ShiftLeft, Random, Draw, SkipKeyPressed, SkipKeyNotPressed
	};
	enum class OperandType {
		None, Label, Register, ImmediateValue, AddressRegister, DelayTimer, SoundTimer, Pointer,
		Key, LoResFont, HiResFont, BCD, UserRPL
	};

	struct VersionData
	{
		unsigned short major;
		unsigned short minor;
		friend std::ostream &operator<<(std::ostream &out, const VersionData version);
	};

	std::ostream &operator<<(std::ostream &out, const VersionData version);

	struct Symbol
	{
		std::string Name;
		SymbolType Type;
		size_t Location;
	};

	struct OperandData
	{
		OperandType Type;
		std::string Data;
	};

	struct InstructionData
	{
		InstructionType Type;
		std::vector<OperandData> OperandList;
		size_t OperandMinimum;
		size_t OperandMaximum;
	};

	struct UnresolvedReferenceData
	{
		std::string Name;
		size_t LineNumber;
		unsigned short Address;
		bool AbsoluteAddressExtended;
	};

	class Application
	{
		public:
			Application(int argc, char *argv[]);
			~Application();
			int GetReturnCode() const;
		private:
			size_t current_line_number;
			unsigned short current_address;
			size_t error_count;
			std::vector<std::string> Args;
			const std::array<std::string, 23> TokenList = {
				"EXTENSION", "ALIGN", "DB", "DW", "CLS", "RET", "JP",
				"CALL", "SE", "SNE", "LD", "ADD", "OR", "AND", "XOR",
				"SUB", "SHR", "SUBN", "SHL", "RND", "DRW", "SKP", "SKNP"
			};
			const std::array<std::string, 4> ExtensionList = {
				"CHIP8", "SCHIP10", "SCHIP11", "HCHIP64"
			};
			const std::array<std::string, 2> ToggleList = {
				"OFF", "ON"
			};
			const std::array<std::string, 16> RegisterList = {
				"V0", "V1", "V2", "V3", "V4", "V5", "V6", "V7",
				"V8", "V9", "VA", "VB", "VC", "VD", "VE", "VF"
			};
			ExtensionType CurrentExtension;
			bool align;
			std::vector<Symbol> SymbolTable;
			std::vector<UnresolvedReferenceData> UnresolvedReferenceList;
			std::vector<unsigned char> ProgramData;
			const VersionData Version = { 0, 1 };
			int retcode;
	};
}

#endif
