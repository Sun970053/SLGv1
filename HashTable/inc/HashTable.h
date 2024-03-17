/*
 * HashTable.h
 *
 *  Created on: Mar 5, 2024
 *      Author: e1406
 */

#ifndef INC_HASHTABLE_H_
#define INC_HASHTABLE_H_

#define HT_MAX_SIZE 50
#define ERROR_NO_VALUE -1

#include <stddef.h>

extern void* (*HT_malloc)(size_t size);
extern void (*HT_free)(void*);

typedef struct
{
    char* key;
    int value;
}HT_Item;

typedef struct linkedlist
{
    HT_Item* item;
    struct linkedlist* next;
}HT_LinkedList;

typedef struct 
{
    HT_LinkedList** itemNodes;
    int size;
    int count;
}Hash_Table;

HT_Item* HT_createItem(char* key, int value);
Hash_Table* HT_createTable(int size);
void HT_freeTable(Hash_Table* table);
void HT_print(Hash_Table* table);
void HT_insertItem(Hash_Table* table, char* key, int value);
void HT_deleteItem(Hash_Table* table, char* key);
int HT_searchKey(Hash_Table* table, char* key);

#endif /* INC_HASHTABLE_H_ */
