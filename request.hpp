#ifndef REQUEST_HPP
#define REQUEST_HPP

#include <unordered_map>
#include <string>
#include <string_view>
#include <vector>
#include <cstdint>

#include "common.hpp"

class Request
{
	public:
		Request(){};
		void body(std::string_view sv){body_=sv;}
		void parsedBody(ParsingResult *data){parsedBody_=data;}
		ParsingResult* parsedBody(){return parsedBody_;}
		bool parseHeaders(std::istream &is, const unsigned bytesToRead);
		RequestMethod method(){return method_;}
		std::string protocol(){return protocol_;}
		std::string path(){return path_;}
		std::unordered_map<std::string, std::string>* params(){return params_;}
		std::unordered_map<std::string, std::string>* headers(){return headers_;}
		std::unordered_map<std::string, std::string>* cookies(){return cookies_;}
		ProtocolVersion protocolVersion(){return protocolVersion_;}
		ContentType contentType(){return contentType_;}
		bool keepAlive(){return keepAlive_;}
		std::string multipartBoundary();
		~Request();

	private:
		RequestMethod method_=RequestMethod::NONE;
		ContentType contentType_=ContentType::NONE;
		std::string body_;
		std::string protocol_;
		std::string path_;
		std::unordered_map<std::string, std::string> *params_=nullptr;
		std::unordered_map<std::string, std::string> *cookies_=nullptr;
		std::unordered_map<std::string, std::string> *headers_=nullptr;
		ParsingResult *parsedBody_=nullptr;
		bool keepAlive_=false;
		ProtocolVersion protocolVersion_{0,0};

		void fillMapFromString(const std::string &str, const char delimiter, std::unordered_map<std::string, std::string> *map);
		bool manageHeaderDuplicate(std::string &key, std::string &val);
		bool parseProtoVersion();
		bool parseConnectionHeader();

		const std::unordered_map<std::string, RequestMethod> methodMap_=
		{
			{"GET", RequestMethod::GET},
			{"POST", RequestMethod::POST},
			{"HEAD", RequestMethod::HEAD},
			{"PUT", RequestMethod::PUT},
			{"DELETE", RequestMethod::DELETE},
			{"CONNECT", RequestMethod::CONNECT},
			{"OPTIONS", RequestMethod::OPTIONS}
		};
		const std::unordered_map<std::string, ContentType> contentTypeMap_=
		{
			{"text/plain", ContentType::TEXT_PLAIN},
			{"application/json", ContentType::APPLICATION_JSON},
			{"application/x-www-form-urlencoded", ContentType::APPLICATION_X_WWW_FORM_URLENCODED},
			{"multipart/form-data", ContentType::MULTIPART_FORM_DATA}
		};
};

#endif
