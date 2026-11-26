#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

// limits for the data structures used by the search system
// this is the biggest size allowed for one word buffer
// only 49 letters can be stored in practice, because the 50th slot holds '\0'
// that '\0' is the c-language end marker, so c knows where the word string stops
#define MAX_WORD_LEN 50
#define MAX_PATH_LEN 256
#define MAX_DOCS 20000
#define MAX_QUERY_LEN 256
#define MAX_QUERY_WORDS 20

// basic information about one text document
typedef struct Document{
    int doc_id; // unique number assigned when the document is loaded
    char filepath[MAX_PATH_LEN]; // location of the document on disk
    int total_terms; // number of searchable words in the document
    int unique_terms; // number of different searchable words in the document
    int is_valid; // 1 means this document can be used, 0 means it cannot
} Document;

// one entry in a term's posting list
// a posting list is basically a small record of every document that contains a word
// for example, "cat" can have one posting for document 1 and another for document 7
// the postings explain: this document has the word, and this is how many times
typedef struct PostingNode {
    int doc_id; // document that contains this term
    int term_freq; // number of times the term appears in that document
    struct PostingNode *next; // pointer to the next posting, like a chain link
} PostingNode;

// one searchable term in the inverted index
typedef struct VocabEntry {
    char term[MAX_WORD_LEN]; // normalized lowercase word
    int doc_freq; // number of documents containing this term
    // postings is the linked posting list for this term
    // it says which documents contain the term and how often it appears in each
    PostingNode *postings;
} VocabEntry;

// all ranking information collected for one search match
typedef struct SearchResult {
    int doc_id; // document number for this match
    double score; // tf-idf score used to rank the match
    int matched_count; // number of unique query terms found here
    char matched_list[256]; // readable list of matching query terms
} SearchResult;

// common words that are skipped because they usually do not help search ranking
const char *STOPWORDS[] = {
    "a", "about", "above", "after", "again", "against", "all", "am", "an", "and",
    "any", "are", "as", "at", "be", "because", "been", "before", "being", "below",
    "between", "both", "but", "by", "can", "could", "did", "do", "does", "doing",
    "down", "during", "each", "few", "for", "from", "further", "had", "has", "have",
    "having", "he", "her", "here", "hers", "herself", "him", "himself", "his", "how",
    "i", "if", "in", "into", "is", "it", "its", "itself", "just", "me",
    "more", "most", "my", "myself", "no", "nor", "not", "now", "of", "off",
    "on", "once", "only", "or", "other", "our", "ours", "ourselves", "out", "over",
    "own", "same", "she", "should", "so", "some", "such", "than", "that", "the",
    "their", "theirs", "them", "themselves", "then", "there", "these", "they", "this", "those",
    "through", "to", "too", "under", "until", "up", "very", "was", "we", "were",
    "what", "when", "where", "which", "while", "who", "whom", "why", "will", "with",
    "would", "you", "your", "yours", "yourself", "yourselves"
};

#define STOPWORD_COUNT (sizeof(STOPWORDS) / sizeof(STOPWORDS[0]))

// reads digits from a string and turns them into a non-negative integer
// leading spaces and tabs are ignored, and invalid input returns -1
int parse_non_negative_int(const char *str) {
    int i = 0; // start at the first character

    // skip any leading whitespace before the number
    while (str[i] == ' ' || str[i] == '\t') {
        i++;
    }

    // an empty or newline-only value is not a valid number
    if (str[i] == '\0' || str[i] == '\r' || str[i] == '\n') {
        return -1;
    }

    // build the integer one digit at a time
    int result = 0;
    while (str[i] != '\0' && str[i] != '\r' && str[i] != '\n') {
        // stop if a character is not between 0 and 9
        if (str[i] < '0' || str[i] > '9') {
            return -1;
        }
        // shift the previous digits and add the newest digit
        result = result * 10 + (str[i] - '0');
        i++;
    }

    return result;
}

// checks whether a word should be ignored during loading, indexing, or searching
int is_stopword(const char *word) {
    // one-letter words and empty words are never useful here
    if (word[0] == '\0' || word[1] == '\0') {
        return 1;
    }

    // compare the word against every item in the stopword list
    for (int i = 0; i < (int)STOPWORD_COUNT; i++) {
        if (strcmp(word, STOPWORDS[i]) == 0) {
            return 1;
        }
    }
    return 0;
}

