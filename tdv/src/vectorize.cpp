#include "vectorize.h"

std::regex MARKUP_RGX("\\{\\{[^\\}]+\\}\\}");
std::regex MARKUP_WIKI_RGX("(\\{\\{|\\[\\[)(w\\||:)?([^\\}]+)(\\|[^\\}]+)?(\\}\\}|\\]\\])");
std::regex MARKUP_LABEL_RGX("(\\{\\{)(l|lb|label)\\|[a-z][a-z]\\|([^\\}\\|]+)(\\|[^\\}]+)?\\}\\})");
std::regex MARKUP_FREE_RGX("('')|(''')|(\\[\\[)|(\\]\\])|(\\&[a-z]+;)");
std::regex EXAMPLE_RGX("'''([^''']+)'''");
vector<string> DECOMP_SINGLE_FIELDS({FLD_ETYM_PREFIX, FLD_ETYM_SUFFIX, FLD_ETYM_STEM});
vector<string> DECOMP_MULTI_FIELDS({FLD_ETYM_CONFIX, FLD_ETYM_AFFIX});

vector<ReprOffsetBase> REPR_OFFSET_BASES({
            ReprOffsetBase::weak,
            ReprOffsetBase::strong,
            ReprOffsetBase::context,
            ReprOffsetBase::synonym,
            ReprOffsetBase::hypernym,
            ReprOffsetBase::homonym,
            ReprOffsetBase::abbreviation,
            ReprOffsetBase::etymLink,
            ReprOffsetBase::prefix,
            ReprOffsetBase::suffix,
            ReprOffsetBase::confix,
            ReprOffsetBase::affix,
            ReprOffsetBase::stem,
            ReprOffsetBase::translation
        });
vector<string> REPR_OFFSET_BASE_NAMES({"WEAK", "STRONG", "CONTEXT", "SYNONYM", "HYPERNYM", "HOMONYM", "ABBREVIATION",
                                        "ETYM_LINK", "PREFIX", "SUFFIX", "CONFIX", "AFFIX", "STEM", "TRANSLATION"});

float TRANSL_MAX_INTERSECT_THRESH = 0.5;
float SIM_EXPR_FIXED_GUESS = 0.6;
float SIM_SYNWEIGHT_MULTIPLIER = 3;


bool distComparator(const std::pair<ulong, float>& a, const std::pair<ulong, float>& b)
{
    return (a.second < b.second);
}


Meaning::Meaning(const string& term)
{
    this->term = term;
    this->repr = MeaningExtractor::getVector(term);
    this->pos = "";
}

Meaning::Meaning(SparseArray vec)
{
    this->repr = vec;
}

Meaning::Meaning(const string& term, const vector<string>& context)
{
    this->term = term;
    this->repr = MeaningExtractor::getVector(term, context);
    this->pos = "";
    this->context = context;
}

Meaning::Meaning(const string& term, const string& pos)
{
    this->term = term;
    this->repr = MeaningExtractor::getVector(term, pos);
    this->pos = pos;
}

Meaning::Meaning(const string& term, const string& pos, const vector<string>& context)
{
    this->term = term;
    this->repr = MeaningExtractor::getVector(term, pos, context);
    this->pos = pos;
    this->context = context;
}

Meaning::Meaning(const string& term, const string& pos, uint index)
{
    this->term = term;
    this->repr = MeaningExtractor::getVector(term, pos, index);
    this->pos = pos;
    this->index = index;
}

SparseArray Meaning::getVector()
{
    return repr;
}

vector<std::pair<ulong, float>> Meaning::similar(uint size, bool reversed)
{
    return MeaningExtractor::similarRepr(repr, size, reversed, pos);
}

WiktDB *MeaningExtractor::wiktdb;
umap<ulong, Meaning> MeaningExtractor::reprCache;
Config MeaningExtractor::config;
std::map<ulong, ulong> MeaningExtractor::effectiveDims;
set<string> MeaningExtractor::stopPOSList({"prefix", "suffix", "infix", "affix", "interfix", "article", "pronoun", 
                                           "adverb", "proverb", "letter", "conjunction", "determiner", "preposition", 
                                           "postposition", "numeral", "number", "particle", "interjection"});
bool MeaningExtractor::graphFill = false;
uint MeaningExtractor::linkSearchDepth = 1;
bool MeaningExtractor::vectorsLoaded = false;

void MeaningExtractor::setDB(WiktDB *wiktdb)
{
    MeaningExtractor::wiktdb = wiktdb;
}

bool MeaningExtractor::findTerm(const string& term, json& termRef)
{
    try
    {
        termRef = (*wiktdb)[term];
        return true;
    }
    catch (std::out_of_range e)
    {
        std::cerr << "Term not found: " << term << std::endl;
        return false;
    }
}

vector<string> MeaningExtractor::findContext(const json& meaningRef)
{
    vector<string> context;
    
    if (meaningRef.count(FLD_ATTRS))
    {
        for (const json& attrRef: meaningRef[FLD_ATTRS])
        {
            const string& attrName = attrRef[0];
            if (attrName == "context" || attrName == "label" || attrName == "lb")
            {
               vector<string> ctxWords = StringUtils::split(attrRef[1], "|");

               for (const string& ctxWord : ctxWords)
               {
                   context.push_back(ctxWord);
               }
            }
        }
    }

    return context;
}

void MeaningExtractor::findHypernymChain(vector<string>& hypernyms, const json& meaningRef, int depth, const uint fullDepth)
{
    if (depth >= 0)
    {
        const string& descr = meaningRef[FLD_MEANING_DESCR];
        string clnDescr = std::regex_replace(descr, MARKUP_RGX, "");
        vector<string> meaningDescr = StringUtils::split(clnDescr);

        for (const string& word : meaningDescr)
        {
            if (wiktdb->exists(word))
            {
                const json& headTermRef = (*wiktdb)[word];
                const json& headTermLangRef = headTermRef[FLD_LANGS].begin().value();
                const string& headTermPrimePOS = headTermLangRef[FLD_POS_ORDER][0];

                if (headTermPrimePOS == "noun" and headTermLangRef[FLD_MEANINGS].count(headTermPrimePOS))
                {
                    hypernyms.push_back(word);
                    const json& headTermPrimeMeaningRefs = headTermLangRef[FLD_MEANINGS][headTermPrimePOS];
                    const json& headTermPrimeMeaningRef = headTermPrimeMeaningRefs.begin().value();
                    MeaningExtractor::findHypernymChain(hypernyms, headTermPrimeMeaningRef, depth - 1, fullDepth);

                    break;
                }
            }
        }
    } 
}

