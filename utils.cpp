#include "utils.hpp"

namespace
{
	int hexValue(unsigned char c)
	{
		if(c>='0'&&c<='9')
			return c-'0';
		if(c>='A'&&c<='F')
			return c-'A'+10;
		if(c>='a'&&c<='f')
			return c-'a'+10;

		return -1;
	}
}

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

bool urlDecode(std::string_view src, std::string &res)
{
	for(size_t i=0;i<src.size();i++)
	{
		if(src[i]!='%')
		{
			res+=src[i];
			continue;
		}
		if(i+2>=src.size())
			return false;

		int hi=hexValue(static_cast<unsigned char>(src[i+1]));
		int lo=hexValue(static_cast<unsigned char>(src[i+2]));
		if(hi<0||lo<0)
			return false;

		res+=static_cast<char>((hi<<4)|lo);
		i+=2;
	}
	return true;
}

bool isTChar(unsigned char c)
{
	return 
		(c>='0'&&c<='9')||
		(c>='A'&&c<='Z')||
		(c>='a'&&c<='z')||
		c=='!'||c=='#'||c=='$'||
		c=='%'||c=='&'||c=='\''||
		c=='*'||c=='+'||c=='-'||
		c=='.'||c=='^'||c=='_'||
		c=='`'||c=='|'||c=='~';
}
