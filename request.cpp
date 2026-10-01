#include "request.hpp"
#include <iostream>
#include <sstream>
#include <stdio.h>

#include <algorithm>
#include <cstring>
//#include <curl/curl.h>

Request::Request(std::string_view body)
{
	body_=body;
}

bool Request::parseHeaders(std::istream &is, const unsigned bytesToRead)
{
	std::string line;
	getline(is, line);
	std::cout<<"Bytes to read: "<<bytesToRead<<std::endl;
	auto readBytes=line.size()+1;
	if(line.back()=='\r')
		line.pop_back();

	{
	auto sp=line.find(' ');
	if(sp==std::string::npos)
	{
		std::cout<<"Request line does not contain a space, aborting\n";
		return false;
	}
	std::string method=line.substr(0, sp);
	if(methodMap_.contains(method))
		method_=methodMap_.at(method);
	else
	{
		std::cout<<"Unknown HTTP method: \""<<method<<"\", aborting\n";
		return false;
	}

	std::cout<<"Method: \""<<method<<"\"\n";
	line.erase(0, sp+1);

	sp=line.rfind(' ');
	std::string proto=line.substr(sp+1);
	if(!proto.starts_with("HTTP"))
	{
		std::cout<<"Unknown protocol: \""<<proto<<"\", aborting\n";
		return false;
	}
	protocol_=proto;

	std::cout<<"Proto: \""<<protocol_<<"\"\n";
	line.erase(sp);
	}

	{
	auto q=line.find('?');
	if(q!=std::string::npos)
	{
		if(params_==nullptr)
			params_=new std::unordered_map<std::string, std::string>();
		const std::string params{line.substr(q+1)};
		fillMapFromString(params, '&', params_);
		line.erase(q);
		std::cout<<"Params: \n";
		for(const auto &[key, val]: *params_)
			std::cout<<"\t\""<<key<<"\": \""<<val<<"\"\n";
	}
	path_=line;
	std::cout<<"URI: \""<<path_<<"\"\n";
	}

	//single line request
	if(readBytes==bytesToRead)
		return true;

	if(headers_==nullptr)
		headers_=new std::unordered_map<std::string, std::string>();

	while(readBytes<bytesToRead)
	{
		getline(is, line);
		readBytes+=line.size()+1;
		if(line.back()=='\r')
			line.pop_back();
		auto delimiter=line.find(':');
		if(delimiter==std::string::npos)
			continue;
		auto name=line.substr(0, delimiter);
		std::transform(name.begin(), name.end(), name.begin(),
			[](unsigned char c){return std::tolower(c);});
		auto val=line.substr(delimiter+1);
		if(val[0]==' ')
			val=val.substr(1);

		if(name=="cookie")
		{
			if(cookies_==nullptr)
				cookies_=new std::unordered_map<std::string, std::string>();
			const std::string sCookies=val;
			fillMapFromString(sCookies, ';', cookies_);
			continue;
		}
		headers_->insert(std::pair<std::string, std::string>(name, val));
	}

	if(headers_->contains("content-type"))
		if(contentTypeMap_.contains(headers_->at("content-type")))
			contentType_=contentTypeMap_.at(headers_->at("content-type"));

	std::cout<<"Headers:\n";
	for(const auto &[key, val]: *headers_)
		std::cout<<"\t\""<<key<<"\": \""<<val<<"\"\n";

	return true;
}

uint16_t Request::parseBody()
{
	//body empty, nothing to parse; if request has to have a body, but there is no, handle on API level
	if(body_.empty())
		return 200;

	//ignore body for non POST, PUT or DELETE requests; still can be parsed on API level if necessary
	if(method_!=RequestMethod::POST&&method_!=RequestMethod::PUT&&method_!=RequestMethod::DELETE)
		return 200;

	//not supported content type
	if(contentType_==ContentType::NONE&&!body_.empty())
		return 415;

	switch(contentType_)
	{
		case(ContentType::TEXT_PLAIN):
		{
			//just a text, no need to parse
			return 200;
		}
		case(ContentType::APPLICATION_X_WWW_FORM_URLENCODED):
		{
			if(formData_==nullptr)
				formData_=new std::unordered_map<std::string, std::vector<std::string>*>();
			std::stringstream ss(body_);
			std::string line;
			while(getline(ss, line, '&'))
			{
				//malformed body, something like "param1&" instead of "param1=&"
				const auto delimiter=line.find('=');
				if(delimiter==std::string::npos)
					return 400;
				std::string name=line.substr(0, delimiter), val=line.substr(delimiter+1);
				if(formData_->contains(name))
					formData_->at(name)->push_back(val);
				else
				{
					auto *vec=new std::vector<std::string>();
					vec->push_back(val);
					formData_->insert(std::pair<std::string, std::vector<std::string>*>(name, vec));
				}
			}
			break;
		}
	};

	return 200;
}

