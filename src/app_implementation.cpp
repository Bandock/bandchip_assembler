module;
#include <iostream>
#include <fstream>
#include <regex>

module application;
import <string>;

std::ostream &BandCHIP_Assembler::operator<<(std::ostream &out, const BandCHIP_Assembler::VersionData version)
{
	out << "V" << version.major << "." << version.minor;
	return out;
}

BandCHIP_Assembler::Application::Application(int argc, char *argv[]) : retcode(0)
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
			char line_data[512];
			input_file.getline(line_data, 512, '\n');
			std::regex label_exp(":");
			if (std::regex_search(line_data, label_exp))
			{
				std::cout << "Test\n";
			}
		}
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
