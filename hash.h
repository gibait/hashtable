#ifndef _HASH_TABLE_H
#define _HASH_TABLE_H

#include <stddef.h>
#include <pthread.h>

/* HashTable entries
 * contains a key which is used to index the hash table
 * and the element itself which is stored at the key */
typedef struct node {
        char* key;
        void* element;
} Node;

/* HashTable structure
 * contains an array of Node
 * and size of the hashtable */
typedef struct hash_table {
        Node *node;
        size_t size;
        size_t num_elements;
        int high_density;
        int low_density;
        pthread_rwlock_t lock;
} HashTable;


/* Create an empty hash table, with size cells. 
 * Return a pointer to a structure with the table's information, of
 * NULL in case of failure (e.g. out of free memory) */
HashTable* create_hash_table(size_t size);

/* Insert the given element, with the given key, to the given hash table. 
 * Return 1 on success, 0 if an element with
 * the same key is already found in the table, or -1 if the table is full */
int hash_insert(HashTable* ht, char* key, void* element);

/* Retrive the element, with the given key, to the given hash table.
 * Return void pointer which should be cast to whatever the element
 * originally was or NULL in case of failure */
void* hash_get(HashTable* ht, char* key);

/* Remove the element with the given key from the hash table. 
 * Return a pointer to the removed element, or NULL
 * if the element was not found in the table. */
void* hash_remove(HashTable* ht, char* key);

/* Return the number of elements found in the given hash table */
size_t hash_num_elements(HashTable* ht);

/* Delete the given hash table, freeing any memory it currently uses
 * (but do NOT free any elements that might still be in it) */
void destroy_hash_table(HashTable* ht);

/* Hashing function */
size_t hash_value(HashTable* ht, char* key);

/* Set the fill density of the table after which it will be expanded. 
   The fill factor is a number between 1 and 100. */
void hash_set_resize_high_density(struct hash_table* ht, int fill_factor);

/* Set the fill density of the table below which it will be shrank. 
   The fill factor is a number between 1 and 100.*/
void hash_set_resize_low_density(struct hash_table* ht, int fill_factor);

#define EMPTY (void*) 0x00
#define TOMBSTONE (void*) 0x01

#endif // _HASH_TABLE_H
