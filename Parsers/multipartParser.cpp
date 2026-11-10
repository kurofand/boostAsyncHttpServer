#include "multipartParser.hpp"

#include <sstream>
#include <algorithm>

#include "../Log/log.hpp"
#include "../utils.hpp"

MultipartParser::MultipartParser(std::string *boundary, const unsigned int &contentLength):
                        boundary_(boundary), contentLength_(contentLength)
{
	result_=new ParsingResult();
	result_->multipartFormData=new std::unordered_map<std::string, std::vector<MultipartFormData*>*>();
	Log(LogLevel::ParserDebug)<<"Multipart/form-data parser";
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
				Log(LogLevel::ParserDebug)<<"Waiting for boundary";
				//chunk smaller than --boundary\r\n - wait for more data
				if(chunk.size()<boundary_->size()+4)
				{
					Log(LogLevel::ParserDebug)<<"Chunk too small, waiting for more";
					parsing=false;
					break;
				}

				std::optional<Boundary> boundary=lookForBoundary(chunk);
				if(!boundary||boundary->type!=BoundaryType::Regular)
				{
					Log(LogLevel::ParserDebug)<<"Failed to find regular boundary, bad request";
					parsing=false;
					currentState_=MultipartParserState::Error;
					return ReadResult::BAD_REQUEST;
				}
				openingBoundaryParsed=true;

				chunk.remove_prefix(boundary->size);
				currentState_=MultipartParserState::Boundary;
				Log(LogLevel::ParserDebug)<<"Boundary parsing complete";
				break;
			}
			//boundary read, reading form headers
			case MultipartParserState::Boundary:
			{
				Log(LogLevel::ParserDebug)<<"Parsing headers";
				//trying to find current headers end point
				//if there is \r\n\r\n sequence, all headers fields in the chunk
				auto headersEnd=chunk.find("\r\n\r\n");
				bool gotAllHeaders=true;
				//it there is only a part of headers trying to find last full line
				if(headersEnd==std::string_view::npos)
				{
					gotAllHeaders=false;

					//case when \r\n\r\n was splitted in half by chunks
					//formData check should separate splitted headers(formData is not null)
					//from malformed or empty headers
					if(chunk.starts_with("\r\n")&&formData_!=nullptr)
					{
						chunk.remove_prefix(2);
						currentState_=MultipartParserState::Headers;
						break;
					}
					else if(chunk.starts_with("\r\n")&&formData_==nullptr)
					{
						Log(LogLevel::ParserDebug)<<"Empty or malformed headers, bad request";
						currentState_=MultipartParserState::Error;
						return ReadResult::BAD_REQUEST;
					}

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
						Log(LogLevel::ParserDebug)<<"No \":\" in header line, bad request";
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
						Log(LogLevel::ParserDebug)<<"Empty value, bad request";
						currentState_=MultipartParserState::Error;
						return ReadResult::BAD_REQUEST;
					}

					if(h=="content-disposition")
					{
						if(!parseContentDisposition(v, key, formData_->fileName)||key.empty())
						{
							Log(LogLevel::ParserDebug)<<"Failed to parse content disposition, bad request";
							currentState_=MultipartParserState::Error;
							return ReadResult::BAD_REQUEST;
						}
					}
					else if(h=="content-type")
						formData_->contentType=v;
				}
				if(gotAllHeaders)
				{
					currentState_=MultipartParserState::Headers;
					Log(LogLevel::ParserDebug)<<"Headers parsing complete";
				}
				else
					parsing=false;
				break;
			}
			case MultipartParserState::Headers:
			{
				Log(LogLevel::ParserDebug)<<"Parsing body";
				const auto boundary=lookForBoundary(chunk);
				if(!boundary)
				{
					parsing=false;
					break;
				}

				size_t pos=boundary->pos;

				if(formData_->content==nullptr)
					formData_->content=new std::string(chunk.substr(0, pos));
				else
					formData_->content->append(chunk.substr(0, pos));
				chunk.remove_prefix(pos);

				if(!result_->multipartFormData->contains(key))
					result_->multipartFormData->insert({key, new std::vector<MultipartFormData*>()});
				result_->multipartFormData->at(key)->push_back(formData_);
				formData_=nullptr;
				key.clear();
				Log(LogLevel::ParserDebug)<<"Body parsing complete";
				if(boundary->type==BoundaryType::Regular)
					currentState_=MultipartParserState::WaitingForData;
				else
				{
					currentState_=MultipartParserState::Complete;
					Log(LogLevel::ParserDebug)<<"Closing boundary reached, parsing complete";
					return ReadResult::OK;
				}

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

bool MultipartParser::complete()
{
	return currentState_==MultipartParserState::Complete;
}

std::optional<Boundary> MultipartParser::lookForBoundary(std::string_view chunk)
{
	std::optional<Boundary> boundary=Boundary();
	size_t pos=chunk.find((openingBoundaryParsed?"\r\n--":"--")+*boundary_);
	if(pos==std::string_view::npos)
		return std::nullopt;
	boundary->pos=pos;

	size_t size=boundary_->size()+(openingBoundaryParsed?4:2);
	chunk.remove_prefix(pos+size);
	if(chunk.substr(0, 2)=="--")
	{
		size+=2;
		boundary->type=BoundaryType::Closing;
		chunk.remove_prefix(2);
	}

	while(!chunk.empty()&&(chunk.front()==' '||chunk.front()=='\t'))
	{
		size++;
		chunk.remove_prefix(1);
	}

	if((chunk.empty()||chunk.substr(0, 2)=="\r\n")&&boundary->type==BoundaryType::Closing)
	{
		boundary->size=size;
		return boundary;
	}
	if(!chunk.empty()&&chunk.substr(0, 2)!="\r\n")
		return std::nullopt;

	boundary->size=size+2;
	return boundary;
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
	{
		if(formData_->content!=nullptr)
			delete formData_->content;
		delete formData_;
	}

	delete boundary_;
}
