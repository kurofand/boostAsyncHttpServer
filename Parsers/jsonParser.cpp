#include "jsonParser.hpp"

#include <boost/json/src.hpp>

#include "../Log/log.hpp"

JsonParser::JsonParser()
{
	result_=new ParsingResult();
	auto *jsonObj=new boost::json::value();
	result_->jsonObj=jsonObj;
	Log(LogLevel::ParserDebug)<<"Application/json parser";
}

ReadResult JsonParser::parseChunk(std::string_view chunk)
{
	boost::system::error_code ec;
	p.write(chunk, ec);
	if(ec)
	{
		Log(LogLevel::Error)<<"Error on json parsing! Error: "<<ec.message()<<". Error code: "<<ec.value()<<"\n";
		return ReadResult::BAD_REQUEST;
	}
	return ReadResult::OK;
}

bool JsonParser::complete()
{
	boost::system::error_code ec;
	p.finish(ec);
	if(ec)
	{
		Log(LogLevel::Error)<<"Error on json parsing! Error: "<<ec.message()<<". Error code: "<<ec.value()<<"\n";
		return false;
	}
	*result_->jsonObj=p.release();
	return true;
}

JsonParser::~JsonParser()
{
	if(result_!=nullptr)
	{
		if(result_->jsonObj!=nullptr)
			delete result_->jsonObj;
		delete result_;
	}
}
