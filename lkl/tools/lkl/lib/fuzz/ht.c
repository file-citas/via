#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <pthread.h>

#define SIZE 0x1000
static void* hashArray[SIZE];
static pthread_mutex_t mu = PTHREAD_MUTEX_INITIALIZER;
static int n_items = 0;

static int hashCode(void *key) {
   return (int)(((unsigned long long) key) % SIZE);
}

int ht_items(void) {
  return __atomic_load_n(&n_items, __ATOMIC_SEQ_CST);
}

void *ht_search(void *key) {
   void *ret = NULL;
   //get the hash
   int hashIndex = hashCode(key);

   pthread_mutex_lock(&mu);
   //move in array until an empty
   while(hashArray[hashIndex] != NULL) {

      if(hashArray[hashIndex] == key) {
         ret = hashArray[hashIndex];
         goto out;
      }

      //go to next cell
      ++hashIndex;

      //wrap around the table
      hashIndex %= SIZE;
   }

out:
   pthread_mutex_unlock(&mu);
   return ret;
}

int ht_insert(void *key) {
   int ret = -1;
   //get the hash
   int hashIndex = hashCode(key);

   pthread_mutex_lock(&mu);
   //move in array until an empty or deleted cell
   while(hashArray[hashIndex] != NULL) {
      if(hashArray[hashIndex] == key) {
         ret = hashIndex;
         goto out;
      }
      //go to next cell
      ++hashIndex;

      //wrap around the table
      hashIndex %= SIZE;
   }

   __atomic_add_fetch(&n_items, 1, __ATOMIC_SEQ_CST);
   hashArray[hashIndex] = key;
out:
   pthread_mutex_unlock(&mu);
   return ret;
}

void* ht_delete(void *key) {
   void *ret = NULL;
   //get the hash
   int hashIndex = hashCode(key);

   pthread_mutex_lock(&mu);
   //move in array until an empty
   while(hashArray[hashIndex] != NULL) {

      if(hashArray[hashIndex] == key) {
         void* temp = hashArray[hashIndex];

         //assign a dummy item at deleted position
         hashArray[hashIndex] = NULL;
         __atomic_sub_fetch(&n_items, 1, __ATOMIC_SEQ_CST);
         ret = temp;
         goto out;
      }

      //go to next cell
      ++hashIndex;

      //wrap around the table
      hashIndex %= SIZE;
   }

out:
   pthread_mutex_unlock(&mu);
   return NULL;
}
