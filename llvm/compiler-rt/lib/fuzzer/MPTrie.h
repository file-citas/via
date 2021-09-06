#ifndef MPTRIE_H
#define MPTRIE_H

#include <iostream>
#include <unordered_map>
using namespace std;

// A Trie node
struct Trie
{
   // true when node is a leaf node
   bool isLeaf;
   uint64_t bt;

   // each node stores a map to its child nodes
   unordered_map<uint8_t, Trie*> map;
};

Trie* getNewTrieNode();
bool haveChildren(Trie const* curr);
bool deletion(Trie*& curr, uint8_t* str);
bool search(Trie* head, uint8_t* str);

#endif
