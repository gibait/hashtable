#if 0
  #define LOG(a) printf a
#else
  #define LOG(a) (void)0
#endif

#include "hash.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>

typedef enum {false, true} Boolean;

#define rdlock pthread_rwlock_rdlock
#define wrlock pthread_rwlock_wrlock
#define rwlunlock pthread_rwlock_unlock

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

        pthread_rwlock_init(&ht->lock, NULL);

        ht->size = size;
        ht->num_elements = 0;
        ht->high_density = TABLE_MAX_LOAD;
        ht->low_density = TABLE_MIN_LOAD;

        return ht;
}

/*
 * Questa funzione viene utilizzata per l'inserimento linear probing
 * dei nodi all'interno della HashTable. Non è protetta da mutex perché
 * viene chiamata da una funziona essa stessa protetta da mutex.
 */
static int insert(Node* node, size_t hash, char* key, void* element, size_t ht_size) {
        // Inizio ciclando sul nodo fornito all'indice dato dal digest
        while (node[hash].key != NULL) {
                // Qualora non sia vuoto confronto la chiave presente con 
                // quella fornita
                if (strcmp(key, node[hash].key) == 0) {
                        // Se la chiave combacia non si tratta di una
                        // collisione bensì di un tentativo di sovrascrittura
                        LOG(("Updating '%s' (at %ld) with %s element\n",
                                        key, hash, (char*) element));
                        node[hash].element = strdup((char*) element);
                        return 0;
                }
                // Nel caso in cui le chiavi non combacino si tratta di una
                // collisione. Procedo con il linear probing: incremento 
                // l'indice e mi sposto a destra di una posizione fino a che 
                // non trovo un nodo NULL che termini il ciclo
                hash++;
                if (hash == ht_size) {
                        // Nel caso in abbia raggiunto il limite imposto
                        // dalle dimensioni della struttura, riparto da 0
                        // trattando l'array come circolare
                        hash = 0;
                }
        }

        // All'uscita del ciclo ho il valore dell'indice che punta al primo
        // nodo non NULL, motivo per il quale posso procedere a inserire i 
        // dati che sono stati forniti dall'utente
        LOG(("Inserting %s (at %ld) with '%s' key\n", 
                (char*) element, hash, key));
        // Effettuo una copia del valore contenuto all'interno del puntatore
        node[hash].key = strdup(key);
        node[hash].element = strdup((char*) element);

        return 1;
}

/*
 * Questa funzione viene chiamata quando la dimensione della HashTable
 * supera il limite fissato. Raddoppia le dimensioni della stessa
 * ed effettua una copia dei nodi inserendoli con hash aggiornato alle
 * nuove dimensioni.
 */
Boolean hash_expand(HashTable* ht) {
        Node* copy;
        size_t doubled;
        size_t i;
        size_t hash;
        size_t original_size = ht->size;


        // Raddoppio le dimensioni della HashTable attuale
        // e controllo che non siano troppo grandi
        doubled = ht->size * 2;
        if (doubled + doubled < doubled) {
                return false;
        }

        // Alloco un nuovo array di nodi con dimensione doppia
        copy = calloc(doubled, sizeof(Node));
        if (copy == NULL) {
                return false;
        }

        // Assegno la nuova dimensione alla HashTable
        // (necessario per utilizzare correttamente la funzione di hash)
        ht->size = doubled;

        // Resetto il numero di elementi
        ht->num_elements = 0;

        // Inserisco nodo per nodo quelli non nulli all'interno del
        // nuovo array di dimensioni raddoppiate, utilizzando la funzione
        // d'insert, e libero la memoria allocata per i nodi originali.
        for (i = 0; i < original_size; i++) {
                if (ht->node[i].key != NULL) {
                        hash = hash_value(ht, ht->node[i].key);
                        if (insert(copy,
                                        hash, 
                                        ht->node[i].key, 
                                        ht->node[i].element, 
                                        ht->size) == 1) {
                                ht->num_elements++;
                        }
                        free(ht->node[i].key);
                        free(ht->node[i].element);
                }
        }
        // Assegno l'array di nodi raddoppiati alla HashTable
        free(ht->node);
        ht->node = copy;

        return true;
}

/*
 * Questa funzione viene chiamata quando la dimensione della HashTable
 * cala sotto il limite fissato. Dimezza le dimensioni della stessa
 * ed effettua una copia dei nodi inserendoli con hash aggiornato alle
 * nuove dimensioni.
 */
Boolean hash_shrink(HashTable* ht) {
        Node* copy;
        size_t half;
        size_t i;
        size_t hash;
        size_t original_size = ht->size;


        // Raddoppio le dimensioni della HashTable attuale
        // e controllo che non siano troppo piccole
        half = ht->size / 2;
        if (half < 1) {
                return false;
        }

        // Alloco un nuovo array di nodi con dimensione dimezzata
        copy = calloc(half, sizeof(Node));
        if (copy == NULL) {
                return false;
        }
        
        // Assegno la nuova dimensione alla HashTable
        // (necessario per utilizzare correttamente la funzione di hash)
        ht->size = half;

        // Resetto il numero di elementi
        ht->num_elements = 0;

        // Inserisco nodo per nodo quelli non nulli all'interno del
        // nuovo array di dimensioni dimezzate, utilizzando la funzione
        // d'insert, e libero la memoria allocata per i nodi originali.
        for (i = 0; i < original_size; i++) {
                if (ht->node[i].key != NULL) {
                        hash = hash_value(ht, ht->node[i].key);
                        if (insert(copy,
                                   hash,
                                ht->node[i].key, 
                                   ht->node[i].element,
                                   ht->size) == 1) {
                                ht->num_elements++;
                        }
                        free(ht->node[i].key);
                        free(ht->node[i].element);
                }
        }
        // Assegno l'array di nodi dimezzati alla HashTable
        free(ht->node);
        ht->node = copy;

        return true;
}

