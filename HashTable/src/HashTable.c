/*
 * HashTable.c
 *
 *  Created on: Mar 5, 2024
 *      Author: e1406
 */

#include "HashTable.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>

/* return index number */
uint32_t ht_hashFunction(Hash_Table* table, char* key);
/* Deal with the duplicate index problem */
void ht_handleCollision(Hash_Table* table);

void ht_freeItem(HT_Item* item);
/* LinkedList: create a list head */
HT_LinkedList* linkedlist_allocate(void);
/* LinkedList: add at the tail */
void linkedlist_insert(HT_LinkedList* head, HT_Item* item);
/* LinkedList: delete node */
void linkedlist_delete(HT_LinkedList** head, char* key);
/* LinkedList: free the list */
void linkedlist_free(HT_LinkedList* head);

void* (*HT_malloc)(size_t size);
void (*HT_free)(void*);

HT_Item* HT_createItem(char* key, int value)
{
    HT_Item* item = (HT_Item*)HT_malloc(sizeof(HT_Item));
    item->key = (char*)HT_malloc(strlen(key) + 1);
    item->value = value;
    // put the content of key into item structure
    strcpy(item->key, key);
    return item;
}

Hash_Table* HT_createTable(int size)
{
    Hash_Table* table = (Hash_Table*)HT_malloc(sizeof(Hash_Table));
    table->itemNodes = (HT_LinkedList**)HT_malloc(size * sizeof(HT_LinkedList*));
    table->size = size;
    table->count = 0;
    // set all of lists' header node to null pointer.
    for(int i = 0; i < table->size; i++)
    {
        table->itemNodes[i] = NULL; 
    }
    return table;
}

void ht_freeItem(HT_Item* item)
{
    HT_free(item->key);
    HT_free(item);
}

void HT_freeTable(Hash_Table* table)
{
    for(int i = 0; i < table->size; i++)
    {
        if(table->itemNodes[i] != NULL)
        {
            HT_LinkedList* tempListNode = table->itemNodes[i];
            linkedlist_free(tempListNode);
        }
    }
    HT_free(table);
}

void HT_print(Hash_Table* table)
{
	printf("------------------------------\r\n");
    for(int i = 0; i < table->size; i++)
    {
        if(table->itemNodes[i] != NULL)
        {
            printf("Index: %d\t", i);
            HT_LinkedList* temp = table->itemNodes[i];
            while(temp)
            {
                printf("Key: %s, ", temp->item->key);
                printf("Value: %d", temp->item->value);
                printf(" -> ");
                temp = temp->next;
            }
        }
        else
        {
            printf("Index: %d\t", i);
        }
        printf("NULL");
        printf("\r\n");
    }
    printf("------------------------------\r\n");
}

void HT_insertItem(Hash_Table* table, char* key, int value)
{
    uint32_t index = ht_hashFunction(table, key);
    if(table->itemNodes[index] == NULL)
    {
        // if itemNodes list is empty, then we need to allocate a new linkedlist
        table->itemNodes[index] = linkedlist_allocate();
        // after allocating a new one, it needs to be define its content.
        table->itemNodes[index]->item = HT_createItem(key, value);
        table->itemNodes[index]->next = NULL;
        table->count++;
    }
    else
    {
        // it isn't same, and then insert a new node at the end.
        HT_Item* newItem = HT_createItem(key, value);
        // Separate chaining
        linkedlist_insert(table->itemNodes[index], newItem);
    }
}

void HT_deleteItem(Hash_Table* table, char* key)
{
    int index = ht_hashFunction(table, key);
    if(table->itemNodes[index] == NULL) 
        return;
    else
    {
    	HT_LinkedList** head;
    	head = &(table->itemNodes[index]);
        linkedlist_delete(head, key);
    }
}

int HT_searchKey(Hash_Table* table, char* key)
{
    uint32_t index = ht_hashFunction(table, key);
    if(table->itemNodes[index] == NULL)
        return -1;
    
    HT_LinkedList* currentNode = table->itemNodes[index];
    while(currentNode)
    {
        // compare its key, if the key is same.
        if(strcmp(currentNode->item->key, key) == 0){
            // then return the current item value
            return currentNode->item->value;
        }
        currentNode = currentNode->next;
    }
    // No matching key
    return -1;
}

HT_LinkedList* linkedlist_allocate()
{
    return (HT_LinkedList*)HT_malloc(sizeof(HT_LinkedList));
}

void linkedlist_insert(HT_LinkedList* head, HT_Item* item)
{
    HT_LinkedList* currentNode = head;
    while(currentNode->next)
    {
    	currentNode = currentNode->next;
    }
    HT_LinkedList* tempNode = linkedlist_allocate();
    tempNode->item = item;
    tempNode->next = NULL;
    currentNode->next = tempNode;
}

void linkedlist_delete(HT_LinkedList** head, char* key)
{
    if(head == NULL) return;
    
    HT_LinkedList** currentNode = head;
    // if head must be delete, its content needs to be replace by the next node.
    // Then delete the next node.
    if(strcmp((*currentNode)->item->key, key) == 0)
    {
        HT_LinkedList* temp;
        temp = (*currentNode)->next;
        ht_freeItem((*currentNode)->item);
        (*currentNode)->item = NULL;
        // List has only one node
        if(temp == NULL)
        {
        	HT_free(*currentNode);
        	(*currentNode) = NULL;
        	return;
        }
        else
        {
        	(*currentNode)->item = HT_createItem(temp->item->key, temp->item->value);
        	(*currentNode)->next = temp->next;
			ht_freeItem(temp->item);
			temp->item = NULL;
			HT_free(temp);
			temp = NULL;
			return;
        }
    }
    // if the other node must be delete
    HT_LinkedList* deleteNode = (*currentNode);
    HT_LinkedList* prevNode = deleteNode;
    deleteNode = deleteNode->next;
    while(deleteNode)
    {
        if(strcmp(deleteNode->item->key, key) == 0)
        {
            prevNode->next = deleteNode->next;
            ht_freeItem(deleteNode->item);
            deleteNode->item = NULL;
            HT_free(deleteNode);
            deleteNode = NULL;
            return;
        }
        prevNode = deleteNode;
        deleteNode = deleteNode->next;
    }
}

void linkedlist_free(HT_LinkedList* head)
{
    if(head == NULL) return;

    HT_LinkedList* currentNode = head;
    while(currentNode)
    {
        HT_LinkedList* temp = currentNode;
        currentNode = currentNode->next;
        ht_freeItem(temp->item);
        HT_free(temp);
    }
}

uint32_t ht_hashFunction(Hash_Table* table, char* key)
{
    uint32_t index = 0;
    
    for(int i = 0; key[i]; i++)
    {
        index += key[i];
    }
    return index % table->size;
}
