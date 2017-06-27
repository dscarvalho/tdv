#include <cppcms/application.h>  
#include <cppcms/applications_pool.h>  
#include <cppcms/service.h>
#include <cppcms/http_request.h>
#include <cppcms/http_response.h>
#include <cppcms/url_dispatcher.h>
#include <cppcms/url_mapper.h>
#include <cppcms/config.h>
#include <json/json.hpp>
#include <iostream>
#include "service.h"

extern vector<ReprOffsetBase> REPR_OFFSET_BASES;
extern vector<string> REPR_OFFSET_BASE_NAMES;

TDVService::TDVService(cppcms::service &srv): cppcms::application(srv)
{
    dispatcher().assign("/similar",&TDVService::similar,this);
    mapper().assign("similar","/similar");
    
    dispatcher().assign("/definition",&TDVService::definition,this);
    mapper().assign("definition","/definition");
    
    dispatcher().assign("/repr",&TDVService::repr,this);
    mapper().assign("repr","/repr");
    
    dispatcher().assign("/similarity",&TDVService::similarity,this);
    mapper().assign("similarity","/similarity");

    dispatcher().assign("/features",&TDVService::features,this);
    mapper().assign("features","/features");
    
    dispatcher().assign("/wiktdef",&TDVService::wiktdef,this);
    mapper().assign("wiktdef","/wiktdef");

    dispatcher().assign("/disambig",&TDVService::disambiguation,this);
    mapper().assign("disambig","/disambig");
    
    mapper().root("/tdv");

    string configFileName = settings()["application"]["config_file"].str();
    MeaningExtractor::config.load(configFileName);
    
    wiktdb = new WiktDB();
    std::cout << "Loading DB..." << std::endl;
    wiktdb->loadDB(MeaningExtractor::config.wiktDBPath);
    std::cout << "DB loaded." << std::endl;
    
    MeaningExtractor::setDB(wiktdb);
    
    std::cout << "Preloading vectors..." << std::endl;
    MeaningExtractor::loadVectorsFromFile(MeaningExtractor::config.meaningFilePath);;

    std::cout << "Ready." << std::endl;
}

string TDVService::printRepr(SparseArray vec)
{
    std::stringbuf demoBuf;
    std::ostream demoStream(&demoBuf);

    for (auto pair : vec)
    {
        ulong idx = pair.first;
        
        if (idx < wiktdb->size())
        {
            demoStream << (*wiktdb)[idx]["title"] << ": " << pair.second << "<br/>" << std::endl;
        }
        else
        {
            bool skip = false;
            for (uint i = 0; i < REPR_OFFSET_BASES.size(); i++)
            {
                if (idx < wiktdb->size() * (REPR_OFFSET_BASES[i] + 1))
                {
                    ulong offset = idx % wiktdb->size();
                    demoStream << REPR_OFFSET_BASE_NAMES[i] << "(" << (*wiktdb)[offset]["title"] << "): " << pair.second << "<br/>" << std::endl;

                    skip = true;
                    break;
                }
            }
            
            if (!skip && idx < wiktdb->size() * (ReprOffsetBase::pos + 1))
                demoStream << "POS(" << wiktdb->posName(idx) << "): " << pair.second << "<br/>" << std::endl;
        }

    }

    return demoBuf.str();
}

SparseArray TDVService::getRepr()
{
    string term = request().get("term");
    string pos = request().get("pos");
    string ctx = request().get("ctx");
    vector<string> context;
    Meaning meaning;
    
    if (ctx != "")
        context = StringUtils::split(ctx, ",");

    try
    {
        if (ctx != "" && pos != "")
            meaning = Meaning(term, pos, context);
        else if (ctx != "")
            meaning = Meaning(term, context);
        else if (pos != "")
            meaning = Meaning(term, pos);
        else
            meaning = Meaning(term);
    }
    catch (std::exception& e)
    {
        response().out() << "{\"!ERR\": \"Exception: " << e.what() << "\"}";
    }

    return meaning.getVector();
}

void TDVService::setHeaders()
{
    response().set_header("Access-Control-Allow-Origin", "*");
    response().set_header("Access-Control-Allow-Credentials", "true");
}

void TDVService::similar()
{
    string term = request().get("term");
    string pos = request().get("pos");
    string ctx = request().get("ctx");
    string rev = request().get("rev");
    bool reverse = (rev == "true");
    vector<string> context;
    SparseArray vec;
    
    setHeaders();

    if (ctx != "")
    {
        context = StringUtils::split(ctx, ",");
        vec = Meaning(term, pos, context).getVector();
    }
    else
    {
        vec = Meaning(term, pos).getVector();
    }

    vec = Meaning(term).getVector();


    try
    {
        vector<std::pair<ulong, float>> simList = MeaningExtractor::similarRepr(vec, 20, reverse, pos);

        json res = json::array();

        for (auto pair : simList)
        {
            const Meaning& meaning = MeaningExtractor::reprCache[pair.first];
            res.push_back(json({{"sim", pair.second}, {"term", meaning.term}, {"pos", meaning.pos}, {"descr", meaning.descr}}));
        }

        response().out() << res;
    }
    catch (std::exception& e)
    {
        response().out() << "{\"!ERR\": \"Exception (similarRepr): " << e.what() << "\"}";
    }
}

