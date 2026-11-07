#ifndef PLAIN_PARSER_HPP
#define PLAIN_PARSER_HPP

#include "parser.hpp"

class PlainParser: public Parser
{
	public:
		PlainParser(const unsigned int contentLength);
		ReadResult parseChunk(std::string_view chunk) override;
		bool complete() override;
		~PlainParser();
	private:
		const unsigned int contentLength_;
		unsigned int receivedBytes_=0;
};

#endif
