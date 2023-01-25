# Operating Systems Design assignment
A small library that implements hash table with synchronization mechanism. Uses Open Addressing techniques 
(Linear Probing), tombstones and `pthread_rwlock_t` locking utilities. Code is in [`lib`](lib) folder.
API are the following:
- `hash_insert(HashTable* ht, char* key, void* element)`
- `hash_get(HashTable* ht, char* key)`
- `hash_remove(HashTable* ht, char* key)`

## Testing
There are 2 scripts to test this library available (inside [test]()). Both do count words of 
[words.txt](sample/words.txt):

- `demo.c` for single thread testing purposes 
- `demo-thread.c` for multi-thread purposes
- `demo.py` for Python's `dict` comparison

## Report
A [report](report.pdf) on the project and its performance is available (in italian) 