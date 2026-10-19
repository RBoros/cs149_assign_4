/**
 * Description: This program counts how many times each individual name appears across one or more files.
 * Author names: Ebsan Iqbal, Raymond Okolo
 * Author emails: ebsan.iqbal@sjsu.edu, raymond.okolo@sjsu.edu
 * Last modified date: 10/10/2026
 * Creation date: 10/5/2026
 **/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>

#include "hashtable.h"

#define NAME_LEN 32         // max length of name
#define SHARED_NAME "/shared_memory_i"

//Message payload
typedef struct {
    char name[NAME_LEN];
    int count;
} NameCountData;


/**
 * This function outputs the names and number of occurrences in a readable format to a PID.out file.
 *
 * Returns: nothing
**/
void outputPIDs(char *pid, int id) {
    char filename[32];
    snprintf(filename, sizeof(filename), "%s.out", pid);
    FILE *fp = fopen(filename, "w");
    if (fp == NULL) {
        fprintf(stderr,"error: cannot open file %s\n", filename);
        exit(1);
    }

    size_t region_size = 0;
    void *GLOBAL= 0;
    NameCountData *region = NULL;
    if(id != -1){
        int memFd = shm_open(SHARED_NAME, O_RDWR, 0);
        if (memFd == -1) {
            perror("shm_open");
            exit(1);
        }
        region_size = ((MAXNAMES + 1) * sizeof(NameCountData));
        GLOBAL = mmap(NULL, (id + 1) * region_size, PROT_READ | PROT_WRITE, MAP_SHARED, memFd, 0);
        if (GLOBAL == MAP_FAILED) {
            perror("mmap");
            exit(1);
        }
        close(memFd);
        region = (NameCountData *)((char *)GLOBAL + id * region_size);
    }

    for(int i = 0; i < nameCount; i++) {
        struct nlist *np = lookup(nameList[i]);
        for(int j = 0; j < np->nCount; j++) {
            if(strcmp(np->names[j], nameList[i]) == 0) {
                if(id != -1){
                    strncpy(region[i].name, nameList[i], NAME_LEN - 1);
                    region[i].count = np->counts[j];
                }
                fprintf(fp,"%s: %d\n", nameList[i], np->counts[j]);
                break;
            }
        }
    }
    if(id != -1){
        munmap(GLOBAL, (id + 1) * region_size);
    }
    fclose(fp);

}

int main(int argc, char *argv[]) {
    FILE *fp = NULL;

    char pidStr[16];
    char *pid;
    char *inputName;

    int id = -1;        //if argc != 4

    /*
     * countnames run directly with no filename:
     * read from stdin and use this process's PID for output files.
     */
    if (argc == 1) {
        snprintf(pidStr, sizeof(pidStr), "%d", getpid());
        pid = pidStr;
        inputName = "stdin";
        fp = stdin;
    }

    /*
     * Optional direct-file mode:
     * ./countnames names.txt
     */
    else if (argc == 2) {
        snprintf(pidStr, sizeof(pidStr), "%d", getpid());
        pid = pidStr;
        inputName = argv[1];

        fp = fopen(argv[1], "r");
        if (fp == NULL) {
            fprintf(stderr, "error: cannot open file %s\n", argv[1]);
            exit(1);
        }
    }

    /*
     * Normal Assignment 2 mode:
     * shell calls:
     *./countnames PID filename
     * "1" is used by shell.c to indicate stdin.
     */
    else if (argc == 4){
        id = atoi(argv[3]);
        pid = argv[1];

        if (strcmp(argv[2], "1") == 0) {
            fp = stdin;
            inputName = "stdin";
        }
        else {
            inputName = argv[2];
            fp = fopen(argv[2], "r");
            if (fp == NULL) {
                fprintf(stderr, "error: cannot open file %s\n", argv[2]);
                exit(1);
            }
        }
    }
    else {
        fprintf(stderr, "error: invalid arguments\n");
        return 1;
    }

    char buffer[32];
    int lineNum = 1;

    char errFile[32];
    snprintf(errFile, sizeof(errFile), "%s.err", pid);
    FILE *ep = fopen(errFile, "w");
    if (ep == NULL) {
        fprintf(stderr, "error: cannot open created error file.\n");

        if (fp != stdin) {
            fclose(fp);
        }
        exit(1);
    }

    while (fgets(buffer, sizeof(buffer), fp) != NULL) {
        buffer[strcspn(buffer, "\r\n")] = '\0';

        if (strlen(buffer) == 0) {
            fprintf(
                ep,
                "Warning - file %s line %d is empty.\n",
                inputName,
                lineNum
            );
        }
        else {
            char *trueLine = strdup(buffer);
            insert(trueLine);
        }

        lineNum++;
    }


    outputPIDs(pid, id);

    if (fp != stdin) {
        fclose(fp);
    }
    fclose(ep);

    return 0;
}
