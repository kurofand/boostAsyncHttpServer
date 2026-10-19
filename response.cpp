#include "response.hpp"
#include <ostream>

//most common response codes and corresponding strings
const std::unordered_map<uint16_t, std::string> Response::responseCodes=
{
	{200, "OK"},
	{204, "No content"},
	{205, "Reset content"},

	{304, "Not modified"},

	{400, "Bad Request"},
	{401, "Unauthorized"},
	{403, "Forbidden"},
	{404, "Not Found"},
	{408, "Request Timeout"},
	{413, "Payload Too Large"},
	{415, "Unsupported Media Type"},
	{431, "Request Header Fields Too Large"},

	{500, "Internal Server Error"}
};

void Response::toStream(std::ostream &stream)
{
//goal is fill stream for writing to socket with protocol
	stream<<proto<<" ";
//response code
	stream<<responseCode_<<" ";
	if(responseCodes.contains(responseCode_))
		stream<<responseCodes.at(responseCode_);
	stream<<"\r\n";

//response headers
	for(auto &[key, val]:headers_)
		stream<<key<<": "<<val<<"\r\n";
	stream<<"\r\n";

//and response body
//	stream<<body_<<"\r\n";
	if(shouldHaveBody())
		stream<<body_;
}

void Response::setConnectionStatus()
{
	headers_["connection"]=keepAlive_?"keep-alive":"close";
}

void Response::setContentLength()
{
//server should always send content-length header
//important note: if response closed with "\r\n" length should be body_.size()+2
//content-length header forbidden by HTTP specification for 204 response
	if(responseCode_!=204)
		headers_["content-length"]=std::to_string(body_.size());
}

bool Response::shouldHaveBody()
{
	return
		requestMethod_!=RequestMethod::HEAD&&
		responseCode_!=204&&
		responseCode_!=205&&
		responseCode_!=304;
}
