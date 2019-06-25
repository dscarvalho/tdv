#include <string>
#include <unordered_map>
#include <fstream>
#include "wiktdb.h"
#include "types.h"

using nlohmann::json;

WiktDB::~WiktDB()
{
    delete db;
}

void WiktDB::loadDB(const string& filename)
{
    if (isLoaded)
        return;

    json jsondb;

    std::ifstream ifile(filename);
    ifile >> jsondb;
    auto size = jsondb.size();
    db = new vector<json>(size);
    
    ulong i = 0;
    ulong posCount = 0;
    for (auto it = jsondb.begin(); it != jsondb.end(); ++it) {
        if (!invIndex.count((*it)[FLD_TITLE]))
        {
            (*db)[i] = it.value();
            invIndex[(*it)[FLD_TITLE]] = i;
            
            uint langOffset = 0;
            json& termLangRefs = (*db)[i][FLD_LANGS];
            for (json& termLangRef : termLangRefs)
            {
                uint meaningOffset = 0;
                json& meaningPosRefs = termLangRef[FLD_MEANINGS];
                json::const_iterator begin = meaningPosRefs.begin();
                json::const_iterator end = meaningPosRefs.end();

                for (auto posIt = begin; posIt != end; ++posIt)
                {
                    string pos = string(posIt.key());
                    json& meaningRefs = meaningPosRefs[pos];

                    for (json& meaningRef : meaningRefs)
                    {
                        meaningRef[FLD_ID] = (ulong)(i * LANG_OFFSET_LIMIT * MEANING_OFFSET_LIMIT + langOffset * MEANING_OFFSET_LIMIT + meaningOffset);
                        meaningOffset++;
                    }

                    if (!posIdx.count(pos))
                    {
                        posIdx[pos] = posCount;
                        posTags.push_back(pos);
                        posCount++;
                    }
                }

                langOffset++;
            }

            i++;
        }
    }

    isLoaded = true;
}

umap<string, long>::size_type WiktDB::size()
{
    return invIndex.size();
}

ulong WiktDB::index(const string& term)
{
    return invIndex.at(term);
}

bool WiktDB::exists(const string& term)
{
    return invIndex.count(term) > 0;
}

ulong WiktDB::linkIndex(const string& term, ReprOffsetBase offsetBase)
{
    return invIndex.size() * offsetBase + invIndex.at(term);
}

ulong WiktDB::etymLinkIndex(const string& term)
{
    return invIndex.size() * ReprOffsetBase::etymLink + invIndex.at(term);
}

ulong WiktDB::etymDecompIndex(const string& term, const string& type)
{
    if (type == FLD_ETYM_PREFIX)
        return invIndex.size() * ReprOffsetBase::prefix + invIndex.at(term);
    else if (type == FLD_ETYM_SUFFIX)
        return invIndex.size() * ReprOffsetBase::suffix + invIndex.at(term);
    else if (type == FLD_ETYM_CONFIX)
        return invIndex.size() * ReprOffsetBase::confix + invIndex.at(term);
    else if (type == FLD_ETYM_AFFIX)
        return invIndex.size() * ReprOffsetBase::affix + invIndex.at(term);
    else if (type == FLD_ETYM_STEM)
        return invIndex.size() * ReprOffsetBase::stem + invIndex.at(term);
    
    std::cerr << "Morphological decomposition type not recognized: " << type << std::endl;
    return invIndex.size() * ReprOffsetBase::stem + invIndex.at(term);
}

ulong WiktDB::posIndex(const string& pos)
{
    return invIndex.size() * ReprOffsetBase::pos + posIdx[pos];
}

const string& WiktDB::posName(ulong index)
{
    return posTags[(index % invIndex.size()) % posTags.size()];
}

ulong WiktDB::reprSize()
{
    return invIndex.size() * ReprOffsetBase::pos + posTags.size();
}

json& WiktDB::operator[] (const string& term)
{
    return db->at(invIndex.at(term));
}

json& WiktDB::operator[] (vector<json>::size_type idx)
{
    return db->at(idx);
}
