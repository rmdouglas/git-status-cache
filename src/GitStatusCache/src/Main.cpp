#include "stdafx.h"
#include <boost/program_options.hpp>
#include "DirectoryMonitor.h"
#include "LoggingModuleSettings.h"
#include "LoggingInitializationScope.h"
#include "NamedPipeServer.h"
#include "StatusCache.h"
#include "StatusController.h"

using namespace boost::program_options;

options_description BuildGenericOptions()
{
	options_description generic{ "Generic options" };
	generic.add_options()
		("help", "Display help message");
	return generic;
}

options_description BuildLoggingOptions(bool* enableFileLogging, bool* quiet, bool* verbose, bool* spam)
{
	options_description logging{ "Logging options" };
	logging.add_options()
		("fileLogging", bool_switch(enableFileLogging), "Enables file logging.")
		("quiet,q", bool_switch(quiet), "Disables all non-critical logging output.")
		("verbose,v", bool_switch(verbose), "Enables verbose logging output.")
		("spam,s", bool_switch(spam), "Enables spam logging output.");
	return logging;
}

void ThrowIfMutuallyExclusiveOptionsSet(
	const variables_map& vm,
	const std::string& option1,
	const std::string& option2,
	const std::string& option3)
{
	if (   (!vm[option1].defaulted() && (!vm[option2].defaulted() || !vm[option3].defaulted()))
		|| (!vm[option2].defaulted() && (!vm[option1].defaulted() || !vm[option3].defaulted()))
		|| (!vm[option3].defaulted() && (!vm[option1].defaulted() || !vm[option2].defaulted())))
	{
		throw std::logic_error(std::string("Options '") + option1 + "', '" + option2 + "', and '" + option3 + "' are mutually exclusive.");
	}
}

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow)
{
	int argc;
	auto argv = ::CommandLineToArgvW(::GetCommandLineW(), &argc);

	Logging::LoggingModuleSettings loggingSettings;
	bool quiet = false;
	bool verbose = false;
	bool spam = false;

	auto generic = BuildGenericOptions();
	auto logging = BuildLoggingOptions(&loggingSettings.EnableFileLogging, &quiet, &verbose, &spam);
	options_description all{ "Allowed options" };
	all.add(generic).add(logging);

	try
	{
		variables_map vm;
		store(wcommand_line_parser(argc, argv).options(all).run(), vm);

		if (vm.count("help"))
		{
			std::cout << generic << std::endl;
			std::cout << logging << std::endl;
			return 1;
		}

		ThrowIfMutuallyExclusiveOptionsSet(vm, "quiet", "verbose", "spam");
		notify(vm);
	}
	catch (std::exception& e)
	{
		std::cerr << "Error: " << e.what() << std::endl;
		std::cout << generic << std::endl;
		std::cout << logging << std::endl;
		return -1;
	}

	if (quiet)
		loggingSettings.MinimumSeverity = Logging::Severity::Error;
	else if (verbose)
		loggingSettings.MinimumSeverity = Logging::Severity::Verbose;
	else if (spam)
		loggingSettings.MinimumSeverity = Logging::Severity::Spam;
	else
		loggingSettings.MinimumSeverity = Logging::Severity::Info;

	Logging::LoggingInitializationScope enableLogging(loggingSettings);

	StatusController statusController;
	NamedPipeServer server([&statusController](const std::string& request) { return statusController.HandleRequest(request); });

	statusController.WaitForShutdownRequest();

	return 0;
}