// searches the vocabulary and returns a term's position, or -1 when missing
int find_term_index(VocabEntry *vocab, int vocab_count, const char *term) {
    for (int i = 0; i < vocab_count; i++) {
        // exact text match means this is the requested term
        if (strcmp(vocab[i].term, term) == 0) {
            return i;
        }
    }
    return -1;
}

void add_to_index(VocabEntry **vocab_ptr, int *vocab_count, int *vocab_capacity,
                  const char *term, int doc_id, int *is_new_in_doc) {
    // assume this is not the first time the term appears in this document
    *is_new_in_doc = 0;
    VocabEntry *vocab = *vocab_ptr;

    // find the term or add it below if this is the first time it was seen
    int idx = find_term_index(vocab, *vocab_count, term);

    if (idx == -1) {
        // grow the dynamic vocabulary array when it runs out of room
        if (*vocab_count >= *vocab_capacity) {
            int new_cap = (*vocab_capacity == 0) ? 1024 : (*vocab_capacity * 2);
            VocabEntry *temp = realloc(vocab, new_cap * sizeof(VocabEntry));
            if (temp == NULL) {
                printf("Error: Out of memory expanding vocabulary.\n");
                return;
            }
            vocab = temp;
            *vocab_ptr = vocab;
            *vocab_capacity = new_cap;
        }

        // create a new vocabulary entry with an empty posting list
        idx = *vocab_count;
        strcpy(vocab[idx].term, term);
        vocab[idx].doc_freq = 0;
        vocab[idx].postings = NULL;
        (*vocab_count)++;
    }

    // walk the term's posting list to find the current document
    // the first node might be document 1, then it points to document 7, and so on
    // prev remembers the last node in case a new node needs to be attached
    PostingNode *curr = vocab[idx].postings;
    PostingNode *prev = NULL;

    while (curr != NULL) {
        if ((*curr).doc_id == doc_id) {
            // this document already has the term, so just increase its frequency
            // for example, "cat" going from 1 to 2 means cat appeared twice here
            (*curr).term_freq++;
            return;
        }
        prev = curr;
        curr = (*curr).next;
    }

    // no posting exists for this document, so allocate and append a new node
    PostingNode *new_node = malloc(sizeof(PostingNode));
    if (new_node == NULL) {
        printf("Error: Out of memory allocating posting node.\n");
        return;
    }
    (*new_node).doc_id = doc_id;
    (*new_node).term_freq = 1;
    (*new_node).next = NULL;

    if (prev == NULL) {
        // this is the first posting for the term
        vocab[idx].postings = new_node;
    } else {
        // connect the new posting after the current final node
        (*prev).next = new_node;
    }

    // this term now appears in one more document
    vocab[idx].doc_freq++;
    *is_new_in_doc = 1;
}

// frees every posting node and then the vocabulary array
void free_index(VocabEntry *vocab, int vocab_count) {
    for (int i = 0; i < vocab_count; i++) {
        PostingNode *curr = vocab[i].postings;
        while (curr != NULL) {
            // save the next pointer before freeing the current node
            PostingNode *next_node = (*curr).next;
            free(curr);
            curr = next_node;
        }
    }
    free(vocab);
}

