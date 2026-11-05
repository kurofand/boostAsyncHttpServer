#ifndef URLENCODED_PARSER
#define URLENCODED_PARSER

#include "parser.hpp"

class UrlencodedParser: public Parser
{
	public:
		UrlencodedParser();
		ReadResult parseChunk(std::string_view chunk) override;
		~UrlencodedParser();
};

#endif
