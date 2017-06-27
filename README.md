## WikTDV: Wiktionary-based Term Definition Vectors

This is an implementation of the Term Definition Vectors (TDV) method for language representation. TDV provides *high dimensional, sparse* vector representations of lexical items (terms) ranging from morphemes to phrases, based on the definitions of their meanings. It contrasts with distributional representation methods, such as [word2vec](https://code.google.com/archive/p/word2vec/) and [GloVe](https://github.com/stanfordnlp/GloVe/), which define term meanings from their usage patterns (context windows). Compared to distributional methods, TDV performs better at *semantic similarity* computation, where the former perform better at *semantic relatedness*.
In this implementation, each concept vector represents a sense described in [Wiktionary](https://www.wiktionary.org), as well as its available translations. The TDV representations can be used in several natural language processing applications and since each vector dimension represents a specific definition property, they are also *human-readable*. See the [paper](https://www.aclweb.org/anthology/E/E17/E17-1085.pdf) for more information on Term Definition Vectors.


## Interesting Features of WikTDV
### Sense polarity
Computing vector cosine will yield positive values for adjectives agreeing in sense, such as "*skinny*" and "*thin*", and negative values for those with opposing senses, such as "*happy*" and "*sad*".

### Multi-language
Translations available in Wiktionary are joined at the same vector space, so that terms like "*today*", "*hoy*" and "*今日*" have similar representations and can be compared meaningfully.

### Part-of-Speech (POS) disambiguation
POS information can be used to reduce the number of senses in the definition vectors, disambiguating their meaning. POS-tagging can be done through popular tools, such as the [Stanford Tagger](https://nlp.stanford.edu/software/tagger.shtml) or the [NLTK](http://www.nltk.org/) toolset.

## Quick start
1. Download the code: git clone https://github.com/dscarvalho/tdv.git
2. Change to the 'tdv' directory, from the project root: cd tdv/tdv
3. Run 'make all'
4. Change to the 'build' directory: cd build
5. Run
  - './run\_service.sh' to start the TDV web service.
  - './gen\_vectors' to generate vectors and write to a file.

You can adjust the base link weights, among other parameters in the configuration files under 'build/cfg/'.
A complete documentation of the system is under construction and will be included in the repository soon.

#### Dependencies
- A UNIX-like system: Linux, Mac OSX.
- A recent C++ compiler, with support for C++11: LLVM/Clang (>= 3.1), GCC (>= 4.8).
- Python 2.7
- [JSON for modern C++](https://github.com/nlohmann/json) (included in the source).
- [CppCMS](http://cppcms.com/) (downloaded during setup).

CppCMS dependencies should be installed prior to setup, in special CMake, Zlib and PCRE. See [CppCMS installation page](http://cppcms.com/wikipp/en/page/cppcms_1x_build) for details.

### Web service methods
The TDV web service provides the following methods:
- similarity: returns a similarity measure (cosine + heuristics) for a given pair of terms and their corresponding POS (optional).
- similar: returns Wiktionary entries that are similar to a provided term, in decreasing order of similarity. Can be reversed to obtain the "most dissimilar" or "opposite" entries.
- repr: returns a the definition vector for the given term and POS (optional).
- disambig: given a sentence and a term from the sentence, with optional POS, return the sense definition of the given term.
- wiktdef: pre-processed Wiktionary entry of a term.

All service responses are JSON compatible.

#### Examples:
* http://localhost:6480/tdv/similarity?term1=happy&term2=sad
* http://localhost:6480/tdv/similarity?term1=cat&pos1=noun&term2=lion&pos2=noun

* http://localhost:6480/tdv/similar?term=city&pos=noun

* http://localhost:6480/tdv/repr?term=move&pos=verb&human=true

* http://localhost:6480/tdv/disambig?sent=the%20conference%20chair%20will%20make%20an%20announcement&term=chair&pos=noun

* http://localhost:6480/tdv/wiktdef?term=cat



## Download concept vectors and pre-processed Wiktionary data
Extracted concept vectors can be download from the following links:

- [Concept vectors for English Wiktionary 2017-04-20 (cn,de,en,es,fr,jp,pt,vn)](http://www.jaist.ac.jp/~s1520009/files/tdv/enwiktdb.vectors.json.bz2)
- [Concept vectors for English Wiktionary 2017-04-20 (cn,de,en,es,fr,jp,pt,vn) (human-readable)](http://www.jaist.ac.jp/~s1520009/files/tdv/enwiktdb.meanings.json.bz2)
- [Concept vectors for English Wiktionary 2017-04-20 (en, Translingua)](http://www.jaist.ac.jp/~s1520009/files/tdv/enwiktdb.vectors_en-tr.json.bz2)
- [Concept vectors for English Wiktionary 2017-04-20 (en, Translingua) (human-readable)](http://www.jaist.ac.jp/~s1520009/files/tdv/enwiktdb.meanings_en-tr.json.bz2)

Wiktionary pre-processed data can be download from the following links:
- [Pre-processed English Wiktionary 2017-04-20 (all languages)](http://www.jaist.ac.jp/~s1520009/files/tdv/enwiktdb_sorted_min.json.bz2)
- [Pre-processed English Wiktionary 2017-04-20 (English, Translingua)](http://www.jaist.ac.jp/~s1520009/files/tdv/enwiktdb_sorted_en-tr_min.json.bz2)

The files are minified and compressed with bzip2. A non-minified version of the pre-processed Wiktionary database is also available:
- [Pre-processed English Wiktionary 2017-04-20 (all languages) (non-minified)](http://www.jaist.ac.jp/~s1520009/files/tdv/enwiktdb_sorted.json.bz2)
- [Pre-processed English Wiktionary 2017-04-20 (English, Translingua) (non-minified)](http://www.jaist.ac.jp/~s1520009/files/tdv/enwiktdb_sorted_en-tr.json.bz2)

These files are distributed under the [Creative Commons Attribution-ShareAlike 3.0](https://creativecommons.org/licenses/by-sa/3.0/) license.


## Processing the Wiktionary database from scratch
You can also process an up-to-date Wiktionary database file (XML) in the following way:

1. Change to the 'wiktparser' directory, from the project root: cd tdv/wiktparser
2. Run 'python extract.py -idx [path of temp. index] -n [num. parallel processes] -pp [num. of entries per process] [path of Wiktionary XML database dump] [path of the output file (JSON)]'
  - The number of processes and entries per process should be adjusted according to the number of CPUs and available main memory.
3. To filter the output file for specific languages, run 'python langfilter.py [path of the preprocessed Wiktionary file (JSON)] [languages]'
  - where [languages] is a comma separated list of the desired languages. Ex: "English,German,Chinese".
  
  
 ## License, Citations and Related projects
 This project is released under the free MIT license. You are free to copy, modify and redistribute the code, under the condition of including the original copyright notice and license in all copies of substantial portions of this software.
 
 If you use this project in your research, please cite the paper:
 
 Danilo S. Carvalho and Minh Le Nguyen. *Building Lexical Vector Representations from Concept Definitions*. [[pdf]](https://www.aclweb.org/anthology/E/E17/E17-1085.pdf) [[bib]](https://aclweb.org/anthology/E/E17/E17-1085.bib)
 
 Feel free to send us links to your project or research paper related to WikTDV, that you think will be useful for others.
 Contact info: {danilo, nguyenml} [at] jaist.ac.jp
 

