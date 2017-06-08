#ifndef VECTORIZE_H
#define VECTORIZE_H
#include <iostream>
#include <cstdio>
#include <fstream>
#include <cmath>
#include <limits>
#include <string>
#include <vector>
#include <map>
#include <utility>
#include <algorithm>
#include <regex>
#include "types.h"
#include "wiktdb.h"
#include "sparsearray.h"
#include "stringutils.h"
#include "config.h"


bool distComparator(const std::pair<ulong, float>& a, const std::pair<ulong, float>& b);

struct Example
{
    string sentence;
    vector<std::pair<uint,uint>> termStartEndPositions;
};

class Meaning 
{
    public:
    SparseArray repr; 
    string term;
    string pos;
    string descr;
    string lang;
    uint index;
    vector<string> context;
    vector<Example> examples; 
    
    Meaning()=default;
    Meaning(const string& term);
    Meaning(SparseArray vec);
    Meaning(const string& term, const string& pos);
    Meaning(const string& term, const vector<string>& context);
    Meaning(const string& term, const string& pos, const vector<string>& context);
    Meaning(const string& term, const string& pos, uint index);
    ~Meaning()=default;
     
    SparseArray getVector();
    vector<std::pair<ulong, float>> similar(uint size, bool reversed);
};

class MeaningExtractor
{   
    static WiktDB *wiktdb;
    static vector<string> languages;
    static set<string> stopPOSList;
    static bool vectorsLoaded;
    
    static bool findTerm(const string& term, json& termRef);
    static vector<string> findContext(const json& meaningRef);
    static void findHypernymChain(vector<string>& hypernyms, const json& meaningRef, int depth, const uint fullDepth);
    static void fillWeak(SparseArray& vec, const json& meaningRef);
    static void fillContext(SparseArray& vec, const vector<string>& context);
    static void fillStrong(SparseArray& vec, const json& meaningRef);
    static void fillSynonym(SparseArray& vec, const string& pos, const json& meaningRef, const json& termRef, const vector<string>& context);
    static void fillHypernymChain(SparseArray& vec, const json& meaningRef);
    static void fillGraph(SparseArray& vec, const string& pos, const json& meaningRef, int depth, const uint fullDepth);
    static void fillInflec(SparseArray& vec, const json& meaningRef);
    static void fillMorphoInfo(SparseArray& vec, const string& pos, const json& meaningRef);
    static void fillTranslation(SparseArray& vec, const string& pos, const json& meaningRef, const json& termRef);
    static void fillAll(SparseArray& vec, const string& pos, const json& meaningRef, const json& termRef, const vector<string>& context);
    static bool checkContextSyn(const string& senseWord, const json& meaningRef, const vector<string>& context);

    public:
    static umap<ulong, Meaning> reprCache;
    static std::map<ulong, ulong> effectiveDims;
    static bool graphFill;
    static uint linkSearchDepth;
    static Config config;
    
    static void setDB(WiktDB *wiktdb);
    static void loadVectorsFromFile(const string& meaningFilename);
    static void preloadVectors();

    static SparseArray getVector(const string& term);
    static SparseArray getVector(const string& term, const string& pos);
    static SparseArray getVector(const string& term, const vector<string>& context);
    static SparseArray getVector(const string& term, const string& pos, const vector<string>& context);
    static SparseArray getVector(const string& term, const string& pos, uint index);
    static SparseArray getVector(const json& meaningRef, const json& termRef, const string& pos);
    static vector<ulong> getMeaningRefIds(const string& term, const string& pos);
    static vector<std::pair<ulong, float>> similar(const string& term, uint size, bool reversed);
    static vector<std::pair<ulong, float>> similar(const string& term, uint size, bool reversed, const string& pos);
    static vector<std::pair<ulong, float>> similar(const string& term, uint size, bool reversed, const string& pos, const vector<string>& context);
    static vector<std::pair<ulong, float>> similarRepr(const SparseArray& vec, uint size, bool reversed);
    static vector<std::pair<ulong, float>> similarRepr(const SparseArray& vec, uint size, bool reversed, const string& pos);
    static vector<std::pair<ulong, float>> similarRepr(const SparseArray& vec, uint size, bool reversed, const string& pos, const vector<string>& context);
    static float similarity(const string& term1, const string& pos1, const string& term2, const string& pos2, const vector<string>& context, float scale);

    static void idfWeak();
    static void markEffective();
    static void joinTranslations();
    static void translationVectorJoin(ulong translIdx, const Meaning& meaning, Meaning& translMeaning);
    static SparseArray effectiveRepr(const SparseArray& vec);
    static string stringRepr(const SparseArray& vec, const string separator, bool dense, bool named);
    static string stringRepr(Meaning meaning, const string separator, bool dense, bool named);
    static json jsonRepr(const SparseArray& vec, bool named);
    static SparseArray fromJsonRepr(const json& jsonVec, bool named);
};

#endif
