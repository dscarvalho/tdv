#include <iostream>
#include <fstream>
#include "types.h"
#include "wiktdb.h"
#include "sparsearray.h"
#include "vectorize.h"

void loadData(const string& configFileName)
{ 
    MeaningExtractor::config.load(configFileName);
    
    WiktDB *wiktdb = new WiktDB();
    std::cout << "Loading DB..." << std::endl;
    wiktdb->loadDB(MeaningExtractor::config.wiktDBPath);
    std::cout << "DB loaded." << std::endl;
    
    MeaningExtractor::setDB(wiktdb);
    
    std::cout << "Preloading vectors..." << std::endl;
    MeaningExtractor::preloadVectors();
}

void writeMeanings(const string& oFileName)
{
    std::ofstream fMeanings;
    json meaningList = json::array();
    bool humanReadable = MeaningExtractor::config.humanReadable;
    
    fMeanings.open(oFileName);

    for (auto it = MeaningExtractor::reprCache.begin(); it != MeaningExtractor::reprCache.end(); ++it)
    {
        json meaning = json::object();

        meaning[FLD_ID] = it->first;
        meaning[FLD_TERM] = it->second.term;
        meaning[FLD_POS] = it->second.pos;
        meaning[FLD_DESCR] = it->second.descr;
        meaning[FLD_LANG] = it->second.lang;
        meaning[FLD_REPR] = MeaningExtractor::jsonRepr(it->second.repr, humanReadable);

        meaningList.push_back(meaning); 
    }

    if (humanReadable)
        fMeanings <<  std::setw(2) << meaningList << std::endl;
    else
        fMeanings << meaningList;

    fMeanings.flush();
    fMeanings.close();
}

void writeVectors(const string& oFileName)
{
    std::ofstream fVectors;
    json meaningVectors = json::object();

    fVectors.open(oFileName);

    for (auto it = MeaningExtractor::reprCache.begin(); it != MeaningExtractor::reprCache.end(); ++it)
    {
        if (!meaningVectors.count(it->second.term))
        {
            meaningVectors[it->second.term] = json::array();
        }

        json meaning = json::object();
        meaning[FLD_ID] = it->first;
        meaning[FLD_POS] = it->second.pos;
        meaning["vec"] = MeaningExtractor::jsonRepr(it->second.repr, false);

        meaningVectors[it->second.term].push_back(meaning);
    }

    fVectors << meaningVectors;

    fVectors.flush();
    fVectors.close();
}

void writeCosines(const string& oFileName)
{
    std::ofstream fCosines;
    std::ofstream fConcepts;
    json concepts = json::array();

    fCosines.open(oFileName + ".cosines.tsv");
    fConcepts.open(oFileName + ".concepts.json");

    ulong num_concepts = MeaningExtractor::reprCache.size();
    ulong count_done = 0;

    for (auto it1 = MeaningExtractor::reprCache.begin(); it1 != MeaningExtractor::reprCache.end(); ++it1)
    {
        json meaning = json::object();
        meaning[FLD_ID] = it->first;
        meaning[FLD_TERM] = it->second.term;
        meaning[FLD_POS] = it->second.pos;
        concepts.push_back(meaning);

        for (auto it2 = MeaningExtractor::reprCache.begin(); it2 != MeaningExtractor::reprCache.end(); ++it2)
        {
            float sim = 0;
            if (it1->first != it2->first)
                sim = MeaningExtractor::similarity(it1->second.term, it1->second.pos, it2->second.term, it2->second.pos, vector<string>(), 1);
            else
                sim = 1;

            if (it2 != MeaningExtractor::reprCache.begin())
                fCosines << "\t";

            fCosines << sim;
        }

        fCosines << std::endl;
    }

    fConcepts << concepts << std::endl;

    fConcepts.flush();
    fCosines.flush();
    fConcepts.close();
    fCosines.close();
}

int main(int argc, char **argv)
{
    if (argc != 4)
    {
        std::cout << "Usage: " << argv[0] << " <config. filename> <output filename> <mode: (vectors|meanings)>" << std::endl;
    }
    else
    {
        string configFileName(argv[1]);
        string oFileName(argv[2]);
        string mode(argv[3]);

        loadData(configFileName);

        if (mode == "vectors")
            writeVectors(oFileName);
        else
            writeMeanings(oFileName);
    }

    return 0;
}
