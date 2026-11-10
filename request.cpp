#include "request.hpp"
#include <iostream>
#include <sstream>
#include <stdio.h>

#include <algorithm>
#include <cstring>

#include "Log/log.hpp"
#include "utils.hpp"

bool Request::parseHeaders(std::istream &is, const unsigned bytesToRead)
{
	Log(LogLevel::Debug)<<"Parsing headers...";
	std::string line;
	getline(is, line);
	auto readBytes=line.size()+1;
	if(!line.empty()&&line.back()=='\r')
		line.pop_back();

	{
	auto sp=line.find(' ');
	if(sp==std::string::npos)
	{
		Log(LogLevel::Warning)<<"Request line does not contain a space, aborting";
		return false;
	}
	std::string method=line.substr(0, sp);
	if(methodMap_.contains(method))
		method_=methodMap_.at(method);
	else
	{
		Log(LogLevel::Warning)<<"Unknown HTTP method: \""<<method<<"\", aborting";
		return false;
	}

	Log(LogLevel::Debug)<<"Request method: "<<method;
	line.erase(0, sp+1);

	sp=line.rfind(' ');
	std::string proto=line.substr(sp+1);
	//usual protocol str HTTP/1.0
	if(!proto.starts_with("HTTP/"))
	{
		Log(LogLevel::Warning)<<"Unknown protocol: \""<<proto<<"\", aborting";
		return false;
	}
	protocol_=proto;

	Log(LogLevel::Debug)<<"Protocol: "<<protocol_;
	line.erase(sp);
	}
	if(!parseProtoVersion())
	{
		Log(LogLevel::Warning)<<"Failed to parse protocol version, aborting";
		return false;
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
		std::string logstr="Params: \n";
		for(const auto &[key, val]: *params_)
			logstr+="\t\""+key+"\": \""+val+"\"\n";
		Log(LogLevel::Debug)<<logstr;
	}
	path_=line;
	Log(LogLevel::Debug)<<"URI: \""<<path_<<"\"";
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
		if(!line.empty()&&line.back()=='\r')
			line.pop_back();
		if(line.empty())
			continue;
		auto delimiter=line.find(':');
		if(delimiter==std::string::npos)
			return false;
		auto name=line.substr(0, delimiter);
		std::transform(name.begin(), name.end(), name.begin(),
			[](unsigned char c){return std::tolower(c);});
		auto val=line.substr(delimiter+1);
		if(!val.empty()&&val[0]==' ')
			val=val.substr(1);

		if(name=="cookie")
		{
			if(cookies_==nullptr)
				cookies_=new std::unordered_map<std::string, std::string>();
			const std::string sCookies=val;
			fillMapFromString(sCookies, ';', cookies_);
			continue;
		}

		if(headers_->contains(name))
		{
			if(!manageHeaderDuplicate(name, val))
				return false;
		}
		else
			headers_->insert(std::pair<std::string, std::string>(name, val));
	}

	//host header is mandatory by HTTP/1.1
	if(protocolVersion_.min==1&&(!headers_->contains("host")||headers_->at("host").empty()))
		return false;

	if(headers_->contains("content-length")&&headers_->contains("transfer-encoding"))
		return false;

	if(headers_->contains("content-type"))
	{
		auto contentType=headers_->at("content-type");
		//some content-type fields(e.g. multipart form data) contains additional info, erasing it to get type
		if(contentType.find(';')!=std::string::npos)
			contentType.erase(contentType.find(';'));
		if(contentTypeMap_.contains(contentType))
			contentType_=contentTypeMap_.at(contentType);
	}

	keepAlive_=parseConnectionHeader();

	std::string logstr="Headers:\n";
	for(const auto &[key, val]: *headers_)
		logstr+="\t\""+key+"\": \""+val+"\"\n";
	Log(LogLevel::Debug)<<logstr;

	return true;
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

bool Request::parseProtoVersion()
{
	if(protocol_.empty())
		return false;
	if(protocol_=="HTTP/1.0")
	{
		protocolVersion_={1,0};
		return true;
	}
	else if(protocol_=="HTTP/1.1")
	{
		protocolVersion_={1,1};
		return true;
	}
	return false;
}

