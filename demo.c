/* 
   Questo programma utilizza una HashTable per memorizzare le occorrenze
   delle parole presenti all'interno del file. Il file deve essere già 
   formattato prevedendo una sola parola per linea. Prima effettua una get
   per verificare che la parola non sia presente, se così non fosse la 
   aggiunge. Se invece è già presente, salva il valore dell'elemento, in
   questo caso facente funzione di contatore, ed effettua una insert con
   il valore del contatore incrementato di uno.
   Per utilizzare il programma devono essere passati due parametri:
   - TABLE_SIZE: dimensione iniziale della HashTable
   - FILE_NAME: nome del file di cui effettuare la conta delle parole 
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "hash.h"

void usage(void) {
        printf("usage: demo [TABLE_SIZE] [FILE_NAME]\n");
}


int main(int argc, char* argv[]) {
        HashTable *ht;
        size_t buffer_size = 100;
        char str[buffer_size];
        char* buffer;
        void* element;
        FILE* fp;
        int read;
        int fails;
        int counter;

        if (argc != 3) {
                usage();
                perror("Numero di parametri errato");
                exit(1);
        }

        ht = create_hash_table(atoi(argv[1]));
        if (ht == NULL) {
                usage();
                exit(2);
        }
                
        fp = fopen(argv[2], "r");
        if (fp == NULL) {
                usage();
                perror("Errore apertura file");
                exit(3);
        }

        buffer = malloc(buffer_size * sizeof(char));
        if (buffer == NULL) {
                perror("Errore allocazione buffer");
                exit(4);
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

        printf("size after INSERTION: %lu\n", ht->size);

        printf("insertion fails: %d\n", fails);
        printf("ht->num_elements: %ld\n", ht->num_elements);
        printf("hash_num_elements: %ld\n", hash_num_elements(ht));


        rewind(fp);
        while ((read = getline(&buffer, &buffer_size, fp)) != -1) {
                if (read > 0) {
                        strtok(buffer, "\n");
                        if (hash_get(ht, buffer) != NULL) {
                                hash_remove(ht, buffer);
                        }
                }
        }

        printf("\n\n");
        printf("size after DELETION: %lu\n", ht->size);

        printf("ht->num_elements: %ld\n", ht->num_elements);
        printf("hash_num_elements: %ld\n", hash_num_elements(ht));

        destroy_hash_table(ht);
        free(buffer);

        return 0;
}