// opens filelist.txt, checks each listed document, and records valid documents
int load_document_collection(const char *list_filename, Document *docs, long *total_terms_out) {
    FILE *list_file = fopen(list_filename, "r");
    if (list_file == NULL) {
        printf("Error: Could not open '%s'.\n", list_filename);
        return 0;
    }

    // counters and temporary storage used while scanning the collection
    int doc_count = 0;
    int next_doc_id = 1;
    long collection_total_terms = 0;
    char filepath[MAX_PATH_LEN];

    // process one filepath from the list on each pass
    while (fgets(filepath, sizeof(filepath), list_file) != NULL) {
        // enforce the maximum collection size
        if (doc_count >= MAX_DOCS) {
            break;
        }

        // remove newline and carriage-return characters from the filepath
        filepath[strcspn(filepath, "\r\n")] = '\0';
        if (filepath[0] == '\0') {
            continue;
        }

        // try to open this filepath just to validate and count its terms
        FILE *fp = fopen(filepath, "r");
        if (fp == NULL) {
            printf("Warning: Could not open '%s' (skipping)\n", filepath);
            continue;
        }

        // count searchable terms in this one document
        // word_buffer is a temporary character box used to build one word
        // if max_word_len is 50, the box holds 49 characters plus the final '\0'
        // buffer_index tracks how many letters have been put into the box
        int term_count = 0;
        char word_buffer[MAX_WORD_LEN];
        int buffer_index = 0;
        int ch;

        while ((ch = fgetc(fp)) != EOF) {
            // make matching case-insensitive
            ch = tolower((unsigned char)ch);
            if (isalpha((unsigned char)ch)) {
                // keep at most one less than the buffer limit for a null terminator
                if (buffer_index < MAX_WORD_LEN - 1) {
                    word_buffer[buffer_index++] = (char)ch;
                }
            } else {
                if (buffer_index > 0) {
                    // finish the current word and count it unless it is a stopword
                    word_buffer[buffer_index] = '\0';
                    if (!is_stopword(word_buffer)) {
                        term_count++;
                    }
                    buffer_index = 0;
                }
            }
        }

        // a word left at end-of-file may not end with punctuation
        if (buffer_index > 0) {
            word_buffer[buffer_index] = '\0';
            if (!is_stopword(word_buffer)) {
                term_count++;
            }
        }

        fclose(fp);

        // skip documents that have no usable searchable terms
        if (term_count == 0) {
            printf("Warning: '%s' is empty or has no searchable terms (skipping)\n", filepath);
            continue;
        }

        // store the validated document and reset the unique-term count for later indexing
        docs[doc_count].doc_id = next_doc_id;
        strncpy(docs[doc_count].filepath, filepath, MAX_PATH_LEN - 1);
        docs[doc_count].filepath[MAX_PATH_LEN - 1] = '\0';
        docs[doc_count].total_terms = term_count;
        docs[doc_count].unique_terms = 0;
        docs[doc_count].is_valid = 1;

        // include this document's terms in the collection-wide total
        collection_total_terms += term_count;
        doc_count++;
        next_doc_id++;
    }

    fclose(list_file);
    // report collection totals to the caller
    *total_terms_out = collection_total_terms;
    return doc_count;
}

// reads every loaded document and adds its terms to the inverted index
void build_inverted_index(Document *docs, int doc_count,
                          VocabEntry **vocab_ptr, int *vocab_count, int *vocab_capacity,
                          long *total_postings_out) {
    // count the number of document-to-term posting nodes created
    long total_postings = 0;

    for (int d = 0; d < doc_count; d++) {
        FILE *fp = fopen(docs[d].filepath, "r");
        if (fp == NULL) {
            // the file passed validation earlier, but skip it if it became unavailable
            continue;
        }

        // temporary buffer for constructing one word at a time
        // think of buffer_index as a cursor: index 0 is the first slot
        // each letter goes in the slot, buffer_index moves up, and '\0' ends the word
        // the extra '\0' slot is why this code checks MAX_WORD_LEN - 1
        char word_buffer[MAX_WORD_LEN];
        int buffer_index = 0;
        int ch;
        int is_new_in_doc = 0;
        docs[d].unique_terms = 0;

        while ((ch = fgetc(fp)) != EOF) {
            // normalize letters before comparison
            ch = tolower((unsigned char)ch);
            if (isalpha((unsigned char)ch)) {
                if (buffer_index < MAX_WORD_LEN - 1) {
                    word_buffer[buffer_index++] = (char)ch;
                }
            } else {
                if (buffer_index > 0) {
                    word_buffer[buffer_index] = '\0';
                    if (!is_stopword(word_buffer)) {
                        // add or update this term in the index
                        add_to_index(vocab_ptr, vocab_count, vocab_capacity,
                                     word_buffer, docs[d].doc_id, &is_new_in_doc);
                        if (is_new_in_doc) {
                            // count a posting only when this term is new to this document
                            docs[d].unique_terms++;
                            total_postings++;
                        }
                    }
                    buffer_index = 0;
                }
            }
        }

        // process a final word if punctuation was not present after it
        if (buffer_index > 0) {
            word_buffer[buffer_index] = '\0';
            if (!is_stopword(word_buffer)) {
                add_to_index(vocab_ptr, vocab_count, vocab_capacity,
                             word_buffer, docs[d].doc_id, &is_new_in_doc);
                if (is_new_in_doc) {
                    docs[d].unique_terms++;
                    total_postings++;
                }
            }
        }

        fclose(fp);
    }

    // send the posting count back to the menu for reporting
    *total_postings_out = total_postings;
}

