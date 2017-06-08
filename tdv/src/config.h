#ifndef CONFIG_H
#define CONFIG_H

#include <string>
#include "types.h"

#define CONFIG_PATH "cfg/global.conf"

#define SOURCE_LANG "lang"
#define LANGUAGES "languages"
#define LINK_WEIGHTS "link_weights"

#define LINK_WEAK "link_weak"
#define LINK_CONTEXT "link_context"
#define LINK_POS "link_pos"
#define LINK_ETYM "link_etym"
#define LINK_STRONG "link_strong"
#define LINK_HYP "link_hyp"
#define LINK_HOM "link_hom"
#define LINK_SYN "link_syn"
#define LINK_TRANSL "link_transl"

#define LINK_SEARCH_DEPTH "link_search_depth"

#define WIKT_DB "wikt_db_path"
#define MEANING_FILE "meaning_file_path"
#define HUMAN_READABLE "human_readable"

struct LinkWeights
{
    float link_weak;
    float link_context;
    float link_pos;
    float link_etym;
    float link_strong;
    float link_hyp;
    float link_hom;
    float link_syn;
    float link_transl;
};

class Config
{
    public:
    string lang;
    vector<string> languages;
    uint linkSearchDepth;
    LinkWeights linkWeights;
    string wiktDBPath;
    string meaningFilePath;
    bool humanReadable;

    void load(const string& configFilePath);
};


#endif // CONFIG_H
