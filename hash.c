#include "hash.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>

#define lock pthread_mutex_lock
#define unlock pthread_mutex_unlock

size_t hash_value(HashTable* ht, char* key) {
        int i;
        size_t hash = 123;
        // Funzione di hash: prende tutte i caratteri della chiave e li somma 
        // in modulo alla dimensione della HashTable

        for (i = 0; key[i] != '\0'; i++) {
                hash += (int) key[i];
        }
        return hash % ht->size;
}

HashTable* create_hash_table(size_t size) {
        HashTable *ht;
        
        // Controllo che la dimensione non sia troppo grande
        // e quindi faccia overflow
        if (size + size < size) {
                perror("Dimensione troppo grande");
                return NULL;
        }

        // Alloco prima lo spazio per la HashTable stessa
        ht = (HashTable*) malloc(sizeof(HashTable));
        if (ht == NULL) {
                perror("Errore durante l'allocazione della HashTable");
                return NULL;
        }
        
        // Alloco lo spazio per i singoli nodi
        ht->node = calloc(size, sizeof(Node));
        if (ht->node == NULL) {
                perror("Errore durante l'allocazione dei nodi");
                return NULL;
        }

        ht->size = size;

        return ht;
}

int hash_insert(HashTable* ht, char *key, void* element) {
        size_t hash;
        Node* node;
        
        // Viene prima di tutto verificato che vi sia spazio a disposizione
        // per un nuovo elemento. Questo impedisce anche al ciclo sottostante
        // di ciclare all'infinito alla ricerca di un nodo NULL
        if (hash_num_elements(ht) == ht->size) {
                return -1;
        }

        // Alloco e popolo il nodo con i parametri forniti
        node = malloc(sizeof(Node));
        if (node == NULL) {
                perror("Errore allocazione nodo");
                exit(-1);
        }
        node->key = key;
        node->element = element;
        
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
                        ht->node[hash] = *node;
                        return 1;
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
        ht->node[hash] = *node;
        return 1;
}

void* hash_get(HashTable* ht, char* key) {
        size_t hash;
        
        // Viene computato il digest della chiave fornita
        hash = hash_value(ht, key);

        printf("Sto cercando l'elemento di chiave %s\n", key);
       
        // Inizio ciclando sul nodo fornito all'indice dato dal digest
        while (ht->node[hash].key != NULL) {
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
                if (hash > ht->size) {
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

void* test_nodes(HashTable* ht, Node* n1, Node *n2) {
        size_t h1, h2;
        
        if (n2->key == NULL) {
                n1->key = NULL;
                n1->element = NULL;
                return n1;
        }

        printf("Testing per lo switch di %s e %s\n", n1->key, n2->key);
        
        // Viene confrontato l'hash per appurara l'avvenuta collisione
        h1 = hash_value(ht, n1->key);
        h2 = hash_value(ht, n2->key);

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
                
                // E viene ritornato il nodo n1 
                // ora con i dati del nodo n2 

        } else {
                // In caso contrario viene semplicemente
                // pulito il nodo n1
                n1->key = NULL;
                n1->element = NULL;

        }
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
                        return test_nodes(ht, 
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

