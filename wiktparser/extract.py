#!/usr/bin/env python2.7
# -*- coding: utf8 -*-
__author__ = "Danilo S. Carvalho <danilo@jaist.ac.jp>"

import sys
import os
import re
import math
import json
import pickle
import argparse
from multiprocessing import Process, JoinableQueue
from wikiexpr import RGX, WIKI_REJECT, ATTR_GEN, ATTR_TYPES_ORDER, clean_expr
from reindex import sort_db

BUFFER_SIZE = 1024 * 1024 * 16  # 16M input buffer
OUTPUT_BUFFER_DIV = 4  # 16 / 4 = 4M output buffer


classes = ["prefix", "suffix", "infix", "affix", "interfix", "noun", "proper noun", "adjective", "pronoun", "adverb",
           "proverb", "letter", "verb", "conjunction", "determiner", "preposition", "postposition", "numeral", "number",
           "particle", "phrase", "interjection", "article", "abbreviation", "romanization", "initialism", "participle",
           "contraction", "symbol", "punctuation mark", "idiom"]

clsrgx = dict([(cls, re.compile(r"====?" + cls + "====?")) for cls in classes])


class WiktionaryParser(Process):
    def __init__(self, queue, ifilename, ofilename, procnum=0, numprocs=1, numpieces=1, pgindex=None):
        Process.__init__(self)
        self.queue = queue
        self.ifile = open(ifilename, "r", BUFFER_SIZE)
        self.ofile = open(ofilename, "w", BUFFER_SIZE / OUTPUT_BUFFER_DIV)
        self.procNum = procnum
        self.numProcs = numprocs
        self.numPieces = numpieces
        self.pgIndex = {}
        self.totalPgs = 0

        self.inContent = False
        self.inEtym = False
        self.inSyn = False
        self.inAnt = False
        self.inHyper = False
        self.inHypo = False
        self.inAbbrev = False
        self.inTransl = False
        self.empty = True
        self.ignorePage = False
        self.pgTerm = None
        self.curLang = None
        self.transLangParent = None
        
        if (pgindex):
            self.pgIndex = pgindex
            self.totalPgs = len(pgindex)

    
    def __qinit__(self, input_range=None):
        self.range = input_range
        self.procPgs = 0
        self.pgCount = 0
        self.pgTerm = None
        self.inContent = False
        self.curLang = ""
        self.inEtym = False
        self.inClass = {}
        self.inSyn = False
        self.inAnt = False
        self.inHyper = False
        self.inHypo = False
        self.inAbbrev = False
        self.inTransl = False
        self.empty = True
        self.ignorePage = False
        self.transLangParent = ""
        

    def init_index(self):
        pgid = -1
        pgtitle = ""
        
        while True:
            line = self.ifile.readline()
            if (not line):
                break
            
            mo = RGX["page"].search(line)
            if (mo):
                if (self.totalPgs > 0):
                    if (WIKI_REJECT not in pgtitle):
                        self.pgIndex[pgtitle] = pgid

                    pgid = -1
                    pgtitle = ""

                self.totalPgs += 1
                
                if (self.totalPgs % 10000 == 0):
                    sys.stdout.write("Counting pages... " + str(self.totalPgs) + "\r")
                    sys.stdout.flush()

                continue

            if (pgid == -1 or pgtitle == ""):
                mo = RGX["title"].search(line)
                if (mo):
                    pgtitle = mo.group("title")
                    continue
                       
                mo = RGX["wikid"].search(line)
                if (mo):
                    pgid = int(mo.group("id"))

        if (WIKI_REJECT not in pgtitle):
            self.pgIndex[pgtitle] = int(pgid)
        
        sys.stdout.write("Counting pages... " + str(self.totalPgs) + "\r")
        print("\n")
        self.ifile.seek(0)


    def page_start(self, line):
        mo = RGX["page"].search(line)
        if (mo):
            if (self.pgTerm is not None and not self.empty):
                self.prune_pagedoc()
                self.ofile.write("\n")
                json.dump(self.pgTerm, self.ofile)
                #self.ofile.write(",")
                
            self.inContent = False
            self.inEtym = False
            self.inSyn = False
            self.inAnt = False
            self.inHyper = False
            self.inHypo = False
            self.inAbbrev = False
            self.inTransl = False

            for cls in classes:
                self.inClass[cls] = False

            self.empty = True
            self.ignorePage = False

            self.pgTerm = {"wikid": 0, "title": "", "langs": {}}
             
            self.procPgs += 1
            self.pgCount += 1

            return True

        return False


    def property_collect(self, line):
        mo = RGX["title"].search(line)
        if (mo):
            self.pgTerm["title"] = mo.group("title")

            if (WIKI_REJECT in self.pgTerm["title"]):
                self.ignorePage = True
                return False
            else:
                self.pgTerm["wikid"] = self.pgIndex[self.pgTerm["title"]]
               
        return True


    def check_redirect(self, line):
        mo = RGX["redirect"].search(line)
        if (mo):
            try:
                self.pgTerm["redirect"] = mo.group("term").split("#")
                self.empty = False
            except KeyError:
                print("Unindexed term: ", mo.group("term"))

            return True
        
        return False


    def check_content(self, line):
        mo = RGX["lang_content_section"].search(line)
        if (mo):
            self.inContent = True
            self.empty = False
            self.curLang = mo.group("lang")
            self.pgTerm["langs"][self.curLang] = {}
            self.pgTerm["langs"][self.curLang]["meanings"] = {}
            self.pgTerm["langs"][self.curLang]["pos_order"] = []
            
            for cls in classes:
                self.pgTerm["langs"][self.curLang]["meanings"][cls] = []
                self.inClass[cls] = False

            self.inEtym = False
            self.inSyn = False
            self.inAnt = False
            self.inHyper = False
            self.inHypo = False
            self.inAbbrev = False
            self.inTransl = False

            return True

        return False


    def collect_content(self, line):
        if (self.inContent): 
            skip = self.collect_etymology_section(line)
            if (skip): return True
            skip = self.check_etymology_section(line)
            if (skip): return True

            skip = self.check_class_section(line)
            if (skip): return True
            skip = self.collect_meaning_section(line)

            return True

        return False


    def collect_etymology_section(self, line):
        if (self.inEtym):
            if (RGX["section_begin"].match(line)):
                self.inEtym = False
                return False
            else:
                self.pgTerm["langs"][self.curLang]["etymology"]["morph_links"].extend(self.get_etymology_links(line))
                clean_txt = clean_expr(line, "term")
                attrs, clean_txt = self.get_attributes(clean_txt)
                self.pgTerm["langs"][self.curLang]["etymology"]["attrs"].extend(attrs)
                self.pgTerm["langs"][self.curLang]["etymology"]["descr"] += clean_txt
                self.get_morph_info(line)

            return True

        return False


    def check_etymology_section(self, line):
        if (RGX["etymology_section"].match(line)):
            self.pgTerm["langs"][self.curLang]["etymology"] = {"descr": "", "morph_links": [], "attrs": []}
            self.inEtym = True

            return True

        return False


    def check_class_section(self, line):
        for cls in classes:
            if (clsrgx[cls].search(line.lower())): 
                self.inClass[cls] = True

                if (cls not in self.pgTerm["langs"][self.curLang]["pos_order"]):
                    self.pgTerm["langs"][self.curLang]["pos_order"].append(cls)

                for othcls in classes:
                    if (othcls != cls):
                        self.inClass[othcls] = False 
                        self.inSyn = False
                        self.inAnt = False
                        self.inHyper = False
                        self.inHypo = False
                        self.inAbbrev = False
                        self.inTransl = False

                return True

        return False


    def collect_meaning_section(self, line):
        for cls in classes:
            if (self.inClass[cls]):
                meaning_capture_len = 1
                mo = RGX["sense"].search(line)
                if (mo):
                    meanings = self.get_meanings(mo.group("sense"))
                    meaning_capture_len = len(meanings)
                    self.pgTerm["langs"][self.curLang]["meanings"][cls].extend(meanings)
                    self.empty = False

                mo = RGX["example"].search(line)
                if (mo and len(self.pgTerm["langs"][self.curLang]["meanings"][cls]) > 0):
                    if ("examples" not in self.pgTerm["langs"][self.curLang]["meanings"][cls][-meaning_capture_len]):
                        self.pgTerm["langs"][self.curLang]["meanings"][cls][-meaning_capture_len]["examples"] = []

                    self.pgTerm["langs"][self.curLang]["meanings"][cls][-meaning_capture_len]["examples"].append(mo.group("example"))

                skip = self.collect_nym_section(line, cls, "inSyn", RGX["synonym"], "synonyms", "syn")
                if (skip): return True
                skip = self.collect_nym_section(line, cls, "inAnt", RGX["antonym"], "antonyms", "ant")
                if (skip): return True
                skip = self.collect_nym_section(line, cls, "inHyper", RGX["hypernym"], "hypernyms", "hyper")
                if (skip): return True
                skip = self.collect_nym_section(line, cls, "inHypo", RGX["hyponym"], "hyponyms", "hypo")
                if (skip): return True
                skip = self.collect_nym_section(line, cls, "inAbbrev", RGX["abbreviation"], "abbreviations", "abbrev")
                if (skip): return True
                skip = self.collect_translation_section(line, cls)
                if (skip): return True
                skip = self.check_nym_section(line, "inSyn", "synonym")
                if (skip): return True
                skip = self.check_nym_section(line, "inAnt", "antonym")
                if (skip): return True
                skip = self.check_nym_section(line, "inHyper", "hypernym")
                if (skip): return True
                skip = self.check_nym_section(line, "inHypo", "hyponym")
                if (skip): return True
                skip = self.check_nym_section(line, "inAbbrev", "abbreviation")
                if (skip): return True
                skip = self.check_nym_section(line, "inTransl", "translation")

        return False


    def collect_nym_section(self, line, cls, insectionattr, sectionlinergx, sectionname, sectionrgxgroupname):
        if (getattr(self, insectionattr)):
            if (RGX["section_begin"].search(line)):
                setattr(self, insectionattr, False)
                return False

            mo = sectionlinergx.search(line)
            if (mo):
                if (cls not in self.pgTerm["langs"][self.curLang][sectionname]):
                    self.pgTerm["langs"][self.curLang][sectionname][cls] = []

                self.pgTerm["langs"][self.curLang][sectionname][cls].extend(self.get_meanings(mo.group(sectionrgxgroupname), keeplabels=True))

                return True

        return False


    def check_nym_section(self, line, insectionattr, sectionnamesingl):
        if (RGX[sectionnamesingl + "_section"].search(line)):
            if (sectionnamesingl + "s" not in self.pgTerm["langs"][self.curLang]):
                self.pgTerm["langs"][self.curLang][sectionnamesingl + "s"] = {}
            
            setattr(self, insectionattr, True)

            return True
        
        return False


    def collect_translation_section(self, line, cls):
        if (self.inTransl):
            if (RGX["section_begin"].search(line)):
                self.inTransl = False
                return False

            mo = RGX["translation_open"].search(line)
            if (mo):
                if (cls not in self.pgTerm["langs"][self.curLang]["translations"]):
                    self.pgTerm["langs"][self.curLang]["translations"][cls] = []

                self.pgTerm["langs"][self.curLang]["translations"][cls].append({"meaning": clean_expr(mo.group("mng")), "transl": {}})

                return True
           
            if (cls in self.pgTerm["langs"][self.curLang]["translations"]):
                mo = RGX["translation_child"].search(line)
                if (mo):
                    if (not mo.group("child")):
                        self.transLangParent = ""

                    lang_name = mo.group("lang") if (not self.transLangParent) else self.transLangParent + ">>" + mo.group("lang")
                    if (lang_name not in self.pgTerm["langs"][self.curLang]["translations"][cls][-1]["transl"]):
                        self.pgTerm["langs"][self.curLang]["translations"][cls][-1]["transl"][lang_name] = []
                    
                    miter = RGX["translation_item"].finditer(mo.group("trls"))
                    for moi in miter:
                        self.pgTerm["langs"][self.curLang]["translations"][cls][-1]["transl"][lang_name].append((moi.group("term"), moi.group("langcode")))

                    return True
                    
                mo = RGX["translation_parent"].search(line)
                if (mo):
                    self.transLangParent = mo.group("lang")

                    return True

        return False


    def get_meanings(self, definition, keeplabels=False):
        clndef = RGX["definition_prefix1"].sub("", definition)
        clndef = RGX["html_escape"].sub("", clndef)

        clndef = clndef.split(".")[0]
        meaning_strs_init = re.split(r";", clndef)
        meanings = []

        for mngstr in meaning_strs_init:
            meaning = {"meaning": ""}
            pg_links = []
            miter = RGX["term"].finditer(mngstr)
            for mo in miter:
                if (mo.group("term") in self.pgIndex):
                    pg_links.append(mo.group("term"))

            clean_txt = clean_expr(mngstr, "term")
            clean_txt_wlabels = clean_expr(mngstr, keeplabels)

            attrs, clean_txt = self.get_attributes(clean_txt)

            if (keeplabels):
                clean_txt = clean_txt_wlabels

            meaning["attrs"] = attrs 
            for attr in meaning["attrs"]:
                if (attr[1] in self.pgIndex):
                    pg_links.append(attr[1])
            
            meaning["meaning"] = clean_txt
            meaning["links"] = pg_links
             
            meanings.append(meaning)

        return meanings


    def get_etymology_links(self, etym):
        morph_links = []
        miter = RGX["term"].finditer(etym)
        for mo in miter:
            if (mo.group("term") in self.pgIndex):
                morph_links.append(mo.group("term"))

        miter = RGX["etym_link"].finditer(etym)
        for mo in miter:
            lang = mo.group("lang_orig")
            term = mo.group("term")

            if (term in self.pgIndex):
                morph_links.append((term, lang))

        return morph_links


    def get_morph_info(self, etym):
        mo_pre = RGX["prefix"].search(etym)
        mo_suf = RGX["suffix"].search(etym)
        mo_con = RGX["confix"].search(etym)
        mo_aff = RGX["affix"].search(etym)

        if (mo_pre):
            self.pgTerm["langs"][self.curLang]["etymology"]["prefix"] = mo_pre.group("pre") + "-"
            self.pgTerm["langs"][self.curLang]["etymology"]["stem"] = mo_pre.group("stem")
        
        if (mo_suf):
            self.pgTerm["langs"][self.curLang]["etymology"]["suffix"] = "-" + mo_suf.group("suf")
            self.pgTerm["langs"][self.curLang]["etymology"]["stem"] = mo_suf.group("stem")

        if (mo_con):
            self.pgTerm["langs"][self.curLang]["etymology"]["confix"] = mo_con.group("parts").split("|")
            self.pgTerm["langs"][self.curLang]["etymology"]["confix"][0] += "-"
            self.pgTerm["langs"][self.curLang]["etymology"]["confix"][-1] += "-"

        if (mo_aff):
            self.pgTerm["langs"][self.curLang]["etymology"]["affix"] = mo_aff.group("parts").split("|")


    @staticmethod
    def get_attributes(descr):
        attrs = []

        for attr_type in ATTR_TYPES_ORDER:
            miter = RGX[attr_type].finditer(descr)
            for mo in miter:
                attr = ATTR_GEN[attr_type](mo)
                if (len(attr) > 1):
                    attrs.append(attr)

            descr = clean_expr(descr, attr_type)

        descr = clean_expr(descr, "attr_specific")

        return (attrs, descr)


    def prune_pagedoc(self):
        for lang in self.pgTerm["langs"]:
            if ("etymology" in self.pgTerm["langs"][lang]):
                if (not self.pgTerm["langs"][lang]["etymology"]["descr"] and not self.pgTerm["langs"][lang]["etymology"]["morph_links"]):
                    del self.pgTerm["langs"][lang]["etymology"]
                else:
                    if (not self.pgTerm["langs"][lang]["etymology"]["attrs"]):
                        del self.pgTerm["langs"][lang]["etymology"]["attrs"]
                    if (not self.pgTerm["langs"][lang]["etymology"]["morph_links"]):
                        del self.pgTerm["langs"][lang]["etymology"]["morph_links"]

            clsdellst = []
            for cls in self.pgTerm["langs"][lang]["meanings"]:
                if (not self.pgTerm["langs"][lang]["meanings"][cls]):
                    clsdellst.append(cls)
                else:
                    for meaning in self.pgTerm["langs"][lang]["meanings"][cls]:
                        if (not meaning["attrs"]):
                            del meaning["attrs"]
                        if (not meaning["links"]):
                            del meaning["links"]

            for cls in clsdellst:
                del self.pgTerm["langs"][lang]["meanings"][cls]

            if (not self.pgTerm["langs"][lang]["meanings"]):
                del self.pgTerm["langs"][lang]["meanings"]
            
            clsdellst = []
            for group in ["synonyms", "antonyms", "hypernyms", "hyponyms", "abbreviations"]:
                if (group in self.pgTerm["langs"][lang]):
                    for cls in self.pgTerm["langs"][lang][group]:
                        if (not self.pgTerm["langs"][lang][group][cls]):
                            clsdellst.append(cls)
                        else:
                            for meaning in self.pgTerm["langs"][lang][group][cls]:
                                if (not meaning["attrs"]):
                                    del meaning["attrs"]
                                if (not meaning["links"]):
                                    del meaning["links"]

                    for cls in clsdellst:
                        del self.pgTerm["langs"][lang][group][cls]

                    if (not self.pgTerm["langs"][lang][group]):
                        del self.pgTerm["langs"][lang][group]

        langdellst = []
        for lang in self.pgTerm["langs"]:
            if (not self.pgTerm["langs"][lang]):
                langdellst.append(lang)

        for lang in langdellst:
            del self.pgTerm["langs"][lang]


    def parse(self):
        while (True):
            end = (self.ifile.tell() >= self.range[1])
            line = self.ifile.readline()
            if (not line):
                break
            
            pagestart = self.page_start(line)
            if (pagestart and end):
                break

            if (self.pgTerm and not self.ignorePage):
                if (not self.property_collect(line)):
                    continue
                self.check_redirect(line)
                self.check_content(line)
                self.collect_content(line)
    

    def run(self):
        for input_range in iter(self.queue.get, None):
            self.__qinit__(input_range)
            self.ifile.seek(self.range[0])
            self.parse()
            self.queue.task_done()
            
            print(" {0:.4}".format((float(self.numPieces - self.queue.qsize()) / self.numPieces) * 100) + " %")

        self.ofile.close()
        self.ifile.close()
        self.queue.task_done()


