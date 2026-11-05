#ifndef PARSER_HPP
#define PARSER_HPP

#include "../common.hpp"

class Parser
{
	public:
		virtual ReadResult parseChunk(std::string_view chunk)=0;
/*		std::variant<
			std::monostate,
			std::string,
			std::unordered_map<std::string, std::vector<MultipartFormData*>*>*,
			std::unordered_map<std::string, std::vector<std::string>*>*
		> result(){return result_;}*/
		ParsingResult* result(){ParsingResult *tmp=result_;result_=nullptr;return tmp;}
		virtual ~Parser()=default;
	protected:
		std::string remainingPart_;
		ParsingResult *result_;
/*		std::variant<
			std::monostate,
			std::string,
			std::unordered_map<std::string, std::vector<MultipartFormData*>*>*,
			std::unordered_map<std::string, std::vector<std::string>*>*
		> result_;*/
};

#endif
