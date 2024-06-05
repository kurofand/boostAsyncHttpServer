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
		params_=new std::map<std::string, std::string>();
		while(sParams.good())
		{
			std::string param;
			getline(sParams, param, '&');
			std::size_t pos=param.find('=');
			//if no '=' in name-val pair - skip
			if(pos==std::string::npos)
				continue;
			std::string val=param.substr(pos+1);
			params_->insert(std::pair<std::string, std::string>(param.substr(0, pos), urlDecode(std::move(val))));
		}
		line.erase(line.find('?'));
	}
	path_=line;
	//first line parsed, parse headers
	headers_=new std::map<std::string, std::string>();
	while(std::getline(s, line))
	{
		auto delimiter=line.find(':');
		if(delimiter==std::string::npos)
			continue;
		auto name=line.substr(0, delimiter);
		auto val=line.substr(delimiter+1);
		if(val[0]==' ')
			val=val.substr(1);
		headers_->insert(std::pair<std::string, std::string>(name, val));
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

Request::~Request()
{
	if(params_!=nullptr)
		delete params_;
	if(headers_!=nullptr)
		delete headers_;
}

