#include "multipartParser.hpp"

#include <sstream>
#include <algorithm>

#include "../Log/log.hpp"
#include "../utils.hpp"

MultipartParser::MultipartParser(std::string *boundary, const uint16_t &contentLength):
                        boundary_(boundary), contentLength_(contentLength)
{
	result_=new ParsingResult();
	result_->multipartFormData=new std::unordered_map<std::string, std::vector<MultipartFormData*>*>();
}

ReadResult MultipartParser::parseChunk(std::string_view chunk)
{
	std::string tmp;
	tmp.append(remainingPart_);
	remainingPart_.clear();
	tmp.append(chunk.data(), chunk.size());
	chunk=tmp;

	bool parsing=true;
	while(parsing)
	{
		switch(currentState_)
		{
			//waiting for opening boundary
			case MultipartParserState::WaitingForData:
			{
				//chunk smaller than --boundary\r\n - wait for more data
				if(chunk.size()<boundary_->size()+4)
				{
					parsing=false;
					break;
				}

				//checking for correct boundary line
				auto pos=chunk.find("\r\n");
				if(pos==std::string_view::npos)
				{
					parsing=false;
					currentState_=MultipartParserState::Error;
					return ReadResult::BAD_REQUEST;
				}
				std::string_view line=chunk.substr(0, pos+2);
				if(line!="--"+*boundary_+"\r\n")
				{
					parsing=false;
					currentState_=MultipartParserState::Error;
					return ReadResult::BAD_REQUEST;
				}

				chunk.remove_prefix(line.size());
				currentState_=MultipartParserState::Boundary;
				break;
			}
			//boundary read, reading form headers
			case MultipartParserState::Boundary:
			{
				//trying to find current headers end point
				//if there is \r\n\r\n sequence, all headers fields in the chunk
				auto headersEnd=chunk.find("\r\n\r\n");
				bool gotAllHeaders=true;
				//it there is only a part of headers trying to find last full line
				if(headersEnd==std::string_view::npos)
				{
					gotAllHeaders=false;
					headersEnd=chunk.rfind("\r\n");
					//if there is only a part of single line waiting for more data
					if(headersEnd==std::string_view::npos)
					{
						parsing=false;
						break;
					}
					else
						headersEnd+=2;
				}
				else
					headersEnd+=4;

				if(formData_==nullptr)
					formData_=new MultipartFormData();

				std::string headers{chunk.substr(0, headersEnd)};
				chunk.remove_prefix(headers.size());
				std::stringstream ss(headers);
				std::string line;
				while(getline(ss, line, '\n')&&line!="\r")
				{
					if(!line.empty()&&line.back()=='\r')
						line.pop_back();
					const auto delimiter=line.find(':');
					//malformed header
					if(delimiter==std::string::npos)
					{
						currentState_=MultipartParserState::Error;
						return ReadResult::BAD_REQUEST;
					}

					std::string h=line.substr(0, delimiter);
					std::transform(h.begin(), h.end(), h.begin(),
						[](unsigned char c){return std::tolower(c);});
					std::string_view v(line.data()+delimiter+1, line.size()-delimiter-1);
					while(!v.empty()&&v.front()==' ')
						v.remove_prefix(1);
					if(v.empty())
					{
						currentState_=MultipartParserState::Error;
						return ReadResult::BAD_REQUEST;
					}

					if(h=="content-disposition")
					{
						if(!parseContentDisposition(v, key, formData_->fileName)||key.empty())
						{
							currentState_=MultipartParserState::Error;
							return ReadResult::BAD_REQUEST;
						}
					}
					else if(h=="content-type")
						formData_->contentType=v;
				}
				if(gotAllHeaders)
					currentState_=MultipartParserState::Headers;
				else
					parsing=false;
				break;
			}
			case MultipartParserState::Headers:
			{
				size_t pos=chunk.find("\r\n--"+*boundary_+"\r\n");
				if(pos==std::string_view::npos)
					pos=chunk.find("\r\n--"+*boundary_+"--\r\n");
				//all data between first character and boundary
				//or last character if there is no boundary in the chunk is body
				if(pos!=std::string_view::npos)
					currentState_=MultipartParserState::WaitingForData;
				else
				{
					//body incomplete, wait for next chunk
					parsing=false;
					break;
				}

				if(formData_->content==nullptr)
					formData_->content=new std::string(chunk.substr(0, pos));
				else
					formData_->content->append(chunk.substr(0, pos));
				chunk.remove_prefix(pos);

				if(currentState_==MultipartParserState::WaitingForData)
				{
					if(!result_->multipartFormData->contains(key))
						result_->multipartFormData->insert({key, new std::vector<MultipartFormData*>()});
					result_->multipartFormData->at(key)->push_back(formData_);
					formData_=nullptr;
					key.clear();
				}

				if(chunk=="\r\n--"+*boundary_+"--\r\n")
				{
					currentState_=MultipartParserState::Complete;
					return ReadResult::OK;
				}

				//remove \r\n before next boundary
				chunk.remove_prefix(2);
				break;
			}
		};

	}

	if(!chunk.empty())
		remainingPart_.append(chunk.data(), chunk.size());
	return ReadResult::OK;
}

