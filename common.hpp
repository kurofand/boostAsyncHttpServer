#ifndef COMMON_HPP
#define COMMON_HPP

enum class ReadResult: uint16_t
{
	OK=200,
	BAD_REQUEST=400,
	PAYLOAD_TOO_LARGE=413,
	HEADERS_TOO_LARGE=431,
	UNSUPPORTED_MEDIA_TYPE=415,
	TIMEOUT,
	NETWORK_ERROR
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

struct ProtocolVersion
{
	uint8_t maj, min;
};

#endif
