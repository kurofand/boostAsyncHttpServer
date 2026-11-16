#ifndef COMMON_HPP
#define COMMON_HPP

#include <ranges>
#include <vector>
#include <cstdint>
#include <string>
#include <unordered_map>

#include <boost/json.hpp>

enum class ReadResult: uint16_t
{
	OK=200,
	BAD_REQUEST=400,
	TIMEOUT=408,
	PAYLOAD_TOO_LARGE=413,
	UNSUPPORTED_MEDIA_TYPE=415,
	HEADERS_TOO_LARGE=431,
	NOT_IMPLEMENTED=501,
	NETWORK_ERROR,
	//scenarios when error did not occure, but connection should be closed
	CLOSE_CONNECTION
};

enum class RequestMethod
{
	NONE=0,
	GET,
	POST,
	HEAD,
	PUT,
	DELETE,
	CONNECT,
	OPTIONS
};

enum class ContentType
{
	NONE=0,
	TEXT_PLAIN,
	APPLICATION_JSON,
	APPLICATION_X_WWW_FORM_URLENCODED,
	MULTIPART_FORM_DATA
};

enum class ParamDataType
{
	STRING,
	UINT,
	INT,
	BOOL
};

enum class ParamType
{
	HEADERS,
	QUERY,
	COOKIES,
	BODY
};

enum class UnknownParamPolicy
{
	ALLOW,
	REJECT
};

struct ProtocolVersion
{
	uint8_t maj, min;
};

struct MultipartFormData
{
	std::string fileName, contentType;
	std::string *content=nullptr;
};

struct ParsingResult
{
	std::string *plainText=nullptr;
	std::unordered_map<std::string, std::vector<std::string>> *formData=nullptr;
	std::unordered_map<std::string, std::vector<MultipartFormData*>*> *multipartFormData=nullptr;
	boost::json::value *jsonObj=nullptr;

	std::vector<const std::string*>* getParamNames(const ContentType type) const
	{
		switch(type)
		{
			case ContentType::TEXT_PLAIN:
				return nullptr;
			case ContentType::APPLICATION_X_WWW_FORM_URLENCODED:
			case ContentType::MULTIPART_FORM_DATA:
			{
				if((type==ContentType::APPLICATION_X_WWW_FORM_URLENCODED&&formData==nullptr)||
					(type==ContentType::MULTIPART_FORM_DATA&&multipartFormData==nullptr))
					return nullptr;

				auto *res=new std::vector<const std::string*>();

				if(type==ContentType::MULTIPART_FORM_DATA)
					for(const auto &key: std::views::keys(*multipartFormData))
						res->push_back(&key);
				else
					for(const auto &key: std::views::keys(*formData))
						res->push_back(&key);

				return res;
			}
			case ContentType::APPLICATION_JSON:
			{
				if(jsonObj==nullptr||!jsonObj->is_object())
					return nullptr;

				auto *res=new std::vector<const std::string*>();
				for(const auto &[key, val]: jsonObj->as_object())
					res->push_back(new std::string(key));
				return res;
			}
			default:
				return nullptr;
		};
	}
};

struct RequestParam
{
	const char* name;
	ParamDataType dataType;
//	ParamType type;
};

struct ParamSet
{
	std::span<RequestParam> required;
	std::span<RequestParam> optional;
	UnknownParamPolicy policy;
};

struct ApiContract
{
	ParamSet query;
	ParamSet headers;
	ParamSet cookies;
	ParamSet body;
	RequestMethod method;
	ContentType contentType=ContentType::NONE;
};

#endif
