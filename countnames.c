/**
 * Description: This program counts how many times each individual name appears across one or more files.
 * Author names: Ebsan Iqbal, Raymond Okolo
 * Author emails: ebsan.iqbal@sjsu.edu, raymond.okolo@sjsu.edu
 * Last modified date: 10/21/2026
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


//Message payload
typedef struct {
    char name[NAME_LEN];
    int count;
} NameCountData;


/**
 * This function outputs the names and number of occurrences in a readable format to a PID.out file.
 * In shell mode (id != -1) it first attaches to the shared memory object by name and also
 * writes the counts into this child's region. The attach happens BEFORE PID.out is created,
 * so a failed attach never leaves a half-written .out file behind.
 * Input parameters: pid (used for the .out name), id (region index or -1), shared_name (shared object name)
 *
 * Returns: 0 on success, -1 on failure (everything this function opened is already released)
**/
static int outputPIDs(char *pid, int id, char *shared_name) {
    size_t region_size = ((MAXNAMES + 1) * sizeof(NameCountData));
    void *GLOBAL = NULL;
    NameCountData *region = NULL;

    if(id != -1){
        int memFd = shm_open(shared_name, O_RDWR, 0);
        if (memFd == -1) {
            perror("shm_open");
            return -1;
        }
        GLOBAL = mmap(NULL, (id + 1) * region_size, PROT_READ | PROT_WRITE, MAP_SHARED, memFd, 0);
        close(memFd);
        if (GLOBAL == MAP_FAILED) {
            perror("mmap");
            return -1;
        }
        region = (NameCountData *)((char *)GLOBAL + id * region_size);
    }

    char filename[32];
    snprintf(filename, sizeof(filename), "%s.out", pid);
    FILE *fp = fopen(filename, "w");
    if (fp == NULL) {
        fprintf(stderr,"error: cannot open file %s\n", filename);
        if (id != -1) {
            munmap(GLOBAL, (id + 1) * region_size);
        }
        return -1;
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
    return 0;
}

int main(int argc, char *argv[]) {
    FILE *fp = NULL;

    char pidStr[16];
    char *pid;
    char *inputName;

    int id = -1;        //-1 means no shared memory
    char *shared_name = NULL;

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
     * Normal shell mode:
     * shell calls:
     *./countnames PID filename shared_name regionIndex
     * "1" is used by shell.c to indicate stdin.
     */
    else if (argc == 5){
        pid = argv[1];
        shared_name = argv[3];
        id = atoi(argv[4]);

        if (id < 0) {
            fprintf(stderr, "error: invalid shared memory region\n");
            return 1;
        }

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
            insert(buffer);
        }
        lineNum++;
    }


    if (outputPIDs(pid, id, shared_name) == -1) {
        if (fp != stdin) {
            fclose(fp);
        }
        fclose(ep);
        clearTable();
        return 1;
    }
    if (fp != stdin) {
        fclose(fp);
    }
    fclose(ep);

    return 0;
}