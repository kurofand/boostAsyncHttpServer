#ifndef MULTIPART_PARSER_HPP
#define MULTIPART_PARSER_HPP

#include <string>
#include <vector>
#include <cstdint>
#include <optional>
#include <string_view>
#include <unordered_map>

#include "parser.hpp"
#include "../common.hpp"

enum class MultipartParserState
{
	WaitingForData,
	Boundary,
	Headers,
	Body,
	Complete,
	Error
};

enum class BoundaryType
{
	Regular,
	Closing
};

struct Boundary
{
	BoundaryType type=BoundaryType::Regular;
	size_t pos=std::string_view::npos;
	size_t size=0;
};

class MultipartParser: public Parser
{
	public:
		MultipartParser(std::string *boundary, const unsigned int &contentLength);
		ReadResult parseChunk(std::string_view chunk) override;
		bool complete() override;
		~MultipartParser();
	private:
		const unsigned int contentLength_;
		std::string *boundary_;
		MultipartFormData *formData_=nullptr;
		std::string key;
		MultipartParserState currentState_=MultipartParserState::WaitingForData;
		bool parseContentDisposition(std::string_view line, std::string &name, std::string &filename);
		std::optional<Boundary> lookForBoundary(std::string_view chunk);
		bool openingBoundaryParsed=false;
};

#endif
