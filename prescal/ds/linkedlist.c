#include "../include/ds/linkedlist.h"
#include <stdio.h>
#include <stdlib.h>

struct linkedlist *linkedlist_init(void){
    struct linkedlist *ll = 
        malloc(sizeof(struct linkedlist));

    if (!ll) {
        perror("Failed to allocate linkedlist");
        return NULL;
    }

    ll->first = NULL;
    ll->last = NULL;
    ll->size = 0;

    return ll;
}

struct node *new_node(
    struct node *prev,
    char *value,
    struct node *next
) {
    struct node *n =
        malloc(sizeof(struct node));
    
    if (!n) {
        perror("Failed to allocate node");
        return NULL;
    }

    n->prev = prev;
    n->value = value;
    n->next = next;
    return n;
}

void append_node(struct linkedlist *ll, char *value) {
    struct node *n = new_node(NULL, value, NULL);

    if (!ll->first) {
        ll->first = n;
        ll->last = n;
        ll->size++;
        return;
    }

    ll->last->next = n;
    n->prev = ll->last;
    ll->last = n;
    ll->size++;
}

void print_ll(struct linkedlist *ll) {
    struct node *current = ll->first;

    while (current) {
        printf("%s\n", current->value);
        current = current->next;
    }
}

struct node *get_node_at(struct linkedlist *ll, int index) {
    if (index < 0 || index >= ll->size) {
        return NULL; // Index out of bounds
    }

    struct node *current = ll->first;
    for (int i = 0; i < index; i++) {
        if (current == NULL) {
            return NULL; // Should not happen if size is correct
        }
        current = current->next;
    }
    return current;
}