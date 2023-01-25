/* 
   Questo programma utilizza una HashTable per memorizzare le occorrenze
   delle parole presenti all'interno del file. Il funzionamento è lo stesso
   di quello del programma demo.c, con la differenza che vengono utilizzati
   N_THREAD per leggere dallo stesso file, e popolare la HashTable.
   Il file viene suddiviso in N_THREAD porzioni e ogni thread legge il file
   nel proprio intervallo. Devono essere forniti i seguenti parametri 
   in fase d'invocazione:
   - TABLE_SIZE: dimensione iniziale della HashTable
   - FILE: nome del file di cui effettuare la conta delle parole
   - N_THREADS: numero di thread
*/

#include "hash.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

struct ft
{
    char* file_name;
    int start_index;
    int end_index;
};


void usage(void) {
        printf("usage: demo [TABLE_SIZE] [FILE] [N_THREAD]\n");
}


HashTable* ht;


void* test_delete(void* _args) {
        char* buffer;
        FILE* fp;
        size_t buffer_size = 100;
        int read;

        struct ft* fi = (struct ft*) _args;

        fp = fopen(fi->file_name, "r");
        if (fp == NULL) {
                perror("Errore apertura file");
                exit(1);
        }
        fseek (fp , fi->start_index, SEEK_SET);

        buffer = malloc(buffer_size * sizeof(char));

        while ((read = getline(&buffer, &buffer_size, fp)) != -1) {
                if (ftell(fp) >= fi->end_index) {
                        break;
                }
                if (read > 0) {
                        strtok(buffer, "\n");
                        if (hash_get(ht, buffer) != NULL) {
                                hash_remove(ht, buffer);
                        }
                }
        }

        fclose(fp);
        free(buffer);
        pthread_exit((void*) _args);
}


void* test_insert(void* _args) {
        size_t buffer_size = 100;
        char* buffer;
        char str[buffer_size];
        void* element;
        FILE* fp;
        int read;
        int fails;
        int counter;

        struct ft* fi = (struct ft*) _args;

        fp = fopen(fi->file_name, "r");
        if (fp == NULL) {
                perror("Errore apertura file");
                exit(1);
        }
        fseek (fp , fi->start_index, SEEK_SET);

        buffer = malloc(buffer_size * sizeof(char));

        fails = 0;
        while ((read = getline(&buffer, &buffer_size, fp)) != -1) {
                if (ftell(fp) >= fi->end_index) {
                        break;
                }
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

        fclose(fp);
        free(buffer);
        printf("Thread-%lu\t fails: %d\n", pthread_self(), fails);
        pthread_exit((void*) _args);
}


int main(int argc, char** argv) {
        size_t table_size;
        int n_threads;
        FILE* fp;
        long size;
        struct ft *ft;
        int i;
        int idx_start, idx_end, idx_n;
        pthread_t* thread;
        int* taskids;
        char* retr;


        if (argc != 4) {
                usage();
                perror("Numero di parametri errato");
                exit(1);
        }

        table_size = strtol(argv[1], NULL, 10);
        if (table_size < 1) {
                usage();
                perror("Dimensione della HashTable troppo piccola");
                exit(2);
        }

        n_threads = (int) strtol(argv[3], NULL, 10);
        if (n_threads < 1) {
                usage();
                perror("Numero di thread troppo piccolo");
                exit(3);
        }

        fp = fopen(argv[2], "r");
        if (fp == NULL) {
                usage();
                perror("Errore apertura file");
                exit(4);
        }

        fseek(fp, 0L, SEEK_END);
        size = ftell(fp);

        fclose(fp);

        idx_n = (int) size / n_threads;

        ft = malloc((size / n_threads) * sizeof(struct ft));
        if (ft == NULL) {
                perror("Errore allocazione ft");
                exit(5);
        }

        idx_start = 0;
        idx_end = idx_start + idx_n;
        for (i = 0; i < n_threads; i++) {
                ft[i].file_name = argv[2];
                ft[i].start_index = idx_start;
                ft[i].end_index = idx_end;
                idx_start = idx_end + 1;
                idx_end = idx_end + idx_n;
                if (i == n_threads - 1) {
                        idx_end = size;
                }
        }
        
        ht = create_hash_table(table_size);
        if (ht == NULL) {
                exit(3);
        }

        thread = malloc(n_threads * sizeof(pthread_t));
        if (thread == NULL) {
                perror("Errore allocazione pthread");
                exit(4);
        }
        
        taskids = malloc(n_threads * sizeof(int));
        if (taskids == NULL) {
                perror("Errore allocazione taskids");
                exit(5);
        }

        for (i = 0; i < n_threads; i++) {
                taskids[i] = i;
                
                pthread_create(&thread[i], 
                                NULL, 
                                test_insert,
                                (void*) &ft[i]);
        }

        for (i = 0; i < n_threads; i++) {
                pthread_join(thread[i], (void**) &retr);
        }

        printf("\n\nsize after INSERTION: %lu\n", ht->size);
        printf("ht->num_elements: %ld\n", ht->num_elements);
        printf("hash_num_elements: %ld\n", hash_num_elements(ht));

        for (i = 0; i < n_threads; i++) {
                taskids[i] = i;

                pthread_create(&thread[i],
                               NULL,
                               test_delete,
                               (void*) &ft[i]);
        }

        for (i = 0; i < n_threads; i++) {
                pthread_join(thread[i], (void**) &retr);
        }

        printf("\n\nsize after DELETION: %lu\n", ht->size);
        printf("ht->num_elements: %ld\n", ht->num_elements);
        printf("hash_num_elements: %ld\n", hash_num_elements(ht));

        destroy_hash_table(ht);
        free(taskids);
        free(ft);
        free(thread);

        return 0;
}
