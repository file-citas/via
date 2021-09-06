#ifndef FUZZ_BST_H
#define FUZZ_BST_H

#include <stdint.h>
struct node {
   unsigned long long key;
   size_t len;
   struct node *left, *right;
};


struct node* insert(struct node* node, unsigned long long key, size_t len);
struct node* deleteNode(struct node* root, unsigned long long key);
struct node* search(struct node* root, unsigned long long key);
void clearTree(struct node *root);

#endif
