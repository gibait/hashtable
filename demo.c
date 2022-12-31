/* 
   Questo programma utilizza una HashTable per memorizzare le occorrenze
   delle parole presenti all'interno del file. Il file deve essere già 
   formattato prevedendo una sola parola per linea. Prima effettua una get
   per verificare che la parola non sia presente, se così non fosse la 
   aggiunge. Se invece è già presente, salva il valore dell'elemento, in
   questo caso facente funzione di contatore, ed effettua una insert con
   il valore del contatore incrementato di 1. 
   Per utilizzare il programma devono essere modificati due parametri:
   - TABLE_SIZE: dimensione iniziale della HashTable
   - FILE_NAME: nome del file di cui effettuare la conta delle parole 
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "hash.h"

#define TABLE_SIZE 1000
#define FILE_NAME "words.txt"

int main(void) {
        HashTable *ht;
        char* buffer; 
        void* element;
        FILE* fp;
        size_t buffer_size = 100;
        char str[buffer_size];
        int read;
        int fails;
        int counter;

        ht = create_hash_table(TABLE_SIZE);
        if (ht == NULL) {
                exit(3);
        }
                
        fp = fopen(FILE_NAME, "r");
        if (fp == NULL) {
                perror("Errore apertura file");
                exit(1);
        }

        fails = 0;
        while ((read = getline(&buffer, &buffer_size, fp)) != -1) {
                counter = 0;
                if (read > 0) {
                        strtok(buffer, "\n");
                        element = hash_get(ht, buffer);
                        if (element != NULL) {
                                counter = strtol(element, NULL, 10);
                        }
                        sprintf(str, "%d", counter + 1);
                        if (hash_insert(ht, buffer, str) == -1) {
                                fails++;
                        }
                }
        }

        rewind(fp);
        while ((read = getline(&buffer, &buffer_size, fp)) != -1) {
                if (read > 0) {
                        strtok(buffer, "\n");
                        hash_remove(ht, buffer);
                }
        }


        printf("size: %lu\n", ht->size);

        printf("insertion fails: %d\n", fails);
        printf("ht->num_elements: %ld\n", ht->num_elements);
        printf("hash_num_elements: %ld\n", hash_num_elements(ht));

        destroy_hash_table(ht);

        return 0;
}