/*
 * Questa funzione agisce da wrapper della funzione d'inserimento.
 * Controlla che la chiave e l'elemento non siano nulli, in caso
 * contrario ritorna -1; controlla che la dimensione della HashTable
 * non superi il limite superiore fissato, ridimensionandola in tal caso;
 * e utilizza la write lock per effettuare l'inserimento.
 */
int hash_insert(HashTable* ht, char* key, void* element) {
        size_t hash;
        int retr;
        
        // Controllo che chiave ed elemento non siano nulli
        if (key == NULL || element == NULL) {
                return -1;
        }

        // Acquisisco il lock per la scrittura
        wrlock(&ht->lock);

        // Verifico che il numero di nodi all'interno della HashTable non
        // superi il valore di densità superiore stabilito. In caso contrario
        // procedo ad espandere la HashTable raddoppiandone le dimensioni
        if ((int) (ht->num_elements*100/ht->size) >= ht->high_density) {
                LOG(("HashTable troppo PICCOLA, devo ridimensionare!\n"));
                
                if (hash_expand(ht)) {
                        LOG(("HashTable espansa! Nuova dimensione: %ld\n", 
                                ht->size));
        }
        }
        
        // Computo il digest della chiave data
        hash = hash_value(ht, key);
        LOG(("Key: %s --> Digest: %lu\n", key, hash));
        
        // Utilizzo la funzione d'inserimento e controllo il valore restituito.
        retr = insert(ht->node, hash, key, element, ht->size);
        if (retr == 1) {
                // Nel caso in cui sia uno, cioè di nuova chiave,
                // incremento il numero di elementi della HashTable
        ht->num_elements++;
        }

        // Rilascio il lock
        rwlunlock(&ht->lock);
        // Ritorno il valore restituito dall'inserimento
        return retr;
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

        LOG(("Sto cercando l'elemento di chiave %s\n", key)); 

        rdlock(&ht->lock);
        // Inizio ciclando sul nodo fornito all'indice dato dal digest
        while (ht->node[hash].key != NULL) {
                counter++;
                if (counter == ht->size) {
                        // Controllo di non aver superato la dimensione
                        // della HashTable in iterazioni del ciclo
                        rwlunlock(&ht->lock);
                        return NULL;
                }
                // Qualora non sia vuoto confronto la chiave presente con 
                // quella fornita
                if (strcmp(key, ht->node[hash].key) == 0) {
                        LOG(("Trovato alla pos. %lu\n", hash));
                        rwlunlock(&ht->lock);
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
                //LOG(("Continuo cercando alla pos. %lu\n", hash));
        }
        
        rwlunlock(&ht->lock);
        // In caso contrario non è stato trovato niente
        return NULL; 
}

void* _test_nodes(HashTable* ht, Node* n1, Node *n2) {
        size_t h1, h2;
        
        h1 = hash_value(ht, n1->key);
        if (n2->key == NULL) {

                LOG(("Nodo NULL alla posizione successiva\n"));
                n1->key = NULL;
                n1->element = NULL;
                
                return n1;
        }

        h2 = hash_value(ht, n2->key);

        LOG(("Testing per lo switch di %s e %s\n", n1->key, n2->key));
        // Viene confrontato l'hash per appurare l'avvenuta collisione
        if (h1 == h2) {
                // In caso collidano si effettua lo switch
                // dal nodo n2 verso quello n1
                LOG(("Trovati due nodi compatibili per lo switch\n"));
                n1->key = n2->key;
                n1->element = n2->element;
                
                // Viene pulito il nodo n2
                LOG(("Procedo con l'eliminazione\n"));
                n2->key = NULL;
                n2->element = NULL;
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
        size_t counter = 0;

        wrlock(&ht->lock);

        if ((int) (ht->num_elements*100/ht->size) <= ht->low_density) {
                LOG(("HashTable troppo GRANDE, devo ridimensionare!\n"));

                if (hash_shrink(ht)) {
                        LOG(("HashTable rimpicciolita! Nuova dimensione: "
                             "%ld\n", ht->size));
                }
        }
        
        // Computo l'hash della chiave data
        hash = hash_value(ht, key);
        LOG(("Inizio la rimozione dell'elemento con chiave: %lu\n", hash));
        
        for (; counter < ht->size; counter++, hash = (hash + 1) % ht->size) {
                // Controllo che all'indice indicato dal digest 
                // il nodo non sia NULL
                if (ht->node[hash].key == NULL) {
                        continue;
                }
                // Nel caso in cui la condizione precedente sia soddisfatta
                // procedo comparando le chiavi
                if (strcmp(key, ht->node[hash].key) == 0) {
                        LOG(("Trovato nodo alla posizione indicata\n"));
                        // Decremento il numero di elementi
                        ht->num_elements--;
                        // Prima di procedere con l'eliminazione faccio il
                        // probing del nodo alla posizione successiva. In
                        // questo modo cerco di tenere la HashTable il più
                        // libera possibile
                        prober = hash + 1;
                        if (prober == ht->size) {
                                prober = 0;
                        }
                        _test_nodes(ht, &ht->node[hash], &ht->node[prober]);

                        rwlunlock(&ht->lock);
                        return &ht->node[hash];
                }
        }

        rwlunlock(&ht->lock);
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
        for (size_t i = 0; i < ht->size; i++) {
                free(ht->node[i].key);
                free(ht->node[i].element);
        }

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

