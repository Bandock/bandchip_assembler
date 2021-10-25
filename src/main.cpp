#include "../include/application.h"

int main(int argc, char *argv[])
{
	BandCHIP_Assembler::Application MainApp(argc, argv);
	return MainApp.GetReturnCode();
}