bool Request::parse()
{
	std::istringstream s(std::string{body_});
	std::string line;
	//get first line and try to recognize request method, path, params and protocol
	std::getline(s, line);
	std::size_t size=0;
	std::cout<<"Parsing request...\n"<<"\tMethod: ";
	if(method_==RequestMethod::NONE)
		if(line.find("GET")!=std::string::npos)
		{
			std::cout<<"GET\n";
			method_=RequestMethod::GET;
			size=3;
		}
		else if(line.find("POST")!=std::string::npos)
		{
			std::cout<<"POST\n";
			method_=RequestMethod::POST;
			size=4;
		}
		else if(line.find("HEAD")!=std::string::npos)
		{
			std::cout<<"HEAD\n";
			method_=RequestMethod::HEAD;
			size=4;
		}
		else if(line.find("PUT")!=std::string::npos)
		{
			std::cout<<"PUT\n";
			method_=RequestMethod::PUT;
			size=3;
		}
		else if(line.find("CONNECT")!=std::string::npos)
		{
			std::cout<<"CONNECT\n";
			method_=RequestMethod::CONNECT;
			size=7;
		}
		else if(line.find("OPTIONS")!=std::string::npos)
		{
			std::cout<<"OPTIONS\n";
			method_=RequestMethod::OPTIONS;
			size=7;
		}
		else
		{
			std::cout<<"Failed to parse method, aborting\n";
			return false;
		}
	line.erase(0, size+1);
	protocol_=line.substr(line.rfind(' ')+1);
	std::cout<<"\tProtocol: "<<protocol_<<std::endl;;
	if(protocol_.find("HTTP")==std::string::npos)
		return false;
	line.erase(line.rfind(' '));
	if(line.find('?')!=std::string::npos)
	{
		std::cout<<"\tFound params, parsing...\n";
/*		std::stringstream sParams(line.substr(line.find('?')+1));
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
		}*/
		if(params_==nullptr)
			params_=new std::unordered_map<std::string, std::string>();
		const std::string paramsLine(line.substr(line.find('?')+1));
		fillMapFromString(paramsLine, '&', params_);
		for(const auto& [key, val]: *params_)
			std::cout<<"\t\t\""<<key<<"\": \""<<val<<"\"\n";
		line.erase(line.find('?'));
	}
	path_=line;
	//first line parsed, parse headers
	std::cout<<"\tParsing headers...\n";
	if(headers_==nullptr)
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
		std::transform(name.begin(), name.end(), name.begin(),
			[](unsigned char c){return std::tolower(c);});
		auto val=line.substr(delimiter+1);
		if(val[0]==' ')
			val=val.substr(1);
		if(name=="cookie")
		{
			std::cout<<"\tFound Cookies, parsing...\n";
			if(cookies_==nullptr)
				cookies_=new std::unordered_map<std::string, std::string>();
			const std::string sCookies=val;
			fillMapFromString(sCookies, ';', cookies_);
			std::cout<<"\tCookies:\n";
			for(const auto& [key, val]: *cookies_)
				std::cout<<"\t\t\""<<key<<"\": \""<<val<<"\"\n";
			continue;
		}
		headers_->insert(std::pair<std::string, std::string>(name, val));
	}
	std::cout<<"\tHeaders:\n";
	for(const auto& [key, val]: *headers_)
		std::cout<<"\t\t\""<<key<<"\": \""<<val<<"\"\n";

	//support form-data POST
	if(method_==RequestMethod::POST)
	{
		std::cout<<"\tParsing form-data...\n";
		std::string boundary="";
		if(headers_->find("content-type")!=headers_->end())
		{
			boundary=headers_->at("content-type");
			boundary=boundary.substr(boundary.find("=")+1);
		}

		multipartFormData_=new std::unordered_map<std::string, std::vector<MultipartFormData*>*>();
		MultipartFormData *data=nullptr;
		bool readingHeader=false;
		std::string key;
		while(getline(s, line))
		{
			//tested with curl and had to remove \r from back. this approach faster, but not safe - if problem occures change pop_back to erase
			if(!line.empty())
				line.pop_back();
//			line.erase(line.begin()+line.rfind('\r'), line.end());

			//skipping boundaries
			//next line will be a next form entity
			if(line.find(boundary)!=std::string::npos)
			{
				//if current form field is not the first one write data to vector and create new data
				if(data!=nullptr)
				{
					if(!multipartFormData_->contains(key))
						multipartFormData_->insert({key, new std::vector<MultipartFormData*>()});
					multipartFormData_->at(key)->push_back(data);
				}
				//according to form documentation request closing boundary ends with "--",
				//so no need to continue, just break
				if(line.find(boundary+"--")!=std::string::npos)
					break;
				data=new MultipartFormData();
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
//						data->fieldName=getFormHeaderVal(" name", &line);
						key=getFormHeaderVal(" name", &line);
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

void Request::fillMapFromString(const std::string &str, const char delimiter, std::unordered_map<std::string, std::string> *map)
{
	if(map==nullptr)
		return;
	std::stringstream sstream{str};
	std::string line;
	while(getline(sstream, line, delimiter))
	{
		std::size_t pos=line.find('=');
		if(pos==std::string::npos)
			continue;
		std::string key=line.substr(0, pos);
		std::string val=line.substr(pos+1);
		if(map->find(key)==map->end())
			map->insert(std::pair<std::string, std::string>(key, urlDecode(std::move(val))));
		else
			map->at(key)+=","+urlDecode(std::move(val));
	}
}

Request::~Request()
{
	if(params_!=nullptr)
		delete params_;
	if(headers_!=nullptr)
		delete headers_;

	if(multipartFormData_!=nullptr)
	{
		for(auto &[key, val]:*multipartFormData_)
			if(val!=nullptr)
			{
				for(auto &it:*multipartFormData_->at(key))
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
	{
		for(auto &[key, val]: *formData_)
			if(val!=nullptr)
				delete val;
		delete formData_;
	}
}

