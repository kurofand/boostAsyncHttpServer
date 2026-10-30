#include "log.hpp"

#include <fstream>
#include <chrono>
#include <format>

#include "../config.hpp"

Log::Log(LogLevel lvl): logLevel_(lvl)
{
	static bool logFileCreated=false;
	if(!logFileCreated)
	{
		std::ofstream logFile("log.txt");
		logFileCreated=true;
		logFile.close();
	}
}

Log::~Log()
{
	if(static_cast<uint8_t>(logLevel_)>=MIN_LOG_LEVEL)
	{
		stream_<<std::endl;
		const auto now=std::chrono::system_clock::now();
		std::ofstream logFile("log.txt", std::ofstream::app);
		logFile<<std::format("{:%d/%m/%Y %H:%M:%S} --[{}]-- {}", now, levelMap_.at(logLevel_), stream_.str());
		logFile.close();
	}
}
