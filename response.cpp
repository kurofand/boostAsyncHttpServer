#include "response.hpp"

//most common response codes and corresponding strings
const std::unordered_map<uint16_t, std::string> Response::responseCodes=
{
	{200, "OK"},
	{400, "Bad Request"},
	{401, "Unauthorized"},
	{404, "Not Found"},
	{500, "Internal Server Error"},
	{503, "Forbidden"}
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
	stream<<body_<<"\r\n";
}

void Response::setContentLength()
{
	if(!body_.empty())
		headers_["content-length"]=std::to_string(body_.size());
}
