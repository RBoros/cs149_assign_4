/**
 * Description: This program creates multiple simultaneous processes for countnames.c and combines
 * the results to compute a total names count and print to stdout.
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
#include <sys/wait.h>

#include "hashtable.h"

#define MAX_LINE 1024   // max characters read per prompt line
#define MAX_TOKENS 64   // max tokens (command + filenames) per line
#define NAME_LEN 32     // max length of name



//Message payload
typedef struct {
    char name[NAME_LEN];
    int count;
} NameCountData;

/**
  * This function creates child processes
  * each process executes the countnames program
  * Returns: nothing
**/
void makeChildren(char* cmd, char* filename, char *shared_name, int child_region){
    pid_t pid = fork();

    if(pid < 0){
        fprintf(stderr,"Fork failed\n");
        return;
    }
    if(pid == 0){
        char childPidStr[16];
        char childRegionStr[16];

        snprintf(childPidStr, sizeof(childPidStr), "%d", getpid());
        snprintf(childRegionStr, sizeof(childRegionStr), "%d", child_region);

        execl(cmd, cmd, childPidStr, filename, shared_name, childRegionStr, NULL);

        // execl only returns if it failed
        fprintf(stderr,"error: cannot exec %s\n", cmd);
        _exit(1);
    }
}

/**
 * Copies the merged hashtable into the summation region
 */
void aggregate_regions(NameCountData *sum_region){
    for(int i = 0; i < nameCount; i++) {
        struct nlist *np = lookup(nameList[i]);
        for(int j = 0; j < np->nCount; j++) {
            if(strcmp(np->names[j], nameList[i]) == 0) {
                strncpy(sum_region[i].name, nameList[i], NAME_LEN - 1);
                sum_region[i].count = np->counts[j];
                break;
            }
        }
    }
}

/**
  * This function prints the names and number of occurrences in a readable format.
  * Returns: nothing
**/
void printSummation(NameCountData *sum_region){
    for (int i = 0; i < MAXNAMES && sum_region[i].name[0] != '\0'; i++) {
        printf("%s: %d\n", sum_region[i].name, sum_region[i].count);
    }
}

int main() {
    char line[MAX_LINE];

    int cmd_i = 0;            //for unique shared memory name
    while (1){
        printf("%% ");
        fflush(stdout);

        // EOF (Ctrl+D) ends the shell
        if (fgets(line, sizeof(line), stdin) == NULL) {
            printf("\n");
            break;
        }

        // split the line into whitespace-separated tokens
        char *tokens[MAX_TOKENS];
        int count = 0;
        char *token = strtok(line, " \t\r\n");
        while (token != NULL && count < MAX_TOKENS) {
            tokens[count++] = token;
            token = strtok(NULL, " \t\r\n");
        }

        if (count == 0) {
            continue;   // blank line: just show the prompt again
        }
        if (strcmp(tokens[0], "exit") == 0) {
            break;
        }

        int children = (count == 1) ? 1 : count - 1;

        char shared_name[64];
        snprintf(shared_name, sizeof(shared_name), "/shared_memory_%d", cmd_i++);
        shm_unlink(shared_name);

        int memFd = shm_open(shared_name, O_CREAT | O_EXCL | O_RDWR, S_IRUSR | S_IWUSR);
        if (memFd == -1) {
            perror("shm_open");
            continue;
        }

        // (bytes per entry * number of regions) + (summation region) * (structs per region plus null struct)
        size_t SIZE = sizeof(NameCountData) * (MAXNAMES + 1) * (children + 1);

        if (ftruncate(memFd, SIZE) == -1) {
            perror("ftruncate");
            close(memFd);
            shm_unlink(shared_name);
            continue;
        }

        void *GLOBAL = mmap(NULL, SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, memFd, 0);
        if (GLOBAL == MAP_FAILED) {
            perror("mmap");
            close(memFd);
            shm_unlink(shared_name);
            continue;
        }

        if (count == 1) {
            // no files given: one child reads stdin ("1" tells countnames and 0 means first region)
            makeChildren(tokens[0], "1", shared_name, 0);
        }else {
            // one child per input file, all running in parallel
            for (int i = 1; i < count; i++) {
                makeChildren(tokens[0], tokens[i], shared_name, i - 1);
            }
        }

        // Parent: reap every child so none are left as zombies, and report
        // whether each exited normally or was killed by a signal.
        int status;
        pid_t pid;
        while ((pid = wait(&status)) > 0) {
            if (WIFEXITED(status)) {
                fprintf(stderr,
                        "Child %d terminated normally with exit code: %d\n",
                        pid,
                        WEXITSTATUS(status));
            } else if (WIFSIGNALED(status)) {
                fprintf(stderr,
                        "Child %d terminated abnormally with signal number: %d\n",
                        pid,
                        WTERMSIG(status));
            }
        }

        size_t region_size = ((MAXNAMES + 1) * sizeof(NameCountData));

        int flag = 1;
        // inserts data from all child processes into hashtable
        for (int i = 0; i < children && flag; i++) {
            NameCountData *child_region = (NameCountData *)((char *)GLOBAL + i * region_size);
            for(int j = 0; j < MAXNAMES && child_region[j].name[0] != '\0'; j++){
                if (insertCount(child_region[j].name, child_region[j].count) == -1) {
                    flag = 0;       //the hashtable is full
                    break;
                }
            }
        }

        //if every name is in the table
        if (flag) {
            NameCountData *sum_region = (NameCountData *)((char *)GLOBAL + children * region_size);
            aggregate_regions(sum_region);
            printSummation(sum_region);
        }

        //Clearing
        clearTable();               //clears hashtable for next command
        munmap(GLOBAL, SIZE);
        close(memFd);
        shm_unlink(shared_name);
    }

    return 0;
}