#include "hash.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>

typedef enum {false, true} Boolean;

#define lock pthread_mutex_lock
#define unlock pthread_mutex_unlock

// Di seguito sono definite alcune funzioni di hashing diverse.
// Viene utilizzata quella indicata con il define

#define hash_value hash_value_FNV1a

#define FNV_OFFSET 14695981039346656037UL
#define FNV_PRIME 1099511628211UL

size_t hash_value_FNV1a(HashTable* ht, char* key) {
    size_t hash = FNV_OFFSET;

    for (const char* p = key; *p; p++) {
        hash ^= (size_t)(unsigned char)(*p);
        hash *= FNV_PRIME;
    }

    return hash % ht->size;
}

size_t hash_value_sdbm(HashTable* ht, char* key) {
        size_t hash = 0;
        size_t counter;

        for (counter = 0; key[counter] != '\0'; counter++) {
                hash = key[counter] + (hash << 6) + (hash << 16) - hash;
        }

        return hash % ht->size;
}

size_t hash_value_djb2(HashTable* ht, char* key) {
        size_t hash = 0;
        size_t counter;

        for (counter = 0; key[counter] != '\0'; counter++) {
                hash = ((hash << 5) + hash) + key[counter];
        }

        return hash % ht->size;
}

#define TABLE_MAX_LOAD 70
#define TABLE_MIN_LOAD 30

HashTable* create_hash_table(size_t size) {
        HashTable *ht;
        
        // Controllo che la dimensione non sia troppo grande
        // e quindi faccia overflow
        if (size + size < size) {
                perror("Dimensione troppo grande");
                return NULL;
        }

        if (size == 0) {
                perror("Dimensione troppo piccola");
                return NULL;
        }

        // Alloco prima lo spazio per la HashTable stessa
        ht = malloc(sizeof(HashTable));
        if (ht == NULL) {
                perror("Errore durante l'allocazione della HashTable");
                return NULL;
        }
        
        // Alloco lo spazio per i singoli nodi
        ht->node = calloc(size, sizeof(Node));
        if (ht->node == NULL) {
                free(ht);
                perror("Errore durante l'allocazione dei nodi");
                return NULL;
        }

        ht->size = size;
        ht->num_elements = 0;
        ht->high_density = TABLE_MAX_LOAD;
        ht->low_density = TABLE_MIN_LOAD;

        return ht;
}

Boolean hash_expand(HashTable* ht) {
        HashTable* expanded;
        size_t doubled;
        size_t i;

        // Raddoppio le dimensioni della HashTable attuale
        // e controllo che non siano troppo grandi
        doubled = ht->size * 2;
        if (doubled + doubled < doubled) {
                return false;
        }

        // Creo una HashTable temporanea di dimensioni doppie
        expanded = create_hash_table(doubled);
        if (expanded == NULL) {
                return false;
        }
        
        // Procedo ad inserire tutti gli elementi della HashTable originale
        // in quella di dimensioni raddoppiate, così facendo gli hash verranno
        // calcolati sulla base delle nuove dimensioni
        for (i = 0; i < ht->size; i++) {  
                if (ht->node[i].key != NULL) {
                        hash_insert(expanded, 
                                ht->node[i].key, 
                                ht->node[i].element);
                }
        }

        // Procedo a copiare la memoria puntata dalla neontata HashTable
        // alla locazione di memoria originariamente puntata da ht
        memcpy(ht, expanded, sizeof(HashTable));
        return true;
}

int hash_insert(HashTable* ht, char *key, void* element) {
        size_t hash;
        char* key_copy;
        void* element_copy;
        
        // Controllo che chiave ed elemento non siano nulli
        if (key == NULL || element == NULL) {
                return -1;
        }

        // Verifico che il numero di nodi all'interno della HashTable non
        // superi il valore di densità superiore stabilito. In caso contrario
        // procedo ad espandere la HashTable raddoppiandone le dimensioni
        if ((int) (ht->num_elements/ht->size)*100 >= ht->high_density) {
                if (hash_expand(ht)) {
                        printf("HashTable espansa! Nuova dimensione: %ld\n", 
                                ht->size);
        }
        }
        
        // Computo il digest della chiave data
        hash = hash_value(ht, key);
        printf("Key: %s --> Digest: %lu\n", key, hash);
        
        // Inizio ciclando sul nodo fornito all'indice dato dal digest
        while (ht->node[hash].key != NULL) {
                // Qualora non sia vuoto confronto la chiave presente con 
                // quella fornita
                if (strcmp(key, ht->node[hash].key) == 0) {
                        // Se la chiave combacia non si tratta di una
                        // collisione bensì di un tentativo di sovrascrittura
                        printf("Updating '%s' (at %ld) with %s element\n",
                                        key, hash, (char*) element);
                        ht->node[hash].element = element;
                        ht->num_elements++;
                        return 0;
                }
                // Nel caso in cui le chiavi non combacino si tratta di una
                // collisione. Procedo con il linear probing: incremento 
                // l'indice e mi sposto a destra di una posizione fino a che 
                // non trovo un nodo NULL che termini il ciclo
                hash++;
                if (hash == ht->size) {
                        // Nel caso in abbia raggiunto il limite imposto
                        // dalle dimensioni della struttura, riparto da 0
                        // trattando l'array come circolare
                        hash = 0;
                }
        }

        // All'uscita del ciclo ho il valore dell'indice che punta al primo
        // nodo non NULL, motivo per il quale posso procedere a inserire i 
        // dati che sono stati forniti dall'utente
        printf("Inserting %s (at %ld) with '%s' key\n", 
                (char*) element, hash, key);
        // Effettuo una copia del valore contenuto all'interno del puntatore
        key_copy = strdup(key);
        element_copy = strdup((char*) element);
        ht->node[hash].key = (char*) key_copy;
        ht->node[hash].element = element_copy;
        ht->num_elements++;

        return 1;
}

