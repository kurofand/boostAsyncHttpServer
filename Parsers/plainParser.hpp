#ifndef PLAIN_PARSER_HPP
#define PLAIN_PARSER_HPP

#include "parser.hpp"

class PlainParser: public Parser
{
	public:
		PlainParser(uint16_t contentLength);
		ReadResult parseChunk(std::string_view chunk) override;
		~PlainParser();
};

#endif
