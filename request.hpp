#ifndef REQUEST_HPP
#define REQUEST_HPP

#include <unordered_map>
#include <string>
#include <string_view>
#include <vector>
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

struct formData
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
		Request(std::string_view body);
		bool parse();
		bool parseHeaders(std::istream &is, const unsigned bytesToRead);
		RequestMethod method(){return method_;}
		std::string protocol(){return protocol_;}
		std::string path(){return path_;}
		std::unordered_map<std::string, std::string>* params(){return params_;}
		std::unordered_map<std::string, std::string>* headers(){return headers_;}
		std::unordered_map<std::string, std::string>* cookies(){return cookies_;}
//		std::unordered_map<std::string, std::string*>* data(){return data_;}
//		std::vector<formData*>* data(){return data_;}
		std::unordered_map<std::string, std::vector<formData*>*>* data(){return data_;}
		~Request();

	private:
		RequestMethod method_=RequestMethod::NONE;
		std::string urlDecode(std::string src);
		std::string_view body_;
		std::string protocol_;
		std::string path_;
		std::unordered_map<std::string, std::string> *params_=nullptr;
		std::unordered_map<std::string, std::string> *cookies_=nullptr;
		std::unordered_map<std::string, std::string> *headers_=nullptr;
//		std::unordered_map<std::string, std::string*> *data_=nullptr;
		std::unordered_map<std::string, std::vector<formData*>*> *data_=nullptr;
//		std::vector<formData*> *data_=nullptr;

		std::string getFormHeaderVal(const char* headerName, std::string *line);
		void fillMapFromString(const std::string &str, const char delimiter, std::unordered_map<std::string, std::string> *map);

		const static inline std::unordered_map<std::string, RequestMethod> methodMap_=
		{
			{"GET", RequestMethod::GET},
			{"POST", RequestMethod::POST},
			{"HEAD", RequestMethod::HEAD},
			{"PUT", RequestMethod::PUT},
			{"DELETE", RequestMethod::DELETE},
			{"CONNECT", RequestMethod::CONNECT},
			{"OPTIONS", RequestMethod::OPTIONS}
		};
};

#endif
