#ifndef EXAMPLE_HPP
#define EXAMPLE_HPP

#include "api.hpp"

class ExampleApi: public Api
{
	public:
		explicit ExampleApi(const Request *request, const ApiContract contract): Api(request, contract){};
		void execute() override;

};

#endif
