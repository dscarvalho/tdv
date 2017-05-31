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
    MeaningExtractor::preloadVectors(MeaningExtractor::config.vectorFilePath);
}

void writeVectors(const string& oFileName)
{
    std::ofstream fReprs;
    json meaningList = json::array();
    bool humanReadable = MeaningExtractor::config.humanReadable;
    
    fReprs.open(oFileName);

    for (auto it = MeaningExtractor::reprCache.begin(); it != MeaningExtractor::reprCache.end(); ++it)
    {
        json meaning = json::object();

        meaning["id"] = it->first;
        meaning["term"] = it->second.term;
        meaning["pos"] = it->second.pos;
        meaning["repr"] = MeaningExtractor::jsonRepr(it->second.repr, humanReadable);

        meaningList.push_back(meaning); 
    }

    if (humanReadable)
        fReprs <<  meaningList.setw(2) << std::endl;
    else
        fReprs << meaningList;

    fReprs.close();
}

int main(int argc, char **argv)
{
    if (argc != 3)
    {
        std::cout << "Usage: " << argv[0] << " <config. filename> <output filename>" << std::endl;
    }
    else
    {
        string configFileName(argv[1]);
        string oFileName(argv[2]);

        loadData(configFileName);
        writeVectors(oFileName);
    }

    return 0;
}
