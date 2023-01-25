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
    char* p;

    for (p = key; *p; p++) {
        hash *= FNV_PRIME;
        hash ^= (size_t)(unsigned char)(*p);
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
        size_t hash = 5381;
        size_t counter;

        for (counter = 0; key[counter] != '\0'; counter++) {
                hash = ((hash << 5) + hash) + key[counter];
        }

        return hash % ht->size;
}

#define TABLE_MAX_LOAD 70
#define TABLE_MIN_LOAD 30

HashTable* create_hash_table(size_t size) {
        HashTable* ht;
        size_t i;

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

        // Inizializzo i nodi
        for (i = 0; i < size; i++) {
                ht->node[i].key = NULL;
                ht->node[i].element = EMPTY;
        }

        // Inizializzo il lock
        pthread_rwlock_init(&ht->lock, NULL);

        // Inizializzo gli altri membri della struct
        ht->size = size;
        ht->num_elements = 0;
        ht->high_density = TABLE_MAX_LOAD;
        ht->low_density = TABLE_MIN_LOAD;

        return ht;
}

/*
 * Questa funzione è il cuore dell'intera HashTable: utilizza il meccanismo
 * di LINEAR PROBING per trovare un nodo non NULL, spostandosi a destra di
 * una posizione ogni volta che ne incontra uno.
 */
static
Node* find_node(Node* node, size_t hash, char* key, size_t ht_size) {
        Node* found = NULL;
        size_t counter = 0;

        for (; counter < ht_size; counter++, hash = (hash + 1) % ht_size) {
                // Controllo che il nodo non sia NULL
                if (node[hash].key == NULL) {
                        if (node[hash].element == EMPTY) {
                                // Se l'elemento è EMPTY allora è libero
                                // per l'assegnazione
                                return found != NULL ? found : &node[hash];
                        }
                        // Altrimenti il nodo è una TOMBSTONE, cioè un nodo
                        // rimosso in precedenza
                        if (found == NULL) {
                                // Lo salviamo per il caso in cui questa
                                // funzione venga usata da hash_insert,
                                // cioè per la ricerca di un nodo NULL
                                found = &node[hash];
                        }
                        continue;
                }

                // Qualora non sia vuoto confronto la chiave presente con
                // quella fornita
                if (strcmp(key, node[hash].key) == 0) {
                        LOG(("Trovato alla pos. %lu\n", hash));
                        return &node[hash];
                }
        }

        // Nel caso in cui il ciclo sopra termini significa che non è
        // presente l'elemento all'interno della HashTable
        return NULL;
}

static
int insert(Node* node, size_t hash, char* key, void* element, size_t ht_size) {
        size_t counter = 0;

        for (; counter < ht_size; counter++, hash = (hash + 1) % ht_size) {
                if (node[hash].key == NULL) {
                        // Se il nodo è NULL o la chiave è NULL lo tratto
                        // come EMPTY e inscrivo i valori
                        LOG(("Inserting %s (at %ld) with '%s' key\n",
                                (char *) element, hash, key));
                        // Effettuo una copia del valore contenuto
                        // all'interno del puntatore
                        node[hash].key = strdup(key);
                        node[hash].element = strdup(element);
                        return 1;
                }
                if (strcmp(key, node[hash].key) == 0) {
                        // Se il nodo è popolato si tratta di un tentativo
                        // di sovrascrittura
                        LOG(("Updating '%s' (at %ld) with %s element\n",
                                key, hash, (char *) element));
                        free(node[hash].element);
                        node[hash].element = strdup(element);
                        return 0;
                }
        }
        return -1;
}

/*
 * Questa funzione viene chiamata quando la dimensione della HashTable
 * supera il limite fissato. Raddoppia le dimensioni della stessa
 * ed effettua una copia dei nodi inserendoli con hash aggiornato alle
 * nuove dimensioni.
 */
static
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

        for (i = 0; i < doubled; i++) {
                copy[i].key = NULL;
                copy[i].element = EMPTY;
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
static
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

        for (i = 0; i < half; i++) {
                copy[i].key = NULL;
                copy[i].element = EMPTY;
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
        // procedo a espandere la HashTable raddoppiandone le dimensioni
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
        
        // Utilizzo la funzione d'inserimento e controllo il valore restituito
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
        Node* found;
        size_t hash;

        // Acquisisco il lock
        rdlock(&ht->lock);

        // Controllo che il numero di elementi sia > 1 
        // così da evitare il blocco di codice seguente
        if (ht->num_elements == 0) {
                rwlunlock(&ht->lock);
                return NULL;
        }

        // Viene computato il digest della chiave fornita
        hash = hash_value(ht, key);
        LOG(("Sto cercando l'elemento di chiave %s\n", key)); 

        // Cerco il nodo indicato
        found = find_node(ht->node, hash, key, ht->size);

        // Rilascio il lock
        rwlunlock(&ht->lock);

        // Il nodo risulta vuoto nel caso in cui la chiave sia NULL oppure
        // il nodo stesso. In caso contrario viene il nodo è popolato e
        // restituisco l'elemento
        if (found == NULL) {
                return NULL;
        }
        if (found->key == NULL) {
                return NULL;
        } else {
                return found->element;
        }
}

void* hash_remove(HashTable* ht, char* key) {
        Node* found;
        size_t hash;

        // Acquisisco il lock
        wrlock(&ht->lock);

        // Se il numero di elementi è 0 non vi sono nodi da rimuovere
        if (ht->num_elements < 1) {
                rwlunlock(&ht->lock);
                return NULL;
        }

        // Verifico che il numero di nodi all'interno della HashTable non
        // sia al di sotto del valore di densità superiore stabilito.
        // In caso contrario procedo a espandere la HashTable dimezzandone
        // le dimensioni
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

        // Cerco il nodo indicato
        found = find_node(ht->node, hash, key, ht->size);

        // Il nodo risulta vuoto nel caso in cui la chiave oppure
        // il nodo stesso sia NULL. In caso contrario il nodo è popolato e
        // procedo a rimuoverlo e decrementare il numero di elementi
        if (found == NULL) {
                rwlunlock(&ht->lock);
                return NULL;
        }
        if (found->key != NULL) {
                // Decremento il numero di elementi
                ht->num_elements--;
                free(found->key);
                free(found->element);
                // Pongo la chiave NULL e l'elemento a TOMBSTONE
                found->key = NULL;
                found->element = TOMBSTONE;

                // Rilascio il lock
                rwlunlock(&ht->lock);
                return found;
        }

        // Rilascio il lock
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
                if (ht->node[i].key != NULL) {
                        free(ht->node[i].key);
                        free(ht->node[i].element);
                }
        }

        free(ht->node);
        free(ht);
}

/*
 * Stampa a schermo il contenuto della HashTable formattato
 */
void pretty_print(HashTable* ht) {
        size_t i;
        
        printf("\n\n");
        printf("    index\t\t key\t\t element\t \n\n");
        for (i = 0; i < ht->size; i++) {
                if (ht->node[i].key != NULL) {
                        printf("    %-10lu\t\t %-12s\t\t %8s\t \n\n",
                               i,
                               ht->node[i].key,
                               (char*) ht->node[i].element);
                }
        }
        printf("\n\n");
}
