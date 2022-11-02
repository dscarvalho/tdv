#include "stringutils.h"
#include "vectorutils.h"


std::string StringUtils::escapeChar(char character) {const std::unordered_map<char, std::string> 
        ScapedSpecialCharacters = {
            {'.', "\\."}, {'|', "\\|"}, {'*', "\\*"}, {'?', "\\?"},
            {'+', "\\+"}, {'(', "\\("}, {')', "\\)"}, {'{', "\\{"},
            {'}', "\\}"}, {'[', "\\["}, {']', "\\]"}, {'^', "\\^"},
            {'$', "\\$"}, {'\\', "\\\\"}
        };

    auto it = ScapedSpecialCharacters.find(character);
    
    if (it == ScapedSpecialCharacters.end())
    {
        return std::string(1, character);
    }
    
    return it->second;
}

std::string StringUtils::escapeString(const std::string & str)
{
    std::stringstream stream;
    std::for_each(begin(str), end(str), [&stream](const char character) { stream << escapeChar(character); });
    
    return stream.str();
}

std::vector<std::string> StringUtils::escapeStrings(const std::vector<std::string>& delimiters) 
{
    return VectorUtils::map<std::string>(delimiters, escapeString);
}

std::vector<std::string> StringUtils::split(const std::string & str, const std::vector<std::string> & delimiters) 
{
    std::regex rgx(join(escapeStrings(delimiters), "|"));
    std::sregex_token_iterator first{begin(str), end(str), rgx, -1}, last;
    
    return{first, last};
}

std::vector<std::string> StringUtils::split(const std::string & str, const std::string & delimiter) 
{
    std::vector<std::string> delimiters = {delimiter};
    
    return split(str, delimiters);
}

std::vector<std::string> StringUtils::split(const std::string & str) 
{
    return split(str, " ");
}

std::string StringUtils::join(const std::vector<std::string> & tokens, const std::string & delimiter) 
{
    std::stringstream stream;
    stream << tokens.front();
    std::for_each(begin(tokens) + 1, end(tokens), [&](const std::string &elem) 
    {
        stream << delimiter << elem;
    });

    return stream.str();
}

std::string StringUtils::toLower(const std::string& str)
{
    std::locale loc;
    std::string strLower = str;

    for (std::string::size_type i=0; i<str.length(); ++i)
        strLower[i] = std::tolower(str[i], loc);

    return strLower;
}
