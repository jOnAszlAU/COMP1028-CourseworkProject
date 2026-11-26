#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_DOCS 10

typedef struct {
    int doc_id;
    char filename[100];
    char content[500];
    int raw_terms;
    int total_terms;
    int unique_terms;
} Document;

int load_mock_collection(Document docs[]){
    Document d0 = {
        1,
        "doc01.txt",
        "Renewable energy can reduce dependence on fossil fuels. Solar power is widely used.",
        13,
        9,
        9
    };

    Document d1 = {
        2,
        "doc02.txt",
        " Wind energy and solar energy are important renewable energy sources.",
        10,
        7,
        5
    };

    Document d2 = {
        3,
        "doc03.txt",
        "Public transport can reduce traffic congestion in cities.",
        8,
        6,
        6
    };

    docs[0] = d0;
    docs[1] = d1;
    docs[2] = d2;

    return 3;
}

void view_documents(Document docs[], int count){
    printf("\n Loaded Document Collection (%d documents) \n", count);
    for (int i = 0; i < count; i++)
    {
        printf("[%d] %s | Raw: %d | Searchable: %d | Unique: %d\n", docs[i].doc_id, docs[i].filename, docs[i].raw_terms, docs[i].total_terms, docs[i].unique_terms);
        printf("Content: %s\n", docs[i].content);
    }
}

void run_menu(void) {

    Document docs[MAX_DOCS];
    int doc_count = 0;
    int is_loaded = 0;
    int is_indexed = 0;
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

        if (choice < 1 || choice > 7){
            printf("Invalid choice. Please enter a number between 1 and 7.\n");
            continue;
        }

        if (choice == 7) {
            printf("Existing program. \n");
            break;
        }

        printf("You selected option %d.\n", choice);

        switch (choice){
            case 1:
                doc_count = load_mock_collection(docs);
                is_loaded = 1;
                is_indexed = 0;
                printf("Loaded %d mock documents successfully.\n", doc_count);
                break;

            case 2:
                if (!is_loaded){
                    printf("Please load the collection first (Option 1).\n");
                }
                else {
                    is_indexed = 1;
                    printf("Index built for %d documents.\n", doc_count);
                }
                break;

            case 3:
                if (!is_loaded){
                    printf("Please load the collection first (Option 1).\n");
                }
                else {
                    printf("Showing collection statistics for %d documents...\n", doc_count);
                }
                break;

            case 4:
                if (!is_loaded || !is_indexed){
                    printf("Please load the collection first (Option 1) and build index (Option 2) first.\n");
                }
                else {
                    printf("Searching indexed documents...\n");
                }
                break;

            case 5:
                if (!is_loaded){
                    printf("Please load the collection first (Option 1).\n");
                }
                else {
                    view_documents(docs, doc_count);
                }
                break;

            case 6:
                if (!is_loaded){
                    printf("Please load the collection first (Option 1).\n");
                }
                else {
                    printf("Saving results/report...\n");
                }
                break;
        }

    }
}

int main(void){
    run_menu();
    return 0;
}