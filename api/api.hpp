#ifndef API_HPP
#define API_HPP

#include "../common.hpp"
#include "../request.hpp"

class Api
{
	public:
		virtual void execute()=0;

		virtual ~Api();
	protected:
		explicit Api(const Request *request, const ApiContract contract);

		const Request *request_;
		const ApiContract contract_;
		bool valid;
	private:
		bool checkParams(ParamType type);
		bool checkBody();
};

#endif
