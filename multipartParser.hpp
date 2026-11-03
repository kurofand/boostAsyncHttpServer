#ifndef MULTIPART_PARSER_HPP
#define MULTIPART_PARSER_HPP

#include <string>
#include <cstdint>
#include <string_view>
#include <unordered_map>

#include "common.hpp"
#include "request.hpp"

enum class MultipartParserState
{
	WaitingForData,
	Boundary,
	Headers,
	Body,
	Complete,
	Error
};

class MultipartParser
{
	public:
		MultipartParser(std::string_view boundary, const uint16_t &contentLength):
			boundary_(boundary), contentLength_(contentLength){};
		ReadResult parseChunk(std::string_view chunk);
		MultipartParserState currentState(){return currentState_;}
		~MultipartParser();
	private:
		const uint16_t contentLength_;
		std::string_view boundary_;
		std::string remainingPart_;
		std::unordered_map<std::string, std::vector<MultipartFormData*>*> *multipartFormData_=nullptr;
		MultipartFormData *formData_=nullptr;
		std::string key;
		MultipartParserState currentState_=MultipartParserState::WaitingForData;
		bool parseContentDisposition(std::string_view line, std::string &name, std::string &filename);
		size_t findSemicolon(std::string_view line);
};

#endif
