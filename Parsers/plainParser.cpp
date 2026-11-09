#include "plainParser.hpp"

#include "../Log/log.hpp"

PlainParser::PlainParser(const unsigned int contentLength): contentLength_(contentLength)
{
	result_=new ParsingResult();
	auto *plainText=new std::string();
	plainText->reserve(contentLength);
	result_->plainText=plainText;
	Log(LogLevel::ParserDebug)<<"Text/pain parser";
}

ReadResult PlainParser::parseChunk(std::string_view chunk)
{
	receivedBytes_+=chunk.size();
	result_->plainText->append(chunk.data(), chunk.size());
	return ReadResult::OK;
}

bool PlainParser::complete()
{
	return receivedBytes_==contentLength_;
}

PlainParser::~PlainParser()
{
	if(result_!=nullptr)
	{
		if(result_->plainText!=nullptr)
			delete result_->plainText;
		delete result_;
	}
}