void MeaningExtractor::fillWeak(SparseArray& vec, const json& meaningRef)
{
    vector<string> meaningWords = StringUtils::split(meaningRef[FLD_MEANING_DESCR]);
    float linkValue = config.linkWeights.link_weak;
    ReprOffsetBase linkType = ReprOffsetBase::weak;

    if (meaningWords.size() == 1)
    {
        linkValue = config.linkWeights.link_strong;
        linkType = ReprOffsetBase::strong;
    }

    for(const string& word : meaningWords)
    {
        if (wiktdb->exists(word))
        {
            vec[wiktdb->linkIndex(word, linkType)] = linkValue;
        }
        else 
        {
            string lcword = StringUtils::toLower(word);

            if (wiktdb->exists(lcword))
                vec[wiktdb->linkIndex(lcword, linkType)] = linkValue;
        }
    }
}

void MeaningExtractor::fillContext(SparseArray& vec, const vector<string>& context)
{
    for (const string& ctxWord : context)
    {
        if (wiktdb->exists(ctxWord))
        {
            vec[wiktdb->linkIndex(ctxWord, ReprOffsetBase::context)] = config.linkWeights.link_context;
        }
    }
}

void MeaningExtractor::fillStrong(SparseArray& vec, const json& meaningRef)
{
    if (meaningRef.count(FLD_LINKS))
    {
        for (const string& link: meaningRef[FLD_LINKS])
        {
            if (wiktdb->exists(link))
            {
                vec[wiktdb->linkIndex(link, ReprOffsetBase::strong)] = config.linkWeights.link_strong;
            }
        }
    }
}

void MeaningExtractor::fillSynonym(SparseArray& vec, const string& pos, const json& meaningRef, const json& termRef, const vector<string>& context)
{
    if (termRef.count(FLD_REDIRECT) && wiktdb->exists(termRef[FLD_REDIRECT][0]))
    {
        vec[wiktdb->linkIndex(termRef[FLD_REDIRECT][0], ReprOffsetBase::synonym)] = config.linkWeights.link_syn;
        return;
    }

    if (meaningRef.count(FLD_LINKS) && meaningRef[FLD_LINKS].size() == 1)
    {
        const string& link = meaningRef[FLD_LINKS][0];
        if (wiktdb->exists(link))
        {
            const string& meaningDescr = meaningRef[FLD_MEANING_DESCR];
            if (std::regex_match(meaningDescr, std::regex("^not? .*")))
                vec[wiktdb->linkIndex(link, ReprOffsetBase::synonym)] = -config.linkWeights.link_syn;
            else if (std::regex_match(meaningDescr, std::regex("^(a|an)? " + link)))
                vec[wiktdb->linkIndex(link, ReprOffsetBase::synonym)] = config.linkWeights.link_syn;
        }
    }
    
    umap<string, float> synTypes ({{FLD_SYNONYMS, config.linkWeights.link_syn}, {FLD_ANTONYMS, -config.linkWeights.link_syn}});

    for (auto it = synTypes.begin(); it != synTypes.end(); ++it)
    {
        for (const string& lang : MeaningExtractor::config.languages)
        {
            if (!termRef[FLD_LANGS].count(lang))
                continue;

            if (termRef[FLD_LANGS][lang].count(it->first) and termRef[FLD_LANGS][lang][it->first].count(pos))
            {
                for (const json& synRef : termRef[FLD_LANGS][lang][it->first][pos])
                {
                    if (synRef.count(FLD_ATTRS))
                    {
                        for (const json& attrRef : synRef[FLD_ATTRS])
                        {
                            const string& attrName = attrRef[0];
                            if (attrName == "sense")
                            {
                                vector<string> senseWords = StringUtils::split(attrRef[1], ",");

                                for (const string& senseWord : senseWords)
                                {
                                    if (wiktdb->exists(senseWord))
                                    {
                                        if (checkContextSyn(senseWord, meaningRef, context))
                                        {
                                            vec[wiktdb->linkIndex(senseWord, ReprOffsetBase::synonym)] = it->second;
                                        }
                                    }
                                }
                            }
                            else if (attrName == "l" || attrName == "label")
                            {
                                const string& synTerm = attrRef[1];
                                if (wiktdb->exists(synTerm))
                                    vec[wiktdb->linkIndex(synTerm, ReprOffsetBase::synonym)] = it->second;
                            }
                        }
                    }

                    else
                    { 
                        if (wiktdb->exists(synRef[FLD_MEANING_DESCR]))
                        {
                            vec[wiktdb->linkIndex(synRef[FLD_MEANING_DESCR], ReprOffsetBase::synonym)] = it->second;
                        }
                    }
                }
            }
        }
    }
}

void MeaningExtractor::fillHypernymChain(SparseArray& vec, const json& meaningRef)
{
    vector<string> hypernyms;
    MeaningExtractor::findHypernymChain(hypernyms, meaningRef, 3, 3);

    for (const string& hyp : hypernyms)
        vec[wiktdb->linkIndex(hyp, ReprOffsetBase::hypernym)] = config.linkWeights.link_hyp;
}

void MeaningExtractor::fillGraph(SparseArray& vec, const string& pos, const json& meaningRef, int depth, const uint fullDepth)
{
    SparseArray graphVec;
    if (depth >= 0)
    {
        vector<string> links;
        
        if (meaningRef.count(FLD_LINKS))
        {
            for (const string& link: meaningRef[FLD_LINKS])
            {
                links.push_back(link);
            }
        }
        
        if (meaningRef.count(FLD_ATTRS))
        {
            for (const json& attrRef : meaningRef[FLD_ATTRS])
            {
                const string& attrName = attrRef[0];
                if (attrName == "inflec")
                {
                    const string& stem = attrRef[2];
                    links.push_back(stem);
                }
            }
        }


        for (const string& link: links)
        {
            if (wiktdb->exists(link))
            {
                const json& linkTermRef = (*wiktdb)[link];
                
                for (const string& lang : MeaningExtractor::config.languages)
                {
                    if (!linkTermRef[FLD_LANGS].count(lang))
                        continue;

                    const json& linkDocLangRef = linkTermRef[FLD_LANGS][lang];
                    const json& linkDocMeaningRefs = linkDocLangRef[FLD_MEANINGS];
                    
                    if (linkDocMeaningRefs.count(pos))
                    {
                        const json& linkMeaningRefs = linkDocMeaningRefs[pos];
    
                        for (const json& linkMeaningRef : linkMeaningRefs)
                        {
                            MeaningExtractor::fillGraph(vec, pos, linkMeaningRef, depth - 1, fullDepth);
                        }

                    }

                    if (pos != "noun" && linkDocMeaningRefs.count("noun"))
                    {
                        const json& linkMeaningRefs = linkDocMeaningRefs["noun"];
    
                        for (const json& linkMeaningRef : linkMeaningRefs)
                        {
                            MeaningExtractor::fillGraph(vec, "noun", linkMeaningRef, depth - 1, fullDepth);
                        }
                    }

                    for (auto posIt = linkDocMeaningRefs.begin(); posIt != linkDocMeaningRefs.end(); ++posIt)
                    {
                        string synPOS = string(posIt.key());
                        SparseArray synVec;
                        fillSynonym(synVec, synPOS, linkDocMeaningRefs[synPOS][0], linkTermRef, vector<string>());
                        graphVec += synVec / ((fullDepth - depth + 1) * 2);
                    }
                    
                    graphVec[wiktdb->linkIndex(link, ReprOffsetBase::weak)] = config.linkWeights.link_strong / ((fullDepth - depth + 1) * 2);
                }
            }
        }

        vec += graphVec;
    }
}

