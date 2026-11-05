#include "urlencodedParser.hpp"

#include "../utils.hpp"

UrlencodedParser::UrlencodedParser()
{
	result_=new ParsingResult();
	auto *formData=new std::unordered_map<std::string, std::vector<std::string>>;
	result_->formData=formData;
}

ReadResult UrlencodedParser::parseChunk(std::string_view chunk)
{
	std::string tmp;
	tmp.append(remainingPart_);
	tmp.append(chunk.data(), chunk.size());
	chunk=tmp;

	bool parsing=true;
	while(parsing)
	{
		auto pairEnd=chunk.find('&');
		std::string_view pair;
		if(pairEnd==std::string_view::npos)
		{
			pair=chunk;
			parsing=false;
		}
		else
		{
			pair=chunk.substr(0, pairEnd);
			chunk.remove_prefix(pair.size()+1);
		}

		auto delimiter=pair.find('=');
		if(delimiter==std::string_view::npos)
			return ReadResult::BAD_REQUEST;

		std::string key(pair.substr(0, delimiter));
		std::string val(pair.substr(delimiter+1));
		while(key.find('+')!=std::string::npos)
			key[key.find('+')]=' ';
		while(val.find('+')!=std::string::npos)
			val[val.find('+')]=' ';

		key=urlDecode(std::move(key));
		val=urlDecode(std::move(val));
		if(result_->formData->contains(key))
			result_->formData->at(key).push_back(val);
		else
		{
			std::vector<std::string> v;
			v.push_back(val);
			result_->formData->insert(std::pair<std::string, std::vector<std::string>>(key, v));
		}
	}

	if(!chunk.empty())
	{
		remainingPart_.clear();
		remainingPart_.append(chunk.data(), chunk.size());
	}
	return ReadResult::OK;
}

UrlencodedParser::~UrlencodedParser()
{
	if(result_!=nullptr)
	{
		delete result_->formData;
		delete result_;
	}
}
