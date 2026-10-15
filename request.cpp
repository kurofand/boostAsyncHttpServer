#include "request.hpp"
#include <iostream>
#include <sstream>
#include <stdio.h>

#include <algorithm>
#include <cstring>

bool Request::parseHeaders(std::istream &is, const unsigned bytesToRead)
{
	std::string line;
	getline(is, line);
	std::cout<<"Bytes to read: "<<bytesToRead<<std::endl;
	auto readBytes=line.size()+1;
	if(!line.empty()&&line.back()=='\r')
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
	//usual protocol str HTTP/1.0
	if(!proto.starts_with("HTTP/"))
	{
		std::cout<<"Unknown protocol: \""<<proto<<"\", aborting\n";
		return false;
	}
	protocol_=proto;

/*	{
	const size_t pos=proto.rfind('/');
	if(pos==std::string::npos||pos+1>proto.size())
		return false;
	const auto version=proto.substr(++pos);
	//according to HTTP specification 1.x versions have to be 3 symbols long
	if(version.size()!=3&&version[0]!='1'&&(version[2]!='0'||version[2]!='1'))
		return false;
	protocolVersion_.maj=version[0]-'0';
	protocolVersion_.min=version[2]-'0';
	}*/

	std::cout<<"Proto: \""<<protocol_<<"\"\n";
	line.erase(sp);
	}
	if(!parseProtoVersion())
	{
		std::cout<<"Failed to parse protocol version"<<std::endl;
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
		if(!line.empty()&&line.back()=='\r')
			line.pop_back();
		auto delimiter=line.find(':');
		if(delimiter==std::string::npos)
			continue;
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
		//TODO: figure out why parser ignores last field;
		//getline erases \n from files, figure out how to avoid it or switch from getline
		//additional field headers can be sent in both camel case and lower case, support it(currently camel case only)
		case(ContentType::MULTIPART_FORM_DATA):
		{
			auto pos=headers_->at("content-type").find("boundary=");
			//malformed headers, no boundary field in content-type
			if(pos==std::string::npos)
				return 400;
			std::string boundary=headers_->at("content-type").substr(pos+9);
			//malformed body
			if(body_.find(boundary)==std::string::npos)
				return 400;
			multipartFormData_=new std::unordered_map<std::string, std::vector<MultipartFormData*>*>();
			while(body_!="--"+boundary+"--\r\n")
			{
				//erase opening boundary, +4 is "--" at the beginning and "\r\n"
				body_.erase(0, boundary.size()+4);
				std::string bodyPart=body_.substr(0, body_.find("--"+boundary));
				//there is an "empty" line between field headers and content
				const auto headersEnd=bodyPart.find("\r\n\r\n");
				std::string key;
				MultipartFormData *data=new MultipartFormData();
				{
				std::string headers=bodyPart.substr(0, headersEnd);
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
						delete data;
						return 400;
					}
					std::string h=line.substr(0, delimiter), v=line.substr(delimiter+1);
					std::transform(h.begin(), h.end(), h.begin(),
						[](unsigned char c){return std::tolower(c);});
					if(h=="content-disposition")
					{
						key=getFormHeaderVal(" name", &line);
						data->fileName=getFormHeaderVal(" fileName", &line);
					}
					else if(h=="content-type")
						data->contentType=v;
				}
				}
				if(!multipartFormData_->contains(key))
					multipartFormData_->insert({key, new std::vector<MultipartFormData*>()});
				//erase headers part plus \r\n\r\n
				bodyPart.erase(0, headersEnd+4);
				//erase \r\n from the end
				bodyPart.erase(bodyPart.size()-2);
				data->content=new std::string(bodyPart);
				multipartFormData_->at(key)->push_back(data);
				body_.erase(0, body_.find("--"+boundary));
			}
			break;
		}
	};

	return 200;
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
	if(pos==std::string::npos)
	{
		std::string s{headerName};
		std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){return std::tolower(c);});
		pos=line->find(s);
	}
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
	//multiple tokens
	if(!val.empty()&&val.find(',')!=std::string::npos)
	{
		std::vector<std::string> tokens;
		std::stringstream ss{val};
		std::string line;
		while(getline(ss, line, ','))
			if(!line.empty())
			{
				if(line.front()==' ')
					line.erase(0, 1);
				tokens.push_back(line);
			}
		for(const auto &s: tokens)
			if(s=="close")
				return false;
	}
	else
		if(val=="close")
			return false;
	//keep-alive is default for HTTP/1.1
	return protocolVersion_.min==1;
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