void MeaningExtractor::fillInflec(SparseArray& vec, const json& meaningRef)
{
    if (meaningRef.count(FLD_ATTRS))
    {
        for (const json& attrRef : meaningRef[FLD_ATTRS])
        {
            const string& attrName = attrRef[0];
            if (attrName == "inflec")
            {
                const string& stem = attrRef[2];
                {
                    if (wiktdb->exists(stem))
                    {
                        vec[wiktdb->linkIndex(stem, ReprOffsetBase::stem)] = config.linkWeights.link_pos;
                        vec[wiktdb->linkIndex(stem, ReprOffsetBase::strong)] = config.linkWeights.link_strong;
                    }
                }
            }
        }
    }
}

void MeaningExtractor::fillMorphoInfo(SparseArray& vec, const string& pos, const json& termRef)
{
    vec[wiktdb->posIndex(pos)] = config.linkWeights.link_pos;
    vec[wiktdb->linkIndex(termRef[FLD_TITLE], ReprOffsetBase::homonym)] = config.linkWeights.link_hom;

    if (!termRef.count(FLD_LANGS))
        return;

    for (const json& termLangRef : termRef[FLD_LANGS])
    {
        if (termLangRef.count(FLD_ETYMOLOGY))
        {
            if (termLangRef[FLD_ETYMOLOGY].count(FLD_LINKS))
            {
                for (const string& link : termLangRef[FLD_ETYMOLOGY][FLD_LINKS])
                {
                    if (wiktdb->exists(link))
                        vec[wiktdb->etymLinkIndex(link)] = config.linkWeights.link_etym;
                }
            }
 
            for (const string& decompField : DECOMP_SINGLE_FIELDS)
            {
                if (termLangRef[FLD_ETYMOLOGY].count(decompField))
                {
                    const string& morpheme = termLangRef[FLD_ETYMOLOGY][decompField];
                    if (wiktdb->exists(morpheme))
                        vec[wiktdb->etymDecompIndex(morpheme, decompField)] = config.linkWeights.link_etym;
                }
            }
            
            for (const string& decompField : DECOMP_MULTI_FIELDS)
            {
                if (termLangRef[FLD_ETYMOLOGY].count(decompField))
                {
                    for (const string& morpheme : termLangRef[FLD_ETYMOLOGY][decompField])
                    {
                        if (wiktdb->exists(morpheme))
                            vec[wiktdb->etymDecompIndex(morpheme, decompField)] = config.linkWeights.link_etym;
                    }
                }
            }
        }
    }
}

void MeaningExtractor::fillTranslation(SparseArray& vec, const string& pos, const json& meaningRef, const json& termRef)
{
    vector<string> meaningWords = StringUtils::split(StringUtils::toLower(meaningRef[FLD_MEANING_DESCR]));
    set<string> meaningWordSet = set<string>(meaningWords.begin(), meaningWords.end());
    float intersect = 0.0;
    float maxIntersect = 0.0;
    json *selTransl;

    if (!termRef.count(FLD_LANGS))
        return;

    for (const json& termLangRef : termRef[FLD_LANGS])
    {
        if (termLangRef.count(FLD_TRANSLATIONS) && termLangRef[FLD_TRANSLATIONS].count(pos))
        {
            if (termLangRef[FLD_TRANSLATIONS][pos].size() == 1)
            {
                selTransl = &(json&)termLangRef[FLD_TRANSLATIONS][pos][0][FLD_TRANSL];
                maxIntersect = 1.0;
            }
            else
            {
                for (const json& translMeaning: termLangRef[FLD_TRANSLATIONS][pos])
                {
                    vector<string> translMeaningWords = StringUtils::split(StringUtils::toLower(translMeaning[FLD_MEANING_DESCR]));
                    set<string> translMeaningWordSet = set<string>(translMeaningWords.begin(), translMeaningWords.end());
                    vector<string> wordIntersec;

                    std::set_intersection(meaningWordSet.begin(), meaningWordSet.end(),
                                          translMeaningWordSet.begin(), translMeaningWordSet.end(),
                                          std::back_inserter(wordIntersec));

                    intersect = float(wordIntersec.size()) / meaningWordSet.size();
                    if (intersect > maxIntersect)
                    {
                        maxIntersect = intersect;
                        selTransl = &(json&)translMeaning[FLD_TRANSL];
                    }
                }
            }
        }
    }

    if (maxIntersect > TRANSL_MAX_INTERSECT_THRESH)
    {
        for (auto translTermIt = selTransl->begin(); translTermIt != selTransl->end(); ++translTermIt)
        {
            //const string& translTermLang = translTermIt.key();
            json& translTermLst = translTermIt.value();

            for (json& translTermRef : translTermLst)
            {
                const string& translTerm = translTermRef[0];

                if (wiktdb->exists(translTerm))
                    vec[wiktdb->linkIndex(translTerm, ReprOffsetBase::translation)] = config.linkWeights.link_transl;
            }
        }
    }
}

void MeaningExtractor::fillAll(SparseArray& vec, const string& pos, const json& meaningRef, const json& termRef, const vector<string>& context)
{
    fillWeak(vec, meaningRef);
    fillContext(vec, context);
    fillStrong(vec, meaningRef);
    fillSynonym(vec, pos, meaningRef, termRef, context);
    fillHypernymChain(vec, meaningRef);
    fillInflec(vec, meaningRef);
    fillMorphoInfo(vec, pos, termRef);
    fillTranslation(vec, pos, meaningRef, termRef);

    if (MeaningExtractor::graphFill)
        fillGraph(vec, pos, meaningRef, MeaningExtractor::linkSearchDepth, MeaningExtractor::linkSearchDepth);
}

