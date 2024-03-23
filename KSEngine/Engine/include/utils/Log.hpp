#pragma once
#include <iostream>
#include <format>

namespace KSE {

enum class LogType {
	INFO, //Use for debugging and general info
	WARN, //Use when something is out of place, but still recoverable
	ERROR //Use when crashing is what needs to happen
};

class Log {
public:
	//Static class
	Log() = delete;

	//Use macros for invoking this function
	template<typename FormatString, typename... Args>
	static void Message(LogType type, const char* source_file, uint32_t line_number, FormatString&& fmt, Args&&... args)
	{
		*output 
			<< format_message_type(type)
			<< std::format("({}, Line {})", source_file, line_number) << "\n"
			<< std::vformat(fmt, std::make_format_args(args...)) << "\n" << "\n";
	}

private:

	inline static std::ostream* output = &std::cout;

	//Type message
	static constexpr const char* format_message_type(LogType type) {
		switch (type) {
		case LogType::INFO: return "\033[32m" "INFO: " "\033[0m";
		case LogType::WARN: return "\033[33m" "WARN:" "\033[0m";
		case LogType::ERROR: return "\033[31m" "ERROR:" "\033[0m";
		default: return "";
		}
	}

	//Corrects all __FILE__ paths to not show full path and only relevant folder structure
	static constexpr size_t path_offset = std::string(__FILE__).find("KSEngine");
	static_assert(path_offset != std::string::npos); //If this fails then fiz line above

	//Text Colours
	static constexpr auto green = "\033[32m";
	static constexpr auto yellow = "\033[33m";
	static constexpr auto red = "\033[31m";
	static constexpr auto reset = "\033[0m";
};
}

#ifndef DISABLE_LOG

#define LOG(type, format_string, ...) KSE::Log::Message(type, __FILE__, __LINE__, format_string, __VA_ARGS__)

#endif // DISABLE_LOG
