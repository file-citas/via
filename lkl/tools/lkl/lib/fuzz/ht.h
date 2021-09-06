#ifndef FUZZ_HT_H
#define FUZZ_HT_H
int ht_items(void);
void *ht_search(void *key);
int ht_insert(void *key);
void* ht_delete(void *key);
#endif
