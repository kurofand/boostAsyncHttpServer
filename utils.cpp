#include "utils.hpp"

size_t findSemicolon(std::string_view sv)
{
        size_t pos=0;
        bool quoted=false;
        while(pos<sv.size())
        {
                if(sv.at(pos)==';'&&!quoted)
                        break;
                if(sv.at(pos)=='"')
                        if((pos>0&&sv.at(pos-1)!='\\')||pos==0)
                                quoted=!quoted;
                pos++;
        }
        return pos;
}

std::string urlDecode(std::string src)
{
	std::string res;
	char c;
	int ii;
	for(unsigned i=0;i<src.size();i++)
		if(src[i]=='%')
		{
			sscanf(src.substr(i+1, 2).c_str(), "%x", &ii);
			c=static_cast<char>(ii);
			res+=c;
			i+=2;
		}
		else
			res+=src[i];
	return res;
}
