#ifndef COMMON_HPP
#define COMMON_HPP

#include <vector>
#include <cstdint>
#include <string>
#include <unordered_map>

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


struct ProtocolVersion
{
	uint8_t maj, min;
};

struct MultipartFormData
{
	std::string fileName, contentType;
	std::string *content;

//	void clear(){fileName=""; contentType=""; content=nullptr;}
};

struct ParsingResult
{
	std::string *plainText;
	std::unordered_map<std::string, std::vector<std::string>> *formData;
	std::unordered_map<std::string, std::vector<MultipartFormData*>*> *multipartFormData;
};

#endif