//method for manage duplicates
//default policy is override
//some header duplicates are protocol violation, in this cases return false
bool Request::manageHeaderDuplicate(std::string &key, std::string &val)
{
	//content-length can have a duplicate and request is correct if values are equal
	if(key=="content-length")
	{
		if(headers_->at(key)==val)
			return true;
		return false;
	}
	else if(key=="connection")
	{
		headers_->at(key)+=", "+val;
		return true;
	}
	else if(key=="host")
	{
		//for HTTP/1.1 host duplicate is protocol violation
		if(protocolVersion_.min==1)
			return false;
		//for HTTP/1.0 just ignore duplicate
		else
			return true;
	}
	headers_->at(key)=val;
	return true;
}

//there also can be parsing for additional tokens
bool Request::parseConnectionHeader()
{
	std::string val{""};
	if(headers_->contains("connection"))
	{
		val=headers_->at("connection");
		std::transform(val.begin(), val.end(), val.begin(),
			[](unsigned char c){return std::tolower(c);});
	}

	//connection header is absent
	if(val.empty())
	{
		//keep-alive is default for HTTP/1.1 and close is default for HTTP/1.0
		if(protocolVersion_.min==1)
			return true;
		else
			return false;
	}

	//multiple tokens
	if(!val.empty()&&val.find(',')!=std::string::npos)
	{
		std::vector<std::string> tokens;
		std::stringstream ss{val};
		std::string line;
		while(getline(ss, line, ','))
			if(!line.empty())
			{
				while(!line.empty()&&line.front()==' ')
					line.erase(0, 1);
				tokens.push_back(line);
			}
		bool close=false, keepAlive=false;
		for(const auto &s: tokens)
			if(s=="close")
			{
				close=true;
				break;
			}
			else if(s=="keep-alive")
				keepAlive=true;

		//close has priority, if close was not found use default protocol behavior
		if(close)
			return false;
		else if(protocolVersion_.min==1)
			return true;
		else
			return keepAlive;
	}
	else
		if(val=="close")
			return false;
		else if(protocolVersion_.min==1)
			return true;
		else
			return val=="keep-alive";
}

std::string Request::multipartBoundary()
{
	std::string boundary;

	if(headers_==nullptr||!headers_->contains("content-type"))
		return "";
	std::string_view contentType(headers_->at("content-type").c_str(), headers_->at("content-type").size());
	auto pos=contentType.find(';');
	if(pos==std::string_view::npos)
		return "";
	contentType.remove_prefix(pos+1);
	while(!contentType.empty()&&contentType.front()==' ')
		contentType.remove_prefix(1);

	while(!contentType.empty())
	{
		pos=contentType.find('=');
		if(pos==std::string_view::npos)
			return "";

		std::string key{contentType.substr(0, pos)};
		while(!key.empty()&&key.back()==' ')
			key.pop_back();
		std::transform(key.begin(), key.end(), key.begin(),
			[](unsigned char c){return std::tolower(c);});

		contentType.remove_prefix(pos+1);
		while(!contentType.empty()&&contentType.front()==' ')
			contentType.remove_prefix(1);
		pos=findSemicolon(contentType);
		auto val=contentType.substr(0, pos);
		if(val.empty())
			return "";
		contentType.remove_prefix(val.size());
		if(val.front()=='"')
		{
			if(val.back()!='"')
				return "";
			val.remove_prefix(1);
			val.remove_suffix(1);
		}

		if(key=="boundary")
		{
			boundary=val;
			if(boundary.size()>70)
				return "";
			return boundary;
		}
		while(!contentType.empty()&&(contentType.front()==';'||contentType.front()==' '))
			contentType.remove_prefix(1);
	}

	return "";
}

Request::~Request()
{
	if(params_!=nullptr)
		delete params_;
	if(headers_!=nullptr)
		delete headers_;
	if(cookies_!=nullptr)
		delete cookies_;

	if(parsedBody_!=nullptr)
	{
		if(parsedBody_->multipartFormData!=nullptr)
		{
			auto *data=parsedBody_->multipartFormData;
			for(auto &[key, val]: *data)
				if(val!=nullptr)
				{
					for(auto &it:*data->at(key))
						if(it!=nullptr)
						{
							if(it->content!=nullptr)
								delete it->content;
							delete it;
						}
					delete val;
				}
			delete data;
		}
		if(parsedBody_->formData!=nullptr)
			delete parsedBody_->formData;

		if(parsedBody_->plainText!=nullptr)
			delete parsedBody_->plainText;

		delete parsedBody_;
	}
}

