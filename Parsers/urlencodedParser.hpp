#ifndef URLENCODED_PARSER
#define URLENCODED_PARSER

#include "parser.hpp"

class UrlencodedParser: public Parser
{
	public:
		UrlencodedParser(uint16_t contentLength);
		ReadResult parseChunk(std::string_view chunk) override;
		~UrlencodedParser();
	private:
		uint16_t contentLength_;
		uint16_t receivedBytes_=0;
};

#endif
