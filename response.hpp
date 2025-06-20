#ifndef RESPONSE_HPP
#define RESPONSE_HPP

#include <unordered_map>
#include <cstdint>

class Response
{
	public:
		void responseCode(uint16_t code){responseCode_=code;}
		void addHeader(std::string &&header, std::string &&val){headers_[header]=val;}
		void setContentLength();
		void basicRealm(std::string &&s){basicRealm_=s;}
		void body(std::string &&s){body_=s;}
		void toStream(std::ostream &stream);
		uint16_t responseCode(){return responseCode_;}

		static const std::unordered_map<uint16_t, std::string> responseCodes;

	private:
		uint16_t responseCode_=200;
		std::unordered_map<std::string, std::string> headers_;
		const char* proto="HTTP/1.0";
		std::string basicRealm_;
		std::string body_;
};

#endif
