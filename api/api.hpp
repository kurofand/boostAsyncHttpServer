#ifndef API_HPP
#define API_HPP

#include "../common.hpp"
#include "../request.hpp"

class Api
{
	public:
		explicit Api(const Request *request, const ApiContract contract);

		virtual void execute()=0;

		virtual ~Api();
	protected:
		const Request *request_;
		bool valid;
	private:
		const ApiContract contract_;
		bool checkParams(ParamType type);
		bool checkBody();
};

#endif
