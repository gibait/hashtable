/* 
   Questo programma utilizza una HashTable per memorizzare le occorrenze
   delle parole presenti all'interno del file. Il funzionamento è lo stesso
   di quello del programma demo.c, con la differenza che vengono utilizzati
   N_THREAD per leggere ogununo da un rispettivo file, e popolare la HashTable.
   Per utilizzare il programma devono essere modificati i nomi dei file e
   consguentemente il numero di thread.
   Viene fornito come parametro di invocazione del programma:
   - TABLE_SIZE: dimensione iniziale della HashTable
*/

#include "hash.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#define N_THREADS 4

HashTable* ht;

void* test_delete(void* _args) {
        char* file;
        char* buffer;
        FILE* fp;
        size_t buffer_size = 100;
        int read;

        file = (char*) _args;

        fp = fopen(file, "r");
        if (fp == NULL) {
                perror("Errore apertura file");
                exit(1);
        }

        while ((read = getline(&buffer, &buffer_size, fp)) != -1) {
                if (read > 0) {
                        strtok(buffer, "\n");
                        hash_remove(ht, buffer);
                }
        }

        pthread_exit((void*) _args);
}


void* test_insert(void* _args) {
        size_t buffer_size = 100;
        char* file;
        char* buffer;
        char str[buffer_size];
        void* element;
        FILE* fp;
        int read;
        int fails;
        int counter;

        file = (char*) _args;

        fp = fopen(file, "r");
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

        printf("Thread-%lu\t fails: %d\n", pthread_self(), fails);

        pthread_exit((void*) _args);
}

int main(int argc, char** argv) {
        pthread_t* thread;
        int* taskids;
        int i;
        size_t table_size;
        char* retr;

        char file_name[][100] = {
                "test/xaa",
                "test/xab",
                "test/xac",
                "test/xad"
        };

        if (argc != 2) {
                perror("Numero di parametri errato");
                exit(1);
        }
        
        table_size = atoi(argv[1]);
        if (table_size < 1) {
                perror("Dimensione della HashTable troppo piccola");
                exit(2);
        }
        
        ht = create_hash_table(table_size);
        if (ht == NULL) {
                exit(3);
        }

        thread = malloc(N_THREADS * sizeof(pthread_t));
        if (thread == NULL) {
                perror("Errore allocazione pthread");
                exit(4);
        }
        
        taskids = malloc(N_THREADS * sizeof(int));
        if (taskids == NULL) {
                perror("Errore allocazione taskids");
                exit(5);
        }

        for (i = 0; i < N_THREADS; i++) {
                taskids[i] = i;
                
                pthread_create(&thread[i], 
                                NULL, 
                                test_insert,
                                (void*) file_name[i]);
        }

        for (i = 0; i < N_THREADS; i++) {
                pthread_join(thread[i], (void**) &retr);
        }

        printf("size: %lu\n", ht->size);

        printf("ht->num_elements: %ld\n", ht->num_elements);
        printf("hash_num_elements: %ld\n", hash_num_elements(ht));

        for (i = 0; i < N_THREADS; i++) {
                taskids[i] = i;

                pthread_create(&thread[i],
                               NULL,
                               test_delete,
                               (void*) file_name[i]);
        }

        for (i = 0; i < N_THREADS; i++) {
                pthread_join(thread[i], (void**) &retr);
        }

        printf("size: %lu\n", ht->size);

        printf("ht->num_elements: %ld\n", ht->num_elements);
        printf("hash_num_elements: %ld\n", hash_num_elements(ht));

        return 0;
}
