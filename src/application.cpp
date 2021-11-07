#include "../include/application.h"
#include <iomanip>
#include <fstream>
#include <sstream>
#include <cstring>
#include <regex>

std::ostream &BandCHIP_Assembler::operator<<(std::ostream &out, const BandCHIP_Assembler::VersionData version)
{
	out << "V" << version.major << "." << version.minor;
	return out;
}

BandCHIP_Assembler::Application::Application(int argc, char *argv[]) : current_line_number(1), current_address(0x200), error_count(0), CurrentOutputType(BandCHIP_Assembler::OutputType::Binary), CurrentExtension(BandCHIP_Assembler::ExtensionType::CHIP8), align(true), retcode(0)
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
			bool string_mode = false;
			bool escape_mode = false;
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
								if (CurrentExtension == ExtensionType::XOCHIP && opcode == 0xA)
								{
									ProgramData.push_back(0xF0);
									ProgramData.push_back(0x00);
									ProgramData.push_back(s.Location >> 8);
									ProgramData.push_back(s.Location & 0xFF);
									current_address += 4;
									return;
								}
								else if (CurrentExtension != ExtensionType::HyperCHIP64)
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
							if (current_address > 0xFFF && CurrentExtension != ExtensionType::XOCHIP && CurrentExtension != ExtensionType::HyperCHIP64)
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
					UnresolvedReferenceList.push_back({ std::move(current_instruction.OperandList[operand].Data), current_line_number, static_cast<unsigned short>(current_address - 0x200), true, (CurrentExtension == ExtensionType::XOCHIP || CurrentExtension == ExtensionType::HyperCHIP64) ? true : false });
					if (CurrentExtension == ExtensionType::XOCHIP && opcode == 0xA)
					{
						ProgramData.push_back(0xF0);
						ProgramData.push_back(0x00);
						ProgramData.push_back(0x00);
						ProgramData.push_back(0x00);
						current_address += 4;
						return;
					}
					else if (CurrentExtension == ExtensionType::HyperCHIP64)
					{
						ProgramData.push_back(0xF0);
						ProgramData.push_back(0xB0);
						current_address += 2;
					}
					ProgramData.push_back((opcode & 0xF) << 4);
					ProgramData.push_back(0x00);
					current_address += 2;
					if (current_address > 0xFFF && CurrentExtension != ExtensionType::HyperCHIP64)
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
					if (CurrentExtension == ExtensionType::XOCHIP && opcode == 0xA)
					{
						ProgramData.push_back(0xF0);
						ProgramData.push_back(0x00);
						ProgramData.push_back(address >> 8);
						ProgramData.push_back(address & 0xFF);
						current_address += 2;
						return;
					}
					else if (CurrentExtension != ExtensionType::HyperCHIP64)
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
			auto ProcessAddressRegisterOffsetPointerOperand = [this, &current_instruction](unsigned char operand, unsigned char &reg)
			{
				std::string uptr_data = "";
				for (size_t c = 0; c < current_instruction.OperandList[operand].Data.size(); ++c)
				{
					uptr_data += toupper(static_cast<unsigned char>(current_instruction.OperandList[operand].Data[c]));
				}
				for (auto r : RegisterList)
				{
					std::string ptr_reg_offstr = "I+" + r;
					if (uptr_data == ptr_reg_offstr)
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
			auto ProcessOrigin = [&token, &error, &error_type]()
			{
				std::regex hex("0x[a-fA-F0-8]{1,}");
				std::regex dec("[0-9]{1,}");
				std::smatch match;
				unsigned short address = 0;
				if (std::regex_search(token, match, hex))
				{
					if (match.prefix().str().size() > 0 || match.suffix().str().size() > 0)
					{
						error = true;
						error_type = ErrorType::InvalidValue;
						return static_cast<unsigned short>(0);
					}
					std::istringstream hex_str(match.str());
					hex_str >> std::hex >> address;
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
					dec_str >> address;
				}
				return address;
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
			auto ProcessDataWord = [this, &token, &error, &error_type]()
			{
				unsigned short value = 0;
				if (isdigit(static_cast<unsigned char>(token[0])))
				{
					std::regex hex("0x[a-fA-F0-9]{1,}");
					std::regex bin("0b[0-1]{1,16}");
					std::regex dec("[0-9]{1,}");
					std::smatch match;
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
				}
				else
				{
					bool symbol_found = false;
					for (auto s : SymbolTable)
					{
						if (s.Type == SymbolType::Label)
						{
							if (token == s.Name)
							{
								symbol_found = true;
								if (s.Location > 0xFFF && CurrentExtension != ExtensionType::XOCHIP && CurrentExtension != ExtensionType::HyperCHIP64)
								{
									error = true;
									error_type = ErrorType::Only4KBSupported;
									return static_cast<unsigned short>(0);
								}
								value = s.Location;
								break;
							}
						}
					}
					if (!symbol_found)
					{
						UnresolvedReferenceList.push_back({ std::move(token), current_line_number, static_cast<unsigned short>((current_address + ((align && ProgramData.size() % 2 != 0) ? 1 : 0)) - 0x200), false, false });
					}
				}
				return value;
			};
			auto ProcessStringDataByte = [this, &error, &error_type](unsigned char data)
			{
				ProgramData.push_back(data);
				++current_address;
				if (current_address > 0xFFF && CurrentExtension != ExtensionType::XOCHIP && CurrentExtension != ExtensionType::HyperCHIP64)
				{
					error = true;
					error_type = ErrorType::Only4KBSupported;
				}
			};
			for (size_t i = 0; i < characters_read; ++i)
			{
				switch (line_data[i])
				{
					case '\\':
					{
						if (string_mode)
						{
							if (!escape_mode)
							{
								escape_mode = true;
							}
							else
							{
								switch (token_type)
								{
									case TokenType::Origin:
									{
										token += line_data[i];
									}
									case TokenType::DataByte:
									{
										ProcessStringDataByte(line_data[i]);
										break;
									}
								}
								escape_mode = false;
							}
						}
						break;
					}
					case ';':
					{
						if (!string_mode)
						{
							if (!comment)
							{
								if (!pointer_mode)
								{
									comment = true;
								}
								else
								{
									error = true;
								}
							}
						}
						else
						{
							switch (token_type)
							{
								case TokenType::DataByte:
								{
									ProcessStringDataByte(line_data[i]);
									break;
								}
							}
							if (escape_mode)
							{
								escape_mode = false;
							}
						}
						break;
					}
					case ' ':
					{
						if (!string_mode)
						{
							if (token.size() > 0 && !comment && !pointer_mode)
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
												if (t == "OUTPUT")
												{
													token_type = TokenType::Output;
												}
												if (t == "EXTENSION")
												{
													token_type = TokenType::Extension;
												}
												else if (t == "ALIGN")
												{
													token_type = TokenType::Align;
												}
												else if (t == "ORG")
												{
													token_type = TokenType::Origin;
												}
												else if (t == "INCBIN")
												{
													token_type = TokenType::BinaryInclude;
												}
												else if (t == "DB")
												{
													token_type = TokenType::DataByte;
												}
												else if (t == "DW")
												{
													token_type = TokenType::DataWord;
												}
												else if (t == "SCD")
												{
													token_type = TokenType::Instruction;
													current_instruction.Type = InstructionType::ScrollDown;
													current_instruction.OperandMinimum = current_instruction.OperandMaximum = 1;
													if (CurrentExtension != ExtensionType::SuperCHIP11 && CurrentExtension != ExtensionType::XOCHIP && CurrentExtension != ExtensionType::HyperCHIP64)
													{
														error = true;
														error_type = ErrorType::SuperCHIP11Required;
														break;
													}
												}
												else if (t == "SCU")
												{
													token_type = TokenType::Instruction;
													current_instruction.Type = InstructionType::ScrollUp;
													current_instruction.OperandMinimum = current_instruction.OperandMaximum = 1;
													if (CurrentExtension != ExtensionType::XOCHIP && CurrentExtension != ExtensionType::HyperCHIP64)
													{
														error = true;
														error_type = ErrorType::XOCHIPRequired;
														break;
													}
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
												else if (t == "SCR")
												{
													token_type = TokenType::Instruction;
													current_instruction.Type = InstructionType::ScrollRight;
													current_instruction.OperandMinimum = current_instruction.OperandMaximum = 0;
													if (CurrentExtension != ExtensionType::SuperCHIP11 && CurrentExtension != ExtensionType::XOCHIP && CurrentExtension != ExtensionType::HyperCHIP64)
													{
														error = true;
														error_type = ErrorType::SuperCHIP11Required;
														break;
													}
												}
												else if (t == "SCL")
												{
													token_type = TokenType::Instruction;
													current_instruction.Type = InstructionType::ScrollLeft;
													current_instruction.OperandMinimum = current_instruction.OperandMaximum = 0;
													if (CurrentExtension != ExtensionType::SuperCHIP11 && CurrentExtension != ExtensionType::XOCHIP && CurrentExtension != ExtensionType::HyperCHIP64)
													{
														error = true;
														error_type = ErrorType::SuperCHIP11Required;
														break;
													}
												}
												else if (t == "EXIT")
												{
													token_type = TokenType::Instruction;
													current_instruction.Type = InstructionType::Exit;
													current_instruction.OperandMinimum = current_instruction.OperandMaximum = 0;
													if (CurrentExtension != ExtensionType::SuperCHIP10 && CurrentExtension != ExtensionType::SuperCHIP11 && CurrentExtension != ExtensionType::XOCHIP && CurrentExtension != ExtensionType::HyperCHIP64)
													{
														error = true;
														error_type = ErrorType::SuperCHIP10Required;
														break;
													}
												}
												else if (t == "LOW")
												{
													token_type = TokenType::Instruction;
													current_instruction.Type = InstructionType::Low;
													current_instruction.OperandMinimum = current_instruction.OperandMaximum = 0;
													if (CurrentExtension != ExtensionType::SuperCHIP10 && CurrentExtension != ExtensionType::SuperCHIP11 && CurrentExtension != ExtensionType::XOCHIP && CurrentExtension != ExtensionType::HyperCHIP64)
													{
														error = true;
														error_type = ErrorType::SuperCHIP10Required;
														break;
													}
												}
												else if (t == "HIGH")
												{
													token_type = TokenType::Instruction;
													current_instruction.Type = InstructionType::High;
													current_instruction.OperandMinimum = current_instruction.OperandMaximum = 0;
													if (CurrentExtension != ExtensionType::SuperCHIP10 && CurrentExtension != ExtensionType::SuperCHIP11 && CurrentExtension != ExtensionType::XOCHIP && CurrentExtension != ExtensionType::HyperCHIP64)
													{
														error = true;
														error_type = ErrorType::SuperCHIP10Required;
														break;
													}
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
													current_instruction.OperandMinimum = 2;
													current_instruction.OperandMaximum = 3;
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
												else if (t == "ROR")
												{
													token_type = TokenType::Instruction;
													current_instruction.Type = InstructionType::RotateRight;
													current_instruction.OperandMinimum = current_instruction.OperandMaximum = 2;
													if (CurrentExtension != ExtensionType::HyperCHIP64)
													{
														error = true;
														error_type = ErrorType::HyperCHIP64Required;
														break;
													}
												}
												else if (t == "ROL")
												{
													token_type = TokenType::Instruction;
													current_instruction.Type = InstructionType::RotateLeft;
													current_instruction.OperandMinimum = current_instruction.OperandMaximum = 2;
													if (CurrentExtension != ExtensionType::HyperCHIP64)
													{
														error = true;
														error_type = ErrorType::HyperCHIP64Required;
														break;
													}
												}
												else if (t == "TEST")
												{
													token_type = TokenType::Instruction;
													current_instruction.Type = InstructionType::Test;
													current_instruction.OperandMinimum = current_instruction.OperandMaximum = 2;
													if (CurrentExtension != ExtensionType::HyperCHIP64)
													{
														error = true;
														error_type = ErrorType::HyperCHIP64Required;
														break;
													}
												}
												else if (t == "NOT")
												{
													token_type = TokenType::Instruction;
													current_instruction.Type = InstructionType::Not;
													current_instruction.OperandMinimum = current_instruction.OperandMaximum = 2;
													if (CurrentExtension != ExtensionType::HyperCHIP64)
													{
														error = true;
														error_type = ErrorType::HyperCHIP64Required;
														break;
													}
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
												else if (t == "PLANE")
												{
													token_type = TokenType::Instruction;
													current_instruction.Type = InstructionType::Plane;
													current_instruction.OperandMinimum = current_instruction.OperandMaximum = 1;
													if (CurrentExtension != ExtensionType::XOCHIP && CurrentExtension!= ExtensionType::HyperCHIP64)
													{
														error = true;
														error_type = ErrorType::XOCHIPRequired;
														break;
													}
												}
												else if (t == "AUDIO")
												{
													token_type = TokenType::Instruction;
													current_instruction.Type = InstructionType::Audio;
													current_instruction.OperandMinimum = current_instruction.OperandMaximum = 0;
													if (CurrentExtension != ExtensionType::XOCHIP && CurrentExtension != ExtensionType::HyperCHIP64)
													{
														error = true;
														error_type = ErrorType::XOCHIPRequired;
														break;
													}
												}
												else if (t == "PITCH")
												{
													token_type = TokenType::Instruction;
													current_instruction.Type = InstructionType::Pitch;
													current_instruction.OperandMinimum = current_instruction.OperandMaximum = 1;
													if (CurrentExtension != ExtensionType::XOCHIP && CurrentExtension != ExtensionType::HyperCHIP64)
													{
														error = true;
														error_type = ErrorType::XOCHIPRequired;
														break;
													}
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
						}
						else
						{
							switch (token_type)
							{
								case TokenType::BinaryInclude:
								{
									token += line_data[i];
								}
								case TokenType::DataByte:
								{
									ProcessStringDataByte(line_data[i]);
									break;
								}
							}
							if (escape_mode)
							{
								escape_mode = false;
							}
						}
						break;
					}
					case '"':
					{
						if (!comment)
						{
							if (!string_mode)
							{
								switch (token_type)
								{
									case TokenType::BinaryInclude:
									{
										if (token.size() > 0)
										{
											error = true;
											break;
										}
										string_mode = true;
										break;
									}
									case TokenType::DataByte:
									{
										string_mode = true;
										break;
									}
								}
							}
							else
							{
								if (!escape_mode)
								{
									string_mode = false;
								}
								else
								{
									switch (token_type)
									{
										case TokenType::DataByte:
										{
											ProcessStringDataByte(line_data[i]);
											break;
										}
									}
									escape_mode = false;
								}
							}
						}
						break;
					}
					case '[':
					{
						if (!comment)
						{
							if (!string_mode)
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
							}
							else
							{
								switch (token_type)
								{
									case TokenType::DataByte:
									{
										ProcessStringDataByte(line_data[i]);
										break;
									}
								}
								if (escape_mode)
								{
									escape_mode = false;
								}
							}
						}
						break;
					}
					case ']':
					{
						if (!comment)
						{
							if (!string_mode)
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
							}
							else
							{
								switch (token_type)
								{
									case TokenType::DataByte:
									{
										ProcessStringDataByte(line_data[i]);
										break;
									}
								}
								if (escape_mode)
								{
									escape_mode = false;
								}
							}
						}
						break;
					}
					case ',':
					{
						if (!comment)
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
								case TokenType::BinaryInclude:
								{
									if (!string_mode)
									{
										error = true;
										break;
									}
									break;
								}
								case TokenType::DataByte:
								{
									if (!string_mode)
									{
										if (token.size() > 0)
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
											if (current_address > 0xFFF && CurrentExtension != ExtensionType::XOCHIP && CurrentExtension != ExtensionType::HyperCHIP64)
											{
												error = true;
												error_type = ErrorType::Only4KBSupported;
												break;
											}
											token = "";
											u_token = "";
										}
									}
									else
									{
										ProcessStringDataByte(line_data[i]);
										if (escape_mode)
										{
											escape_mode = false;
										}
									}
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
										if (current_address > 0xFFF && CurrentExtension != ExtensionType::XOCHIP && CurrentExtension != ExtensionType::HyperCHIP64)
										{
											error = true;
											error_type = ErrorType::Only4KBSupported;
											break;
										}
									}
									ProgramData.push_back(static_cast<unsigned char>(value >> 8));
									ProgramData.push_back(static_cast<unsigned char>(value & 0xFF));
									current_address += 2;
									if (current_address > 0xFFF && CurrentExtension != ExtensionType::XOCHIP && CurrentExtension != ExtensionType::HyperCHIP64)
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
						}
						break;
					}
					case ':':
					{
						if (!comment)
						{
							if (!string_mode)
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
							}
							else
							{
								switch (token_type)
								{
									case TokenType::BinaryInclude:
									{
										token += line_data[i];
										break;
									}
									case TokenType::DataByte:
									{
										ProcessStringDataByte(line_data[i]);
										break;
									}
								}
								if (escape_mode)
								{
									escape_mode = false;
								}
							}
						}
						break;
					}
					case '\0':
					{
						if (string_mode)
						{
							error = true;
							break;
						}
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
											if (t == "SCD")
											{
												token_type = TokenType::Instruction;
												current_instruction.Type = InstructionType::ScrollDown;
												current_instruction.OperandMinimum = current_instruction.OperandMaximum = 1;
												error = true;
												error_type = (CurrentExtension != ExtensionType::SuperCHIP11 && CurrentExtension != ExtensionType::XOCHIP && CurrentExtension != ExtensionType::HyperCHIP64) ? ErrorType::SuperCHIP11Required : ErrorType::TooFewOperands;
											}
											else if (t == "SCU")
											{
												token_type = TokenType::Instruction;
												current_instruction.Type = InstructionType::ScrollUp;
												current_instruction.OperandMinimum = current_instruction.OperandMaximum = 1;
												error = true;
												error_type = (CurrentExtension != ExtensionType::XOCHIP && CurrentExtension != ExtensionType::HyperCHIP64) ? ErrorType::XOCHIPRequired : ErrorType::TooFewOperands;
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
											else if (t == "SCR")
											{
												token_type = TokenType::Instruction;
												current_instruction.Type = InstructionType::ScrollRight;
												current_instruction.OperandMinimum = current_instruction.OperandMaximum = 0;
												if (CurrentExtension != ExtensionType::SuperCHIP11 && CurrentExtension != ExtensionType::XOCHIP && CurrentExtension != ExtensionType::HyperCHIP64)
												{
													error = true;
													error_type = ErrorType::SuperCHIP11Required;
												}
											}
											else if (t == "SCL")
											{
												token_type = TokenType::Instruction;
												current_instruction.Type = InstructionType::ScrollLeft;
												current_instruction.OperandMinimum = current_instruction.OperandMaximum = 0;
												if (CurrentExtension != ExtensionType::SuperCHIP11 && CurrentExtension != ExtensionType::XOCHIP && CurrentExtension != ExtensionType::HyperCHIP64)
												{
													error = true;
													error_type = ErrorType::SuperCHIP11Required;
												}
											}
											else if (t == "EXIT")
											{
												token_type = TokenType::Instruction;
												current_instruction.Type = InstructionType::Exit;
												current_instruction.OperandMinimum = current_instruction.OperandMaximum = 0;
												if (CurrentExtension != ExtensionType::SuperCHIP10 && CurrentExtension != ExtensionType::SuperCHIP11 && CurrentExtension != ExtensionType::XOCHIP && CurrentExtension != ExtensionType::HyperCHIP64)
												{
													error = true;
													error_type = ErrorType::SuperCHIP10Required;
												}
											}
											else if (t == "LOW")
											{
												token_type = TokenType::Instruction;
												current_instruction.Type = InstructionType::Low;
												current_instruction.OperandMinimum = current_instruction.OperandMaximum = 0;
												if (CurrentExtension != ExtensionType::SuperCHIP10 && CurrentExtension != ExtensionType::SuperCHIP11 && CurrentExtension != ExtensionType::XOCHIP && CurrentExtension != ExtensionType::HyperCHIP64)
												{
													error = true;
													error_type = ErrorType::SuperCHIP10Required;
												}
											}
											else if (t == "HIGH")
											{
												token_type = TokenType::Instruction;
												current_instruction.Type = InstructionType::High;
												current_instruction.OperandMinimum = current_instruction.OperandMaximum = 0;
												if (CurrentExtension != ExtensionType::SuperCHIP10 && CurrentExtension != ExtensionType::SuperCHIP11 && CurrentExtension != ExtensionType::XOCHIP && CurrentExtension != ExtensionType::HyperCHIP64)
												{
													error = true;
													error_type = ErrorType::SuperCHIP10Required;
												}
											}
											else if (t == "JP")
											{
												token_type = TokenType::Instruction;
												current_instruction.Type = InstructionType::Jump;
												current_instruction.OperandMinimum = 1;
												current_instruction.OperandMaximum = 2;
												error = true;
												error_type = ErrorType::TooFewOperands;
											}
											else if (t == "CALL")
											{
												token_type = TokenType::Instruction;
												current_instruction.Type = InstructionType::Call;
												current_instruction.OperandMinimum = 1;
												current_instruction.OperandMaximum = 2;
												error = true;
												error_type = ErrorType::TooFewOperands;
											}
											else if (t == "SE")
											{
												token_type = TokenType::Instruction;
												current_instruction.Type = InstructionType::SkipEqual;
												current_instruction.OperandMinimum = current_instruction.OperandMaximum = 2;
												error = true;
												error_type = ErrorType::TooFewOperands;
											}
											else if (t == "SNE")
											{
												token_type = TokenType::Instruction;
												current_instruction.Type = InstructionType::SkipNotEqual;
												current_instruction.OperandMinimum = current_instruction.OperandMaximum = 2;
												error = true;
												error_type = ErrorType::TooFewOperands;
											}
											else if (t == "LD")
											{
												token_type = TokenType::Instruction;
												current_instruction.Type = InstructionType::Load;
												current_instruction.OperandMinimum = 2;
												current_instruction.OperandMaximum = 3;
												error = true;
												error_type = ErrorType::TooFewOperands;
											}
											else if (t == "ADD")
											{
												token_type = TokenType::Instruction;
												current_instruction.Type = InstructionType::Add;
												current_instruction.OperandMinimum = current_instruction.OperandMaximum = 2;
												error = true;
												error_type = ErrorType::TooFewOperands;
											}
											else if (t == "OR")
											{
												token_type = TokenType::Instruction;
												current_instruction.Type = InstructionType::Or;
												current_instruction.OperandMinimum = current_instruction.OperandMaximum = 2;
												error = true;
												error_type = ErrorType::TooFewOperands;
											}
											else if (t == "AND")
											{
												token_type = TokenType::Instruction;
												current_instruction.Type = InstructionType::And;
												current_instruction.OperandMinimum = current_instruction.OperandMaximum = 2;
												error = true;
												error_type = ErrorType::TooFewOperands;
											}
											else if (t == "XOR")
											{
												token_type = TokenType::Instruction;
												current_instruction.Type = InstructionType::Xor;
												current_instruction.OperandMinimum = current_instruction.OperandMaximum = 2;
												error = true;
												error_type = ErrorType::TooFewOperands;
											}
											else if (t == "SUB")
											{
												token_type = TokenType::Instruction;
												current_instruction.Type = InstructionType::Subtract;
												current_instruction.OperandMinimum = current_instruction.OperandMaximum = 2;
												error = true;
												error_type = ErrorType::TooFewOperands;
											}
											else if (t == "SHR")
											{
												token_type = TokenType::Instruction;
												current_instruction.Type = InstructionType::ShiftRight;
												current_instruction.OperandMinimum = current_instruction.OperandMaximum = 2;
												error = true;
												error_type = ErrorType::TooFewOperands;
											}
											else if (t == "SUBN")
											{
												token_type = TokenType::Instruction;
												current_instruction.Type = InstructionType::SubtractN;
												current_instruction.OperandMinimum = current_instruction.OperandMaximum = 2;
												error = true;
												error_type = ErrorType::TooFewOperands;
											}
											else if (t == "ROR")
											{
												token_type = TokenType::Instruction;
												current_instruction.Type = InstructionType::RotateRight;
												current_instruction.OperandMinimum = current_instruction.OperandMaximum = 2;
												error = true;
												error_type = (CurrentExtension != ExtensionType::HyperCHIP64) ? ErrorType::HyperCHIP64Required : ErrorType::TooFewOperands;
											}
											else if (t == "ROL")
											{
												token_type = TokenType::Instruction;
												current_instruction.Type = InstructionType::RotateLeft;
												current_instruction.OperandMinimum = current_instruction.OperandMaximum = 2;
												error = true;
												error_type = (CurrentExtension != ExtensionType::HyperCHIP64) ? ErrorType::HyperCHIP64Required : ErrorType::TooFewOperands;
											}
											else if (t == "TEST")
											{
												token_type = TokenType::Instruction;
												current_instruction.Type = InstructionType::Test;
												current_instruction.OperandMinimum = current_instruction.OperandMaximum = 2;
												error = true;
												error_type = (CurrentExtension != ExtensionType::HyperCHIP64) ? ErrorType::HyperCHIP64Required : ErrorType::TooFewOperands;
											}
											else if (t == "NOT")
											{
												token_type = TokenType::Instruction;
												current_instruction.Type = InstructionType::Not;
												current_instruction.OperandMinimum = current_instruction.OperandMaximum = 2;
												error = true;
												error_type = (CurrentExtension != ExtensionType::HyperCHIP64) ? ErrorType::HyperCHIP64Required : ErrorType::TooFewOperands;
											}
											else if (t == "SHL")
											{
												token_type = TokenType::Instruction;
												current_instruction.Type = InstructionType::ShiftLeft;
												current_instruction.OperandMinimum = current_instruction.OperandMaximum = 2;
												error = true;
												error_type = ErrorType::TooFewOperands;
											}
											else if (t == "RND")
											{
												token_type = TokenType::Instruction;
												current_instruction.Type = InstructionType::Random;
												current_instruction.OperandMinimum = current_instruction.OperandMaximum = 2;
												error = true;
												error_type = ErrorType::TooFewOperands;
											}
											else if (t == "DRW")
											{
												token_type = TokenType::Instruction;
												current_instruction.Type = InstructionType::Draw;
												current_instruction.OperandMinimum = current_instruction.OperandMaximum = 3;
												error = true;
												error_type = ErrorType::TooFewOperands;
											}
											else if (t == "SKP")
											{
												token_type = TokenType::Instruction;
												current_instruction.Type = InstructionType::SkipKeyPressed;
												current_instruction.OperandMinimum = current_instruction.OperandMaximum = 1;
												error = true;
												error_type = ErrorType::TooFewOperands;
											}
											else if (t == "SKNP")
											{
												token_type = TokenType::Instruction;
												current_instruction.Type = InstructionType::SkipKeyNotPressed;
												current_instruction.OperandMinimum = current_instruction.OperandMaximum = 1;
												error = true;
												error_type = ErrorType::TooFewOperands;
											}
											else if (t == "PLANE")
											{
												token_type = TokenType::Instruction;
												current_instruction.Type = InstructionType::Plane;
												current_instruction.OperandMinimum = current_instruction.OperandMaximum = 1;
												error = true;
												error_type = (CurrentExtension != ExtensionType::XOCHIP && CurrentExtension != ExtensionType::HyperCHIP64) ? ErrorType::XOCHIPRequired : ErrorType::TooFewOperands;
											}
											else if (t == "AUDIO")
											{
												token_type = TokenType::Instruction;
												current_instruction.Type = InstructionType::Audio;
												current_instruction.OperandMinimum = current_instruction.OperandMaximum = 0;
												if (CurrentExtension != ExtensionType::XOCHIP && CurrentExtension != ExtensionType::HyperCHIP64)
												{
													error = true;
													error_type = ErrorType::XOCHIPRequired;
												}
											}
											else if (t == "PITCH")
											{
												token_type = TokenType::Instruction;
												current_instruction.Type = InstructionType::Pitch;
												current_instruction.OperandMinimum = current_instruction.OperandMaximum = 1;
												error = true;
												error_type = (CurrentExtension != ExtensionType::XOCHIP && CurrentExtension != ExtensionType::HyperCHIP64) ? ErrorType::XOCHIPRequired : ErrorType::TooFewOperands;
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
												current_operand.Type = OperandType::Label;
											}
										}
									}
									current_operand.Data = std::move(token);
									current_instruction.OperandList.push_back(current_operand);
									break;
								}
								case TokenType::Output:
								{
									for (auto o : OutputTypeList)
									{
										if (u_token == o)
										{
											valid_token = true;
											if (o == "BINARY")
											{
												CurrentOutputType = OutputType::Binary;
												std::cout << "Using binary output mode.\n";
											}
											else if (o == "HEXASCIISTRING")
											{
												CurrentOutputType = OutputType::HexASCIIString;
												std::cout << "Using Hex ASCII String output mode.\n";
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
											else if (e == "XOCHIP")
											{
												CurrentExtension = ExtensionType::XOCHIP;
												std::cout << "Using the XO-CHIP extension.\n";
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
								case TokenType::Origin:
								{
									valid_token = true;
									unsigned short address = ProcessOrigin();
									if (error)
									{
										break;
									}
									if (address < 0x200)
									{
										error = true;
										error_type = ErrorType::ReservedAddress;
										break;
									}
									if (address < current_address)
									{
										error = true;
										error_type = ErrorType::BelowCurrentAddress;
										break;
									}
									current_address = address;
									for (size_t a = ProgramData.size(); a < current_address - 0x200; ++a)
									{
										ProgramData.push_back(0x00);
									}
									if (current_address > 0xFFF && CurrentExtension != ExtensionType::XOCHIP && CurrentExtension != ExtensionType::HyperCHIP64)
									{
										error = true;
										error_type = ErrorType::Only4KBSupported;
										break;
									}
									break;
								}
								case TokenType::BinaryInclude:
								{
									valid_token = true;
									std::ifstream binary_file(token, std::ios::binary);
									if (binary_file.fail())
									{
										error = true;
										error_type = ErrorType::BinaryFileDoesNotExist;
										break;
									}
									binary_file.seekg(0, std::ios::end);
									size_t file_size = binary_file.tellg();
									binary_file.seekg(0, std::ios::beg);
									for (size_t c = 0; c < file_size; ++c)
									{
										ProgramData.push_back(binary_file.get());
										++current_address;
										if (current_address > 0xFFF && CurrentExtension != ExtensionType::XOCHIP && CurrentExtension != ExtensionType::HyperCHIP64)
										{
											error = true;
											error_type = ErrorType::Only4KBSupported;
											break;
										}
									}
									break;
								}
								case TokenType::DataByte:
								{
									valid_token = true;
									if (token.size() > 0)
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
										if (current_address > 0xFFF && CurrentExtension != ExtensionType::XOCHIP && CurrentExtension != ExtensionType::HyperCHIP64)
										{
											error = true;
											error_type = ErrorType::Only4KBSupported;
											break;
										}
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
										if (current_address > 0xFFF && CurrentExtension != ExtensionType::XOCHIP && CurrentExtension != ExtensionType::HyperCHIP64)
										{
											error = true;
											error_type = ErrorType::Only4KBSupported;
											break;
										}
									}
									ProgramData.push_back(static_cast<unsigned char>(value >> 8));
									ProgramData.push_back(static_cast<unsigned char>(value & 0xFF));
									current_address += 2;
									if (current_address > 0xFFF && CurrentExtension != ExtensionType::XOCHIP && CurrentExtension != ExtensionType::HyperCHIP64)
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
								break;
							}
						}
						if (token_type == TokenType::Instruction)
						{
							switch (current_instruction.Type)
							{
								case InstructionType::ScrollDown:
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
										case OperandType::ImmediateValue:
										{
											unsigned char value = Process8BitImmediateValueOperand(0);
											if (error)
											{
												break;
											}
											ProgramData.push_back(0x00);
											ProgramData.push_back(0xC0 | (value & 0xF));
											current_address += 2;
											if (current_address > 0xFFF && CurrentExtension != ExtensionType::XOCHIP && CurrentExtension != ExtensionType::HyperCHIP64)
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
								case InstructionType::ScrollUp:
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
										case OperandType::ImmediateValue:
										{
											unsigned char value = Process8BitImmediateValueOperand(0);
											if (error)
											{
												break;
											}
											ProgramData.push_back(0x00);
											ProgramData.push_back(0xD0 | (value & 0xF));
											current_address += 2;
											break;
										}
									}
									break;
								}
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
									if (current_address > 0xFFF && CurrentExtension != ExtensionType::XOCHIP && CurrentExtension != ExtensionType::HyperCHIP64)
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
									if (current_address > 0xFFF && CurrentExtension != ExtensionType::XOCHIP && CurrentExtension != ExtensionType::HyperCHIP64)
									{
										error = true;
										error_type = ErrorType::Only4KBSupported;
										break;
									}
									break;
								}
								case InstructionType::ScrollRight:
								{
									if (current_instruction.OperandList.size() > 0)
									{
										error = true;
										error_type = ErrorType::NoOperandsSupported;
										break;
									}
									ProgramData.push_back(0x00);
									ProgramData.push_back(0xFB);
									current_address += 2;
									if (current_address > 0xFFF && CurrentExtension != ExtensionType::XOCHIP && CurrentExtension != ExtensionType::HyperCHIP64)
									{
										error = true;
										error_type = ErrorType::Only4KBSupported;
										break;
									}
									break;
								}
								case InstructionType::ScrollLeft:
								{
									if (current_instruction.OperandList.size() > 0)
									{
										error = true;
										error_type = ErrorType::NoOperandsSupported;
										break;
									}
									ProgramData.push_back(0x00);
									ProgramData.push_back(0xFC);
									current_address += 2;
									if (current_address > 0xFFF && CurrentExtension != ExtensionType::XOCHIP && CurrentExtension != ExtensionType::HyperCHIP64)
									{
										error = true;
										error_type = ErrorType::Only4KBSupported;
										break;
									}
									break;
								}
								case InstructionType::Exit:
								{
									if (current_instruction.OperandList.size() > 0)
									{
										error = true;
										error_type = ErrorType::NoOperandsSupported;
										break;
									}
									ProgramData.push_back(0x00);
									ProgramData.push_back(0xFD);
									current_address += 2;
									if (current_address > 0xFFF && CurrentExtension != ExtensionType::XOCHIP && CurrentExtension != ExtensionType::HyperCHIP64)
									{
										error = true;
										error_type = ErrorType::Only4KBSupported;
										break;
									}
									break;
								}
								case InstructionType::Low:
								{
									if (current_instruction.OperandList.size() > 0)
									{
										error = true;
										error_type = ErrorType::NoOperandsSupported;
										break;
									}
									ProgramData.push_back(0x00);
									ProgramData.push_back(0xFE);
									current_address += 2;
									if (current_address > 0xFFF && CurrentExtension != ExtensionType::XOCHIP && CurrentExtension != ExtensionType::HyperCHIP64)
									{
										error = true;
										error_type = ErrorType::Only4KBSupported;
										break;
									}
									break;
								}
								case InstructionType::High:
								{
									if (current_instruction.OperandList.size() > 0)
									{
										error = true;
										error_type = ErrorType::NoOperandsSupported;
										break;
									}
									ProgramData.push_back(0x00);
									ProgramData.push_back(0xFF);
									current_address += 2;
									if (current_address > 0xFFF && CurrentExtension != ExtensionType::XOCHIP && CurrentExtension != ExtensionType::HyperCHIP64)
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
										case OperandType::Pointer:
										{
											if (CurrentExtension != ExtensionType::HyperCHIP64)
											{
												error = true;
												error_type = ErrorType::HyperCHIP64Required;
												break;
											}
											unsigned char reg = 0x0;
											if (!ProcessAddressRegisterOffsetPointerOperand(0, reg))
											{
												error = true;
												error_type = ErrorType::InvalidRegister;
												break;
											}
											ProgramData.push_back(0xF0 | (reg & 0xF));
											ProgramData.push_back(0x20);
											current_address += 2;
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
										case OperandType::Pointer:
										{
											unsigned char reg = 0x0;
											if (!ProcessAddressRegisterOffsetPointerOperand(0, reg))
											{
												error = true;
												error_type = ErrorType::InvalidRegister;
												break;
											}
											ProgramData.push_back(0xF0 | (reg & 0xF));
											ProgramData.push_back(0x21);
											current_address += 2;
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
													if (current_address > 0xFFF && CurrentExtension != ExtensionType::XOCHIP && CurrentExtension != ExtensionType::HyperCHIP64)
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
													if (current_address > 0xFFF && CurrentExtension != ExtensionType::XOCHIP && CurrentExtension != ExtensionType::HyperCHIP64)
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
													if (current_address > 0xFFF && CurrentExtension != ExtensionType::XOCHIP && CurrentExtension != ExtensionType::HyperCHIP64)
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
													if (current_address > 0xFFF && CurrentExtension != ExtensionType::XOCHIP && CurrentExtension != ExtensionType::HyperCHIP64)
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
													if (current_instruction.OperandList.size() == 2)
													{
														ProgramData.push_back(0x80 | (reg1 & 0xF));
														ProgramData.push_back(reg2 << 4);
													}
													else
													{
														if (CurrentExtension != ExtensionType::XOCHIP && CurrentExtension != ExtensionType::HyperCHIP64)
														{
															error = true;
															error_type = ErrorType::XOCHIPRequired;
															break;
														}
														if (current_instruction.OperandList[2].Type == OperandType::Pointer)
														{
															std::string uptr_data;
															for (size_t c = 0; c < current_instruction.OperandList[2].Data.size(); ++c)
															{
																uptr_data += toupper(static_cast<unsigned char>(current_instruction.OperandList[2].Data[c]));
															}
															if (uptr_data == "I")
															{
																ProgramData.push_back(0x50 | (reg1 & 0xF));
																ProgramData.push_back((reg2 << 4) | 0x3);
															}
														}
													}
													current_address += 2;
													if (current_address > 0xFFF && CurrentExtension != ExtensionType::XOCHIP && CurrentExtension != ExtensionType::HyperCHIP64)
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
													if (current_address > 0xFFF && CurrentExtension != ExtensionType::XOCHIP && CurrentExtension != ExtensionType::HyperCHIP64)
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
													if (current_address > 0xFFF && CurrentExtension != ExtensionType::XOCHIP && CurrentExtension != ExtensionType::HyperCHIP64)
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
														if (current_address > 0xFFF && CurrentExtension != ExtensionType::XOCHIP && CurrentExtension != ExtensionType::HyperCHIP64)
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
													if (current_address > 0xFFF && CurrentExtension != ExtensionType::XOCHIP && CurrentExtension != ExtensionType::HyperCHIP64)
													{
														error = true;
														error_type = ErrorType::Only4KBSupported;
														break;
													}
													break;
												}
												case OperandType::UserRPL:
												{
													if (CurrentExtension != ExtensionType::SuperCHIP10 && CurrentExtension != ExtensionType::SuperCHIP11 && CurrentExtension != ExtensionType::XOCHIP && CurrentExtension != ExtensionType::HyperCHIP64)														
													{
														error = true;
														error_type = ErrorType::SuperCHIP10Required;
														break;
													}
													ProgramData.push_back(0xF0 | (reg1 & 0xF));
													ProgramData.push_back(0x85);
													current_address += 2;
													if (current_address > 0xFFF && CurrentExtension != ExtensionType::XOCHIP && CurrentExtension != ExtensionType::HyperCHIP64)
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
												case OperandType::Pointer:
												{
													if (CurrentExtension != ExtensionType::HyperCHIP64)
													{
														error = true;
														error_type = ErrorType::HyperCHIP64Required;
														break;
													}
													unsigned char reg = 0x0;
													if (!ProcessAddressRegisterOffsetPointerOperand(1, reg))
													{
														error = true;
														error_type = ErrorType::InvalidRegister;
														break;
													}
													ProgramData.push_back(0xF0 | (reg & 0xF));
													ProgramData.push_back(0xA2);
													current_address += 2;
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
												if (current_address > 0xFFF && CurrentExtension != ExtensionType::XOCHIP && CurrentExtension != ExtensionType::HyperCHIP64)
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
												if (current_address > 0xFFF && CurrentExtension != ExtensionType::XOCHIP && CurrentExtension != ExtensionType::HyperCHIP64)
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
													unsigned char reg1 = 0x0;
													if (!ProcessRegisterOperand(1, reg1))
													{
														error = true;
														error_type = ErrorType::InvalidRegister;
														break;
													}
													if (current_instruction.OperandList.size() == 2)
													{
														ProgramData.push_back(0xF0 | (reg1 & 0xF));
														ProgramData.push_back(0x55);
													}
													else
													{
														if (current_instruction.OperandList[2].Type == OperandType::Register)
														{
															if (CurrentExtension != ExtensionType::XOCHIP && CurrentExtension != ExtensionType::HyperCHIP64)
															{
																error = true;
																error_type = ErrorType::XOCHIPRequired;
																break;
															}
															unsigned char reg2 = 0x0;
															if (!ProcessRegisterOperand(2, reg2))
															{
																error = true;
																error_type = ErrorType::InvalidRegister;
																break;
															}
															ProgramData.push_back(0x50 | (reg1 & 0xF));
															ProgramData.push_back((reg2 << 4) | 0x2);
														}
													}
													current_address += 2;
													if (current_address > 0xFFF && CurrentExtension != ExtensionType::XOCHIP && CurrentExtension != ExtensionType::HyperCHIP64)
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
												if (current_address > 0xFFF && CurrentExtension != ExtensionType::XOCHIP && CurrentExtension != ExtensionType::HyperCHIP64)
												{
													error = true;
													error_type = ErrorType::Only4KBSupported;
													break;
												}
											}
											break;
										}
										case OperandType::HiResFont:
										{
											if (CurrentExtension != ExtensionType::SuperCHIP11 && CurrentExtension != ExtensionType::XOCHIP && CurrentExtension != ExtensionType::HyperCHIP64)
											{
												error = true;
												error_type = ErrorType::SuperCHIP11Required;
												break;
											}
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
												ProgramData.push_back(0x30);
												current_address += 2;
												if (current_address > 0xFFF && CurrentExtension != ExtensionType::XOCHIP && CurrentExtension != ExtensionType::HyperCHIP64)
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
												if (current_address > 0xFFF && CurrentExtension != ExtensionType::XOCHIP && CurrentExtension != ExtensionType::HyperCHIP64)
												{
													error = true;
													error_type = ErrorType::Only4KBSupported;
													break;
												}
											}
											break;
										}
										case OperandType::UserRPL:
										{
											if (CurrentExtension != ExtensionType::SuperCHIP10 && CurrentExtension != ExtensionType::SuperCHIP11 && CurrentExtension != ExtensionType::XOCHIP && CurrentExtension != ExtensionType::HyperCHIP64)
											{
												error = true;
												error_type = ErrorType::SuperCHIP10Required;
												break;
											}
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
												ProgramData.push_back(0x75);
												current_address += 2;
												if (current_address > 0xFFF && CurrentExtension != ExtensionType::XOCHIP && CurrentExtension != ExtensionType::HyperCHIP64)
												{
													error = true;
													error_type = ErrorType::Only4KBSupported;
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
													if (current_address > 0xFFF && CurrentExtension != ExtensionType::XOCHIP && CurrentExtension != ExtensionType::HyperCHIP64)
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
													if (current_address > 0xFFF && CurrentExtension != ExtensionType::XOCHIP && CurrentExtension != ExtensionType::HyperCHIP64)
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
												if (current_address > 0xFFF && CurrentExtension != ExtensionType::XOCHIP && CurrentExtension != ExtensionType::HyperCHIP64)
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
												if (current_address > 0xFFF && CurrentExtension != ExtensionType::XOCHIP && CurrentExtension != ExtensionType::HyperCHIP64)
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
												if (current_address > 0xFFF && CurrentExtension != ExtensionType::XOCHIP && CurrentExtension != ExtensionType::HyperCHIP64)
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
												if (current_address > 0xFFF && CurrentExtension != ExtensionType::XOCHIP && CurrentExtension != ExtensionType::HyperCHIP64)
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
												if (current_address > 0xFFF && CurrentExtension != ExtensionType::XOCHIP && CurrentExtension != ExtensionType::HyperCHIP64)
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
												if (current_address > 0xFFF && CurrentExtension != ExtensionType::XOCHIP && CurrentExtension != ExtensionType::HyperCHIP64)
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
												if (current_address > 0xFFF && CurrentExtension != ExtensionType::XOCHIP && CurrentExtension != ExtensionType::HyperCHIP64)
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
								case InstructionType::Plane:
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
										case OperandType::ImmediateValue:
										{
											unsigned char value = Process8BitImmediateValueOperand(0);
											if (error)
											{
												break;
											}
											ProgramData.push_back(0xF0 | (value & 0xF));
											ProgramData.push_back(0x01);
											current_address += 2;
											break;
										}
									}
									break;
								}
								case InstructionType::Audio:
								{
									if (current_instruction.OperandList.size() > 0)
									{
										error = true;
										error_type = ErrorType::NoOperandsSupported;
										break;
									}
									ProgramData.push_back(0xF0);
									ProgramData.push_back(0x02);
									current_address += 2;
									break;
								}
								case InstructionType::Pitch:
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
											ProgramData.push_back(0xF0 | (reg & 0xF));
											ProgramData.push_back(0x3A);
											current_address += 2;
											break;
										}
									}
									break;
								}
								case InstructionType::RotateRight:
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
												ProgramData.push_back((reg2 << 4) | 0x8);
												current_address += 2;
											}
											break;
										}
									}
									break;
								}
								case InstructionType::RotateLeft:
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
												ProgramData.push_back((reg2 << 4) | 0x9);
												current_address += 2;
											}
											break;
										}
									}
									break;
								}
								case InstructionType::Test:
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
												error_type = ErrorType::InvalidValue;
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
												ProgramData.push_back((reg2 << 4) | 0xA);
												current_address += 2;
											}
											break;
										}
									}
									break;
								}
								case InstructionType::Not:
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
												error_type = ErrorType::InvalidValue;
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
												ProgramData.push_back((reg2 << 4) | 0xB);
												current_address += 2;
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
													if (current_address > 0xFFF && CurrentExtension != ExtensionType::XOCHIP && CurrentExtension != ExtensionType::HyperCHIP64)
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
											if (current_address > 0xFFF && CurrentExtension != ExtensionType::XOCHIP && CurrentExtension != ExtensionType::HyperCHIP64)
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
											if (current_address > 0xFFF && CurrentExtension != ExtensionType::XOCHIP && CurrentExtension != ExtensionType::HyperCHIP64)
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
						}
						break;
					}
					default:
					{
						if (!comment)
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
									case TokenType::Origin:
									{
										if (isspace(static_cast<unsigned char>(line_data[i])))
										{
											break;
										}
										token += line_data[i];
										break;
									}
									case TokenType::DataByte:
									{
										if (!string_mode)
										{
											if (isspace(static_cast<unsigned char>(line_data[i])))
											{
												break;
											}
											token += line_data[i];
										}
										else
										{
											ProcessStringDataByte(line_data[i]);
											if (escape_mode)
											{
												escape_mode = false;
											}
										}
										break;
									}
									case TokenType::DataWord:
									{
										if (!string_mode)
										{
											if (isspace(static_cast<unsigned char>(line_data[i])))
											{
												break;
											}
											token += line_data[i];
										}
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
								case InstructionType::ScrollRight:
								{
									std::cout << "SCR";
									break;
								}
								case InstructionType::ScrollLeft:
								{
									std::cout << "SCL";
									break;
								}
								case InstructionType::Exit:
								{
									std::cout << "EXIT";
									break;
								}
								case InstructionType::Low:
								{
									std::cout << "LOW";
									break;
								}
								case InstructionType::High:
								{
									std::cout << "HIGH";
									break;
								}
								case InstructionType::Audio:
								{
									std::cout << "AUDIO";
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
								case InstructionType::ScrollDown:
								{
									std::cout << "SCD";
									break;
								}
								case InstructionType::ScrollUp:
								{
									std::cout << "SCU";
									break;
								}
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
								case InstructionType::Plane:
								{
									std::cout << "PLANE";
									break;
								}
								case InstructionType::Pitch:
								{
									std::cout << "PITCH";
									break;
								}
								case InstructionType::RotateRight:
								{
									std::cout << "ROR";
									break;
								}
								case InstructionType::RotateLeft:
								{
									std::cout << "ROL";
									break;
								}
								case InstructionType::Test:
								{
									std::cout << "TEST";
									break;
								}
								case InstructionType::Not:
								{
									std::cout << "NOT";
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
							break;
						}
						case ErrorType::TooManyOperands:
						{
							switch (current_instruction.Type)
							{
								case InstructionType::ScrollDown:
								{
									std::cout << "SCD";
									break;
								}
								case InstructionType::ScrollUp:
								{
									std::cout << "SCU";
									break;
								}
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
								case InstructionType::Plane:
								{
									std::cout << "PLANE";
									break;
								}
								case InstructionType::Pitch:
								{
									std::cout << "PITCH";
									break;
								}
								case InstructionType::RotateRight:
								{
									std::cout << "ROR";
									break;
								}
								case InstructionType::RotateLeft:
								{
									std::cout << "ROL";
									break;
								}
								case InstructionType::Test:
								{
									std::cout << "TEST";
									break;
								}
								case InstructionType::Not:
								{
									std::cout << "NOT";
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
						case ErrorType::ReservedAddress:
						{
							std::cout << "Addresses 0x000-0x1FF are reserved.\n";
							break;
						}
						case ErrorType::BelowCurrentAddress:
						{
							std::cout << "Attempting to the set the address below the current address.\n";
							break;
						}
						case ErrorType::Only4KBSupported:
						{
							std::cout << "Current extension only supports up to 4KB (maxed at 0xFFF).\n";
							break;
						}
						case ErrorType::SuperCHIP10Required:
						{
							switch (current_instruction.Type)
							{
								case InstructionType::Exit:
								{
									std::cout << "EXIT";
									break;
								}
								case InstructionType::Low:
								{
									std::cout << "LOW";
									break;
								}
								case InstructionType::High:
								{
									std::cout << "HIGH";
									break;
								}
								case InstructionType::Load:
								{
									std::cout << "LD ";
									if (current_instruction.OperandList.size() == 2)
									{
										switch (current_instruction.OperandList[0].Type)
										{
											case OperandType::UserRPL:
											{
												std::cout << "R, VX";
												break;
											}
											case OperandType::Register:
											{
												if (current_instruction.OperandList[1].Type == OperandType::UserRPL)
												{
													std::cout << "VX, R";
												}
												break;
											}
										}
									}
									break;
								}
							}
							std::cout << " instruction requires using at least the SuperCHIP V1.0 extension to use.\n";
							break;
						}
						case ErrorType::SuperCHIP11Required:
						{
							switch (current_instruction.Type)
							{
								case InstructionType::ScrollDown:
								{
									std::cout << "SCD";
									break;
								}
								case InstructionType::ScrollRight:
								{
									std::cout << "SCR";
									break;
								}
								case InstructionType::ScrollLeft:
								{
									std::cout << "SCL";
									break;
								}
								case InstructionType::Load:
								{
									std::cout << "LD ";
									if (current_instruction.OperandList.size() == 2)
									{
										if (current_instruction.OperandList[0].Type == OperandType::HiResFont)
										{
											std::cout << "HF, VX";
										}
									}
									break;
								}
							}
							std::cout << " instruction requires using at least the SuperCHIP V1.1 extension to use.\n";
							break;
						}
						case ErrorType::XOCHIPRequired:
						{
							switch (current_instruction.Type)
							{
								case InstructionType::ScrollUp:
								{
									std::cout << "SCU";
									break;
								}
								case InstructionType::Load:
								{
									if (current_instruction.OperandList.size() >= 2 && current_instruction.OperandList.size() <= 3)
									{
										std::cout << "LD ";
										switch (current_instruction.OperandList[0].Type)
										{
											case OperandType::Register:
											{
												if (current_instruction.OperandList[1].Type == OperandType::Register)
												{
													if (current_instruction.OperandList[2].Type == OperandType::Pointer)
													{
														std::cout << "VX, VY, [I]";
													}
												}
												break;
											}
											case OperandType::Pointer:
											{
												if (current_instruction.OperandList[1].Type == OperandType::Register)
												{
													if (current_instruction.OperandList[2].Type == OperandType::Register)
													{
														std::cout << "[I], VX, VY";
													}
												}
												break;
											}
										}
									}
									break;
								}
								case InstructionType::Plane:
								{
									std::cout << "PLANE";
									break;
								}
								case InstructionType::Audio:
								{
									std::cout << "AUDIO";
									break;
								}
								case InstructionType::Pitch:
								{
									std::cout << "PITCH";
									break;
								}
							}
							std::cout << " requires using at least the XO-CHIP extension to use.\n";
							break;
						}
						case ErrorType::HyperCHIP64Required:
						{
							switch (current_instruction.Type)
							{
								case InstructionType::RotateRight:
								{
									std::cout << "ROR";
									break;
								}
								case InstructionType::RotateLeft:
								{
									std::cout << "ROL";
									break;
								}
								case InstructionType::Test:
								{
									std::cout << "TEST";
									break;
								}
								case InstructionType::Not:
								{
									std::cout << "NOT";
									break;
								}
								case InstructionType::Jump:
								{
									std::cout << "JP ";
									if (current_instruction.OperandList.size() == 1)
									{
										if (current_instruction.OperandList[0].Type == OperandType::Pointer)
										{
											std::cout << "[I + VX]";
										}
									}
									break;
								}
								case InstructionType::Call:
								{
									std::cout << "CALL ";
									if (current_instruction.OperandList.size() == 1)
									{
										if (current_instruction.OperandList[0].Type == OperandType::Pointer)
										{
											std::cout << "[I + VX]";
										}
									}
									break;
								}
								case InstructionType::Load:
								{
									if (current_instruction.OperandList.size() >= 2 && current_instruction.OperandList.size() <= 3)
									{
										std::cout << "LD ";
										switch (current_instruction.OperandList[0].Type)
										{
											case OperandType::AddressRegister:
											{
												if (current_instruction.OperandList[1].Type == OperandType::Pointer)
												{
													std::cout << "I, [I + VX]";
												}
												break;
											}
										}
									}
									break;
								}
							}
							std::cout << " instruction requires using at least the HyperCHIP-64 extension to use.\n";
							break;
						}
						case ErrorType::BinaryFileDoesNotExist:
						{
							std::cout << '\'' << token << "' does not exist.\n";
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
						if (u.IsInstruction)
						{
							unsigned short offset = 0;
							if (u.AbsoluteAddressExtended)
							{
								ProgramData[u.Address] |= (s.Location >> 12);
								offset += 2;
							}
							ProgramData[u.Address + offset] |= ((s.Location & 0xF00) >> 8);
							ProgramData[u.Address + offset + 1] = (s.Location & 0xFF);
						}
						else
						{
							ProgramData[u.Address] = (s.Location >> 8);
							ProgramData[u.Address + 1] = (s.Location & 0xFF);
						}
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
			switch (CurrentOutputType)
			{
				case OutputType::Binary:
				{
					output_file.write(reinterpret_cast<char *>(ProgramData.data()), ProgramData.size());
					break;
				}
				case OutputType::HexASCIIString:
				{
					std::ostringstream hex_data;
					hex_data << std::hex;
					for (size_t c = 0; c < ProgramData.size(); ++c)
					{
						hex_data << std::setfill('0') << std::setw(2) << static_cast<unsigned short>(ProgramData[c]);
					}
					output_file.write(hex_data.str().c_str(), hex_data.str().size());
					break;
				}
			}
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
