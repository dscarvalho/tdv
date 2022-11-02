#ifndef _STRINGUTIL_H_
#define _STRINGUTIL_H_
#include <string>
#include <clocale>
#include <locale>
#include <cstdlib>
#include <vector>
#include <unordered_map>
#include <regex>
#include <sstream>

class StringUtils
{
    public:
    static std::string escapeChar(char);
    
    static std::string escapeString(const std::string &);
    
    static std::vector<std::string> escapeStrings(const std::vector<std::string> &); 

    static std::vector<std::string> split(const std::string &, const std::vector<std::string> &);

    static std::vector<std::string> split(const std::string &, const std::string &);
    
    static std::vector<std::string> split(const std::string &);

    static std::string join(const std::vector<std::string> &, const std::string &);

    static std::string toLower(const std::string& str);
};


#endif