bool MeaningExtractor::checkContextSyn(const string& senseWord, const json& meaningRef, const vector<string>& context)
{
    bool contextMatch = false;

    if (meaningRef.count(FLD_LINKS))
    {
        for (const string& link: meaningRef[FLD_LINKS])
        {
            if (wiktdb->exists(link) and link == senseWord)
            {
                contextMatch = true;
            }
        }
    }
    else
    {
        for (const string& ctxWord : context)
        {
            if (senseWord == ctxWord)
            {
                contextMatch = true;
                break;
            }

            if (wiktdb->exists(ctxWord))
            {
                const json& ctxRef = (*wiktdb)[ctxWord];
                
                if (ctxRef.count(FLD_SYNONYMS))
                {
                    for (const json& ctxSynRef : ctxRef[FLD_SYNONYMS])
                    {
                        if (ctxSynRef[FLD_MEANING_DESCR] == senseWord)
                        {
                            contextMatch = true;
                            break;
                        }
                    }
                }
                
                if (ctxRef.count(FLD_ABBREV))
                {
                    for (const string& abbr : ctxRef[FLD_ABBREV])
                    {
                        if (abbr == senseWord)
                        {
                            contextMatch = true;
                            break;
                        }
                    }
                }
            }
        }
    }

    return contextMatch;
}

SparseArray MeaningExtractor::getVector(const string& term)
{
    return getVector(term, vector<string>());
}

SparseArray MeaningExtractor::getVector(const string& term, const string& pos)
{
    SparseArray vec;
    json termRef;
    uint vecNormDenominator = 0;
    
    if (!findTerm(term, termRef))
        return vec;

    for (const string& lang : MeaningExtractor::config.languages)
    {
        if (!termRef[FLD_LANGS].count(lang))
            continue;

        const json& docLangRef = termRef[FLD_LANGS][lang];
        const json& docMeaningRefs = docLangRef[FLD_MEANINGS];

        if (!docMeaningRefs.count(pos))
            return vec;

        const json& meaningRefs = docMeaningRefs[pos];
        vecNormDenominator += meaningRefs.size();
       
        for (const json& meaningRef : meaningRefs)
        {
            SparseArray meaningVec = MeaningExtractor::getVector(meaningRef, termRef, pos);
            vec += meaningVec;
        }
    }

    vec /= vecNormDenominator;
    
    return vec;
}

SparseArray MeaningExtractor::getVector(const string& term, const vector<string>& context)
{
    SparseArray vec;
    json termRef;
    
    if (!findTerm(term, termRef))
        return vec;

    uint numSelected = 0;

    for (const string& lang : MeaningExtractor::config.languages)
    {
        if (!termRef[FLD_LANGS].count(lang))
            continue;

        const json& meaningPosRefs = termRef[FLD_LANGS][lang][FLD_MEANINGS];
        json::const_iterator begin = meaningPosRefs.begin();
        json::const_iterator end = meaningPosRefs.end();

        for (auto posIt = begin; posIt != end; ++posIt)
        {
            string pos = string(posIt.key());
            
            if (context.size() > 0)
                vec += MeaningExtractor::getVector(term, pos, context);
            else
                vec += MeaningExtractor::getVector(term, pos);

            numSelected += meaningPosRefs[pos].size();
        }
    }

    if (numSelected)
        vec /= numSelected;
    
    return vec;
}


SparseArray MeaningExtractor::getVector(const string& term, const string& pos, const vector<string>& context)
{
    SparseArray vec;
    json termRef;
     
    if (!findTerm(term, termRef))
        return vec;
    
    for (const string& lang : MeaningExtractor::config.languages)
    {
        if (!termRef[FLD_LANGS].count(lang))
            continue;

        const json& docLangRef = termRef[FLD_LANGS][lang];
        const json& docMeaningRefs = docLangRef[FLD_MEANINGS];

        if (!docMeaningRefs.count(pos))
            return vec;

        const json& meaningRefs = docMeaningRefs[pos];
        float minTotalDistance = std::numeric_limits<float>::max();

        for (const json& meaningRef : meaningRefs)
        {
            vector<string> meaningCtx = findContext(meaningRef);
            vector<string> meaningDescr = StringUtils::split(meaningRef[FLD_MEANING_DESCR]);
            SparseArray meaningVec = MeaningExtractor::getVector(meaningRef, termRef, pos);
            SparseArray extMeaningVec = meaningVec;
            float totalDistance = 0.0;
            fillGraph(extMeaningVec, pos, meaningRef, config.linkSearchDepth, config.linkSearchDepth);
            
            for (const string& inputCtxWord : context)
            {
                if (wiktdb->exists(inputCtxWord))
                {
                    vector<json> inputCtxMeanings;
                    bool skip = false;

                    for (const string& lang : MeaningExtractor::config.languages)
                    {
                        if (!(*wiktdb)[inputCtxWord][FLD_LANGS].count(lang))
                            continue;

                        const json& inputCtxTermLangRef = (*wiktdb)[inputCtxWord][FLD_LANGS][lang];

                        for (const string& stopPOS : MeaningExtractor::stopPOSList)
                        {
                            if (string(inputCtxTermLangRef[FLD_MEANINGS].begin().key()) == stopPOS)
                            {
                                skip = true;
                                break;
                            }   
                        }

                        if (skip) break;

                        json::const_iterator begin = inputCtxTermLangRef[FLD_MEANINGS].begin();
                        json::const_iterator end = inputCtxTermLangRef[FLD_MEANINGS].end();

                        for (auto posIt = begin; posIt != end; ++posIt)
                        {
                            string pos = string(posIt.key());
                            const json& inputCtxMeaningRefs = inputCtxTermLangRef[FLD_MEANINGS][pos];

                            for (const json& inputCtxMeaningRef : inputCtxMeaningRefs)
                            {
                                inputCtxMeanings.push_back(inputCtxMeaningRef);
                            }
                        }
                    }
                    
                    if (skip) continue;

                    for (const string& ctxWord : meaningCtx)
                    {
                        if (ctxWord == inputCtxWord)
                        {
                            skip = true;
                            break;
                        }

                    }

                    if (meaningRef.count(FLD_LINKS))
                    { 
                        for (const string& link: meaningRef[FLD_LINKS])
                        {
                            if (link == inputCtxWord)
                            {
                                skip = true;
                                break;
                            }
                        }
                    }

                    for (const string& ctxWord : meaningDescr)
                    {
                        if (ctxWord == inputCtxWord)
                        {
                            skip = true;
                            break;
                        }

                    }

                    if (skip) continue;
                    
                    
                    bool graphFill = MeaningExtractor::graphFill;
                    MeaningExtractor::graphFill = true;

                    float maxRelatedness = 0.0;
                    for (const json& inputCtxMeaningRef : inputCtxMeanings)
                    {
                        SparseArray inputCtxVec = MeaningExtractor::reprCache[inputCtxMeaningRef[FLD_ID]].getVector();
                        float relatedness = fabs(SparseArray::cosine(extMeaningVec, inputCtxVec));
                        
                        if (relatedness > maxRelatedness)
                            maxRelatedness = relatedness;
                    }

                    MeaningExtractor::graphFill = graphFill;

                    totalDistance += 1 - maxRelatedness;
                }
            }

            if (totalDistance < minTotalDistance)
            {
                vec = meaningVec;
                minTotalDistance = totalDistance;
            }
        }
    }

    return vec;
}

