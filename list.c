#include <stdio.h>
#include <stdlib.h> 
#include <assert.h>
#include "list.h"

//Creates a node for a List
Node* createNode(void *ptr, int data) {
    Node *newNode = malloc(sizeof(Node));
    if (newNode != NULL) {
        newNode->data = data;
        newNode->nodeptr = ptr;
        newNode->next = NULL;
    }
    return newNode;
}

//Destructs a List
void destructor(Node *head){
	Node *current;
    while(head){
        current = head->next;
        free(head);
        head = current;
    }
}

//insert a node at the tail of the List
void insertNodeAtTail(Node **head, Node *newNode) {
    newNode->next = NULL;

    if (*head == NULL) {
        *head = newNode;
    } 
	else{
        Node *current = *head;
        while (current->next != NULL) {
            current = current->next;
        }
        current->next = newNode;
    }
}

//counts the nodes in the List
int countNodes(Node *head){
   	int count = 0;         
    Node *current = head; 
    while (current != NULL) {
        count++;
        current = current->next;
    }
    return count;
}

//deletes a Node
void deleteNode(Node **head, Node *node){
	assert(head != NULL);
	assert(*head != NULL);

	if (*head == node){
		*head = node->next;
	}
	else{
		Node *current = *head;
		while (current->next != node && current != NULL){
			current = current->next;
		}
		assert(current->next == node);
		current->next = node->next;
	}
	free(node);
}

//calculates and returns the sum of the data values stored in all nodes of a linked list
int sumNodesData(Node *head) {
    int sum = 0;
    Node *current = head;
    while(current != NULL) {
        sum += current->data;
        current = current->next;
    }
    return sum;
}

















