import sys
import json
from tqdm import tqdm

def main(argv):
    wiktdb = None
    with open(argv[1]) as db_file:
        wiktdb = json.load(db_file)

    rel = argv[2] # synonyms / antonyms
    syndb = dict()

    for term in tqdm(wiktdb, desc=f"Loading {rel}"):
        title = term["title"]

        if (title not in syndb):
            syndb[title] = dict()

        for lang in term["langs"]:
            if (rel in term["langs"][lang]):
                if (lang not in syndb):
                    syndb[lang] = dict()
                if (title not in syndb[lang]):
                    syndb[lang][title] = dict()

                syns = term["langs"][lang][rel]
                for pos in syns:
                    syndb[lang][title][pos] = list()
                    for syn in syns[pos]:
                        if ("links" in syn):
                            syndb[lang][title][pos].extend(syn["links"])

    excl_lang = list()
    for lang in tqdm(syndb, desc=f"Cleaning {rel}"):
        excl_term = list()
        for term in syndb[lang]:
            excl_pos = list()
            for pos in syndb[lang][term]:
                if (len(syndb[lang][term][pos]) == 0):
                    excl_pos.append(pos)
            for pos in excl_pos:
                del syndb[lang][term][pos]

            if (len(syndb[lang][term]) == 0):
                excl_term.append(term)

        for term in excl_term:
            del syndb[lang][term]

        if (len(syndb[lang]) == 0):
            excl_lang.append(lang)

    for lang in excl_lang:
        del syndb[lang]

                    
    print(json.dumps(syndb, indent=2))





if __name__ == "__main__":
    main(sys.argv)
