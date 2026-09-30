#include "response.hpp"
#include <ostream>

//most common response codes and corresponding strings
const std::unordered_map<uint16_t, std::string> Response::responseCodes=
{
	{200, "OK"},
	{400, "Bad Request"},
	{401, "Unauthorized"},
	{403, "Forbidden"},
	{404, "Not Found"},
	{413, "Payload Too Large"},
	{431, "Request Header Fields Too Large"},
	{500, "Internal Server Error"}
};

void Response::toStream(std::ostream &stream)
{
//goal is fill stream for writing to socket with protocol
	stream<<proto<<" ";
//response code
	stream<<responseCode_<<" "<<responseCodes.at(responseCode_)<<"\r\n";

//response headers
	for(auto &[key, val]:headers_)
		stream<<key<<": "<<val<<"\r\n";
	stream<<"\r\n";

//and response body
//	stream<<body_<<"\r\n";
	stream<<body_;
}

void Response::setContentLength()
{
//	if(!body_.empty())
//server should always send content-length header
//important note: if response closed with "\r\n" length should be body_.size()+2
		headers_["content-length"]=std::to_string(body_.size());
}
