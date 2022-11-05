#-*- coding: utf8 -*-
__author__ = "Danilo S. Carvalho <danilo@jaist.ac.jp>"

import jsonlines

def clean_empty(db):
    cl_db = []

    print("Total entries before cleaning", len(db))

    docExclList = set()
    for doc in db:
        langExclList = []
        for lang in doc["langs"]:
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

    print("Total entries after cleaning", len(cl_db))

    return cl_db


def sort_db(wiktdb_path):
    print("Loading DB...")
    with jsonlines.open(wiktdb_path, "r") as wiktdb_reader: 
        db = list(wiktdb_reader)

    db = clean_empty(db)

    print("Sorting...")
    sorted_db = sorted(db, key=lambda doc: doc["title"])
    print("Sorted.")

    print("Writing...")
    with jsonlines.open(wiktdb_path.replace(".jsonl", "") + "_sorted.jsonl", mode="w") as sorted_writer:
        sorted_writer.write_all(sorted_db)
