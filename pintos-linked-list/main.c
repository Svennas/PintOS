#include "list.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const int SIZE = 30;

typedef struct student {
	char *name;
	struct list_elem elem;
} Student;

/* This function fetches a student name from the terminal
	 output and adds it to the list. */
void insert (struct list *student_list) {
	char *input = (char*)malloc(sizeof(char)*SIZE);
	printf("Write the students name: \n");
	scanf("%s", input);

	Student *student = malloc(sizeof(Student));
	student->name = input;

	list_push_back(student_list, &student->elem);

}

/* This function gets a student name from the terminal
input, removes it from the list, and dellaocates the
appropriate memory (if the input exists). */
void delete (struct list *student_list) {

	char *input = (char*)malloc(sizeof(char)*SIZE);
	printf("Write the name of the student you want to remove: \n");
	scanf("%s", input);

	struct list_elem *i;
	i = list_begin(student_list);

	while (i != list_end(student_list)) {
		Student *student = list_entry (i, Student, elem);

		if (strcmp(student->name, input) == 0) {
			printf("Deleted student %s\n", student->name);
			list_remove(i);
			free(student->name);
			free(student);
			free(input);
			break;
		}
		i = list_next(i);
	}
}

/* This function prints the entire list. */
void list (struct list *student_list) {

	struct list_elem *i;

	for (i = list_begin (student_list); i != list_end (student_list);
			 i = list_next (i))
	{
			Student *student = list_entry (i, Student, elem);
			printf("%s\n", student->name);
	}
}

/* This function clears the list and deallocates the memory
for all items in the list. */
void quit (struct list *student_list) {

	while (!list_empty (student_list))
	{
    struct list_elem *i = list_pop_front(student_list);
    Student *student = list_entry(i, Student, elem);
		free(student->name);
		free(student);
  }

	printf("Exiting the program...\n");
	exit(0);
}


int main() {
	struct list student_list;
	list_init (&student_list);
	int opt;

	do {
		printf("Menu:\n");
		printf("1 - Insert student\n");
		printf("2 - Delete student\n");
		printf("3 - List students\n");
		printf("4 - Exit\n");
		scanf("%d", &opt);
		switch (opt) {
			case 1:
				{
					insert(&student_list);
					break;
				}
			case 2:
				{
					delete(&student_list);
					break;
				}
			case 3:
				{
					list(&student_list);
					break;
				}

			case 4:
				{
					quit(&student_list);
					break;
				}
			default:
				{
					printf("Quit? (1/0):\n");
					scanf("%d", &opt);
					if (opt)
						quit(&student_list);
					break;
				}
		}
	} while(1);

	return 0;
}