bool MultipartParser::parseContentDisposition(std::string_view line, std::string &name, std::string &filename)
{
	auto pos=line.find(';');
	if(pos==std::string_view::npos)
		return false;
	std::string cd{line.substr(0, pos)};
	std::transform(cd.begin(), cd.end(), cd.begin(),
		[](unsigned char c){return std::tolower(c);});
	if(cd!="form-data")
		return false;
	line.remove_prefix(pos+1);

	//remove spaces in front if any, split to key and val by = and remove spaces from key back if any
	//lowercase key, erase it from line, clear spaces from val front if any, read val till ; or end
	while(!line.empty())
	{
		while(!line.empty()&&line.front()==' ')
			line.remove_prefix(1);
		pos=line.find('=');
 		if(pos==std::string_view::npos)
			return false;
		std::string paramName{line.substr(0, pos)};
		while(!paramName.empty()&&paramName.back()==' ')
			paramName.pop_back();
		std::transform(paramName.begin(), paramName.end(), paramName.begin(),
			[](unsigned char c){return std::tolower(c);});
		line.remove_prefix(pos+1);
		while(!line.empty()&&line.front()==' ')
			line.remove_prefix(1);
		std::string paramVal;

		pos=findSemicolon(line);
		paramVal=line.substr(0, pos);

		if(paramVal.empty())
			return false;
		line.remove_prefix(paramVal.size());
		if(paramVal.front()=='"'&&paramVal.back()=='"')
		{
			paramVal.erase(0, 1);
			if(paramVal.empty())
				return false;
			paramVal.pop_back();
		}
		//only opening or closing quote, malformed data
		else if(paramVal.front()=='"'||paramVal.back()=='"')
			return false;
		if(paramName=="name")
		{
			if(paramVal.empty())
				return false;
			name=paramVal;
		}
		else if(paramName=="filename")
			filename=paramVal;
		if(!line.empty()&&line.front()==';')
			line.remove_prefix(1);
	}
	return true;
}

MultipartParser::~MultipartParser()
{
	if(result_!=nullptr)
	{
		auto *multipartFormData=result_->multipartFormData;
		if(multipartFormData!=nullptr)
		{
			for(auto &[key, val]: *multipartFormData)
				if(val!=nullptr)
				{
					for(auto &it: *multipartFormData->at(key))
						if(it!=nullptr)
						{
							if(it->content!=nullptr)
								delete it->content;
							delete it;
						}
					delete val;
				}
			delete multipartFormData;
		}
		delete result_;
	}
	if(formData_!=nullptr)
		delete formData_;

	delete boundary_;
}
