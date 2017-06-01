#-*- coding: utf8 -*-
__author__ = "Danilo S. Carvalho <danilo@jaist.ac.jp>"

import json

def clean_empty(db):
    cl_db = []

    print "Total entries before cleaning", len(db)

    docExclList = set()
    for doc in db:
        langExclList = []
        for lang in doc["langs"].keys():
            if (("meanings" not in doc["langs"][lang]) or (not doc["langs"][lang]["meanings"])):
                langExclList.append(lang)
                
        for lang in langExclList:
            del doc["langs"][lang]
            
        if (not doc["langs"]):
            docExclList.add(doc["title"])

    for doc in db:
        if (doc["title"] in docExclList):
            continue
        else:
            cl_db.append(doc)

    print "Total entries after cleaning", len(cl_db)

    return cl_db


def sort_db(wiktdb_path):
    print "Loading DB..."
    with open(wiktdb_path, "r") as wiktdb_file:
        db = json.load(wiktdb_file)

    db = clean_empty(db)

    print "Sorting..."
    sorted_db = sorted(db, key=lambda doc: doc["title"])
    print "Sorted."

    print "Writing..."
    with open(wiktdb_path.replace(".json", "") + "_sorted.json", "w") as sorted_file:
        json.dump(sorted_db, sorted_file, indent=2)
