#include "urlencodedParser.hpp"

#include "../utils.hpp"
#include "../Log/log.hpp"

UrlencodedParser::UrlencodedParser(const unsigned int contentLength): contentLength_(contentLength)
{
	result_=new ParsingResult();
	auto *formData=new std::unordered_map<std::string, std::vector<std::string>>;
	result_->formData=formData;
	Log(LogLevel::ParserDebug)<<"Application/x-www-form-urlencoded parser";
}

ReadResult UrlencodedParser::parseChunk(std::string_view chunk)
{
	receivedBytes_+=chunk.size();
	std::string tmp;
	tmp.append(remainingPart_);
	tmp.append(chunk.data(), chunk.size());
	remainingPart_.clear();
	chunk=tmp;

	//no complete key=val in current chunk, store and wait for more
	if(receivedBytes_<contentLength_&&chunk.find('&')==std::string_view::npos)
	{
		remainingPart_.append(chunk.data(), chunk.size());
		return ReadResult::OK;
	}

	bool parsing=true;
	while(parsing)
	{
		auto pairEnd=chunk.find('&');
		std::string_view pair;
		if(pairEnd==std::string_view::npos)
		{
			//current last key=val pair could be splitted by chunks
			//so check if body is read or still awaiting more data
			if(receivedBytes_<contentLength_)
			{
				remainingPart_.append(chunk.data(), chunk.size());
				return ReadResult::OK;
			}
			else
			{
				pair=chunk;
				parsing=false;
			}
		}
		else
		{
			pair=chunk.substr(0, pairEnd);
			chunk.remove_prefix(pair.size()+1);
		}

		auto delimiter=pair.find('=');
		if(delimiter==std::string_view::npos)
		{
			Log(LogLevel::ParserDebug)<<"Malformed pair without \"=\" symbol, bad request";
			return ReadResult::BAD_REQUEST;
		}

		std::string key(pair.substr(0, delimiter));
		std::string val(pair.substr(delimiter+1));
		while(key.find('+')!=std::string::npos)
			key[key.find('+')]=' ';
		while(val.find('+')!=std::string::npos)
			val[val.find('+')]=' ';

		std::string dKey, dVal;
		if(!urlDecode(key, dKey))
		{
			Log(LogLevel::ParserDebug)<<"Failed to decode key";
			return ReadResult::BAD_REQUEST;
		}
		if(!urlDecode(val, dVal))
		{
			Log(LogLevel::ParserDebug)<<"Failed to decode val";
			return ReadResult::BAD_REQUEST;
		}
		if(result_->formData->contains(dKey))
			result_->formData->at(dKey).push_back(dVal);
		else
		{
			std::vector<std::string> v;
			v.push_back(dVal);
			result_->formData->insert(std::pair<std::string, std::vector<std::string>>(dKey, v));
		}
	}

	if(!chunk.empty())
		remainingPart_.append(chunk.data(), chunk.size());
	return ReadResult::OK;
}

bool UrlencodedParser::complete()
{
	return receivedBytes_==contentLength_;
}

UrlencodedParser::~UrlencodedParser()
{
	if(result_!=nullptr)
	{
		delete result_->formData;
		delete result_;
	}
}