void TDVService::definition()
{
    vector<string> definition = StringUtils::split(request().get("def"));
    SparseArray defVec;

    for (const string& term : definition)
    {
        SparseArray vec = Meaning(term).getVector();
        defVec += vec;
    }

    //defVec /= definition.size();
    
    setHeaders();

    try
    {
        vector<std::pair<ulong, float>> simList = MeaningExtractor::similarRepr(defVec, 20, false);

        json res = json::array();

        for (auto pair : simList)
        {
            const Meaning& meaning = MeaningExtractor::reprCache[pair.first];

            if(std::find(definition.begin(), definition.end(), meaning.term) != definition.end())
                continue;

            res.push_back(json({{"sim", pair.second}, {"term", meaning.term}, {"pos", meaning.pos}, {"descr", meaning.descr}}));
        }

        response().out() << res;
    }
    catch (std::exception& e)
    {
        response().out() << "{\"!ERR\": \"Exception (similarRepr): " << e.what() << "\"}";
    }
}

void TDVService::repr()  
{
    SparseArray vec = getRepr();
    string human = request().get("human");
    bool named = (human == "true");
    
    setHeaders();

    response().out() <<  MeaningExtractor::jsonRepr(vec, named) << std::endl;
}

void TDVService::disambiguation()
{
    string term = request().get("term");
    string sentence = request().get("sent");
    string pos = request().get("pos");
    
    setHeaders();

    if (!wiktdb->exists(term))
    {
        response().out() << "{\"!ERR\": \"Term not in the dictionary\"}";
        return;
    }

    size_t matchPos = sentence.find(term);
    if (matchPos == string::npos)
    {
        response().out() << "{\"!ERR\": \"Term not in the sentence\"}";
        return;
    }

    if (matchPos > 0)
        sentence.erase(matchPos - 1, matchPos + term.length() + 1);
    else
        sentence.erase(matchPos, matchPos + term.length() + 1);

    vector<string> context = StringUtils::split(sentence);
    
    SparseArray vec;
    if (pos != "")
        vec = Meaning(term, pos).getVector();
    else
        vec = Meaning(term).getVector();

    Meaning& disambigMeaning = MeaningExtractor::disambiguate(term, pos, context);
    
    json res = json::array();
    res.push_back(json({{"term", disambigMeaning.term}, {"pos", disambigMeaning.pos}, {"descr", disambigMeaning.descr}}));
      
    response().out() << res;

}

void TDVService::similarity()  
{
    string term1 = request().get("term1");
    string pos1 = request().get("pos1");
    string term2 = request().get("term2");
    string pos2 = request().get("pos2");
    float scale = atof(request().get("scale").c_str());
    
    if (scale < 0.001)
        scale = 1;
    
    setHeaders();

    response().out() <<  MeaningExtractor::similarity(term1, pos1, term2, pos2, vector<string>(), scale) << std::endl;
}

void TDVService::features()
{
    string term1 = request().get("term1");
    string pos1 = request().get("pos1");
    string term2 = request().get("term2");
    string pos2 = request().get("pos2");

    SparseArray vec1, vec2;
    MeaningExtractor::graphFill = true;
    uint linkSearchDepth = MeaningExtractor::linkSearchDepth;
    MeaningExtractor::linkSearchDepth = 1;
    vec1 = Meaning(term1, pos1).getVector();
    vec2 = Meaning(term2, pos2).getVector();
    MeaningExtractor::graphFill = false;
    MeaningExtractor::linkSearchDepth = linkSearchDepth;
    
    setHeaders();

    if (wiktdb->exists(term1) && wiktdb->exists(term2))
    {
        ulong idx_synterm1 = wiktdb->size() * ReprOffsetBase::synonym + wiktdb->index(term1);
        ulong idx_synterm2 = wiktdb->size() * ReprOffsetBase::synonym + wiktdb->index(term2);
        ulong idx_strterm1 = wiktdb->size() * ReprOffsetBase::strong + wiktdb->index(term1);
        ulong idx_strterm2 = wiktdb->size() * ReprOffsetBase::strong + wiktdb->index(term2);
        ulong idx_wkterm1 = wiktdb->size() * ReprOffsetBase::weak + wiktdb->index(term1);
        ulong idx_wkterm2 = wiktdb->size() * ReprOffsetBase::weak + wiktdb->index(term2);
        ulong idx_hypterm1 = wiktdb->size() * ReprOffsetBase::hypernym + wiktdb->index(term1);
        ulong idx_hypterm2 = wiktdb->size() * ReprOffsetBase::hypernym + wiktdb->index(term2);

        if (vec1.count(idx_wkterm2))
            response().out() << "1:" << 1.0 << " ";
        if (vec2.count(idx_wkterm1))
            response().out() << "2:" << 1.0 << " ";
        if (vec1.count(idx_strterm2))
            response().out() << "3:" << 1.0 << " ";
        if (vec2.count(idx_strterm1))
            response().out() << "4:" << 1.0 << " ";
        if (vec1.count(idx_hypterm2))
            response().out() << "5:" << 1.0 << " ";
        if (vec2.count(idx_hypterm1))
            response().out() << "6:" << 1.0 << " ";
        if (vec1.count(idx_synterm2))
            response().out() << "7:" << 1.0 << " ";
        if (vec2.count(idx_synterm1))
            response().out() << "8:" << 1.0 << " ";
    }
    else
    {
        for (int i=1; i < 9; i++)
            response().out() << i << ":" << 0.0 << " ";
    }
}

void TDVService::wiktdef()
{
    string term = request().get("term");

    setHeaders();
    
    if (wiktdb->exists(term))
    {
        const json& termRef = (*wiktdb)[term];
        response().out() <<  
            termRef << "\n";
    }
    else
    {
        response().out() <<  
            "Term not found" << "\n";
    }
}

int main(int argc,char **argv)  
{
    try 
    {
        cppcms::service srv(argc,argv);
        srv.applications_pool().mount(
                cppcms::applications_factory<TDVService>()
        );
        
        srv.run();  
    }  
    catch(std::exception const &e)
    {
        std::cerr << e.what() << std::endl;  
    }

    return 0;
}