SparseArray MeaningExtractor::getVector(const string& term, const string& pos, uint index)
{
    SparseArray vec;
    json termRef;
    
    if (!findTerm(term, termRef))
        return vec;

    const json& docLangRef = termRef[FLD_LANGS][MeaningExtractor::config.languages[0]];
    const json& docMeaningRefs = docLangRef[FLD_MEANINGS];

    if (!docMeaningRefs.count(pos))
        return vec;

    const json& meaningRefs = docMeaningRefs[pos];
    const json& meaningRef = meaningRefs[index];
    vec = MeaningExtractor::getVector(meaningRef, termRef, pos);

    return vec;
}

SparseArray MeaningExtractor::getVector(const json& meaningRef, const json& termRef, const string& pos)
{
    SparseArray vec;

    if (MeaningExtractor::reprCache.count(meaningRef[FLD_ID]) && !MeaningExtractor::graphFill)
    {
        return MeaningExtractor::reprCache[meaningRef[FLD_ID]].getVector();
    }
    
    vector<string> context = findContext(meaningRef);
    MeaningExtractor::fillAll(vec, pos, meaningRef, termRef, context);

    return vec;
}

vector<ulong> MeaningExtractor::getMeaningRefIds(const string& term, const string& pos)
{
    vector<ulong> meaningRefIds;
    json termRef;

    if (!findTerm(term, termRef))
        return meaningRefIds;

    for (const string& lang : MeaningExtractor::config.languages)
    {
        if (!termRef[FLD_LANGS].count(lang))
            continue;

        const json& docLangRef = termRef[FLD_LANGS][lang];
        const json& docMeaningRefs = docLangRef[FLD_MEANINGS];

        if (pos != "")
        {
            if (!docMeaningRefs.count(pos))
                break;

            const json& meaningRefs = docMeaningRefs[pos];

            for (const json& meaningRef : meaningRefs)
            {
                meaningRefIds.push_back(meaningRef[FLD_ID]);
            }
        }
        else
        {
            const json& meaningPosRefs = termRef[FLD_LANGS][lang][FLD_MEANINGS];
            json::const_iterator begin = meaningPosRefs.begin();
            json::const_iterator end = meaningPosRefs.end();

            for (auto posIt = begin; posIt != end; ++posIt)
            {
                string pos = string(posIt.key());

                const json& meaningRefs = docMeaningRefs[pos];

                for (const json& meaningRef : meaningRefs)
                {
                    meaningRefIds.push_back(meaningRef[FLD_ID]);
                }
            }
        }
    }

    return meaningRefIds;
}

vector<std::pair<ulong, float>> MeaningExtractor::similar(const string& term, uint size, bool reversed)
{
    return similar(term, size, reversed, ""); 
}

vector<std::pair<ulong, float>> MeaningExtractor::similar(const string& term, uint size, bool reversed, const string& pos)
{
    return similar(term, size, reversed, pos, vector<string>());
}

vector<std::pair<ulong, float>> MeaningExtractor::similar(const string& term, uint size, bool reversed, const string& pos, const vector<string>& context)
{
    SparseArray vec1;

    if (context.size() > 0)
    {
        vec1 = getVector(term, pos, context);
        if (vec1.size() == 0)
        {
            vec1 = getVector(term, pos);
        }
    }
    else if (pos != "")
    {
        vec1 = getVector(term, pos);
    }
    else
    {
        vec1 = getVector(term);
    } 

    return similarRepr(vec1, size, reversed, pos);
}

vector<std::pair<ulong, float>> MeaningExtractor::similarRepr(const SparseArray& vec, uint size, bool reversed)
{
    return similarRepr(vec, size, reversed, "");
}

vector<std::pair<ulong, float>> MeaningExtractor::similarRepr(const SparseArray& vec, uint size, bool reversed, const string& pos)
{
    vector<std::pair<ulong, float>> compTerms(MeaningExtractor::reprCache.size());

    ulong i = 0;
    for (auto it = MeaningExtractor::reprCache.begin(); it != MeaningExtractor::reprCache.end(); ++it)
    {
        Meaning& meaning = it->second;
        const SparseArray& vec2 = meaning.getVector();

        if (pos == "" || meaning.pos == pos)
            compTerms[i] = std::make_pair(it->first, SparseArray::cosine(vec, vec2));

        i++;
    }

    std::sort(compTerms.begin(), compTerms.end(), distComparator);

    vector<std::pair<ulong, float>> results;
    for (uint i = 0; i < size; i++)
    {
        if (!reversed)
        {
            results.push_back(compTerms.back());
            compTerms.pop_back();
        }
        else
        {
            results.push_back(compTerms[i]);
        }
    }

    return results;
}

Meaning& MeaningExtractor::disambiguate(const string& term, const string& pos, const vector<string>& context)
{
    vector<ulong> meaningRefIds = getMeaningRefIds(term, pos);
    vector<std::pair<ulong, float>> contextDist(meaningRefIds.size());
    uint i = 0;

    for (ulong mRefId : meaningRefIds)
    {
        Meaning& meaning = MeaningExtractor::reprCache[mRefId];
        contextDist[i] = std::make_pair(mRefId, 0);

        for (const string& ctxTerm : context)
        {
            contextDist[i].second += std::fabs(SparseArray::cosine(meaning.repr, Meaning(ctxTerm).getVector()));
        }

        i++;
    }

    std::sort(contextDist.begin(), contextDist.end(), distComparator);

    return MeaningExtractor::reprCache[contextDist.back().first];

}

float MeaningExtractor::similarity(const string& term1, const string& pos1, const string& term2, const string& pos2, const vector<string>& context, float scale)
{
    SparseArray vec1, vec2;
    //MeaningExtractor::graphFill = true;
    MeaningExtractor::graphFill = false;
    uint linkSearchDepth = MeaningExtractor::linkSearchDepth;
    MeaningExtractor::linkSearchDepth = 1;

    Meaning concept1 = (pos1 != "") ? Meaning(term1, pos1) : Meaning(term1);
    Meaning concept2 = (pos2 != "") ? Meaning(term2, pos2) : Meaning(term2);
    MeaningExtractor::graphFill = false;
    MeaningExtractor::linkSearchDepth = linkSearchDepth;

    return MeaningExtractor::similarity(concept1, concept2, scale);
}

