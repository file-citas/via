#include <lkl_host.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "bst.h"

#define TRACE_BST
#undef TRACE_BST

struct node* newNode(unsigned long long item, size_t len)
{
   struct node* temp
      = (struct node*)lkl_host_ops.mem_alloc(sizeof(struct node));
   temp->key = item;
   temp->len = len;
   temp->left = NULL;
   temp->right = NULL;
   return temp;
}

struct node* insert(struct node* node, unsigned long long key, size_t len)
{
   /* If the tree is empty, return a new node */
   if (node == NULL) {
      struct node *n = newNode(key, len);
#ifdef TRACE_BST
      lkl_printf("New Node @%llx %llx+%ld\n", (uint64_t)n, key, len);
#endif
      return n;
   }

   /* Otherwise, recur down the tree */
   if (key + len <= node->key) {
      struct node *n = insert(node->left, key, len);
      node->left = n;
   } else if (node->key + node->len <= key) {
      struct node *n = insert(node->right, key, len);
      node->right = n;
   } else {
      //lkl_printf("ERROR overlap %lld+%ld with %lld+%ld\n", key, len, node->key, node->len);
      return node;
   }

   /* return the (unchanged) node pointer */
   return node;
}

/* Given a non-empty binary search
   tree, return the node
   with minimum key value found in
   that tree. Note that the
   entire tree does not need to be searched. */
static struct node* minValueNode(struct node* node)
{
   struct node* current = node;

   /* loop down to find the leftmost leaf */
   while (current && current->left != NULL) {
      current = current->left;
   }

   return current;
}

/* Given a binary search tree
   and a key, this function
   deletes the key and
   returns the new root */
struct node* deleteNode(struct node* root, unsigned long long key)
{
   // base case
   if (root == NULL)
      return root;

   // If the key to be deleted
   // is smaller than the root's
   // key, then it lies in left subtree
   if (key < root->key)
      root->left = deleteNode(root->left, key);

   // If the key to be deleted
   // is greater than the root's
   // key, then it lies in right subtree
   else if (key >= root->key + root->len)
      root->right = deleteNode(root->right, key);

   // if key is same as root's key,
   // then This is the node
   // to be deleted
   else if (key >= root->key && key <root->key + root->len) {
      // node with only one child or no child
      if (root->left == NULL) {
         struct node* temp = root->right;
#ifdef TRACE_BST
         if(!temp)
         lkl_printf("Free Node r @%llx %llx+%ld\n", (uint64_t)root, root->key, root->len);
         else
         lkl_printf("Free Node r @%llx %llx+%ld, ret @%llx %llx+%ld\n",
               (uint64_t)root, root->key, root->len,
               (uint64_t)temp, temp->key, temp->len
               );
#endif
         lkl_host_ops.mem_free(root);
         return temp;
      }
      else if (root->right == NULL) {
         struct node* temp = root->left;
#ifdef TRACE_BST
         if(!temp)
         lkl_printf("Free Node r @%llx %llx+%ld\n", (uint64_t)root, root->key, root->len);
         else
         lkl_printf("Free Node r @%llx %llx+%ld, ret @%llx %llx+%ld\n",
               (uint64_t)root, root->key, root->len,
               (uint64_t)temp, temp->key, temp->len
               );
#endif
         lkl_host_ops.mem_free(root);
         return temp;
      }

      // node with two children:
      // Get the inorder successor
      // (smallest in the right subtree)
      struct node* temp = minValueNode(root->right);

      // Copy the inorder
      // successor's content to this node
      root->key = temp->key;

      // Delete the inorder successor
      root->right = deleteNode(root->right, temp->key);
   }
   return root;
}

// C function to search a given start in a given BST
struct node* search(struct node* root, unsigned long long key)
{
    // Base Cases: root is null or key is present at root
    if (root == NULL || (root->key <= key && root->key + root->len > key)) {
#ifdef TRACE_BST
       if(root)
          lkl_printf("Found Node @%llx %llx+%ld / %llx\n", (uint64_t)root, root->key, root->len, key);
#endif
       return root;
    }

    // key is greater than root's key
    if (root->key + root->len <= key)
       return search(root->right, key);
    else if(root->key >= key)
       return search(root->left, key);
    return NULL;
}

void clearTree(struct node *root) {
   if(root == NULL) {
      return;
   }
   clearTree(root->right);
   clearTree(root->left);

#ifdef TRACE_BST
   lkl_printf("Clear Node @%llx %llx+%ld\n", (uint64_t)root, root->key, root->len);
#endif
   lkl_host_ops.mem_free(root);
}

