#ifndef REQUEST_HPP
#define REQUEST_HPP

#include <unordered_map>
#include <string>
#include <string_view>
#include <vector>
#include <cstdint>

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

struct MultipartFormData
{
//	std::string fieldName;
	std::string fileName, contentType;
	std::string *content;

//	void clear(){fieldName=""; fileName=""; contentType=""; content=nullptr;}
	void clear(){fileName=""; contentType=""; content=nullptr;}
};

class Request
{
	public:
		Request(){};
		void body(std::string_view sv){body_=sv;}
		bool parseHeaders(std::istream &is, const unsigned bytesToRead);
		//uint16_t to return parsing state with HTTP response codes:
		//200 - parsing complete w/o issues;
		//400 - incorrect body format(e.g. invalid JSON);
		//415 - unsupported content type, etc
		uint16_t parseBody();
		RequestMethod method(){return method_;}
		std::string protocol(){return protocol_;}
		std::string path(){return path_;}
		std::unordered_map<std::string, std::string>* params(){return params_;}
		std::unordered_map<std::string, std::string>* headers(){return headers_;}
		std::unordered_map<std::string, std::string>* cookies(){return cookies_;}
//		std::unordered_map<std::string, std::string*>* data(){return data_;}
//		std::vector<formData*>* data(){return data_;}
		std::unordered_map<std::string, std::vector<std::string>*>* formData(){return formData_;}
		std::unordered_map<std::string, std::vector<MultipartFormData*>*>* multipartFormData(){return multipartFormData_;}
		bool keepAlive(){return keepAlive_;}
		~Request();

	private:
		RequestMethod method_=RequestMethod::NONE;
		ContentType contentType_=ContentType::NONE;
		std::string urlDecode(std::string src);
		std::string body_;
		std::string protocol_;
		std::string path_;
		std::unordered_map<std::string, std::string> *params_=nullptr;
		std::unordered_map<std::string, std::string> *cookies_=nullptr;
		std::unordered_map<std::string, std::string> *headers_=nullptr;
//		std::unordered_map<std::string, std::string*> *data_=nullptr;
		std::unordered_map<std::string, std::vector<std::string>*> *formData_=nullptr;
		std::unordered_map<std::string, std::vector<MultipartFormData*>*> *multipartFormData_=nullptr;
		bool keepAlive_=false;
//		std::vector<formData*> *data_=nullptr;

		std::string getFormHeaderVal(const char* headerName, std::string *line);
		void fillMapFromString(const std::string &str, const char delimiter, std::unordered_map<std::string, std::string> *map);

//		const static inline std::unordered_map<std::string, RequestMethod> methodMap_=
		const std::unordered_map<std::string, RequestMethod> methodMap_=
		{
			{"GET", RequestMethod::GET},
			{"POST", RequestMethod::POST},
			{"HEAD", RequestMethod::HEAD},
			{"PUT", RequestMethod::PUT},
			{"DELETE", RequestMethod::DELETE},
			{"CONNECT", RequestMethod::CONNECT},
			{"OPTIONS", RequestMethod::OPTIONS}
		};
		const std::unordered_map<std::string, ContentType> contentTypeMap_=
		{
			{"text/plain", ContentType::TEXT_PLAIN},
			{"application/json", ContentType::APPLICATION_JSON},
			{"application/x-www-form-urlencoded", ContentType::APPLICATION_X_WWW_FORM_URLENCODED},
			{"multipart/form-data", ContentType::MULTIPART_FORM_DATA}
		};
};

#endif