float MeaningExtractor::similarity(const Meaning& concept1, const Meaning& concept2, float scale)
{
    SparseArray vec1, vec2;

    vec1 = concept1.repr;
    vec2 = concept2.repr;
    const string& term1 = concept1.term;
    const string& term2 = concept2.term; 
    float linkSynWeight = MeaningExtractor::config.linkWeights.link_syn;
    const float synWeightMult = SIM_SYNWEIGHT_MULTIPLIER;

    if (SparseArray::keyIntersectionSize(vec1, vec2) == 0)
        return 0.0;

    
    ulong idx_synterm1 = wiktdb->size() * ReprOffsetBase::synonym + wiktdb->index(term1);
    ulong idx_synterm2 = wiktdb->size() * ReprOffsetBase::synonym + wiktdb->index(term2);
    ulong idx_strterm1 = wiktdb->size() * ReprOffsetBase::strong + wiktdb->index(term1);
    ulong idx_strterm2 = wiktdb->size() * ReprOffsetBase::strong + wiktdb->index(term2);

    if (wiktdb->exists(term1 + " " + term2) || wiktdb->exists(term2 + " " + term1))
    {
        return SIM_EXPR_FIXED_GUESS;
    }

    if (vec1.count(idx_strterm2))
    {
        vec1[idx_strterm2] += linkSynWeight * synWeightMult;
        vec2[idx_strterm2] = linkSynWeight * synWeightMult;
    }

    if (vec2.count(idx_strterm1))
    {
        vec2[idx_strterm1] += linkSynWeight * synWeightMult;
        vec1[idx_strterm1] = linkSynWeight * synWeightMult;
    }

    if (vec1.count(idx_synterm2) || vec2.count(idx_synterm1))
    {
        float syn_scaling = linkSynWeight;

        if (vec1.count(idx_synterm2) && vec2.count(idx_synterm1))
            syn_scaling *= 2;

        if (std::fabs(vec1.get(idx_synterm2)) > 0)
        {
            vec1[idx_synterm2] *= syn_scaling;
            vec1[idx_synterm1] = std::fabs(vec1[idx_synterm2]);

            if (std::fabs(vec1[idx_synterm2]) > std::fabs(vec2[idx_synterm2]))
                vec2[idx_synterm2] = std::fabs(vec1[idx_synterm2]);
            else
                vec1[idx_synterm2] =  (vec1[idx_synterm2] / std::fabs(vec1[idx_synterm2])) * vec2[idx_synterm2];

        }
        else if (std::fabs(vec2.get(idx_synterm1)) > 0)
        {
            vec2[idx_synterm1] *= syn_scaling;
            vec2[idx_synterm2] = std::fabs(vec2[idx_synterm1]);

            if (std::fabs(vec2[idx_synterm1]) > std::fabs(vec1[idx_synterm1]))
                vec1[idx_synterm1] = std::fabs(vec2[idx_synterm1]);
            else
                vec2[idx_synterm1] = (vec2[idx_synterm1] / std::fabs(vec2[idx_synterm1])) * vec1[idx_synterm1];
        }

        for (auto it = vec1.begin(); it != vec1.end(); ++it)
        {
            if (it->first < wiktdb->size() * ReprOffsetBase::synonym || it->first >= wiktdb->size() * (ReprOffsetBase::synonym + 1))
            {
                if (it->second < linkSynWeight)
                    it->second = 0;
            }
            else if (std::fabs(it->second) < std::fabs(vec1[idx_synterm2]))
            {
                it->second = 0;
            }
        }

        for (auto it = vec2.begin(); it != vec2.end(); ++it)
        {
            if (it->first < wiktdb->size() * ReprOffsetBase::synonym || it->first >= wiktdb->size() * (ReprOffsetBase::synonym + 1))
            {
                if (it->second < linkSynWeight)
                    it->second = 0;
            }
            else if (std::fabs(it->second) < std::fabs(vec2[idx_synterm1]))
            {
                it->second = 0;
            }
        }
    }

    return SparseArray::weightedCosine(vec1, vec2, 1.0, scale);
}

float MeaningExtractor::similarity(const Meaning& concept1, const Meaning& concept2, float scale)
{
    SparseArray vec1, vec2;

    vec1 = concept1.repr;
    vec2 = concept2.repr;
    const string& term1 = concept1.term;
    const string& term2 = concept2.term; 
    float linkSynWeight = MeaningExtractor::config.linkWeights.link_syn;
    const float synWeightMult = SIM_SYNWEIGHT_MULTIPLIER;

    if (SparseArray::keyIntersectionSize(vec1, vec2) == 0)
        return 0.0;

    
    ulong idx_synterm1 = wiktdb->size() * ReprOffsetBase::synonym + wiktdb->index(term1);
    ulong idx_synterm2 = wiktdb->size() * ReprOffsetBase::synonym + wiktdb->index(term2);
    ulong idx_strterm1 = wiktdb->size() * ReprOffsetBase::strong + wiktdb->index(term1);
    ulong idx_strterm2 = wiktdb->size() * ReprOffsetBase::strong + wiktdb->index(term2);

    if (wiktdb->exists(term1 + " " + term2) || wiktdb->exists(term2 + " " + term1))
    {
        return SIM_EXPR_FIXED_GUESS;
    }

    if (vec1.count(idx_strterm2))
    {
        vec1[idx_strterm2] += linkSynWeight * synWeightMult;
        vec2[idx_strterm2] = linkSynWeight * synWeightMult;
    }

    if (vec2.count(idx_strterm1))
    {
        vec2[idx_strterm1] += linkSynWeight * synWeightMult;
        vec1[idx_strterm1] = linkSynWeight * synWeightMult;
    }

    if (vec1.count(idx_synterm2) || vec2.count(idx_synterm1))
    {
        float syn_scaling = linkSynWeight;

        if (vec1.count(idx_synterm2) && vec2.count(idx_synterm1))
            syn_scaling *= 2;

        if (std::fabs(vec1.get(idx_synterm2)) > 0)
        {
            vec1[idx_synterm2] *= syn_scaling;
            vec1[idx_synterm1] = std::fabs(vec1[idx_synterm2]);

            if (std::fabs(vec1[idx_synterm2]) > std::fabs(vec2[idx_synterm2]))
                vec2[idx_synterm2] = std::fabs(vec1[idx_synterm2]);
            else
                vec1[idx_synterm2] =  (vec1[idx_synterm2] / std::fabs(vec1[idx_synterm2])) * vec2[idx_synterm2];

        }
        else if (std::fabs(vec2.get(idx_synterm1)) > 0)
        {
            vec2[idx_synterm1] *= syn_scaling;
            vec2[idx_synterm2] = std::fabs(vec2[idx_synterm1]);

            if (std::fabs(vec2[idx_synterm1]) > std::fabs(vec1[idx_synterm1]))
                vec1[idx_synterm1] = std::fabs(vec2[idx_synterm1]);
            else
                vec2[idx_synterm1] = (vec2[idx_synterm1] / std::fabs(vec2[idx_synterm1])) * vec1[idx_synterm1];
        }

        for (auto it = vec1.begin(); it != vec1.end(); ++it)
        {
            if (it->first < wiktdb->size() * ReprOffsetBase::synonym || it->first >= wiktdb->size() * (ReprOffsetBase::synonym + 1))
            {
                if (it->second < linkSynWeight)
                    it->second = 0;
            }
            else if (std::fabs(it->second) < std::fabs(vec1[idx_synterm2]))
            {
                it->second = 0;
            }
        }

        for (auto it = vec2.begin(); it != vec2.end(); ++it)
        {
            if (it->first < wiktdb->size() * ReprOffsetBase::synonym || it->first >= wiktdb->size() * (ReprOffsetBase::synonym + 1))
            {
                if (it->second < linkSynWeight)
                    it->second = 0;
            }
            else if (std::fabs(it->second) < std::fabs(vec2[idx_synterm1]))
            {
                it->second = 0;
            }
        }
    }

    return SparseArray::weightedCosine(vec1, vec2, 1.0, scale);
}

