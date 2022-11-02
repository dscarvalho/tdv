#-*- coding: utf8 -*-

import re

RGX = {
    "page": r"<page>",
    "title": r"<title>(?P<title>[^<]+)</title>",
    "wikid": r"<id>(?P<id>[0-9]+)</id>",
    "term": r"\[\[(?P<term>[0-9a-zA-Z\-' ]+)(#[^\|]+)?(\|[0-9a-zA-Z\-' ]+)?\]\]",
    "etymology_section": r"===Etymology===",
    "synonym_section": r"=====?Synonyms=====?",
    "antonym_section": r"=====?Antonyms=====?",
    "hypernym_section": r"=====?Hypernyms=====?",
    "hyponym_section": r"=====?Hyponyms=====?",
    "abbreviation_section": r"=====?Abbreviations=====?",
    "translation_section": r"====?Translations====?",
    "lang_content_section": r"(^|[^=])==(?P<lang>[a-zA-Z -]+)==([^=]|$)",
    "redirect": r"#REDIRECT( |:|: )?\[\[(?P<term>[^\]]+)\]\]",
    "section_begin": r"(^| +)=+[^=]+=+",
    "prefix": r"{{prefix\|(?P<pre>[^\|]+)\|(?P<stem>[^\|]+)\|lang=(?P<lang>[^}]+)}}",
    "suffix": r"{{suffix\|(?P<stem>[^\|]+)\|(?P<suf>[^\|]+)\|lang=(?P<lang>[^}]+)}}",
    "confix": r"{{confix\|(?P<parts>[^\}]+)\|lang=(?P<lang>[^}]+)}}",
    "affix": r"{{affix\|(?P<lang>[^\|]+)\|(?P<parts>[^\}]+)}}",
    "attr_generic": r"{{([^}]+)}}",
    "attr_specific": r"{{(?P<attr>(context|sense|qualifier|gloss|term|taxlink))\|(?P<val>[^\}]+)(\|lang=(?P<lang>[a-z]+))?}}",
    "label": r"{{(?P<attr>(lb?|label))\|(?P<lang>[a-z]+)(\|(?P<val>[^\}]+))}}",
    "wikne": r"{{(?P<attr>w)\|(?P<val>[0-9a-zA-Z\-' ]+)(#[^\|]+)?(\|[0-9a-zA-Z\-' ]+)?}}",
    "inflection": r"{{(?P<inflec>[a-zA-Z\- ]+) of\|(?P<val>[^\}]+)(\|lang=(?P<lang>[a-z]+))?}}",
    "etym_link": r"{{m\|(?P<lang_orig>[^\|]+)\|(?P<term>[^\}]+)}}",
    "sense": r"^##? (?P<sense>.+)",
    "example": r"^#: ''(?P<example>.+)''$",
    "synonym": r"^\* (?P<syn>.+)",
    "antonym": r"^\* (?P<ant>.+)",
    "hypernym": r"^\* (?P<hyper>.+)",
    "hyponym": r"^\* (?P<hypo>.+)",
    "abbreviation": r"^\* (?P<abbrev>.+)",
    "translation_child": r"\*(?P<child>:)? (?P<lang>[^:]+): (?P<trls>{{.+)",
    "translation_parent": r"\*:? (?P<lang>[^:]+):$",
    "translation_open": r"{{trans-top\|(?P<mng>[^}]+)}}",
    "translation_item": r"{{t\+?\|(?P<langcode>[a-z]+)\|(?P<term>[^\|}]+)[^}]*}}",
    "definition_prefix1": r"^show ",
    "html_escape": r"&[^;]+;"
}

WIKI_REJECT = r":"

for k in RGX:
    RGX[k] = re.compile(RGX[k])


ATTR_TYPES_ORDER = [
    "inflection",
    "label",
    "wikne",
    #"attr_specific",
    "attr_generic"
]

ATTR_GEN = {
    "inflection": lambda mo: ("inflec", mo.groupdict()["inflec"].split("-")[-1], mo.groupdict()["val"].split("|")[0]),
    "label": lambda mo: ("label", mo.groupdict()["val"], mo.groupdict()["lang"]),
    "wikne": lambda mo: ("wikne", mo.groupdict()["val"]),
    #"attr_specific": lambda mo: (mo.groupdict()["attr"], mo.groupdict()["val"], mo.groupdict()["lang"]),
    "attr_generic": lambda mo: mo.group(1).split("|")
}


def clean_expr(s, attr_type="", keeplabels=False):
    cln_str = s
    
    if (not attr_type or attr_type == "term"):
        cln_str = RGX["term"].sub(r"\g<term>", cln_str)
    if (not attr_type or attr_type == "inflection"):
        cln_str = RGX["inflection"].sub(r"\g<inflec> \g<val>", cln_str)
    if (not attr_type or attr_type == "attr_specific"):
        cln_str = RGX["attr_specific"].sub(r"", cln_str)
    if (not attr_type or attr_type == "label"):
        if (not keeplabels):
            cln_str = RGX["label"].sub(r"", cln_str)
        else:
            cln_str = RGX["label"].sub(r"\g<val>", cln_str)
    if (not attr_type or attr_type == "wikne"):
        cln_str = RGX["wikne"].sub(r"\g<val>", cln_str)
    
    return cln_str.strip()

