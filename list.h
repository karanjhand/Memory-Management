#ifndef __LIST_H__
#define __LIST_H__

typedef struct node{
	int data;
	void *nodeptr;
	struct node *next;
} Node;


//Creates a node for a linked list
Node* createNode(void *ptr, int data);

//Destructs a linked list
void destructor(Node *head);

//inserts a node at the head of the linked list
void insertNodeAtHead(Node **head, Node *node);

//insert a node at the tail of the linked list
void insertNodeAtTail(Node **head, Node *node);

//counts the nodes in the linked list
int countNodes(Node *head);

//finds the node in the linked list with the data we are looking for, return NULL if not found
Node* findNode(Node *head, int data);

//deletes a Node
void deleteNode(Node **head, Node *node);

//Sort the list in ascending order
void sortList(Node **head);

//calculates and returns the sum of the data values stored in all nodes of a linked list
int sumNodesData(Node *head);


#endif