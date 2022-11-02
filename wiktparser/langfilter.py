#!/usr/bin/env python2.7
#-*- coding: utf8 -*-
__author__ = "Danilo S. Carvalho <danilo@jaist.ac.jp>"

import json
import argparse

def main(args):
    print("Loading DB...")
    with open(args.ifilepath, "r") as wiktdb_file:
        db = json.load(wiktdb_file)
    langs = args.langs.split(",")

    selected = []
    print("Filtering...")
    for doc in db:
        exist_langs = []
        for lang in langs:
            if (lang in doc["langs"]):
                exist_langs.append(lang)
            
        if (exist_langs or "redirect" in doc):
            seldoc = dict()
            seldoc["wikid"] = doc["wikid"]
            seldoc["title"] = doc["title"]
            
            if ("redirect" in doc):
                seldoc["redirect"] = doc["redirect"]
                continue

            seldoc["langs"] = {}
            for lang in exist_langs:
                seldoc["langs"][lang] = doc["langs"][lang]

            selected.append(seldoc)

    print("Writing...")
    with open(args.ifilepath.replace(".json", "") + "_" + "-".join([lang.lower()[0:2] for lang in langs]) + ".json", "w") as filtered_file:
        json.dump(selected, filtered_file, indent=2)


def parse_args():
    argparser = argparse.ArgumentParser()
    argparser.add_argument("ifilepath", type=str, help="Input file path (Wiktionary preprocessed JSON file)")
    argparser.add_argument("langs", type=str, help="Selected languages separated by comma (e.g., 'English,Japanese'). Entries without sense info on the selected language will be ommited from the output")

    return argparser.parse_args()


if __name__ == "__main__":
    main(parse_args())