// temporarily disabled menu feature: normalizes a term and prints its first postings
void inspect_term_postings(const char *raw_term, VocabEntry *vocab, int vocab_count) {
    char term[MAX_WORD_LEN];
    int idx = 0;

    // keep only alphabetic characters and convert everything to lowercase
    for (int i = 0; raw_term[i] != '\0' && idx < MAX_WORD_LEN - 1; i++) {
        if (isalpha((unsigned char)raw_term[i])) {
            term[idx++] = (char)tolower((unsigned char)raw_term[i]);
        }
    }
    term[idx] = '\0';

    // do not inspect empty or stopword terms
    if (term[0] == '\0' || is_stopword(term)) {
        printf("'%s' is a stopword or invalid term.\n", raw_term);
        return;
    }

    // stop with a clear message if the normalized term is absent
    int vocab_idx = find_term_index(vocab, vocab_count, term);
    if (vocab_idx == -1) {
        printf("Term '%s' is not in the inverted index.\n", term);
        return;
    }

    printf("\nTerm: %s\n", vocab[vocab_idx].term);
    PostingNode *p = vocab[vocab_idx].postings;
    int shown = 0;

    // show at most ten document postings for this term
    while (p != NULL && shown < 10) {
        printf(" -> Document %d (frequency %d)", (*p).doc_id, (*p).term_freq);
        p = (*p).next;
        shown++;
    }

    // indicate that additional documents exist when the list was truncated
    if (p != NULL) {
        printf(" -> ... (%d total documents)", vocab[vocab_idx].doc_freq);
    }
    printf("\n");
}

// sorts matches by matched-term count, score, and document id
void sort_results(SearchResult *results, int count) {
    // selection sort: place the best remaining result at each position
    for (int i = 0; i < count - 1; i++) {
        int best = i;
        for (int j = i + 1; j < count; j++) {
            // more matched query terms wins
            if (results[j].matched_count > results[best].matched_count) {
                best = j;
            } else if (results[j].matched_count == results[best].matched_count) {
                // for an equal match count, a larger score wins
                if (results[j].score > results[best].score) {
                    best = j;
                } else if (results[j].score == results[best].score &&
                           results[j].doc_id < results[best].doc_id) {
                    // use document id as the final stable tie-breaker
                    best = j;
                }
            }
        }

        if (best != i) {
            // swap the selected best result into the current rank
            SearchResult temp = results[i];
            results[i] = results[best];
            results[best] = temp;
        }
    }
}

