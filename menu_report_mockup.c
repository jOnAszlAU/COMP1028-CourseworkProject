#include <stdio.h>

void run_menu(void) {

    int choice = 0;
    char input[50];

    while (1) {
        printf("Document Search Engine\n");
        printf("1. Load Collection\n");
        printf("2. Build Index\n");
        printf("3. Show Statistics\n");
        printf("4. Search\n");
        printf("5. View Document\n");
        printf("6. Save Results/Report\n");
        printf("7. Exit\n");
        printf("\n");
        printf("Enter choice (1-7): ");

        if (fgets(input, sizeof(input), stdin) == NULL){
            break;
        }

        if (sscanf(input, "%d", &choice) != 1) {
            printf("Invalid input. Please enter a number.\n");
            continue;
        }

        if (choice == 7) {
            printf("Existing program. \n");
            break;
        }

        printf("You selected option %d.\n", choice);

    }
}

int main(void){
    run_menu();
    return 0;
}