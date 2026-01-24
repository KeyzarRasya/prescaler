#ifndef LINKEDLIST
#define LINKEDLIST

struct linkedlist {
    struct node *first;
    struct node *last;
    int size;
};

struct node {
    struct node *prev;
    char *value;
    struct node *next;
};

struct linkedlist *linkedlist_init(void);
struct node *new_node(struct node *prev, char *value, struct node *next);
void append_node(struct linkedlist *ll, char *value);
struct node *get_node_at(struct linkedlist *ll, int index);
void print_ll(struct linkedlist *ll);

#endif