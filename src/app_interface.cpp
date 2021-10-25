module;
#include <iostream>
#include <vector>

export module application;

export namespace BandCHIP_Assembler
{
	struct VersionData
	{
		unsigned short major;
		unsigned short minor;
		friend std::ostream &operator<<(std::ostream &out, const VersionData version);
	};

	std::ostream &operator<<(std::ostream &out, const VersionData version);

	class Application
	{
		public:
			Application(int argc, char *argv[]);
			~Application();
			int GetReturnCode() const;
		private:
			std::vector<std::string> Args;
			VersionData Version = { 0, 1 };
			int retcode;
	};
}
