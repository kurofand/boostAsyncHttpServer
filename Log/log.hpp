#ifndef LOG_HPP
#define LOG_HPP

#include <sstream>
#include <unordered_map>
#include <cstdint>

enum class LogLevel: uint8_t
{
	ParserDebug=0,
	Debug,
	Info,
	Warning,
	Error
};

class Log
{
	public:
		Log(LogLevel lvl);
		template <class T>Log& operator<<(T t)
		{
			stream_<<t;
			return *this;
		};

		Log(const Log&)=delete;
		Log operator=(const Log&)=delete;
		Log(Log&&)=delete;
		Log operator=(Log&&)=delete;
		~Log();
	private:
		LogLevel logLevel_;
		std::ostringstream stream_;
		static inline const std::unordered_map<LogLevel, std::string> levelMap_=
		{
			{LogLevel::ParserDebug, "PARSER"},
			{LogLevel::Debug, "DEBUG"},
			{LogLevel::Info, "INFO"},
			{LogLevel::Warning, "WARNING"},
			{LogLevel::Error, "ERROR"}
		};
};
#endif
