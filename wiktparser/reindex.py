#-*- coding: utf8 -*-
__author__ = "Danilo S. Carvalho <danilo@jaist.ac.jp>"

import json

def sort_db(wiktdb_path):
    print "Loading DB..."
    with open(wiktdb_path, "r") as wiktdb_file:
        db = json.load(wiktdb_file)

    print "Sorting..."
    sorted_db = sorted(db, key=lambda doc: doc["title"])
    print "Sorted."

    print "Writing..."
    with open(wiktdb_path.replace(".json", "") + "_sorted.json", "w") as sorted_file:
        json.dump(sorted_db, sorted_file, indent=2)
