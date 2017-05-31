#ifndef WIKTDB_H
#define WIKTDB_H

#include <string>
#include <vector>
#include <unordered_map>
#include <json/json.hpp>
#include "types.h"

using nlohmann::json;

#define LANG_OFFSET_LIMIT 1000
#define MEANING_OFFSET_LIMIT 10000

#define FLD_LANGS "langs"
#define FLD_MEANINGS "meanings"
#define FLD_ATTRS "attrs"
#define FLD_LINKS "links"
#define FLD_EXAMPLES "examples"
#define FLD_TRANSLATIONS "translations"
#define FLD_TRANSL "transl"
#define FLD_SYNONYMS "synonyms"
#define FLD_ANTONYMS "antonyms"
#define FLD_ABBREV "abbreviations"
#define FLD_TITLE "title"
#define FLD_ID "id"
#define FLD_MEANING_DESCR "meaning"
#define FLD_ETYMOLOGY "etymology"
#define FLD_ETYM_PREFIX "prefix"
#define FLD_ETYM_SUFFIX "suffix"
#define FLD_ETYM_CONFIX "confix"
#define FLD_ETYM_AFFIX "affix"
#define FLD_ETYM_STEM "stem"
#define FLD_POS_ORDER "pos_order"
#define FLD_REDIRECT "redirect"

enum ReprOffsetBase
{
    weak = 0,
    strong = 1,
    context = 2,
    synonym = 3,
    hypernym = 4,
    homonym = 5,
    abbreviation = 6,
    etymLink = 7,
    prefix = 8,
    suffix = 9,
    confix = 10,
    affix = 11,
    stem = 12,
    translation = 13,
    pos = 14
};

class WiktDB
{
    private:
    vector<json> *db;
    umap<string, ulong> posIdx;
    vector<string> posTags;
    bool isLoaded;

    public:
    umap<string, ulong> invIndex;

    WiktDB()=default;
    ~WiktDB();

    void loadDB(const string& filename);
    umap<string, long>::size_type size();
    ulong index(const string& term);
    bool exists(const string& term);
    ulong linkIndex(const string& term, ReprOffsetBase offsetBase);
    ulong etymLinkIndex(const string& term);
    ulong etymDecompIndex(const string& term, const string& type);
    ulong posIndex(const string& pos);
    const string& posName(ulong index);
    ulong reprSize();
    json& operator[] (const string& term);
    json& operator[] (vector<json>::size_type idx);
};

#endif
