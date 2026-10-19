/**
 * Description: This implements a hashtable data structure (header-only, so
 * shell.c and countnames.c compile with no extra source file).
 * Author names: Ebsan Iqbal, Raymond Okolo
 * Author emails: ebsan.iqbal@sjsu.edu, raymond.okolo@sjsu.edu
 * Last modified date: 10/6/2026
 * Creation date: 10/5/2026
 **/

#ifndef HASHTABLE_H
#define HASHTABLE_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define HASHSIZE 101    // number of buckets in the hash table
#define MAXNAMES 1000   // max number of unique names that can be stored


/**
 * Table entry:
 * Each struct has a table of names if same hash value is used.
 **/
struct nlist {
    int nCount;     // number of names in struct.
    int nCapacity;  // used for when to increase allocated memory.
    int *counts;    // array of counts for each name in array of names.
    char **names;   // array of names.
};

/*
 * Everything below is static so that each program that includes this header
 * gets its own private copy of the table. "static inline" keeps -Wall -Werror
 * from complaining about functions a particular program does not use.
 */
static struct nlist *hashtab[HASHSIZE];     // pointer table
static char *nameList[MAXNAMES];            // list of unique names (first-seen order).
static int nameCount = 0;                   // number of unique names.

/**
 * This is the hash function: form hash value for string temp
 * Assumption: temp is a char*
 * Input parameters: temp
 * Returns: a hash value
**/
static inline unsigned hash(char *temp) {
    unsigned hashval;
    for (hashval = 0; *temp != '\0'; temp++) {
        hashval = *temp + 31 * hashval;
    }
    return hashval % HASHSIZE;
}

/**
 * This function performs a lookup of a name in hashtab
 * Assumption: s is a char*
 * Input parameters: s
 * Returns: a pointer to a struct or NULL if no struct exists.
**/
static inline struct nlist *lookup(char *s) {
    struct nlist *np = hashtab[hash(s)];
    if (np != NULL) {
        return np;
    }
    return NULL; /* not found */
}

/**
 * This function creates and inserts a struct in hashtab or updates an existing
 * struct, adding n occurrences of name.
 * Assumption: name is a char*, n >= 1
 * Input parameters: name, n
 * Returns: nothing
**/
static inline void insertCount(char *name, int n) {
    struct nlist *np = lookup(name);
    const int hVal = hash(name);

    if (np == NULL) { //name is not in a struct yet.
        if (nameCount >= MAXNAMES) {
            fprintf(stderr, "error: too many unique names\n");
            exit(1);
        }
        np = malloc(sizeof(*np));
        if (np == NULL) {
            fprintf(stderr, "Allocation failed\n");
            exit(1);
        }
        //initializing variables and allocating memory.
        np->nCount = 1;
        np->nCapacity = 4;
        np->names = malloc(np->nCapacity * sizeof(char*));
        np->names[0] = strdup(name);
        np->counts = malloc(np->nCapacity * sizeof(int));
        np->counts[0] = n;
        hashtab[hVal] = np;

        nameList[nameCount++] = strdup(name); //remember the unique name.
    } else {
        int found = 0;
        //this loop iterates through a struct's names array
        for (int i = 0; i < np->nCount; i++) {
            if (strcmp(np->names[i], name) == 0) { // name is found in struct name array
                found = 1;
                np->counts[i] += n;     //add to count
                break;
            }
        }
        //name is not found but uses same hash value,
        //so insert name in existing struct names array.
        //also allocate more memory if needed and increment nCount
        if (found == 0) {
            if (nameCount >= MAXNAMES) {
                fprintf(stderr, "error: too many unique names\n");
                exit(1);
            }
            if (np->nCount == np->nCapacity) {
                np->nCapacity *= 2;
                np->names = realloc(np->names, np->nCapacity * sizeof(char*));
                np->counts = realloc(np->counts, np->nCapacity * sizeof(int));
            }
            np->names[np->nCount] = strdup(name);
            np->counts[np->nCount] = n;
            np->nCount++;

            nameList[nameCount++] = strdup(name);
        }
    }
}

/**
 * Adds one occurrence of name. Same behavior as the original insert().
 * Input parameters: name
 * Returns: nothing
**/
static inline void insert(char *name) {
    insertCount(name, 1);
}

/**
 * This function frees every entry in the hash table and resets it to empty,
 * so the next command line starts counting from zero.
 * Returns: nothing
**/
static inline void clearTable(void) {
    for (int i = 0; i < HASHSIZE; i++) {
        struct nlist *np = hashtab[i];
        if (np != NULL) {
            for (int j = 0; j < np->nCount; j++) {
                free(np->names[j]);      // each strdup'd name in the bucket
            }
            free(np->names);             // the array of name pointers
            free(np->counts);            // the array of counts
            free(np);                    // the bucket struct itself
            hashtab[i] = NULL;
        }
    }

    for (int i = 0; i < nameCount; i++) {
        free(nameList[i]);               // second strdup'd copy of each name
        nameList[i] = NULL;
    }
    nameCount = 0;
}

#endif