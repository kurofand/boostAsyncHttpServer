#ifndef URLENCODED_PARSER
#define URLENCODED_PARSER

#include "parser.hpp"

class UrlencodedParser: public Parser
{
	public:
		UrlencodedParser(const unsigned int contentLength);
		ReadResult parseChunk(std::string_view chunk) override;
		bool complete() override;
		~UrlencodedParser();
	private:
		const unsigned int contentLength_;
		unsigned int receivedBytes_=0;
};

#endif
