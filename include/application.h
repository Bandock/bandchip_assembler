#ifndef _APPLICATION_H_
#define _APPLICATION_H_

#include <iostream>
#include <string>
#include <array>
#include <vector>

namespace BandCHIP_Assembler
{
	enum class OutputType { Binary, HexASCIIString };
	enum class ExtensionType { CHIP8, SuperCHIP10, SuperCHIP11, XOCHIP, HyperCHIP64 };
	enum class SymbolType { Label };
	enum class ErrorType { 
		NoError, ReservedToken, InvalidToken, NoOperandsSupported, TooFewOperands, TooManyOperands,
		InvalidValue, InvalidRegister, ReservedAddress, BelowCurrentAddress, Only4KBSupported,
		SuperCHIP10Required, SuperCHIP11Required, XOCHIPRequired, HyperCHIP64Required, BinaryFileDoesNotExist
       	};
	enum class TokenType { 
		None, Instruction, Output, Extension, Align, Origin, BinaryInclude, DataByte, DataWord
	};
	enum class InstructionType {
		None, ClearScreen, Return, Jump, Call, SkipEqual, SkipNotEqual, Load, Add, Or, And, Xor,
		Subtract, ShiftRight, SubtractN, ShiftLeft, Random, Draw, SkipKeyPressed, SkipKeyNotPressed,
		ScrollDown, ScrollRight, ScrollLeft, Exit, Low, High, ScrollUp, Plane, Audio, Pitch,
		RotateRight, RotateLeft, Test, Not
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
		bool IsInstruction;
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
			const std::array<std::string, 40> TokenList = {
				"OUTPUT", "EXTENSION", "ALIGN", "ORG", "INCBIN", "DB", "DW",
				"CLS", "RET", "JP", "CALL", "SE", "SNE", "LD", "ADD", "OR",
				"AND", "XOR", "SUB", "SHR", "SUBN", "SHL", "RND", "DRW", "SKP",
				"SKNP", "SCD", "SCR", "SCL", "EXIT", "LOW", "HIGH", "SCU",
				"PLANE", "AUDIO", "PITCH", "ROR", "ROL", "TEST", "NOT"
			};
			const std::array<std::string, 2> OutputTypeList = {
				"BINARY", "HEXASCIISTRING"
			};
			const std::array<std::string, 5> ExtensionList = {
				"CHIP8", "SCHIP10", "SCHIP11", "XOCHIP", "HCHIP64"
			};
			const std::array<std::string, 2> ToggleList = {
				"OFF", "ON"
			};
			const std::array<std::string, 16> RegisterList = {
				"V0", "V1", "V2", "V3", "V4", "V5", "V6", "V7",
				"V8", "V9", "VA", "VB", "VC", "VD", "VE", "VF"
			};
			OutputType CurrentOutputType;
			ExtensionType CurrentExtension;
			bool align;
			std::vector<Symbol> SymbolTable;
			std::vector<UnresolvedReferenceData> UnresolvedReferenceList;
			std::vector<unsigned char> ProgramData;
			const VersionData Version = { 0, 6 };
			int retcode;
	};
}

#endif
