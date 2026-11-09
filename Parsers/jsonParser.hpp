#ifndef JSON_PARSER
#define JSON_PARSER

#include "parser.hpp"

#include <boost/json.hpp>

class JsonParser: public Parser
{
	public:
		JsonParser();
		ReadResult parseChunk(std::string_view chunk) override;
		bool complete() override;
		~JsonParser();
	private:
		boost::json::stream_parser p;
};

#endif
