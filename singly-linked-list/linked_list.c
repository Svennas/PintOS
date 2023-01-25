#include <stdio.h>
#include <stdlib.h>

typedef struct list_item  {
  int value;
  struct list_item *next; // Points to the next item in the list
} Node; // Typedef to save time typing

/* Declaration for all the functions. */
void append(Node *first, int x);
void print(Node *first);
void prepend(Node *first, int x);
void input_sorted(Node *first, int x);
void clear(Node *first);


int main(int argc, char **argv) {
  Node root;
  root.value = -1; /* This value is always ignored. */
  root.next = NULL;

  print(&root);
  append(&root, 31);
  append(&root, 9);
  prepend(&root, 2);
  prepend(&root, 5);
  input_sorted(&root, 32);
  print(&root);
  clear(&root);
  print(&root);
  //prepend(&root, 2);
  input_sorted(&root, 4);
  print(&root);
  clear(&root);
  prepend(&root, 100);
  print(&root);
  clear(&root);
}

/* Puts x at the end of the list.. */
void append(Node *first, int x)
{
  while (first->next != NULL) {
    first = first->next;
  }
  Node *last = malloc(sizeof(Node));
  last->value = x;
  last->next = NULL;
  first->next = last;
}

/* Prints all the elements in the list. */
void print(Node *first) {
  while (first->next != NULL) {
    first = first->next;
    printf("%d\n", first->value);
  }
}

/* Puts x at the beginning of the list. */
void prepend(Node *first, int x) {
  Node *node = malloc(sizeof(Node));
  node->value = x;
  node->next = first->next;

  first->next = node;
}

/* input_sorted: find the first element in the list larger
than x and input x right before that element. */
void input_sorted(Node *first, int x) {
  if (first->next != NULL) {
    while (first->next->value < x) {
      first = first->next;
      if (first->next == NULL) break;
    }
  }
  prepend(first, x);
}

/* Free everything dynamically allocated. */
void clear(Node *first) {
  Node *temp;
  Node *node;
  node = first->next;
  while (node != NULL) {
    //printf("Value in while: %d\n", node->value);
    temp = node->next;
    free(node);
    node = temp;
  }

  first->next = NULL;
  printf("List has been cleared!\n");
}