void MeaningExtractor::loadVectorsFromFile(const string& meaningFilename)
{
    json meaningList;

    try
    {
        std::ifstream ifile(meaningFilename);
        ifile.exceptions(std::ifstream::eofbit | std::ifstream::failbit | std::ifstream::badbit);
        ifile >> meaningList;
    }
    catch (std::system_error& e)
    {
        std::cerr << "Error opening meaning file: " << meaningFilename << ". Preloading vectors from DB." << std::endl;
        preloadVectors();
        return;
    }

    for (auto it = meaningList.begin(); it != meaningList.end(); ++it)
    {
        Meaning meaning;
        meaning.term = (*it)[FLD_TERM];
        meaning.pos = (*it)[FLD_POS];
        meaning.descr = (*it)[FLD_DESCR];
        meaning.lang = (*it)[FLD_LANG];
        meaning.repr = fromJsonRepr((*it)[FLD_REPR], config.humanReadable);

        MeaningExtractor::reprCache[(*it)[FLD_ID]] = meaning;
    }

    MeaningExtractor::vectorsLoaded = true;
}

void MeaningExtractor::preloadVectors()
{
    if (MeaningExtractor::vectorsLoaded)
        return;

    ulong progressCount = 0;

    for (auto it = wiktdb->invIndex.begin(); it != wiktdb->invIndex.end(); ++it)
    {
        const json& termRef = (*wiktdb)[it->first];

        for (const string& lang : MeaningExtractor::config.languages)
        {
            if (!termRef[FLD_LANGS].count(lang))
                continue;

            const json& meaningPosRefs = termRef[FLD_LANGS][lang][FLD_MEANINGS];
            json::const_iterator begin = meaningPosRefs.begin();
            json::const_iterator end = meaningPosRefs.end();

            for (auto posIt = begin; posIt != end; ++posIt)
            {
                string pos = string(posIt.key());
                const json& meaningRefs = meaningPosRefs[pos];

                for (const json& meaningRef : meaningRefs)
                {
                    SparseArray vec = MeaningExtractor::getVector(meaningRef, termRef, pos);
                    Meaning meaning(vec);
                    meaning.term = termRef[FLD_TITLE];
                    meaning.pos = pos;
                    meaning.descr = meaningRef[FLD_MEANING_DESCR];
                    meaning.lang = lang;

                    if (meaningRef.count(FLD_EXAMPLES))
                    {
                        for (const string& example : meaningRef[FLD_EXAMPLES])
                        {
                            Example ex;
                            std::smatch mo;

                            ex.sentence = std::regex_replace(example, MARKUP_WIKI_RGX, "$3");
                            ex.sentence = std::regex_replace(ex.sentence, MARKUP_LABEL_RGX, "$3");
                            std::regex_search(example, mo, EXAMPLE_RGX);
                            for (uint i = 1; i < mo.size(); i++)
                            {
                                ex.termStartEndPositions.push_back(std::make_pair(mo.position(i) - (i * 3), mo.position(i) - (i * 3) +  mo[i].length() - 1));
                            }
                            
                            ex.sentence = std::regex_replace(ex.sentence, EXAMPLE_RGX, "$1");
                            ex.sentence = std::regex_replace(ex.sentence, MARKUP_FREE_RGX, "");
                            
                            if (ex.termStartEndPositions.size() == 0)
                            {
                                int start = ex.sentence.find(meaning.term);

                                if (start >= 0)
                                    ex.termStartEndPositions.push_back(std::make_pair(start, start + meaning.term.length() - 1));
                            }

                     
                            if (ex.termStartEndPositions.size() > 0)
                                meaning.examples.push_back(ex);
                        }
                    }
                    
                    MeaningExtractor::reprCache[meaningRef[FLD_ID]] = meaning;
                }
            }
        }

        progressCount++;
        uint progress = int(float(progressCount) * 100 / wiktdb->size());
        if (progress > 0 && progress % 2 == 0 && progressCount % 1000 == 0)
        {
            printf("%d%%\r", progress);
            fflush(stdout);
        }
    }

    idfWeak();
    markEffective();
    joinTranslations();

    MeaningExtractor::vectorsLoaded = true;
}

void MeaningExtractor::idfWeak()
{
    float *termIdf = new float[wiktdb->size()];

    for (ulong i = 0; i < wiktdb->size(); i++)
    {
        termIdf[i] = 0.0;
    }

    for (auto cacheIt = MeaningExtractor::reprCache.begin(); cacheIt != MeaningExtractor::reprCache.end(); ++cacheIt)
    {
        SparseArray& vec = cacheIt->second.repr;
        for (auto vecIt = vec.begin(); vecIt != vec.end(); ++vecIt)
        {
            if (vecIt->first < wiktdb->size())
            {
                termIdf[vecIt->first]++;
            }
        }
    }

    float maxIdf = log10(wiktdb->size() / std::max((float)1.0, *std::min_element(termIdf, termIdf + wiktdb->size())));

    for (auto cacheIt = MeaningExtractor::reprCache.begin(); cacheIt != MeaningExtractor::reprCache.end(); ++cacheIt)
    {
        SparseArray& vec = cacheIt->second.repr;
        for (auto vecIt = vec.begin(); vecIt != vec.end(); ++vecIt)
        {
            if (vecIt->first < wiktdb->size())
            {
                vecIt->second *= log10(wiktdb->size() / termIdf[vecIt->first]) / maxIdf;
            }
        }
    }

    delete[] termIdf;
}

void MeaningExtractor::markEffective()
{
    std::map<ulong, ulong> featFreq;

    for (auto cacheIt = MeaningExtractor::reprCache.begin(); cacheIt != MeaningExtractor::reprCache.end(); ++cacheIt)
    {
        SparseArray& vec = cacheIt->second.repr;
        for (auto vecIt = vec.begin(); vecIt != vec.end(); ++vecIt)
        {
            if (vecIt->second > 0)
            {
                if (!featFreq.count(vecIt->first))
                    featFreq[vecIt->first] = 0;
                    
                featFreq[vecIt->first]++;
            }
        }
    }

    ulong idx = 0;
    for (auto pair : featFreq)
    {
        ulong homonymIdxStart = wiktdb->size() * (ReprOffsetBase::homonym);
        ulong homonymIdxEnd =  wiktdb->size() * (ReprOffsetBase::homonym + 1);
        
        if (pair.second > 1 or !(pair.first >= homonymIdxStart and pair.first < homonymIdxEnd))
        {
            MeaningExtractor::effectiveDims[pair.first] = idx;
            idx++;
        }
    }
}

