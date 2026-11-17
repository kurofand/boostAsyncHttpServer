#include "api.hpp"

#include <ranges>

#include "../Log/log.hpp"

Api::Api(const Request *request, const ApiContract contract):
	request_(request), contract_(contract)
{
	request=nullptr;
	valid=contract.method==request_->method()&&
		checkParams(ParamType::QUERY)&&
		checkParams(ParamType::HEADERS)&&
		checkParams(ParamType::COOKIES)&&
		checkBody();
	Log(LogLevel::ApiDebug)<<"Primary API validation check "<<(valid?"passed":"failed");
}

bool Api::checkParams(ParamType type)
{
	ParamSet set;
	const std::unordered_map<std::string, std::string> *params;
	switch(type)
	{
		case ParamType::QUERY:
			set=contract_.query;
			params=request_->params();
			break;
		case ParamType::HEADERS:
			set=contract_.headers;
			params=request_->headers();
			break;
		case ParamType::COOKIES:
			set=contract_.cookies;
			params=request_->cookies();
			break;
		default:
			//method was called with unknown/unsupported type
			return false;
	};

	//request params are empty or null and api awaiting nothing, so nothing to check
	if((params==nullptr||params->empty())&&set.required.empty())
		return true;

	//request query is empty or null, but api awaits params
	if((params==nullptr||params->empty())&&!set.required.empty())
		return false;

	//check all required params are there
	for(const auto &p: set.required)
		if(!params->contains(p.name))
			return false;

	//check if there are params that should not be there
	if(set.policy==UnknownParamPolicy::REJECT)
	{
		for(const auto &key: std::views::keys(*params))
			if(
				std::ranges::find_if(set.required,
					[&key](const RequestParam &param)
					{
						return param.name==key.c_str();
					})==set.required.end()&&
				std::ranges::find_if(set.optional,
					[&key](const RequestParam &param)
					{
						return param.name==key.c_str();
					})==set.optional.end()
			)
				return false;
	}

	return true;
}

bool Api::checkBody()
{
	auto set=contract_.body;
	const auto type=contract_.contentType;
	const auto *body=request_->parsedBody();

	//content type was set in contract and does not match the request one
	if(type!=ContentType::NONE&&
		type!=request_->contentType())
		return false;

	//there are required params
	if(!set.required.empty())
	{
		//but the whole body is null
		if(body==nullptr)
			return false;

		//but corresponding part of the body is null or empty
		if([&body](ContentType type)
			{
				switch(type)
				{
					case ContentType::TEXT_PLAIN:
						return body->plainText==nullptr||body->plainText->empty();
					case ContentType::APPLICATION_JSON:
						return body->jsonObj==nullptr||(body->jsonObj->is_object()&&body->jsonObj->as_object().empty());
					case ContentType::APPLICATION_X_WWW_FORM_URLENCODED:
						return body->formData==nullptr||body->formData->empty();
					case ContentType::MULTIPART_FORM_DATA:
						return body->multipartFormData==nullptr||body->multipartFormData->empty();
					default:
						return true;
				};
			}(type))

			return false;
	}

	//policy REJECT
	if(set.policy==UnknownParamPolicy::REJECT)
	{
		auto *requestKeys=body->getParamNames(type);
		if(requestKeys!=nullptr)
		{
			bool checkResult=true;
			//and some body params not in required or optional
			for(const auto &key: *requestKeys)
				if(
					std::ranges::find_if(set.required,
					[&key](const RequestParam &param)
					{
						return param.name==*key;
					})==set.required.end()&&
					std::ranges::find_if(set.optional,
					[&key](const RequestParam &param)
					{
						return param.name==*key;
					})==set.optional.end())

					{
						checkResult=false;
						break;
					}
			if(type==ContentType::APPLICATION_JSON)
				for(auto &s: *requestKeys)
					delete s;
			delete requestKeys;

			if(!checkResult)
				return false;
		}
	}

	return true;
}

Api::~Api()
{
	delete request_;
}
