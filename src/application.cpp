#include "../include/application.h"
#include <fstream>
#include <sstream>
#include <cstring>
#include <regex>

std::ostream &BandCHIP_Assembler::operator<<(std::ostream &out, const BandCHIP_Assembler::VersionData version)
{
	out << "V" << version.major << "." << version.minor;
	return out;
}

BandCHIP_Assembler::Application::Application(int argc, char *argv[]) : current_line_number(1), current_address(0x200), error_count(0), CurrentExtension(BandCHIP_Assembler::ExtensionType::CHIP8), align(true), retcode(0)
{
	std::cout << "BandCHIP Assembler " << Version << " - By Joshua Moss\n\n";
	if (argc > 1)
	{
		for (int i = 1; i < argc; ++i)
		{
			Args.push_back(argv[i]);
		}
		std::ifstream input_file(Args[0]);
		if (input_file.fail())
		{
			std::cout << "Unable to open '" << Args[0] << "'.\n\n";
			retcode = -1;
			return;
		}
		bool output_switch = false;
		std::ofstream output_file;
		for (auto &i : Args)
		{
			if (output_switch)
			{
				if (i == Args[0])
				{
					std::cout << "Do not specify the output file as the input file.\n\n";
					retcode = -1;
					return;
				}
				output_file.open(i, std::ios::binary);
				std::cout << "Attempting to assemble " << Args[0] << " to " << i << "...\n";
				break;
			}
			if (i == "-o")
			{
				output_switch = true;
			}
		}
		if (!output_switch)
		{
			std::cout << "You need to specify an output file.\n\n";
			retcode = -1;
			return;
		}
		while (!input_file.fail())
		{
			std::array<char, 4096> line_data;
			std::string token = "";
			std::string u_token = "";
			input_file.getline(line_data.data(), line_data.size(), '\n');
			size_t characters_read = input_file.gcount();
			bool error = false;
			bool comment = false;
			bool pointer_mode = false;
			ErrorType error_type = ErrorType::NoError;
			TokenType token_type = TokenType::None;
			OperandData current_operand = { OperandType::None, "" };
			InstructionData current_instruction = { InstructionType::None, {}, 0, 0 };
			auto OperandCountCheck = [&error, &error_type, &current_instruction]()
			{
				if (current_instruction.OperandList.size() > current_instruction.OperandMaximum)
				{
					error = true;
					error_type = ErrorType::TooManyOperands;
					return false;
				}
				else if (current_instruction.OperandList.size() < current_instruction.OperandMinimum)
				{
					error = true;
					error_type = ErrorType::TooFewOperands;
					return false;
				}
				return true;
			};
			auto ProcessLabelOperand = [this, &error, &error_type, &current_instruction](unsigned char operand, unsigned char opcode)
			{
				bool label_found = false;
				for (auto s : SymbolTable)
				{
					if (s.Type == SymbolType::Label)
					{
						if (current_instruction.OperandList[operand].Data == s.Name)
						{
							label_found = true;
							if (s.Location > 0xFFF)
							{
								if (CurrentExtension != ExtensionType::HyperCHIP64)
								{
									error = true;
									error_type = ErrorType::Only4KBSupported;
									return;
								}
								ProgramData.push_back(0xF0 | ((s.Location & 0xF000) >> 12));
								ProgramData.push_back(0xB0);
								current_address += 2;
							}
							ProgramData.push_back(((opcode & 0xF) << 4) | ((s.Location & 0xF00) >> 8));
							ProgramData.push_back(s.Location & 0xFF);
							current_address += 2;
							if (current_address > 0xFFF && CurrentExtension != ExtensionType::HyperCHIP64)
							{
								error = true;
								error_type = ErrorType::Only4KBSupported;
								return;
							}
							break;
						}
					}
				}
				if (!label_found)
				{
					UnresolvedReferenceList.push_back({ std::move(current_instruction.OperandList[operand].Data), current_line_number, static_cast<unsigned short>(current_address - 0x200), (CurrentExtension == ExtensionType::HyperCHIP64) ? true : false });
					if (CurrentExtension == ExtensionType::HyperCHIP64)
					{
						ProgramData.push_back(0xF0);
						ProgramData.push_back(0xB0);
						current_address += 2;
					}
					ProgramData.push_back((opcode & 0xF) << 4);
					ProgramData.push_back(0x00);
					current_address += 2;
					if (current_address> 0xFFF && CurrentExtension != ExtensionType::HyperCHIP64)
					{
						error = true;
						error_type = ErrorType::Only4KBSupported;
						return;
					}
				}
			};
			auto ProcessAddressImmediateValueOperand = [this, &error, &error_type, &current_instruction](unsigned char operand, unsigned char opcode)
			{
				std::regex hex("0x[a-fA-F0-9]{1,}");
				std::regex dec("[0-9]{1,}");
				std::smatch match;
				unsigned short address = 0;
				if (std::regex_search(current_instruction.OperandList[operand].Data, match, hex))
				{
					if (match.prefix().str().size() > 0 || match.suffix().str().size() > 0)
					{
						error = true;
						error_type = ErrorType::InvalidValue;
						return;
					}
					std::istringstream hex_str(match.str());
					hex_str >> std::hex >> address;
				}
				else if (std::regex_search(current_instruction.OperandList[operand].Data, match, dec))
				{
					if (match.prefix().str().size() > 0 || match.suffix().str().size() > 0)
					{
						error = true;
						error_type = ErrorType::InvalidValue;
						return;
					}
					std::istringstream dec_str(match.str());
					dec_str >> address;
				}
				if (address > 0xFFF)
				{
					if (CurrentExtension != ExtensionType::HyperCHIP64)
					{
						error = true;
						error_type = ErrorType::Only4KBSupported;
						return;
					}
					ProgramData.push_back(0xF0 | (address >> 12));
					ProgramData.push_back(0xB0);
					current_address += 2;
				}
				ProgramData.push_back(((opcode & 0xF) << 4) | ((address & 0xF00) >> 8));
				ProgramData.push_back(address & 0xFF);
				current_address += 2;
				if (current_address > 0xFFF && CurrentExtension != ExtensionType::HyperCHIP64)
				{
					error = true;
					error_type = ErrorType::Only4KBSupported;
					return;
				}
			};
			auto Process8BitImmediateValueOperand = [this, &error, &error_type, &current_instruction](unsigned char operand)
			{
				std::regex hex("0x[a-fA-F0-9]{1,}");
				std::regex dec("[0-9]{1,}");
				std::smatch match;
				unsigned short value = 0;
				if (std::regex_search(current_instruction.OperandList[operand].Data, match, hex))
				{
					if (match.prefix().str().size() > 0 || match.suffix().str().size() > 0)
					{
						error = true;
						error_type = ErrorType::InvalidValue;
						return static_cast<unsigned char>(0x00);
					}
					std::istringstream hex_str(match.str());
					hex_str >> std::hex >> value;
				}
				else if (std::regex_search(current_instruction.OperandList[operand].Data, match, dec))
				{
					if (match.prefix().str().size() > 0 || match.suffix().str().size() > 0)
					{
						error = true;
						error_type = ErrorType::InvalidValue;
						return static_cast<unsigned char>(0x00);
					}
					std::istringstream dec_str(match.str());
					dec_str >> value;
				}
				return static_cast<unsigned char>(value & 0xFF);
			};
			auto ProcessRegisterOperand = [this, &current_instruction](unsigned char operand, unsigned char &reg)
			{
				for (auto r : RegisterList)
				{
					if (r == current_instruction.OperandList[operand].Data)
					{
						return true;
					}
					if (reg < 0xF)
					{
						++reg;
					}
				}
				return false;
			};
			auto ProcessDataByte = [&token, &error, &error_type]()
			{
				std::regex hex("0x[a-fA-F0-9]{1,}");
				std::regex bin("0b[0-1]{1,8}");
				std::regex dec("[0-9]{1,}");
				std::smatch match;
				unsigned short value = 0;
				if (std::regex_search(token, match, hex))
				{
					if (match.prefix().str().size() > 0 || match.suffix().str().size() > 0)
					{
						error = true;
						error_type = ErrorType::InvalidValue;
						return static_cast<unsigned char>(0);
					}
					std::istringstream hex_str(match.str());
					hex_str >> std::hex >> value;
				}
				else if (std::regex_search(token, match, bin))
				{
					if (match.prefix().str().size() > 0 || match.suffix().str().size() > 0)
					{
						error = true;
						error_type = ErrorType::InvalidValue;
						return static_cast<unsigned char>(0);
					}
					for (size_t c = 0; c < match.str().size() - 2; ++c)
					{
						if (match.str()[2 + c] == '1')
						{
							value |= (0x80 >> (7 - ((match.str().size() - 3) - c)));
						}
					}
				}
				else if (std::regex_search(token, match, dec))
				{
					if (match.prefix().str().size() > 0 || match.suffix().str().size() > 0)
					{
						error = true;
						error_type = ErrorType::InvalidValue;
						return static_cast<unsigned char>(0);
					}
					std::istringstream dec_str(match.str());
					dec_str >> value;
				}
				return static_cast<unsigned char>(value & 0xFF);
			};
			auto ProcessDataWord = [&token, &error, &error_type]()
			{
				std::regex hex("0x[a-fA-F0-9]{1,}");
				std::regex bin("0b[0-1]{1,16}");
				std::regex dec("[0-9]{1,}");
				std::smatch match;
				unsigned short value = 0;
				if (std::regex_search(token, match, hex))
				{
					if (match.prefix().str().size() > 0 || match.suffix().str().size() > 0)
					{
						error = true;
						error_type = ErrorType::InvalidValue;
						return static_cast<unsigned short>(0);
					}
					std::istringstream hex_str(match.str());
					hex_str >> std::hex >> value;
				}
				else if (std::regex_search(token, match, bin))
				{
					if (match.prefix().str().size() > 0 || match.suffix().str().size() > 0)
					{
						error = true;
						error_type = ErrorType::InvalidValue;
						return static_cast<unsigned short>(0);
					}
					for (size_t c = 0; c < match.str().size() - 2; ++c)
					{
						if (match.str()[2 + c] == '1')
						{
							value |= (0x8000 >> (15 - ((match.str().size() - 3) - c)));
						}
					}
				}
				else if (std::regex_search(token, match, dec))
				{
					if (match.prefix().str().size() > 0 || match.suffix().str().size() > 0)
					{
						error = true;
						error_type = ErrorType::InvalidValue;
						return static_cast<unsigned short>(0);
					}
					std::istringstream dec_str(match.str());
					dec_str >> value;
				}
				return value;
			};
			for (size_t i = 0; i < characters_read; ++i)
			{
				switch (line_data[i])
				{
					case ';':
					{
						if (!pointer_mode)
						{
							comment = true;
						}
						else
						{
							error = true;
						}
						break;
					}
					case ' ':
					{
						if (token.size() > 0 && !pointer_mode)
						{
							switch (token_type)
							{
								case TokenType::None:
								{
									bool valid_token = false;
									for (size_t c = 0; c < token.size(); ++c)
									{
										u_token += toupper(static_cast<unsigned char>(token[c]));
									}
									for (auto t : TokenList)
									{
										if (u_token == t)
										{
											valid_token = true;
											if (t == "EXTENSION")
											{
												token_type = TokenType::Extension;
											}
											else if (t == "ALIGN")
											{
												token_type = TokenType::Align;
											}
											else if (t == "DB")
											{
												token_type = TokenType::DataByte;
											}
											else if (t == "DW")
											{
												token_type = TokenType::DataWord;
											}
											else if (t == "CLS")
											{
												token_type = TokenType::Instruction;
												current_instruction.Type = InstructionType::ClearScreen;
												current_instruction.OperandMinimum = current_instruction.OperandMaximum = 0;
											}
											else if (t == "RET")
											{
												token_type = TokenType::Instruction;
												current_instruction.Type = InstructionType::Return;
												current_instruction.OperandMinimum = current_instruction.OperandMaximum = 0;
											}
											else if (t == "JP")
											{
												token_type = TokenType::Instruction;
												current_instruction.Type = InstructionType::Jump;
												current_instruction.OperandMinimum = 1;
												current_instruction.OperandMaximum = 2;
											}
											else if (t == "CALL")
											{
												token_type = TokenType::Instruction;
												current_instruction.Type = InstructionType::Call;
												current_instruction.OperandMinimum = 1;
												current_instruction.OperandMaximum = 2;
											}
											else if (t == "SE")
											{
												token_type = TokenType::Instruction;
												current_instruction.Type = InstructionType::SkipEqual;
												current_instruction.OperandMinimum = current_instruction.OperandMaximum = 2;
											}
											else if (t == "SNE")
											{
												token_type = TokenType::Instruction;
												current_instruction.Type = InstructionType::SkipNotEqual;
												current_instruction.OperandMinimum = current_instruction.OperandMaximum = 2;
											}
											else if (t == "LD")
											{
												token_type = TokenType::Instruction;
												current_instruction.Type = InstructionType::Load;
												current_instruction.OperandMinimum = current_instruction.OperandMaximum = 2;
											}
											else if (t == "ADD")
											{
												token_type = TokenType::Instruction;
												current_instruction.Type = InstructionType::Add;
												current_instruction.OperandMinimum = current_instruction.OperandMaximum = 2;
											}
											else if (t == "OR")
											{
												token_type = TokenType::Instruction;
												current_instruction.Type = InstructionType::Or;
												current_instruction.OperandMinimum = current_instruction.OperandMaximum = 2;
											}
											else if (t == "AND")
											{
												token_type = TokenType::Instruction;
												current_instruction.Type = InstructionType::And;
												current_instruction.OperandMinimum = current_instruction.OperandMaximum = 2;
											}
											else if (t == "XOR")
											{
												token_type = TokenType::Instruction;
												current_instruction.Type = InstructionType::Xor;
												current_instruction.OperandMinimum = current_instruction.OperandMaximum = 2;
											}
											else if (t == "SUB")
											{
												token_type = TokenType::Instruction;
												current_instruction.Type = InstructionType::Subtract;
												current_instruction.OperandMinimum = current_instruction.OperandMaximum = 2;
											}
											else if (t == "SHR")
											{
												token_type = TokenType::Instruction;
												current_instruction.Type = InstructionType::ShiftRight;
												current_instruction.OperandMinimum = current_instruction.OperandMaximum = 2;
											}
											else if (t == "SUBN")
											{
												token_type = TokenType::Instruction;
												current_instruction.Type = InstructionType::SubtractN;
												current_instruction.OperandMinimum = current_instruction.OperandMaximum = 2;
											}
											else if (t == "SHL")
											{
												token_type = TokenType::Instruction;
												current_instruction.Type = InstructionType::ShiftLeft;
												current_instruction.OperandMinimum = current_instruction.OperandMaximum = 2;
											}
											else if (t == "RND")
											{
												token_type = TokenType::Instruction;
												current_instruction.Type = InstructionType::Random;
												current_instruction.OperandMinimum = current_instruction.OperandMaximum = 2;
											}
											else if (t == "DRW")
											{
												token_type = TokenType::Instruction;
												current_instruction.Type = InstructionType::Draw;
												current_instruction.OperandMinimum = current_instruction.OperandMaximum = 3;
											}
											else if (t == "SKP")
											{
												token_type = TokenType::Instruction;
												current_instruction.Type = InstructionType::SkipKeyPressed;
												current_instruction.OperandMinimum = current_instruction.OperandMaximum = 1;
											}
											else if (t == "SKNP")
											{
												token_type = TokenType::Instruction;
												current_instruction.Type = InstructionType::SkipKeyNotPressed;
												current_instruction.OperandMinimum = current_instruction.OperandMaximum = 1;
											}
											token = "";
											u_token = "";
										}
									}
									if (!valid_token)
									{
										error = true;
										error_type = ErrorType::InvalidToken;
									}
									break;
								}
							}
						}
						break;
					}
					case '[':
					{
						if (current_operand.Type == OperandType::None)
						{
							if (!pointer_mode)
							{
								current_operand.Type = OperandType::Pointer;
								pointer_mode = true;
							}
							else
							{
								error = true;
							}
						}
						else
						{
							error = true;
						}
						break;
					}
					case ']':
					{
						if (current_operand.Type == OperandType::Pointer)
						{
							if (pointer_mode)
							{
								pointer_mode = false;
							}
							else
							{
								error = true;
							}
						}
						else
						{
							error = true;
						}
						break;
					}
					case ',':
					{
						for (size_t c = 0; c < token.size(); ++c)
						{
							u_token += toupper(static_cast<unsigned char>(token[c]));
						}
						switch (token_type)
						{
							case TokenType::Instruction:
							{
								if (current_operand.Type != OperandType::ImmediateValue && current_operand.Type != OperandType::Pointer)
								{
									bool operand_type_found = false;
									for (auto r : RegisterList)
									{
										if (u_token == r)
										{
											operand_type_found = true;
											token = u_token;
											current_operand.Type = OperandType::Register;
											break;
										}
									}
									if (!operand_type_found)
									{
										if (u_token == "I")
										{
											current_operand.Type = OperandType::AddressRegister;
										}
										else if (u_token == "DT")
										{
											current_operand.Type = OperandType::DelayTimer;
										}
										else if (u_token == "ST")
										{
											current_operand.Type = OperandType::SoundTimer;
										}
										else if (u_token == "K")
										{
											current_operand.Type = OperandType::Key;
										}
										else if (u_token == "F")
										{
											current_operand.Type = OperandType::LoResFont;
										}
										else if (u_token == "HF")
										{
											current_operand.Type = OperandType::HiResFont;
										}
										else if (u_token == "B")
										{
											current_operand.Type = OperandType::BCD;
										}
										else if (u_token == "R")
										{
											current_operand.Type = OperandType::UserRPL;
										}
									}
								}
								current_operand.Data = std::move(token);
								current_instruction.OperandList.push_back(current_operand);
								current_operand = { OperandType::None, "" };
								u_token = "";
								break;
							}
							case TokenType::DataByte:
							{
								unsigned char value = ProcessDataByte();
								if (error)
								{
									break;
								}
								ProgramData.push_back(value);
								if (align && ProgramData.size() % 2 != 0)
								{
									ProgramData.push_back(0x00);
									current_address += 2;
								}
								else
								{
									++current_address;
								}
								if (current_address > 0xFFF && CurrentExtension != ExtensionType::HyperCHIP64)
								{
									error = true;
									error_type = ErrorType::Only4KBSupported;
									break;
								}
								token = "";
								u_token = "";
								break;
							}
							case TokenType::DataWord:
							{
								unsigned short value = ProcessDataWord();
								if (error)
								{
									break;
								}
								if (align && ProgramData.size() % 2 != 0)
								{
									ProgramData.push_back(0x00);
									++current_address;
									if (current_address > 0xFFF && CurrentExtension != ExtensionType::HyperCHIP64)
									{
										error = true;
										error_type = ErrorType::Only4KBSupported;
										break;
									}
								}
								ProgramData.push_back(static_cast<unsigned char>(value >> 8));
								ProgramData.push_back(static_cast<unsigned char>(value & 0xFF));
								current_address += 2;
								if (current_address > 0xFFF && CurrentExtension != ExtensionType::HyperCHIP64)
								{
									error = true;
									error_type = ErrorType::Only4KBSupported;
									break;
								}
								token = "";
								u_token = "";
								break;
							}
							default:
							{
								error = true;
								break;
							}
						}
						break;
					}
					case ':':
					{
						for (size_t c = 0; c < token.size(); ++c)
						{
							u_token += toupper(static_cast<unsigned char>(token[c]));
						}
						for (auto t : TokenList)
						{
							if (u_token == t)
							{
								error = true;
								error_type = ErrorType::ReservedToken;
								break;
							}
						}
						if (error)
						{
							break;
						}
						if (u_token == "I")
						{
							error = true;
							error_type = ErrorType::ReservedToken;
							break;
						}
						Symbol Label = { std::move(token), SymbolType::Label, current_address };
						SymbolTable.push_back(Label);
						token = "";
						break;
					}
					case '\0':
					{
						if (token.size() > 0)
						{
							bool valid_token = false;
							for (size_t c = 0; c < token.size(); ++c)
							{
								u_token += toupper(static_cast<unsigned char>(token[c]));
							}
							switch (token_type)
							{
								case TokenType::None:
								{
									for (auto t : TokenList)
									{
										if (u_token == t)
										{
											valid_token = true;
											if (t == "CLS")
											{
												token_type = TokenType::Instruction;
												ProgramData.push_back(0x00);
												ProgramData.push_back(0xE0);
												current_address += 2;
												if (current_address > 0xFFF && CurrentExtension != ExtensionType::HyperCHIP64)
												{
													error = true;
													error_type = ErrorType::Only4KBSupported;
												}
											}
											else if (t == "RET")
											{
												token_type = TokenType::Instruction;
												ProgramData.push_back(0x00);
												ProgramData.push_back(0xEE);
												current_address += 2;
												if (current_address > 0xFFF && CurrentExtension != ExtensionType::HyperCHIP64)
												{
													error = true;
													error_type = ErrorType::Only4KBSupported;
												}
											}
											else if (t == "JP")
											{
												current_instruction.Type = InstructionType::Jump;
												current_instruction.OperandMinimum = 1;
												current_instruction.OperandMaximum = 2;
												error = true;
												error_type = ErrorType::TooFewOperands;
											}
											else if (t == "CALL")
											{
												current_instruction.Type = InstructionType::Call;
												current_instruction.OperandMinimum = 1;
												current_instruction.OperandMaximum = 2;
												error = true;
												error_type = ErrorType::TooFewOperands;
											}
											else if (t == "SE")
											{
												current_instruction.Type = InstructionType::SkipEqual;
												current_instruction.OperandMinimum = current_instruction.OperandMaximum = 2;
												error = true;
												error_type = ErrorType::TooFewOperands;
											}
											else if (t == "SNE")
											{
												current_instruction.Type = InstructionType::SkipNotEqual;
												current_instruction.OperandMinimum = current_instruction.OperandMaximum = 2;
												error = true;
												error_type = ErrorType::TooFewOperands;
											}
											else if (t == "LD")
											{
												current_instruction.Type = InstructionType::Load;
												current_instruction.OperandMinimum = current_instruction.OperandMaximum = 2;
												error = true;
												error_type = ErrorType::TooFewOperands;
											}
											else if (t == "ADD")
											{
												current_instruction.Type = InstructionType::Add;
												current_instruction.OperandMinimum = current_instruction.OperandMaximum = 2;
												error = true;
												error_type = ErrorType::TooFewOperands;
											}
											else if (t == "OR")
											{
												current_instruction.Type = InstructionType::Or;
												current_instruction.OperandMinimum = current_instruction.OperandMaximum = 2;
												error = true;
												error_type = ErrorType::TooFewOperands;
											}
											else if (t == "AND")
											{
												current_instruction.Type = InstructionType::And;
												current_instruction.OperandMinimum = current_instruction.OperandMaximum = 2;
												error = true;
												error_type = ErrorType::TooFewOperands;
											}
											else if (t == "XOR")
											{
												current_instruction.Type = InstructionType::Xor;
												current_instruction.OperandMinimum = current_instruction.OperandMaximum = 2;
												error = true;
												error_type = ErrorType::TooFewOperands;
											}
											else if (t == "SUB")
											{
												current_instruction.Type = InstructionType::Subtract;
												current_instruction.OperandMinimum = current_instruction.OperandMaximum = 2;
												error = true;
												error_type = ErrorType::TooFewOperands;
											}
											else if (t == "SHR")
											{
												current_instruction.Type = InstructionType::ShiftRight;
												current_instruction.OperandMinimum = current_instruction.OperandMaximum = 2;
												error = true;
												error_type = ErrorType::TooFewOperands;
											}
											else if (t == "SUBN")
											{
												current_instruction.Type = InstructionType::SubtractN;
												current_instruction.OperandMinimum = current_instruction.OperandMaximum = 2;
												error = true;
												error_type = ErrorType::TooFewOperands;
											}
											else if (t == "SHL")
											{
												current_instruction.Type = InstructionType::ShiftLeft;
												current_instruction.OperandMinimum = current_instruction.OperandMaximum = 2;
												error = true;
												error_type = ErrorType::TooFewOperands;
											}
											else if (t == "RND")
											{
												current_instruction.Type = InstructionType::Random;
												current_instruction.OperandMinimum = current_instruction.OperandMaximum = 2;
												error = true;
												error_type = ErrorType::TooFewOperands;
											}
											else if (t == "DRW")
											{
												current_instruction.Type = InstructionType::Draw;
												current_instruction.OperandMinimum = current_instruction.OperandMaximum = 3;
												error = true;
												error_type = ErrorType::TooFewOperands;
											}
											else if (t == "SKP")
											{
												current_instruction.Type = InstructionType::SkipKeyPressed;
												current_instruction.OperandMinimum = current_instruction.OperandMaximum = 1;
												error = true;
												error_type = ErrorType::TooFewOperands;
											}
											else if (t == "SKNP")
											{
												current_instruction.Type = InstructionType::SkipKeyNotPressed;
												current_instruction.OperandMinimum = current_instruction.OperandMaximum = 1;
												error = true;
												error_type = ErrorType::TooFewOperands;
											}
											break;
										}
									}
									break;
								}
								case TokenType::Instruction:
								{
									valid_token = true;
									if (current_operand.Type != OperandType::ImmediateValue && current_operand.Type != OperandType::Pointer)
									{
										bool operand_type_found = false;
										for (auto r : RegisterList)
										{
											if (u_token == r)
											{
												operand_type_found = true;
												token = u_token;
												current_operand.Type = OperandType::Register;
												break;
											}
										}
										if (!operand_type_found)
										{
											if (u_token == "I")
											{
												current_operand.Type = OperandType::AddressRegister;
											}
											else if (u_token == "DT")
											{
												current_operand.Type = OperandType::DelayTimer;
											}
											else if (u_token == "ST")
											{
												current_operand.Type = OperandType::SoundTimer;
											}
											else if (u_token == "K")
											{
												current_operand.Type = OperandType::Key;
											}
											else if (u_token == "F")
											{
												current_operand.Type = OperandType::LoResFont;
											}
											else if (u_token == "HF")
											{
												current_operand.Type = OperandType::HiResFont;
											}
											else if (u_token == "B")
											{
												current_operand.Type = OperandType::BCD;
											}
											else if (u_token == "R")
											{
												current_operand.Type = OperandType::UserRPL;
											}
											else
											{
												/*
												std::regex ptr("\\[[a-zA-Z0-9\\+]{1,}\\]");
												std::smatch ptr_match;
												if (std::regex_search(token, ptr_match, ptr))
												{
													if (ptr_match.prefix().str().size() > 0 || ptr_match.suffix().str().size() > 0)
													{
														error = true;
														error_type = ErrorType::InvalidValue;
														break;
													}
													current_operand.Type = OperandType::Pointer;
													token = ptr_match.str();
													u_token = "";
													for (size_t c = 0; c < token.size(); ++c)
													{
														u_token += toupper(static_cast<unsigned char>(token[c]));
													}
												}
												else
												{
												*/
												current_operand.Type = OperandType::Label;
												// }
											}
										}
									}
									current_operand.Data = std::move(token);
									current_instruction.OperandList.push_back(current_operand);
									switch (current_instruction.Type)
									{
										case InstructionType::ClearScreen:
										{
											if (current_instruction.OperandList.size() > 0)
											{
												error = true;
												error_type = ErrorType::NoOperandsSupported;
												break;
											}
											ProgramData.push_back(0x00);
											ProgramData.push_back(0xE0);
											current_address += 2;
											if (current_address > 0xFFF && CurrentExtension != ExtensionType::HyperCHIP64)
											{
												error = true;
												error_type = ErrorType::Only4KBSupported;
												break;
											}
											break;
										}
										case InstructionType::Return:
										{
											if (current_instruction.OperandList.size() > 0)
											{
												error = true;
												error_type = ErrorType::NoOperandsSupported;
												break;
											}
											ProgramData.push_back(0x00);
											ProgramData.push_back(0xEE);
											current_address += 2;
											if (current_address > 0xFFF && CurrentExtension != ExtensionType::HyperCHIP64)
											{
												error = true;
												error_type = ErrorType::Only4KBSupported;
												break;
											}
											break;
										}
										case InstructionType::Jump:
										{
											if (!OperandCountCheck())
											{
												break;
											}
											switch (current_instruction.OperandList[0].Type)
											{
												case OperandType::None:
												{
													error = true;
													error_type = ErrorType::InvalidValue;
													break;
												}
												case OperandType::Label:
												{
													ProcessLabelOperand(0, 0x1);
													break;
												}
												case OperandType::Register:
												{
													unsigned char reg = 0x0;
													if (!ProcessRegisterOperand(0, reg))
													{
														error = true;
														error_type = ErrorType::InvalidRegister;
														break;
													}
													if (reg != 0x0 && CurrentExtension == ExtensionType::HyperCHIP64)
													{
														ProgramData.push_back(0xF0 | (reg & 0xF));
														ProgramData.push_back(0xB1);
														current_address += 2;
													}
													switch (current_instruction.OperandList[1].Type)
													{
														case OperandType::Label:
														{
															ProcessLabelOperand(1, 0xB);
															break;
														}
														case OperandType::ImmediateValue:
														{
															ProcessAddressImmediateValueOperand(1, 0xB);
															break;
														}
													}
													break;
												}
												case OperandType::ImmediateValue:
												{
													ProcessAddressImmediateValueOperand(0, 0x1);
													break;
												}
											}
											break;
										}
										case InstructionType::Call:
										{
											if (!OperandCountCheck())
											{
												break;
											}
											switch (current_instruction.OperandList[0].Type)
											{
												case OperandType::None:
												{
													error = true;
													error_type = ErrorType::InvalidValue;
													break;
												}
												case OperandType::Label:
												{
													ProcessLabelOperand(0, 0x2);
													break;
												}
												case OperandType::ImmediateValue:
												{
													ProcessAddressImmediateValueOperand(0, 0x2);
													break;
												}
											}
											break;
										}
										case InstructionType::SkipEqual:
										{
											if (!OperandCountCheck())
											{
												break;
											}
											switch (current_instruction.OperandList[0].Type)
											{
												case OperandType::None:
												{
													error = true;
													error_type = ErrorType::InvalidValue;
													break;
												}
												case OperandType::Register:
												{
													unsigned char reg1 = 0x0;
													if (!ProcessRegisterOperand(0, reg1))
													{
														error = true;
														error_type = ErrorType::InvalidRegister;
														break;
													}
													switch (current_instruction.OperandList[1].Type)
													{
														case OperandType::Register:
														{
															unsigned char reg2 = 0x0;
															if (!ProcessRegisterOperand(1, reg2))
															{
																error = true;
																error_type = ErrorType::InvalidRegister;
																break;
															}
															ProgramData.push_back(0x50 | (reg1 & 0xF));
															ProgramData.push_back(reg2 << 4);
															current_address += 2;
															if (current_address > 0xFFF && CurrentExtension != ExtensionType::HyperCHIP64)
															{
																error = true;
																error_type = ErrorType::Only4KBSupported;
																break;
															}
															break;
														}
														case OperandType::ImmediateValue:
														{
															unsigned char value = Process8BitImmediateValueOperand(1);
															if (error)
															{
																break;
															}
															ProgramData.push_back(0x30 | (reg1 & 0xF));
															ProgramData.push_back(value);
															current_address += 2;
															if (current_address > 0xFFF && CurrentExtension != ExtensionType::HyperCHIP64)
															{
																error = true;
																error_type = ErrorType::Only4KBSupported;
																break;
															}
															break;
														}
													}
													break;
												}
											}
											break;
										}
										case InstructionType::SkipNotEqual:
										{
											if (!OperandCountCheck())
											{
												break;
											}
											switch (current_instruction.OperandList[0].Type)
											{
												case OperandType::None:
												{
													error = true;
													error_type = ErrorType::InvalidValue;
													break;
												}
												case OperandType::Register:
												{
													unsigned char reg1 = 0x0;
													if (!ProcessRegisterOperand(0, reg1))
													{
														error = true;
														error_type = ErrorType::InvalidRegister;
														break;
													}
													switch (current_instruction.OperandList[1].Type)
													{
														case OperandType::Register:
														{
															unsigned char reg2 = 0x0;
															if (!ProcessRegisterOperand(0, reg2))
															{
																error = true;
																error_type = ErrorType::InvalidRegister;
																break;
															}
															ProgramData.push_back(0x90 | (reg1 & 0xF));
															ProgramData.push_back(reg2 << 4);
															current_address += 2;
															if (current_address > 0xFFF && CurrentExtension != ExtensionType::HyperCHIP64)
															{
																error = true;
																error_type = ErrorType::Only4KBSupported;
																break;
															}
															break;
														}
														case OperandType::ImmediateValue:
														{
															unsigned char value = Process8BitImmediateValueOperand(1);
															if (error)
															{
																break;
															}
															ProgramData.push_back(0x40 | (reg1 & 0xF));
															ProgramData.push_back(value);
															current_address += 2;
															if (current_address > 0xFFF && CurrentExtension != ExtensionType::HyperCHIP64)
															{
																error = true;
																error_type = ErrorType::Only4KBSupported;
																break;
															}
															break;
														}
													}
													break;
												}
											}
											break;
										}
										case InstructionType::Load:
										{
											if (!OperandCountCheck())
											{
												break;
											}
											switch (current_instruction.OperandList[0].Type)
											{
												case OperandType::None:
												{
													error = true;
													error_type = ErrorType::InvalidValue;
													break;
												}
												case OperandType::Register:
												{
													unsigned char reg1 = 0x0;
													if (!ProcessRegisterOperand(0, reg1))
													{
														error = true;
														error_type = ErrorType::InvalidRegister;
														break;
													}
													switch (current_instruction.OperandList[1].Type)
													{
														case OperandType::Register:
														{
															unsigned char reg2 = 0x0;
															if (!ProcessRegisterOperand(1, reg2))
															{
																error = true;
																error_type = ErrorType::InvalidRegister;
																break;
															}
															ProgramData.push_back(0x80 | (reg1 & 0xF));
															ProgramData.push_back(reg2 << 4);
															current_address += 2;
															if (current_address > 0xFFF && CurrentExtension != ExtensionType::HyperCHIP64)
															{
																error = true;
																error_type = ErrorType::Only4KBSupported;
																break;
															}
															break;
														}
														case OperandType::ImmediateValue:
														{
															unsigned char value = Process8BitImmediateValueOperand(1);
															if (error)
															{
																break;
															}
															ProgramData.push_back(0x60 | (reg1 & 0xF));
															ProgramData.push_back(value);
															current_address += 2;
															if (current_address > 0xFFF && CurrentExtension != ExtensionType::HyperCHIP64)
															{
																error = true;
																error_type = ErrorType::Only4KBSupported;
																break;
															}
															break;
														}
														case OperandType::DelayTimer:
														{
															ProgramData.push_back(0xF0 | (reg1 & 0xF));
															ProgramData.push_back(0x07);
															current_address += 2;
															if (current_address > 0xFFF && CurrentExtension != ExtensionType::HyperCHIP64)
															{
																error = true;
																error_type = ErrorType::Only4KBSupported;
																break;
															}
															break;
														}
														case OperandType::Pointer:
														{
															std::string uptr_data;
															for (size_t c = 0; c < current_instruction.OperandList[1].Data.size(); ++c)
															{
																uptr_data += toupper(static_cast<unsigned char>(current_instruction.OperandList[1].Data[c]));
															}
															if (uptr_data == "I")
															{
																ProgramData.push_back(0xF0 | (reg1 & 0xF));
																ProgramData.push_back(0x65);
																current_address += 2;
																if (current_address > 0xFFF && CurrentExtension != ExtensionType::HyperCHIP64)
																{
																	error = true;
																	error_type = ErrorType::Only4KBSupported;
																	break;
																}
															}
															break;
														}
														case OperandType::Key:
														{
															ProgramData.push_back(0xF0 | (reg1 & 0xF));
															ProgramData.push_back(0x0A);
															current_address += 2;
															if (current_address > 0xFFF && CurrentExtension != ExtensionType::HyperCHIP64)
															{
																error = true;
																error_type = ErrorType::Only4KBSupported;
																break;
															}
															break;
														}
													}
													break;
												}
												case OperandType::AddressRegister:
												{
													switch (current_instruction.OperandList[1].Type)
													{
														case OperandType::Label:
														{
															ProcessLabelOperand(1, 0xA);
															break;
														}
														case OperandType::ImmediateValue:
														{
															ProcessAddressImmediateValueOperand(1, 0xA);
															break;
														}
													}
													break;
												}
												case OperandType::DelayTimer:
												{
													if (current_instruction.OperandList[1].Type == OperandType::Register)
													{
														unsigned char reg = 0x0;
														if (!ProcessRegisterOperand(1, reg))
														{
															error = true;
															error_type = ErrorType::InvalidRegister;
															break;
														}
														ProgramData.push_back(0xF0 | (reg & 0xF));
														ProgramData.push_back(0x15);
														current_address += 2;
														if (current_address > 0xFFF && CurrentExtension != ExtensionType::HyperCHIP64)
														{
															error = true;
															error_type = ErrorType::Only4KBSupported;
															break;
														}
													}
													break;
												}
												case OperandType::SoundTimer:
												{
													if (current_instruction.OperandList[1].Type == OperandType::Register)
													{
														unsigned char reg = 0x0;
														if (!ProcessRegisterOperand(1, reg))
														{
															error = true;
															error_type = ErrorType::InvalidRegister;
															break;
														}
														ProgramData.push_back(0xF0 | (reg & 0xF));
														ProgramData.push_back(0x18);
														current_address += 2;
														if (current_address > 0xFFF && CurrentExtension != ExtensionType::HyperCHIP64)
														{
															error = true;
															error_type = ErrorType::Only4KBSupported;
															break;
														}
													}
													break;
												}
												case OperandType::Pointer:
												{
													std::string uptr_data;
													for (size_t c = 0; c < current_instruction.OperandList[0].Data.size(); ++c)
													{
														uptr_data += toupper(static_cast<unsigned char>(current_instruction.OperandList[0].Data[c]));
													}
													if (uptr_data == "I")
													{
														if (current_instruction.OperandList[1].Type == OperandType::Register)
														{
															unsigned char reg = 0x0;
															if (!ProcessRegisterOperand(1, reg))
															{
																error = true;
																error_type = ErrorType::InvalidRegister;
																break;
															}
															ProgramData.push_back(0xF0 | (reg & 0xF));
															ProgramData.push_back(0x55);
															current_address += 2;
															if (current_address > 0xFFF && CurrentExtension != ExtensionType::HyperCHIP64)
															{
																error = true;
																error_type = ErrorType::Only4KBSupported;
																break;
															}
														}
													}
													break;
												}
												case OperandType::LoResFont:
												{
													if (current_instruction.OperandList[1].Type == OperandType::Register)
													{
														unsigned char reg = 0x0;
														if (!ProcessRegisterOperand(1, reg))
														{
															error = true;
															error_type = ErrorType::InvalidRegister;
															break;
														}
														ProgramData.push_back(0xF0 | (reg & 0xF));
														ProgramData.push_back(0x29);
														current_address += 2;
														if (current_address > 0xFFF && CurrentExtension != ExtensionType::HyperCHIP64)
														{
															error = true;
															error_type = ErrorType::Only4KBSupported;
															break;
														}
													}
													break;
												}
												case OperandType::BCD:
												{
													if (current_instruction.OperandList[1].Type == OperandType::Register)
													{
														unsigned char reg = 0x0;
														if (!ProcessRegisterOperand(1, reg))
														{
															error = true;
															error_type = ErrorType::InvalidRegister;
															break;
														}
														ProgramData.push_back(0xF0 | (reg & 0xF));
														ProgramData.push_back(0x33);
														current_address += 2;
														if (current_address > 0xFFF && CurrentExtension != ExtensionType::HyperCHIP64)
														{
															error = true;
															error_type = ErrorType::Only4KBSupported;
															break;
														}
													}
													break;
												}
											}
											break;
										}
										case InstructionType::Add:
										{
											if (!OperandCountCheck())
											{
												break;
											}
											switch (current_instruction.OperandList[0].Type)
											{
												case OperandType::None:
												{
													error = true;
													error_type = ErrorType::InvalidValue;
													break;
												}
												case OperandType::Register:
												{
													unsigned char reg1 = 0x0;
													if (!ProcessRegisterOperand(0, reg1))
													{
														error = true;
														error_type = ErrorType::InvalidRegister;
														break;
													}
													switch (current_instruction.OperandList[1].Type)
													{
														case OperandType::Register:
														{
															unsigned char reg2 = 0x0;
															if (!ProcessRegisterOperand(1, reg2))
															{
																error = true;
																error_type = ErrorType::InvalidRegister;
																break;
															}
															ProgramData.push_back(0x80 | (reg1 & 0xF));
															ProgramData.push_back((reg2 << 4) | 0x4);
															current_address += 2;
															if (current_address > 0xFFF && CurrentExtension != ExtensionType::HyperCHIP64)
															{
																error = true;
																error_type = ErrorType::Only4KBSupported;
																break;
															}
															break;
														}
														case OperandType::ImmediateValue:
														{
															unsigned char value = Process8BitImmediateValueOperand(1);
															if (error)
															{
																break;
															}
															ProgramData.push_back(0x70 | (reg1 & 0xF));
															ProgramData.push_back(value);
															current_address += 2;
															if (current_address > 0xFFF && CurrentExtension != ExtensionType::HyperCHIP64)
															{
																error = true;
																error_type = ErrorType::Only4KBSupported;
																break;
															}
															break;
														}
													}
													break;
												}
												case OperandType::AddressRegister:
												{
													if (current_instruction.OperandList[1].Type == OperandType::Register)
													{
														unsigned char reg = 0x0;
														if (!ProcessRegisterOperand(1, reg))
														{
															error = true;
															error_type = ErrorType::InvalidRegister;
															break;
														}
														ProgramData.push_back(0xF0 | (reg & 0xF));
														ProgramData.push_back(0x1E);
														current_address += 2;
														if (current_address > 0xFFF && CurrentExtension != ExtensionType::HyperCHIP64)
														{
															error = true;
															error_type = ErrorType::Only4KBSupported;
															break;
														}
													}
													break;
												}
											}
											break;
										}
										case InstructionType::Or:
										{
											if (!OperandCountCheck())
											{
												break;
											}
											switch (current_instruction.OperandList[0].Type)
											{
												case OperandType::None:
												{
													error = true;
													error_type = ErrorType::InvalidValue;
													break;
												}
												case OperandType::Register:
												{
													unsigned char reg1 = 0x0;
													if (!ProcessRegisterOperand(0, reg1))
													{
														error = true;
														error_type = ErrorType::InvalidRegister;
														break;
													}
													if (current_instruction.OperandList[1].Type == OperandType::Register)
													{
														unsigned char reg2 = 0x0;
														if (!ProcessRegisterOperand(1, reg2))
														{
															error = true;
															error_type = ErrorType::InvalidRegister;
															break;
														}
														ProgramData.push_back(0x80 | (reg1 & 0xF));
														ProgramData.push_back((reg2 << 4) | 0x1);
														current_address += 2;
														if (current_address > 0xFFF && CurrentExtension != ExtensionType::HyperCHIP64)
														{
															error = true;
															error_type = ErrorType::Only4KBSupported;
															break;
														}
													}
													break;
												}
											}
											break;
										}
										case InstructionType::And:
										{
											if (!OperandCountCheck())
											{
												break;
											}
											switch (current_instruction.OperandList[0].Type)
											{
												case OperandType::None:
												{
													error = true;
													error_type = ErrorType::InvalidValue;
													break;
												}
												case OperandType::Register:
												{
													unsigned char reg1 = 0x0;
													if (!ProcessRegisterOperand(0, reg1))
													{
														error = true;
														error_type = ErrorType::InvalidRegister;
														break;
													}
													if (current_instruction.OperandList[1].Type == OperandType::Register)
													{
														unsigned char reg2 = 0x0;
														if (!ProcessRegisterOperand(1, reg2))
														{
															error = true;
															error_type = ErrorType::InvalidRegister;
															break;
														}
														ProgramData.push_back(0x80 | (reg1 & 0xF));
														ProgramData.push_back((reg2 << 4) | 0x2);
														current_address += 2;
														if (current_address > 0xFFF && CurrentExtension != ExtensionType::HyperCHIP64)
														{
															error = true;
															error_type = ErrorType::Only4KBSupported;
															break;
														}
													}
													break;
												}
											}
											break;
										}
										case InstructionType::Xor:
										{
											if (!OperandCountCheck())
											{
												break;
											}
											switch (current_instruction.OperandList[0].Type)
											{
												case OperandType::None:
												{
													error = true;
													error_type = ErrorType::InvalidValue;
													break;
												}
												case OperandType::Register:
												{
													unsigned char reg1 = 0x0;
													if (!ProcessRegisterOperand(0, reg1))
													{
														error = true;
														error_type = ErrorType::InvalidRegister;
														break;
													}
													if (current_instruction.OperandList[1].Type == OperandType::Register)
													{
														unsigned char reg2 = 0x0;
														if (!ProcessRegisterOperand(1, reg2))
														{
															error = true;
															error_type = ErrorType::InvalidRegister;
															break;
														}
														ProgramData.push_back(0x80 | (reg1 & 0xF));
														ProgramData.push_back((reg2 << 4) | 0x3);
														current_address += 2;
														if (current_address > 0xFFF && CurrentExtension != ExtensionType::HyperCHIP64)
														{
															error = true;
															error_type = ErrorType::Only4KBSupported;
															break;
														}
													}
													break;
												}
											}
											break;
										}
										case InstructionType::Subtract:
										{
											if (!OperandCountCheck())
											{
												break;
											}
											switch (current_instruction.OperandList[0].Type)
											{
												case OperandType::None:
												{
													error = true;
													error_type = ErrorType::InvalidValue;
													break;
												}
												case OperandType::Register:
												{
													unsigned char reg1 = 0x0;
													if (!ProcessRegisterOperand(0, reg1))
													{
														error = true;
														error_type = ErrorType::InvalidRegister;
														break;
													}
													if (current_instruction.OperandList[1].Type == OperandType::Register)
													{
														unsigned char reg2 = 0x0;
														if (!ProcessRegisterOperand(1, reg2))
														{
															error = true;
															error_type = ErrorType::InvalidRegister;
															break;
														}
														ProgramData.push_back(0x80 | (reg1 & 0xF));
														ProgramData.push_back((reg2 << 4) | 0x5);
														current_address += 2;
														if (current_address > 0xFFF && CurrentExtension != ExtensionType::HyperCHIP64)
														{
															error = true;
															error_type = ErrorType::Only4KBSupported;
															break;
														}
													}
													break;
												}
											}
											break;
										}
										case InstructionType::ShiftRight:
										{
											if (!OperandCountCheck())
											{
												break;
											}
											switch (current_instruction.OperandList[0].Type)
											{
												case OperandType::None:
												{
													error = true;
													error_type = ErrorType::InvalidValue;
													break;
												}
												case OperandType::Register:
												{
													unsigned char reg1 = 0x0;
													if (!ProcessRegisterOperand(0, reg1))
													{
														error = true;
														error_type = ErrorType::InvalidRegister;
														break;
													}
													if (current_instruction.OperandList[1].Type == OperandType::Register)
													{
														unsigned char reg2 = 0x0;
														if (!ProcessRegisterOperand(1, reg2))
														{
															error = true;
															error_type = ErrorType::InvalidValue;
															break;
														}
														ProgramData.push_back(0x80 | (reg1 & 0xF));
														ProgramData.push_back((reg2 << 4) | 0x6);
														current_address += 2;
														if (current_address > 0xFFF && CurrentExtension != ExtensionType::HyperCHIP64)
														{
															error = true;
															error_type = ErrorType::Only4KBSupported;
															break;
														}
													}
													break;
												}
											}
											break;
										}
										case InstructionType::SubtractN:
										{
											if (!OperandCountCheck())
											{
												break;
											}
											switch (current_instruction.OperandList[0].Type)
											{
												case OperandType::None:
												{
													error = true;
													error_type = ErrorType::InvalidValue;
													break;
												}
												case OperandType::Register:
												{
													unsigned char reg1 = 0x0;
													if (!ProcessRegisterOperand(0, reg1))
													{
														error = true;
														error_type = ErrorType::InvalidRegister;
														break;
													}
													if (current_instruction.OperandList[1].Type == OperandType::Register)
													{
														unsigned char reg2 = 0x0;
														if(!ProcessRegisterOperand(1, reg2))
														{
															error = true;
															error_type = ErrorType::InvalidRegister;
															break;
														}
														ProgramData.push_back(0x80 | (reg1 & 0xF));
														ProgramData.push_back((reg2 << 4) | 0x7);
														current_address += 2;
														if (current_address > 0xFFF && CurrentExtension != ExtensionType::HyperCHIP64)
														{
															error = true;
															error_type = ErrorType::InvalidRegister;
															break;
														}
													}
													break;
												}
											}
											break;
										}
										case InstructionType::ShiftLeft:
										{
											if (!OperandCountCheck())
											{
												break;
											}
											switch (current_instruction.OperandList[0].Type)
											{
												case OperandType::None:
												{
													error = true;
													error_type = ErrorType::InvalidValue;
													break;
												}
												case OperandType::Register:
												{
													unsigned char reg1 = 0x0;
													if (!ProcessRegisterOperand(0, reg1))
													{
														error = true;
														error_type = ErrorType::InvalidRegister;
														break;
													}
													if (current_instruction.OperandList[1].Type == OperandType::Register)
													{
														unsigned char reg2 = 0x0;
														if (!ProcessRegisterOperand(1, reg2))
														{
															error = true;
															error_type = ErrorType::InvalidRegister;
															break;
														}
														ProgramData.push_back(0x80 | (reg1 & 0xF));
														ProgramData.push_back((reg2 << 4) | 0xE);
														current_address += 2;
														if (current_address > 0xFFF && CurrentExtension != ExtensionType::HyperCHIP64)
														{
															error = true;
															error_type = ErrorType::Only4KBSupported;
															break;
														}
													}
													break;
												}
											}
											break;
										}
										case InstructionType::Random:
										{
											if (!OperandCountCheck())
											{
												break;
											}
											switch (current_instruction.OperandList[0].Type)
											{
												case OperandType::None:
												{
													error = true;
													error_type = ErrorType::InvalidValue;
													break;
												}
												case OperandType::Register:
												{
													unsigned char reg = 0x0;
													if (!ProcessRegisterOperand(0, reg))
													{
														error = true;
														error_type = ErrorType::InvalidRegister;
														break;
													}
													if (current_instruction.OperandList[1].Type == OperandType::ImmediateValue)
													{
														unsigned char value = Process8BitImmediateValueOperand(1);
														if (error)
														{
															break;
														}
														ProgramData.push_back(0xC0 | (reg & 0xF));
														ProgramData.push_back(value);
														current_address += 2;
														if (current_address > 0xFFF && CurrentExtension != ExtensionType::HyperCHIP64)
														{
															error = true;
															error_type = ErrorType::Only4KBSupported;
															break;
														}
													}
													break;
												}
											}
											break;
										}
										case InstructionType::Draw:
										{
											if (!OperandCountCheck())
											{
												break;
											}
											switch (current_instruction.OperandList[0].Type)
											{
												case OperandType::None:
												{
													error = true;
													error_type = ErrorType::InvalidValue;
													break;
												}
												case OperandType::Register:
												{
													unsigned char reg1 = 0x0;
													if (!ProcessRegisterOperand(0, reg1))
													{
														error = true;
														error_type = ErrorType::InvalidRegister;
														break;
													}
													if (current_instruction.OperandList[1].Type == OperandType::Register)
													{
														unsigned char reg2 = 0x0;
														if (!ProcessRegisterOperand(1, reg2))
														{
															error = true;
															error_type = ErrorType::InvalidRegister;
															break;
														}
														if (current_instruction.OperandList[2].Type == OperandType::ImmediateValue)
														{
															unsigned char height = Process8BitImmediateValueOperand(2);
															if (error)
															{
																break;
															}
															ProgramData.push_back(0xD0 | (reg1 & 0xF));
															ProgramData.push_back((reg2 << 4) | (height & 0xF));
															current_address += 2;
															if (current_address > 0xFFF && CurrentExtension != ExtensionType::HyperCHIP64)
															{
																error = true;
																error_type = ErrorType::Only4KBSupported;
															}
															break;
														}
													}
													break;
												}
											}
											break;
										}
										case InstructionType::SkipKeyPressed:
										{
											if (!OperandCountCheck())
											{
												break;
											}
											switch (current_instruction.OperandList[0].Type)
											{
												case OperandType::None:
												{
													error = true;
													error_type = ErrorType::InvalidValue;
													break;
												}
												case OperandType::Register:
												{
													unsigned char reg = 0x0;
													if (!ProcessRegisterOperand(0, reg))
													{
														break;
													}
													ProgramData.push_back(0xE0 | (reg & 0xF));
													ProgramData.push_back(0x9E);
													current_address += 2;
													if (current_address > 0xFFF && CurrentExtension != ExtensionType::HyperCHIP64)
													{
														error = true;
														error_type = ErrorType::Only4KBSupported;
														break;
													}
													break;
												}
											}
											break;
										}
										case InstructionType::SkipKeyNotPressed:
										{
											if (!OperandCountCheck())
											{
												break;
											}
											switch (current_instruction.OperandList[0].Type)
											{
												case OperandType::None:
												{
													error = true;
													error_type = ErrorType::InvalidValue;
													break;
												}
												case OperandType::Register:
												{
													unsigned char reg = 0x0;
													if (!ProcessRegisterOperand(0, reg))
													{
														error = true;
														error_type = ErrorType::InvalidRegister;
														break;
													}
													ProgramData.push_back(0xE0 | (reg & 0xF));
													ProgramData.push_back(0xA1);
													current_address += 2;
													if (current_address > 0xFFF && CurrentExtension != ExtensionType::HyperCHIP64)
													{
														error = true;
														error_type = ErrorType::Only4KBSupported;
														break;
													}
													break;
												}
											}
											break;
										}
									}
									break;
								}
								case TokenType::Extension:
								{
									for (auto e : ExtensionList)
									{
										if (u_token == e)
										{
											valid_token = true;
											if (e == "CHIP8")
											{
												CurrentExtension = ExtensionType::CHIP8;
												std::cout << "Using the original CHIP-8 instruction set.\n";
											}
											else if (e == "SCHIP10")
											{
												CurrentExtension = ExtensionType::SuperCHIP10;
												std::cout << "Using the SuperCHIP V1.0 extension.\n";
											}
											else if (e == "SCHIP11")
											{
												CurrentExtension = ExtensionType::SuperCHIP11;
												std::cout << "Using the SuperCHIP V1.1 extension.\n";
											}
											else if (e == "HCHIP64")
											{
												CurrentExtension = ExtensionType::HyperCHIP64;
												std::cout << "Using the HyperCHIP-64 extension.\n";
											}
											break;
										}
									}
									break;
								}
								case TokenType::Align:
								{
									for (auto a : ToggleList)
									{
										if (u_token == a)
										{
											valid_token = true;
											if (a == "OFF")
											{
												align = false;
											}
											else if (a == "ON")
											{
												align = true;
											}
											break;
										}
									}
									break;
								}
								case TokenType::DataByte:
								{
									valid_token = true;
									unsigned char value = ProcessDataByte();
									if (error)
									{
										break;
									}
									ProgramData.push_back(value);
									if (align && ProgramData.size() % 2 != 0)
									{
										ProgramData.push_back(0x00);
										current_address += 2;
									}
									else
									{
										++current_address;
									}
									if (current_address > 0xFFF && CurrentExtension != ExtensionType::HyperCHIP64)
									{
										error = true;
										error_type = ErrorType::Only4KBSupported;
										break;
									}
									break;
								}
								case TokenType::DataWord:
								{
									valid_token = true;
									unsigned short value = ProcessDataWord();
									if (error)
									{
										break;
									}
									if (align && ProgramData.size() % 2 != 0)
									{
										ProgramData.push_back(0x00);
										++current_address;
										if (current_address > 0xFFF && CurrentExtension != ExtensionType::HyperCHIP64)
										{
											error = true;
											error_type = ErrorType::Only4KBSupported;
											break;
										}
									}
									ProgramData.push_back(static_cast<unsigned char>(value >> 8));
									ProgramData.push_back(static_cast<unsigned char>(value & 0xFF));
									current_address += 2;
									if (current_address > 0xFFF && CurrentExtension != ExtensionType::HyperCHIP64)
									{
										error = true;
										error_type = ErrorType::Only4KBSupported;
									}
									break;
								}
							}
							if (error)
							{
								break;
							}
							if (!valid_token)
							{
								error = true;
								error_type = ErrorType::InvalidToken;
							}
						}
						break;
					}
					default:
					{
						if (token.size() == 0)
						{
							switch (token_type)
							{
								case TokenType::Instruction:
								{
									if (isdigit(static_cast<unsigned char>(line_data[i])))
									{
										current_operand.Type = OperandType::ImmediateValue;
									}
									else if (isspace(static_cast<unsigned char>(line_data[i])))
									{
										break;
									}
									token += line_data[i];
									break;
								}
								case TokenType::DataByte:
								case TokenType::DataWord:
								{
									if (isspace(static_cast<unsigned char>(line_data[i])))
									{
										break;
									}
									token += line_data[i];
									break;
								}
								default:
								{
									if (isdigit(static_cast<unsigned char>(line_data[i])))
									{
										error = true;
										break;
									}
									else if (isspace(static_cast<unsigned char>(line_data[i])))
									{
										break;
									}
									token += line_data[i];
									break;
								}
							}
						}
						else
						{
							if (!pointer_mode || (current_operand.Type == OperandType::Pointer && pointer_mode))
							{
								token += line_data[i];
							}
							else
							{
								error = true;
							}
						}
						break;
					}
				}
				if (error)
				{
					++error_count;
					std::cout << "Error at " << current_line_number << ':' << i - token.size() << " : ";
					switch (error_type)
					{
						case ErrorType::ReservedToken:
						{
							std::cout << "Reserved Token '" << u_token << "'\n";
							break;
						}
						case ErrorType::InvalidToken:
						{
							std::cout << "Invalid Token '" << token << "'\n";
							break;
						}
						case ErrorType::NoOperandsSupported:
						{
							switch (current_instruction.Type)
							{
								case InstructionType::ClearScreen:
								{
									std::cout << "CLS";
									break;
								}
								case InstructionType::Return:
								{
									std::cout << "RET";
									break;
								}
							}
							std::cout << " does not support operands.\n";
							break;
						}
						case ErrorType::TooFewOperands:
						{
							switch (current_instruction.Type)
							{
								case InstructionType::Jump:
								{
									std::cout << "JP";
									break;
								}
								case InstructionType::Call:
								{
									std::cout << "CALL";
									break;
								}
								case InstructionType::SkipEqual:
								{
									std::cout << "SE";
									break;
								}
								case InstructionType::SkipNotEqual:
								{
									std::cout << "SNE";
									break;
								}
								case InstructionType::Load:
								{
									std::cout << "LD";
									break;
								}
								case InstructionType::Add:
								{
									std::cout << "ADD";
									break;
								}
								case InstructionType::Or:
								{
									std::cout << "OR";
									break;
								}
								case InstructionType::And:
								{
									std::cout << "AND";
									break;
								}
								case InstructionType::Xor:
								{
									std::cout << "XOR";
									break;
								}
								case InstructionType::Subtract:
								{
									std::cout << "SUB";
									break;
								}
								case InstructionType::ShiftRight:
								{
									std::cout << "SHR";
									break;
								}
								case InstructionType::SubtractN:
								{
									std::cout << "SUBN";
									break;
								}
								case InstructionType::ShiftLeft:
								{
									std::cout << "SHL";
									break;
								}
								case InstructionType::Random:
								{
									std::cout << "RND";
									break;
								}
								case InstructionType::Draw:
								{
									std::cout << "DRW";
									break;
								}
								case InstructionType::SkipKeyPressed:
								{
									std::cout << "SKP";
									break;
								}
								case InstructionType::SkipKeyNotPressed:
								{
									std::cout << "SKNP";
									break;
								}
							}
							std::cout << " only has " << current_instruction.OperandList.size() << " operands (needs at least " << current_instruction.OperandMinimum << ").\n";
							/*
							switch (current_instruction.Type)
							{
								case InstructionType::Jump:
								case InstructionType::Call:
								{
									std::cout << "1";
									break;
								}
								case InstructionType::SkipEqual:
								case InstructionType::SkipNotEqual:
								{
									std::cout << "2";
									break;
								}
								case InstructionType::Load:
								{
									std::cout << "2";
									break;
								}
								case InstructionType::Add:
								case InstructionType::Or:
								case InstructionType::And:
								case InstructionType::Xor:
								case InstructionType::Subtract:
								case InstructionType::ShiftRight:
								case InstructionType::SubtractN:
								case InstructionType::ShiftLeft:
								{
									std::cout << "2";
									break;
								}
							}
							std::cout << ").\n";
							*/
							break;
						}
						case ErrorType::TooManyOperands:
						{
							switch (current_instruction.Type)
							{
								case InstructionType::Jump:
								{
									std::cout << "JP";
									break;
								}
								case InstructionType::Call:
								{
									std::cout << "CALL";
									break;
								}
								case InstructionType::SkipEqual:
								{
									std::cout << "SE";
									break;
								}
								case InstructionType::SkipNotEqual:
								{
									std::cout << "SNE";
									break;
								}
								case InstructionType::Load:
								{
									std::cout << "LD";
									break;
								}
								case InstructionType::Add:
								{
									std::cout << "ADD";
									break;
								}
								case InstructionType::Or:
								{
									std::cout << "OR";
									break;
								}
								case InstructionType::And:
								{
									std::cout << "AND";
									break;
								}
								case InstructionType::Xor:
								{
									std::cout << "XOR";
									break;
								}
								case InstructionType::Subtract:
								{
									std::cout << "SUB";
									break;
								}
								case InstructionType::ShiftRight:
								{
									std::cout << "SHR";
									break;
								}
								case InstructionType::SubtractN:
								{
									std::cout << "SUBN";
									break;
								}
								case InstructionType::ShiftLeft:
								{
									std::cout << "SHL";
									break;
								}
								case InstructionType::Random:
								{
									std::cout << "RND";
									break;
								}
								case InstructionType::Draw:
								{
									std::cout << "DRW";
									break;
								}
								case InstructionType::SkipKeyPressed:
								{
									std::cout << "SKP";
									break;
								}
								case InstructionType::SkipKeyNotPressed:
								{
									std::cout << "SKNP";
									break;
								}
							}
							std::cout << " has too many operands (" << current_instruction.OperandList.size() << ", supports up to " << current_instruction.OperandMaximum << ").\n";
							/*
							switch (current_instruction.Type)
							{
								case InstructionType::Jump:
								{
									std::cout << "2";
									break;
								}
								case InstructionType::Call:
								{
									std::cout << "1";
									break;
								}
								case InstructionType::SkipEqual:
								case InstructionType::SkipNotEqual:
								{
									std::cout << "2";
									break;
								}
								case InstructionType::Load:
								{
									std::cout << "2";
									break;
								}
								case InstructionType::Add:
								case InstructionType::Or:
								case InstructionType::And:
								case InstructionType::Xor:
								{
									std::cout << "2";
									break;
								}
							}
							std::cout << ").\n";
							*/
							break;
						}
						case ErrorType::InvalidValue:
						{
							std::cout << "Invalid Value\n";
							break;
						}
						case ErrorType::InvalidRegister:
						{
							std::cout << "Invalid Register\n";
							break;
						}
						case ErrorType::Only4KBSupported:
						{
							std::cout << "Current extension only supports up to 4KB (maxed at 0xFFF).\n";
							break;
						}
						default:
						{
							std::cout << "Unknown Error\n";
							break;
						}
					}
					break;
				}
				else if (comment)
				{
					break;
				}
			}
			++current_line_number;
		}
		for (auto u : UnresolvedReferenceList)
		{
			bool resolved = false;
			for (auto s : SymbolTable)
			{
				if (s.Type == SymbolType::Label)
				{
					if (u.Name == s.Name)
					{
						resolved = true;
						unsigned short offset = 0;
						if (u.AbsoluteAddressExtended)
						{
							ProgramData[u.Address] |= (s.Location >> 12);
							offset += 2;
						}
						ProgramData[u.Address + offset] |= ((s.Location & 0xF00) >> 8);
						ProgramData[u.Address + offset + 1] = (s.Location & 0xFF);
						break;
					}
				}
			}
			if (!resolved)
			{
				++error_count;
				std::cout << "Unresolved reference '" << u.Name << "' at line " << u.LineNumber << ".\n";
			}
		}
		if (error_count == 0)
		{
			output_file.write(reinterpret_cast<char *>(ProgramData.data()), ProgramData.size());
			std::cout << "Assembly successful!\n";
		}
		std::cout << '\n' << "There " << ((error_count != 1) ? "were " : "was ") << error_count << " error" << ((error_count != 1) ? "s.\n" : ".\n");
	}
	else
	{
		std::cout << "Format:  bandchip_assembler <input> -o <output>\n\n";
	}
}

BandCHIP_Assembler::Application::~Application()
{
}

int BandCHIP_Assembler::Application::GetReturnCode() const
{
	return retcode;
}