void MeaningExtractor::joinTranslations()
{
    set<ulong> joinedMeaningRefIds;
    for (auto cacheIt = MeaningExtractor::reprCache.begin(); cacheIt != MeaningExtractor::reprCache.end(); ++cacheIt)
    {
        const Meaning& meaning = cacheIt->second;
        const SparseArray& vec = cacheIt->second.repr;
        
        if (joinedMeaningRefIds.count(cacheIt->first) || meaning.lang != config.lang)
            continue;

        for (auto vecIt = vec.begin(); vecIt != vec.end(); ++vecIt)
        {
            if (vecIt->first >= wiktdb->invIndex.size() * ReprOffsetBase::translation & &
                vecIt->first < wiktdb->invIndex.size() * ReprOffsetBase::pos)
            {
                string term = (*wiktdb)[vecIt->first % wiktdb->size()]["title"].get<string>();
                vector<ulong> meaningRefIds = getMeaningRefIds(term, meaning.pos);

                if (meaningRefIds.size() == 1)
                {
                    Meaning& translMeaning = MeaningExtractor::reprCache[meaningRefIds[0]];
                    translationVectorJoin(vecIt->first, meaning, translMeaning);
                    joinedMeaningRefIds.insert(meaningRefIds[0]);
                }
                else
                {
                    for (ulong meaningRefId : meaningRefIds)
                    {
                        Meaning& translMeaning = MeaningExtractor::reprCache[meaningRefId];
                        vector<string> translDescr = StringUtils::split(translMeaning.descr);

                        if (translMeaning.descr == meaning.term ||
                            translMeaning.descr == meaning.descr ||
                            (translDescr.size() > 1 && (translDescr[0].find(meaning.term) != string::npos || translDescr[1].find(meaning.term) != string::npos)))
                        {
                            translationVectorJoin(vecIt->first, meaning, translMeaning);
                            joinedMeaningRefIds.insert(meaningRefId);
                        }
                    }
                }
            }
        }
    }
}

void MeaningExtractor::translationVectorJoin(ulong translIdx, const Meaning& meaning, Meaning& translMeaning)
{
    for (auto it = meaning.repr.begin(); it != meaning.repr.end(); ++it)
    {
        if (!translMeaning.repr.count(it->first))
            translMeaning.repr[it->first] += it->second;
    }

    translMeaning.repr[translIdx] = 0;
    translMeaning.repr[wiktdb->linkIndex(meaning.term, ReprOffsetBase::translation)] = config.linkWeights.link_transl;
}

SparseArray MeaningExtractor::effectiveRepr(const SparseArray& vec)
{
    SparseArray effectVec;

    for (auto vecIt = vec.begin(); vecIt != vec.end(); ++vecIt)
    {
        if (MeaningExtractor::effectiveDims.count(vecIt->first))
        {
            effectVec[MeaningExtractor::effectiveDims[vecIt->first]] = vecIt->second;
        }
    }
    
    return effectVec; 
}

string MeaningExtractor::stringRepr(const SparseArray& vec, const string separator, bool dense, bool named)
{
    std::stringbuf strBuf;
    std::ostream strStream(&strBuf);

    if (!dense)
    {
        bool first = true;
        for (auto pair : vec)
        {
            ulong idx = pair.first;
            ulong offset = idx % wiktdb->size();
            
            if (!first)
                strStream << separator;
            
            bool skip = false;
            for (uint i = 0; i < REPR_OFFSET_BASES.size(); i++)
            {
                if (idx < wiktdb->size() * (REPR_OFFSET_BASES[i] + 1))
                {   
                    if (named)    
                        strStream << REPR_OFFSET_BASE_NAMES[i] << "@" << (*wiktdb)[offset]["title"].get<string>() << ":" << pair.second;
                    else
                        strStream << idx << ":" << pair.second;


                    skip = true;
                    break;
                }
            }
            
            if (!skip && idx < wiktdb->size() * (ReprOffsetBase::pos + 1))
            {
                if (named)
                    strStream << "POS@" << wiktdb->posName(idx) << ":" << pair.second;
                else
                    strStream << idx << ":" << pair.second;
            }

            first = false;
        }
    }
    else
    {
        ulong reprSize = wiktdb->reprSize();
        float *denseVec = new float[reprSize];
        vec.toCArray(denseVec, reprSize);
        
        strStream << denseVec[0];

        for (ulong i = 1; i < reprSize; i++)
        {
            strStream << separator << denseVec[i];
        }

        delete[] denseVec;
    }

    return strBuf.str();
}

string MeaningExtractor::stringRepr(Meaning meaning, const string separator, bool dense, bool named)
{
    return stringRepr(meaning.repr, separator, dense, named);
}

json MeaningExtractor::jsonRepr(const SparseArray& vec, bool named)
{
    std::stringbuf strBuf;
    std::ostream strStream(&strBuf);

    json jRepr = json::object();

    for (auto pair : vec)
    {
        ulong idx = pair.first;
        string idxStr = std::to_string(idx);
        ulong offset = idx % wiktdb->size();

        bool skip = false;
        for (uint i = 0; i < REPR_OFFSET_BASES.size(); i++)
        {
            if (idx < wiktdb->size() * (REPR_OFFSET_BASES[i] + 1))
            {
                if (named)
                    jRepr[idxStr] = {{"term", (*wiktdb)[offset]["title"].get<string>()}, {"type", REPR_OFFSET_BASE_NAMES[i]}, {"type_id", REPR_OFFSET_BASES[i]}, {"value", pair.second}};
                else
                    jRepr[idxStr] = pair.second;

                skip = true;
                break;
            }
        }

        if (!skip && idx < wiktdb->size() * (ReprOffsetBase::pos + 1))
        {
            if (named)
                jRepr[idxStr] = {{"term", wiktdb->posName(idx)}, {"type", "POS"}, {"type_id", ReprOffsetBase::pos}, {"value", pair.second}};
            else
                jRepr[idxStr] = pair.second;
        }
    }

    return jRepr;
}

SparseArray MeaningExtractor::fromJsonRepr(const json& jsonVec, bool named)
{
    SparseArray vec;

    for (auto it = jsonVec.begin(); it != jsonVec.end(); ++it)
    {
        ulong idx = std::atoll(it.key().c_str());
        if (named)
            vec[idx] = it.value();
        else
            vec[idx] = it.value()["value"];
    }

    return vec;
}
