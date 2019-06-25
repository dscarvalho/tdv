#ifndef TYPES_H
#define TYPES_H

#include <string>
#include <vector>
#include <set>
#include <unordered_map>
typedef std::string string;
typedef unsigned int uint;
typedef unsigned long ulong;
typedef unsigned char ubyte;
template <class T>
using vector = std::vector<T>;
template <class T>
using set = std::set<T>;
template <class K, class V>
using umap = std::unordered_map<K,V>;

#endif
