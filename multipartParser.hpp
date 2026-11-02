#ifndef MULTIPART_PARSER_HPP
#define MULTIPART_PARSER_HPP

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
		ParserState currentState_=MultipartParserState::WaitingForData;
};

#endif
