#include "multipartParser.hpp"

ReadResult MultipartParser::parseChunk(std::string_view chunk)
{
	ReadResult res;

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
				if(chunk.size()<boundary.size()+4)
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
				if(line!="--"+boundary+"\r\n")
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

				std::string headers=chunk.substr(0, headersEnd);
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
					std::transform(h.begin(), h.end(), h.begin,
						[](unsigned char c){return std::tolower c;});
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
						if(!parseContentDisposition(v, key, data->fileName)||key.empty())
						{
							currentState_=MultipartParserState::Error;
							return ReadResult::BAD_REQUEST;
						}
					}
					else if(h=="content-type")
						data->contentType=v;
				}
				currentState_=MultipartParserState::Headers;
				break;
			}
			case MultipartParserState::Body:
			{
				auto pos=chunk.find("\r\n--"+boundary);
				//all data between first character and boundary
				//or last character if there is no boundary in the chunk is body
				if(pos!=std::string_view::npos)
					currentState_=MultipartParserState::WaitingForData;
				else
					pos=chunk.size();

				if(data->content==nullptr)
					data->content=new std::string(chunk.substr(0, pos));
				else
					data->content->append(chunk.substr(0, pos));
				chunk.remove_prefix(pos);
				break;
			}
		};

		remainingPart_=chunk.data();
	}
	return ReadResult::OK;
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
}