void* hash_get(HashTable* ht, char* key) {
        size_t hash;
        size_t counter = 0;

        // Controllo che il numero di elementi sia > 1 
        // così da evitare il blocco di codice seguente
        if (ht->num_elements == 0) {
                return NULL;
        }
        
        // Viene computato il digest della chiave fornita
        hash = hash_value(ht, key);

        printf("Sto cercando l'elemento di chiave %s\n", key);
       
        // Inizio ciclando sul nodo fornito all'indice dato dal digest
        while (ht->node[hash].key != NULL) {
                counter++;
                if (counter == ht->size) {
                        // Controllo di non aver superato la dimensione
                        // della HashTable in iterazioni del ciclo
                        return NULL;
                }
                // Qualora non sia vuoto confronto la chiave presente con 
                // quella fornita
                if (strcmp(key, ht->node[hash].key) == 0) {
                        printf("Trovato alla pos. %lu\n", hash);
                        return ht->node[hash].element;
                }
                // Nel caso in cui le chiavi non combacino si tratta di una
                // collisione. Procedo con il linear probing: incremento 
                // l'indice e mi sposto a destra di una posizione fino a che 
                // non trovo un nodo NULL che termini il ciclo
                hash++;
                if (hash == ht->size) {
                        // Nel caso in abbia raggiunto il limite imposto
                        // dalle dimensioni della struttura, riparto da 0
                        // trattando l'array come circolare
                        hash = 0;
                }
                printf("Continuo cercando alla pos. %lu\n", hash);
        }
        
        // In caso contrario non è stato trovato niente
        return NULL; 
}

void* _test_nodes(HashTable* ht, Node* n1, Node *n2) {
        size_t h1, h2;
        
        h1 = hash_value(ht, n1->key);
        if (n2->key == NULL) {

                printf("Nodo NULL alla posizione successiva\n");
                n1->key = NULL;
                n1->element = NULL;
                
                return n1;
        
        }

        h2 = hash_value(ht, n2->key);

        printf("Testing per lo switch di %s e %s\n", n1->key, n2->key);
        // Viene confrontato l'hash per appurare l'avvenuta collisione
        if (h1 == h2) {
                // In caso collidano si effettua lo switch
                // dal nodo n2 verso quello n1
                printf("Trovati due nodi compatibili per lo switch\n");
                n1->key = n2->key;
                n1->element = n2->element;
                
                // Viene pulito il nodo n2
                printf("Procedo con l'eliminazione\n");
                n2->key = NULL;
                n2->element = NULL;
        } else {
                // In caso contrario viene semplicemente
                // pulito il nodo n1
                n1->key = NULL;
                n1->element = NULL;
        }

        // Decremento il numero di elementi
        ht->num_elements--;

        // In entrambi i casi viene ritornato il nodo
        // di cui era stata chiesta la rimozione
        return n1;
}

void* hash_remove(HashTable* ht, char* key) {
        size_t hash;
        size_t prober;
        
        // Computo l'hash della chiave data
        hash = hash_value(ht, key);
        printf("Inizio la rimozione dell'elemento con chiave: %lu\n", hash);
        
        // Controllo che all'indice indicato dal digest il nodo non sia NULL
        if (ht->node[hash].key != NULL) {
                // In caso di successo continuo verificando che le chiavi
                // corrispondano
                if (strcmp(key, ht->node[hash].key) == 0) {
                        // Prima di procedere con l'eliminazione faccio il
                        // probing del nodo alla posizione successiva. In
                        // questo modo cerco di tenere la HashTable il più
                        // libera possibile ed evito le cosidette TOMBSTONE
                        printf("Trovato nodo alla posizione indicata\n");
                        prober = hash + 1;
                        if (prober == ht->size) {
                                prober = 0;
                        }

                        return _test_nodes(ht, 
                                        &ht->node[hash], 
                                        &ht->node[prober]);
                }
        }

        return NULL;
}

size_t hash_num_elements(HashTable* ht) {
        size_t busy_nodes = 0;
        size_t i;

        // Controllo nodo per nodo se non sono nulli
        // e in caso affermativo incremento il numero
        // di nodi attualmente occupati
        for (i = 0; i < ht->size; i++) {
                if (ht->node[i].key != NULL) {
                        busy_nodes++;
                }
        }
        return busy_nodes;
}

void hash_set_resize_high_density(struct hash_table* ht, int fill_factor) {
        if (fill_factor < 1 || fill_factor > 99) {
                return;
        }

        if (fill_factor + fill_factor < fill_factor) {
                return;
        }

        ht->high_density = fill_factor;
}

void hash_set_resize_low_density(struct hash_table* ht, int fill_factor) {
        if (fill_factor < 1 || fill_factor > 99) {
                return;
        }

        if (fill_factor + fill_factor < fill_factor) {
                return;
        }

        ht->low_density = fill_factor;
}

void destroy_hash_table(HashTable* ht) {
        free(ht->node);
        free(ht);
}

void pretty_print(HashTable* ht) {
        size_t i;
        
        printf("\n\n");
        printf("\tindex\t|\tkey\t|\telement\n");
        for (i = 0; i < ht->size; i++) {
                printf("\t%lu\t|\t%s\t|\t%s\n", i, 
                                ht->node[i].key,
                                (char*) ht->node[i].element);
        }
        printf("\n\n");
}

