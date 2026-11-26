#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_WORD_LEN 50
#define MAX_UNIQUE_WORDS 3000
#define MAX_PATH_LEN 256

void process_document(int doc_id, const char *filepath) {
    FILE *file = fopen(filepath, "r");
    if (file == NULL) {
        printf("[Doc ID: %d] Warning: Could not open '%s' (skipping)\n", doc_id, filepath);
        return; // Skip missing or unreadable files cleanly without crashing
    }

    int total_terms = 0;
    char word_buffer[MAX_WORD_LEN];
    int buffer_index = 0;
    int ch;

    char unique_words[MAX_UNIQUE_WORDS][MAX_WORD_LEN];
    int unique_count = 0;
    int hit_limit = 0;

    while ((ch = fgetc(file)) != EOF) {
        ch = tolower((unsigned char)ch); // Consistent tokenization by lowercasing all

        if (isalpha((unsigned char)ch)) {
            if (buffer_index < MAX_WORD_LEN - 1) {
                word_buffer[buffer_index++] = (char)ch;
            }
        } else {
            if (buffer_index > 0) { // Ignore empty tokens
                word_buffer[buffer_index] = '\0';
                total_terms++;

                int already_exists = 0;
                for (int i = 0; i < unique_count; i++) {
                    if (strcmp(unique_words[i], word_buffer) == 0) {
                        already_exists = 1;
                        break;
                    }
                }

                if (!already_exists) {
                    if (unique_count < MAX_UNIQUE_WORDS) {
                        strcpy(unique_words[unique_count], word_buffer);
                        unique_count++;
                    } else if (!hit_limit) {
                        hit_limit = 1;
                        printf("[Doc ID: %d] Note: Reached MAX_UNIQUE_WORDS (%d) in '%s'\n",
                               doc_id, MAX_UNIQUE_WORDS, filepath);
                    }
                }

                buffer_index = 0;
            }
        }
    }

    // Handle a word at the very end of the file if there is no trailing non-letter
    if (buffer_index > 0) {
        word_buffer[buffer_index] = '\0';
        total_terms++;

        int already_exists = 0;
        for (int i = 0; i < unique_count; i++) {
            if (strcmp(unique_words[i], word_buffer) == 0) {
                already_exists = 1;
                break;
            }
        }

        if (!already_exists && unique_count < MAX_UNIQUE_WORDS) {
            strcpy(unique_words[unique_count], word_buffer);
            unique_count++;
        }
    }

    fclose(file);

    printf("[Doc ID: %d] File: %s | Total Terms: %d | Unique Terms: %d\n",
           doc_id, filepath, total_terms, unique_count);
}

int main(void) {
    FILE *list_file = fopen("filelist.txt", "r");
    if (list_file == NULL) {
        printf("Error: Could not open 'filelist.txt'. Please create it with your .txt file paths.\n");
        return 1;
    }

    char filepath[MAX_PATH_LEN];
    int doc_id = 1;

    while (fgets(filepath, sizeof(filepath), list_file) != NULL) {
        // Strip newline characters (\n or \r\n)
        filepath[strcspn(filepath, "\r\n")] = '\0';

        // Skipping blank lines
        if (filepath[0] == '\0') {
            continue;
        }

        // Check that the file ends with .txt
        size_t len = strlen(filepath);
        if (len < 4 || strcmp(filepath + len - 4, ".txt") != 0) {
            printf("Skipping non-.txt entry: %s\n", filepath);
            continue;
        }

        process_document(doc_id, filepath);
        doc_id++;
    }

    fclose(list_file);
    return 0;
}