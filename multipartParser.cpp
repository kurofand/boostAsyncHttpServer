#include "multipartParser.hpp"

#include <sstream>
#include <algorithm>

ReadResult MultipartParser::parseChunk(std::string_view chunk)
{
	std::string tmp=remainingPart_+chunk.data();
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
				if(chunk.size()<boundary_.size()+4)
				{
					parsing=false;
//					remainingPart_=chunk.data();
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
				std::string tmp=boundary_.data();
				if(line!="--"+tmp+"\r\n")
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
				//it there is only a part of headers trying to find last full line
				if(headersEnd==std::string_view::npos)
				{
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
				while(getline(ss, line, '\n'))
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
				currentState_=MultipartParserState::Headers;
				break;
			}
			case MultipartParserState::Body:
			{
				size_t pos;
				{
					std::string tmp=boundary_.data();
					pos=chunk.find("\r\n--"+tmp);
				}
				//all data between first character and boundary
				//or last character if there is no boundary in the chunk is body
				if(pos!=std::string_view::npos)
					currentState_=MultipartParserState::WaitingForData;
				else
					pos=chunk.size();

				if(formData_->content==nullptr)
					formData_->content=new std::string(chunk.substr(0, pos));
				else
					formData_->content->append(chunk.substr(0, pos));
				chunk.remove_prefix(pos);
				if(currentState_==MultipartParserState::WaitingForData)
				{

					formData_=nullptr;
					key.clear();
				}
				break;
			}
		};

		remainingPart_=chunk.data();
	}
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

size_t MultipartParser::findSemicolon(std::string_view line)
{
	size_t pos=0;
	bool quoted=false;
	while(pos<line.size())
	{
		if(line.at(pos)==';'&&!quoted)
			break;
		if(line.at(pos)=='"')
			if((pos>0&&line.at(pos-1)!='\\')||pos==0)
				quoted!=quoted;
		pos++;
	}
	return pos;
}

MultipartParser::~MultipartParser()
{
	if(multipartFormData_!=nullptr)
	{
		for(auto &[key, val]: *multipartFormData_)
			if(val!=nullptr)
			{
				for(auto &it: *multipartFormData_->at(key))
					if(it!=nullptr)
					{
						if(it->content!=nullptr)
							delete it->content;
						delete it;
					}
				delete val;
			}
		delete multipartFormData_;
	}
	if(formData_!=nullptr)
		delete formData_;
}
