#include <stdio.h>
#include <stdlib.h> 
#include <assert.h>
#include "list.h"

//Creates a node for a List
Node* createNode(void *ptr, int data) {
    Node *node = malloc(sizeof(Node));
    if (node != NULL) {
        node->data = data;
        node->nodeptr = ptr;
        node->next = NULL;
    }
    return node;
}

//Destructs a List
void destructor (Node *head){
	Node *temp;
    while(head){
        temp = head->next;
        free(head);
        head = temp;
    }
}

//inserts a node at the head of the List
void insertNodeAtHead (Node **head, Node *node){
	if(head){
		node->next = *head;
		*head = node;
	}
}


//insert a node at the tail of the List
void insertNodeAtTail(Node **head, Node *node) {
    node->next = NULL;

    if (*head == NULL) {
        *head = node;
    } else {
        Node *current = *head;
        while (current->next != NULL) {
            current = current->next;
        }
        current->next = node;
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


//finds the node in the List with the data we are looking for, return NULL if not found
Node* findNode(Node *head, int data){
	Node *current = head;
	while(current != NULL){
		if (current->data == data){
			return current;
		}
		else{
			current = current->next;
		}
	}
	return NULL;
}

//deletes a Node
void deleteNode(Node **head, Node *node){
	assert(head != NULL);
	assert(*head != NULL);

	if (*head == node) {
		*head = node->next;
	}
	else {
		Node *current = *head;
		while (current->next != node) {
			current = current->next;
			assert(current != NULL);
		}
		assert(current->next == node);
		current->next = node->next;
	}

	free(node);
}

//Sort the list in ascending order
void sortList (Node **head){
	Node *p, *target;
	int changed = 1;

	if ((target = (Node *)malloc(sizeof(Node))) == NULL){
		exit(1);
	}

	target->next = *head;
	if (*head != NULL && target->next != NULL){
		while(changed == 1){
			changed = 0;
			p = target->next;
			while (p->next != NULL){
				if (p->data > p->next->data){
					int temp = p->data;
					p->data = p->next->data;
					p->next->data = temp;
					changed = 1;
				}
				if (p->next != NULL){
					p = p->next;
				}
			}
		}
		p = target->next;
		free(target);
		*head = p;
	}
}

//calculates and returns the sum of the data values stored in all nodes of a linked list
int sumNodesData(Node *head) {
    int sum = 0;
    Node *curr = head;
    while(curr != NULL) {
        sum += curr->data;
        curr = curr->next;
    }
    return sum;
}

















