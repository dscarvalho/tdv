#include "config.h"
#include <fstream>
#include <unordered_map>
#include <json/json.hpp>

using nlohmann::json;

void Config::load(const string &filename)
{
    json jsonConf;

    std::ifstream ifile(filename);
    ifile >> jsonConf;

    lang = jsonConf[SOURCE_LANG];
    linkSearchDepth = jsonConf[LINK_SEARCH_DEPTH];

    for (const std::string& language : jsonConf[LANGUAGES])
    {
        languages.push_back(language);
    }

    wiktDBPath = jsonConf[WIKT_DB];
    vectorFilePath = jsonConf[VECTOR_FILE];
    humanReadable = jsonConf[HUMAN_READABLE];

    linkWeights.link_weak = jsonConf[LINK_WEIGHTS][LINK_WEAK];
    linkWeights.link_context = jsonConf[LINK_WEIGHTS][LINK_CONTEXT];
    linkWeights.link_pos = jsonConf[LINK_WEIGHTS][LINK_POS];
    linkWeights.link_etym = jsonConf[LINK_WEIGHTS][LINK_ETYM];
    linkWeights.link_strong = jsonConf[LINK_WEIGHTS][LINK_STRONG];
    linkWeights.link_hyp = jsonConf[LINK_WEIGHTS][LINK_HYP];
    linkWeights.link_hom = jsonConf[LINK_WEIGHTS][LINK_HOM];
    linkWeights.link_syn = jsonConf[LINK_WEIGHTS][LINK_SYN];
    linkWeights.link_transl = jsonConf[LINK_WEIGHTS][LINK_TRANSL];
}

