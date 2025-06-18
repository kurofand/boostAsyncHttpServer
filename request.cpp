#include "request.hpp"
#include <iostream>
#include <sstream>
#include <stdio.h>
//#include <curl/curl.h>

Request::Request(std::string_view body)
{
	body_=body;
}

bool Request::parse()
{
	std::istringstream s(std::string{body_});
	std::string line;
	//get first line and try to recognize request method, path, params and protocol
	std::getline(s, line);
	std::size_t size=0;
	if(method_==RequestMethod::NONE)
		if(line.find("GET")!=std::string::npos)
		{
			method_=RequestMethod::GET;
			size=3;
		}
		else if(line.find("POST")!=std::string::npos)
		{
			method_=RequestMethod::POST;
			size=4;
		}
		else if(line.find("HEAD")!=std::string::npos)
		{
			method_=RequestMethod::HEAD;
			size=4;
		}
		else if(line.find("PUT")!=std::string::npos)
		{
			method_=RequestMethod::PUT;
			size=3;
		}
		else if(line.find("CONNECT")!=std::string::npos)
		{
			method_=RequestMethod::CONNECT;
			size=7;
		}
		else if(line.find("OPTIONS")!=std::string::npos)
		{
			method_=RequestMethod::OPTIONS;
			size=7;
		}
		else
			return false;
	line.erase(0, size+1);
	protocol_=line.substr(line.rfind(' ')+1);
	if(protocol_.find("HTTP")==std::string::npos)
		return false;
	line.erase(line.rfind(' '));
	if(line.find('?')!=std::string::npos)
	{
		std::stringstream sParams(line.substr(line.find('?')+1));
		params_=new std::unordered_map<std::string, std::string>();
		while(sParams.good())
		{
			std::string param;
			getline(sParams, param, '&');
			std::size_t pos=param.find('=');
			//if no '=' in name-val pair - skip
			if(pos==std::string::npos)
				continue;
			std::string key=param.substr(0, pos);
			std::string val=param.substr(pos+1);
			if(params_->find(key)==params_->end())
				params_->insert(std::pair<std::string, std::string>(key, urlDecode(std::move(val))));
			else
				params_->at(key)+=","+val;
		}
		line.erase(line.find('?'));
	}
	path_=line;
	//first line parsed, parse headers
	headers_=new std::unordered_map<std::string, std::string>();
	while(std::getline(s, line))
	{
		//reached end of headers if line starts from '\r'
		if(line.at(0)=='\r')
			break;
		//had an issue with some browsers that added symbols to end of the line, so decided to erase lines
		line.erase(line.begin()+line.rfind('\r'), line.end());
		auto delimiter=line.find(':');
		if(delimiter==std::string::npos)
			continue;
		auto name=line.substr(0, delimiter);
		auto val=line.substr(delimiter+1);
		if(val[0]==' ')
			val=val.substr(1);
		headers_->insert(std::pair<std::string, std::string>(name, val));
	}

	//support form-data POST
	if(method_==RequestMethod::POST)
	{
		std::string boundary="";
		if(headers_->find("content-type")!=headers_->end())
		{
			boundary=headers_->at("content-type");
			boundary=boundary.substr(boundary.find("=")+1);
		}

		data_=new std::vector<formData*>();
		formData *data=nullptr;
		bool readingHeader=false;
		while(getline(s, line))
		{
			//\r\n should be the end of the POST body
			if(line.at(0)=='\r')
				break;
			//skipping boundaries
			//next line will be a next form entity
			if(line.find(boundary)!=std::string::npos)
			{
				//if current form field is not the first one write data to vector and create new data
				if(data!=nullptr)
					data_->push_back(data);
				data=new formData();
				readingHeader=!readingHeader;
				continue;
			}
			//searching for form headers - Content-Disposition, name(for regular form-data), fileName and Content-Type(for attached files)
			if(readingHeader)
			{
				if(line.find("Content-Disposition")!=std::string::npos||line.find("Content-Type")!=std::string::npos)
				{
					if(line.find("Content-Disposition")!=std::string::npos)
					{
						data->fieldName=getFormHeaderVal(" name", &line);
						data->fileName=getFormHeaderVal(" fileName", &line);
					}
					else if(line.find("Content-Type")!=std::string::npos)
						data->contentType=line.substr(line.find(" "));
				}
				else
					readingHeader=false;
			}
			//headers readed, reading form content
			else
			{
				if(data->content==nullptr)
					data->content=new std::string();
				data->content->append(line);
			}
		}
		/*last created data must be deleted to avoid leak:
		multipart/form-data structure is
			boundary
			field headers
			\n
			field content
			boundary
			\r\n
		I allocate new data on boundary, include the last one so last allocated data is empty and must be deleted
		*/
		delete data;
	}
	return true;
}

std::string Request::urlDecode(std::string src)
{
	std::string res;
	char ch;
	int ii;
	for(unsigned int i=0;i<src.length();i++)
		if(src[i]=='%')
		{
			sscanf(src.substr(i+1,2).c_str(), "%x", &ii);
			ch=static_cast<char>(ii);
			res+=ch;
			i=i+2;
		}
		else
			res+=src[i];
	return res;
}

std::string Request::getFormHeaderVal(const char*  headerName, std::string *line)
{
	std::string res;
	auto pos=line->find(headerName);
	if(pos!=std::string::npos)
	{
		res=line->substr(pos);
		res.erase(0, strlen(headerName)+2);
		res.erase(res.begin()+res.find('"'), res.end());
	}
	return std::move(res);
}

Request::~Request()
{
	if(params_!=nullptr)
		delete params_;
	if(headers_!=nullptr)
		delete headers_;
	if(data_!=nullptr)
	{
		for(auto &it:*data_)
			if(it!=nullptr)
			{
				if(it->content!=nullptr)
					delete it->content;
				delete it;
			}
		delete data_;
	}
}

