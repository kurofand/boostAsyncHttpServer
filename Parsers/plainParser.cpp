#include "plainParser.hpp"

PlainParser::PlainParser(uint16_t contentLength)
{
	result_=new ParsingResult();
	auto *plainText=new std::string();
	plainText->reserve(contentLength);
	result_->plainText=plainText;
}

ReadResult PlainParser::parseChunk(std::string_view chunk)
{
	result_->plainText->append(chunk.data(), chunk.size());
	return ReadResult::OK;
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
