#ifndef PARSER_HPP
#define PARSER_HPP

#include "../common.hpp"

class Parser
{
	public:
		virtual ReadResult parseChunk(std::string_view chunk)=0;
		virtual bool complete()=0;
		ParsingResult* result(){ParsingResult *tmp=result_;result_=nullptr;return tmp;}
		virtual ~Parser()=default;
	protected:
		std::string remainingPart_;
		ParsingResult *result_;
};

#endif
