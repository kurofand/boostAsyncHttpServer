#ifndef REQUEST_HPP
#define REQUEST_HPP

#include <map>
#include <string>
#include <string_view>

enum class RequestMethod
{
	NONE=0,
	GET,
	POST,
	HEAD,
	PUT,
	CONNECT,
	OPTIONS
};

class Request
{
	public:
		Request(std::string_view body);
		bool parse();
		RequestMethod method(){return method_;}
		std::string protocol(){return protocol_;}
		std::string path(){return path_;}
		std::map<std::string, std::string>* params(){return params_;}
		std::map<std::string, std::string>* headers(){return headers_;}
		~Request();

	private:
		RequestMethod method_=RequestMethod::NONE;
		std::string urlDecode(std::string src);
		std::string_view body_;
		std::string protocol_;
		std::string path_;
		std::map<std::string, std::string> *params_=nullptr;
		std::map<std::string, std::string> *headers_=nullptr;
};

#endif