def divide_set(size, num_pieces):
    piece_size = int(math.ceil(float(size) / num_pieces))
    idx = 0
    pieces = []

    while (idx < size):
        pieces.append([idx, idx + piece_size - 1])
        idx += piece_size

    pieces[-1][1] = size - 1

    return pieces


def main(args):
    wikparser = WiktionaryParser(None, args.ifilepath, args.ofilebasepath)
    wikparser.init_index()
    outfiles = [args.ofilebasepath]

    if (args.pgindexpath):
        pickle.dump(wikparser.pgIndex, open(args.pgindexpath, "wb"), pickle.HIGHEST_PROTOCOL)

    if (args.numprocs):
        ifilepath = args.ifilepath
        ofilebasepath = args.ofilebasepath
        numprocs = args.numprocs
        entriesperproc = args.entriesperproc
        inputsize = os.path.getsize(ifilepath)
        pieces = divide_set(inputsize, numprocs * entriesperproc)
        queue = JoinableQueue()
        outfiles = []
 
        for i in range(0, numprocs):
            outfile = ofilebasepath + "{:03,d}".format(i)
            WiktionaryParser(queue, ifilepath, outfile, i, numprocs, len(pieces), wikparser.pgIndex).start()
            outfiles.append(outfile)
        
        for i in range(0, len(pieces)):
            queue.put((pieces[i][0], pieces[i][1]))
            
        del wikparser

        for i in range(0, numprocs):
            queue.put(None)

    else:
        wikparser.parse()

    queue.join()
    outcat_filepath = args.ofilebasepath + ".jsonl"
    with open(outcat_filepath, "w") as outcat_file:
        #outcat_file.write("[\n")
        for outfile in outfiles:
            with open(outfile) as partout_file:
                for line in partout_file:
                    outcat_file.write(line)
            os.remove(outfile)

        #outcat_file.seek(-1, 1)
        #outcat_file.write("\n]")

    sort_db(outcat_filepath)



def parse_args():
    argparser = argparse.ArgumentParser()
    argparser.add_argument("ifilepath", type=str, help="Input file path (Wiktionary XML dump)")
    argparser.add_argument("ofilebasepath", type=str, help="Output base path")
    argparser.add_argument("-idx", "--pgindexpath", type=str, help="Page index output path")
    argparser.add_argument("-n", "--numprocs", type=int, help="Number of parallel extraction processes")
    argparser.add_argument("-pp", "--entriesperproc", type=int, help="Number of input entries for each process")

    return argparser.parse_args()


if __name__ == "__main__":
    main(parse_args())