// tokenizes a query, ranks matching documents with tf-idf, and prints the top results
void search_query(const char *raw_query, Document *docs, int doc_count,
                  VocabEntry *vocab, int vocab_count, int top_k) {
    // store unique normalized query terms, with a hard limit of 20
    char query_words[MAX_QUERY_WORDS][MAX_WORD_LEN];
    int valid_query_terms = 0;

    // temporary word buffer shared across the tokenizer loop
    // it holds only the word currently being read, not the whole search query
    // buffer_index says how many letters are currently in the buffer
    // for a 50-character buffer size, the highest usable letter slot is 48
    // slot 49 is reserved for '\0', which is the c string end marker
    char word_buffer[MAX_WORD_LEN];
    int buffer_index = 0;

    // scan the raw query one character at a time
    for (int i = 0; ; i++) {
        unsigned char ch = (unsigned char)raw_query[i];

        if (ch != '\0' && isalpha(ch)) {
            if (buffer_index < MAX_WORD_LEN - 1) {
                word_buffer[buffer_index++] = (char)tolower(ch);
            }
        } else {
            if (buffer_index > 0) {
                // finish a word whenever punctuation or whitespace arrives
                word_buffer[buffer_index] = '\0';
                buffer_index = 0;

                if (!is_stopword(word_buffer) && valid_query_terms < MAX_QUERY_WORDS) {
                    // check for repeated terms already accepted from this query
                    int duplicate = 0;
                    for (int q = 0; q < valid_query_terms; q++) {
                        if (strcmp(query_words[q], word_buffer) == 0) {
                            duplicate = 1;
                            break;
                        }
                    }
                    if (!duplicate) {
                        // remember this unique searchable term
                        strcpy(query_words[valid_query_terms], word_buffer);
                        valid_query_terms++;
                    }
                }
            }
            if (ch == '\0') {
                // the null terminator means the tokenizer is finished
                break;
            }
        }
    }

    // stop now if the query had only stopwords or non-letters
    if (valid_query_terms == 0) {
        printf("Query contains no valid searchable terms.\n");
        return;
    }

    // allocate one score, match count, and matched-name text per document
    double *doc_scores = calloc(doc_count, sizeof(double));
    int *matched_counts = calloc(doc_count, sizeof(int));
    char (*matched_names)[256] = calloc(doc_count, sizeof(*matched_names));

    if (doc_scores == NULL || matched_counts == NULL || matched_names == NULL) {
        printf("Error: Out of memory during search.\n");
        // free any allocations that succeeded before the combined failure
        free(doc_scores);
        free(matched_counts);
        free(matched_names);
        return;
    }

    // process each unique query term
    for (int q = 0; q < valid_query_terms; q++) {
        int vocab_idx = find_term_index(vocab, vocab_count, query_words[q]);

        if (vocab_idx != -1 && vocab[vocab_idx].doc_freq > 0) {
            // idf rewards terms that occur in fewer documents
            double idf = log((double)doc_count / (double)vocab[vocab_idx].doc_freq) + 1.0;

            // visit every document containing this query term
            // p starts at the first posting node in this term's posting list
            // each posting tells us a document id and that term's frequency in it
            // p then moves to the next posting
            PostingNode *p = vocab[vocab_idx].postings;
            while (p != NULL) {
                int arr_idx = (*p).doc_id - 1;
                if (arr_idx >= 0 && arr_idx < doc_count) {
                    // tf normalizes frequency by the document's searchable term count
                    double tf = (double)(*p).term_freq / (double)docs[arr_idx].total_terms;
                    // scale the tf-idf value by 100 for a readable score
                    doc_scores[arr_idx] += (tf * idf) * 100.0;

                    if (matched_counts[arr_idx] > 0) {
                        // separate multiple matched terms with a comma
                        strcat(matched_names[arr_idx], ", ");
                    }
                    strcat(matched_names[arr_idx], query_words[q]);
                    matched_counts[arr_idx]++;
                }
                p = (*p).next;
            }
        }
    }

    // count how many documents contain at least one query term
    int total_matches = 0;
    for (int i = 0; i < doc_count; i++) {
        if (matched_counts[i] > 0) {
            total_matches++;
        }
    }

    // return cleanly if no document matched the query
    if (total_matches == 0) {
        printf("No documents matched your query.\n");
        free(doc_scores);
        free(matched_counts);
        free(matched_names);
        return;
    }

    // allocate the final compact result array
    SearchResult *results = malloc(total_matches * sizeof(SearchResult));
    if (results == NULL) {
        printf("Error: Out of memory storing results.\n");
        free(doc_scores);
        free(matched_counts);
        free(matched_names);
        return;
    }

    // copy only matching documents into the result array
    int r_idx = 0;
    for (int i = 0; i < doc_count; i++) {
        if (matched_counts[i] > 0) {
            results[r_idx].doc_id = docs[i].doc_id;
            results[r_idx].score = doc_scores[i];
            results[r_idx].matched_count = matched_counts[i];
            strcpy(results[r_idx].matched_list, matched_names[i]);
            r_idx++;
        }
    }

    // rank the compact result array using the sorting rules
    sort_results(results, total_matches);

    // never request more rows than the number of actual matches
    int results_to_show = (total_matches < top_k) ? total_matches : top_k;

    // print the query and a simple table header
    printf("\nSearch query: %s\n", raw_query);
    printf("Top %d results (out of %d matching documents):\n", results_to_show, total_matches);
    printf("%-6s %-42s %-10s %s\n", "Rank", "File", "Score", "Matched Terms");
    printf("----------------------------------------------------------------------------------\n");

    // print each selected result in rank order
    for (int rank = 0; rank < results_to_show; rank++) {
        int arr_idx = results[rank].doc_id - 1;
        printf("%-6d %-42s %-10.2f %s\n",
               rank + 1,
               docs[arr_idx].filepath,
               results[rank].score,
               results[rank].matched_list);
    }

    // release temporary search allocations
    free(results);
    free(doc_scores);
    free(matched_counts);
    free(matched_names);
}

// starts the program, runs the menu, and cleans up all allocated memory
int main(void) {
    // reserve storage for the largest allowed document collection
    Document *docs = malloc(MAX_DOCS * sizeof(Document));
    if (docs == NULL) {
        printf("Error: Could not allocate memory for documents.\n");
        return 1;
    }

    // the vocabulary starts empty and grows as terms are indexed
    VocabEntry *vocab = NULL;
    int vocab_count = 0;
    int vocab_capacity = 0;

    // track collection, index, and menu state
    int doc_count = 0;
    long total_terms = 0;
    long total_postings = 0;
    int docs_loaded = 0;
    int index_built = 0;
    int top_k = 5;
    int running = 1;

    // reused input buffer for menu choices, searches, and settings
    char input[MAX_QUERY_LEN];

    // keep showing the menu until the user chooses exit
    while (running) {
        // print the available menu actions and the current result count
        printf("\n=== Document Search & Information Retrieval System ===\n");
        printf("1. Load Document Collection\n");
        printf("2. Build Inverted Index\n");
        printf("3. Search Query\n");
        printf("4. disabled momentarily\n");
        printf("5. Set Top Results Count (Current: %d)\n", top_k);
        printf("0. Exit\n");
        printf("Select an option: ");

        // fgets gives one line; EOF or read failure also ends the loop
        if (fgets(input, sizeof(input), stdin) == NULL) {
            break;
        }

        // menu choices must be non-negative integers
        int choice = parse_non_negative_int(input);

        switch (choice) {
            case 0:
                // setting this flag stops the menu loop
                running = 0;
                break;

            case 1:
                // loading twice could otherwise duplicate documents
                if (docs_loaded) {
                    printf("Document collection is already loaded (%d documents).\n", doc_count);
                    break;
                }
                printf("Loading documents from 'filelist.txt'...\n");
                doc_count = load_document_collection("filelist.txt", docs, &total_terms);
                if (doc_count > 0) {
                    // remember that loading succeeded and report the result
                    docs_loaded = 1;
                    printf("Collection loaded: %d documents, %ld total searchable terms.\n",
                           doc_count, total_terms);
                }
                break;

            case 2:
                // documents must exist before an index can be built
                if (!docs_loaded) {
                    printf("Please load the document collection first (Option 1).\n");
                    break;
                }
                // rebuilding could duplicate postings and document frequencies
                if (index_built) {
                    printf("Inverted index is already built (%d unique terms).\n", vocab_count);
                    break;
                }
                printf("Building inverted index...\n");
                build_inverted_index(docs, doc_count, &vocab, &vocab_count, &vocab_capacity, &total_postings);
                // only mark the index ready after the build function completes
                index_built = 1;
                printf("Index built successfully: %d documents, %ld total terms, %d unique terms (%ld posting nodes).\n",
                       doc_count, total_terms, vocab_count, total_postings);
                break;

            case 3:
                // searching requires a completed inverted index
                if (!index_built) {
                    printf("Please build the inverted index first (Option 2).\n");
                    break;
                }
                printf("Enter search query: ");
                if (fgets(input, sizeof(input), stdin) != NULL) {
                    // remove the newline before passing the query to the search function
                    input[strcspn(input, "\r\n")] = '\0';
                    search_query(input, docs, doc_count, vocab, vocab_count, top_k);
                }
                break;

            /* this temporarily disabled option could inspect one term's postings
            case 4:
                if (!index_built) {
                    printf("Please build the inverted index first (Option 2).\n");
                    break;
                }
                printf("Enter a term to inspect: ");
                if (fgets(input, sizeof(input), stdin) != NULL) {
                    input[strcspn(input, "\r\n")] = '\0';
                    inspect_term_postings(input, vocab, vocab_count);
                }
                break;*/

            case 5:
                printf("Enter number of top results to display (e.g., 5 or 10): ");
                if (fgets(input, sizeof(input), stdin) != NULL) {
                    // parse the requested result count as a non-negative integer
                    int new_k = parse_non_negative_int(input);
                    if (new_k > 0) {
                        // a positive value replaces the existing top-k setting
                        top_k = new_k;
                        printf("Top results display count set to %d.\n", top_k);
                    } else {
                        // zero and negative or malformed values are rejected
                        printf("Invalid number.\n");
                    }
                }
                break;

            default:
                // keep the menu running and explain the allowed options
                printf("Invalid option. Please choose 0-5.\n");
                break;
        }
    }

    // free the posting lists and vocabulary only after the menu ends
    free_index(vocab, vocab_count);
    // free the preallocated document array
    free(docs);
    printf("Memory freed cleanly. Goodbye!\n");
    return 0;
}
