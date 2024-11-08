/*
    Voorraad tellen.
    Copyright (C) 2018-2020  Martijn Heil

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.



    This program is written for a standard C11 *hosted* environment.
*/

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>

#include <csv.h>
#include <safe_math.h>

#ifdef __unix__
    #include <unistd.h>
    #ifdef _POSIX_VERSION
        #define POSIX
    #endif
#endif

// Don't forget to free the returned value.
static char *fgetline(FILE *input)
{
    static const size_t CHUNK_SIZE = 256;
    char *buf = malloc(CHUNK_SIZE + 1);
    size_t buf_size = CHUNK_SIZE;
    size_t buf_used = 0;
    if(buf == NULL) return NULL;
    while(true)
    {
        char c = fgetc(input);
        if(c == EOF && feof(input))
        {
            clearerr(input);
            free(buf);
            return NULL;
        }

        if(buf_used == 256)
        {
            size_t size;
            if(!psnip_safe_add(&size, buf_size, CHUNK_SIZE)) { free(buf); return NULL; }
            if(!psnip_safe_add(&size, size, 1)) { free(buf); return NULL; }
            char *tmp = realloc(buf, size);
            if(tmp == NULL)
            {
                free(buf);
                return NULL;
            }
            else
            {
                buf = tmp;
            }
        }
        buf[buf_used] = c;
        if(c == '\n')
        {
            buf[buf_used] = '\0';
            return buf;
        }
        buf_used++;
    }
}

static void print_header(void)
{
    printf("Voorraad tellen. Copyright (C) 2018-2020  Martijn Heil\n"
            "U kunt dit programma op elk moment normaal sluiten, veranderingen worden automatisch opgeslagen.\n\n");
}

static void print_welcome(void)
{
    printf("Voorraad tellen.\n"
            "Copyright (C) 2018-2020  Martijn Heil\n"
            "\n"
            "This program is free software: you can redistribute it and/or modify\n"
            "it under the terms of the GNU General Public License as published by\n"
            "the Free Software Foundation, either version 3 of the License, or\n"
            "(at your option) any later version.\n"
            "\n"
            "This program is distributed in the hope that it will be useful,\n"
            "but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
            "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n"
            "GNU General Public License for more details.\n"
            "\n"
            "You should have received a copy of the GNU General Public License\n"
            "along with this program.  If not, see <http://www.gnu.org/licenses/>.\n"
            "\n"
            "\n"
            "\n"
            "Pas op: Het CSV bestand waar u het pad voor geeft wordt aangepast met de veranderingen.\n"
            "Als er een fout optreed tijdens het aanpassen van dit bestand, IS HET BESTAND MOGELIJK VERLOREN.\n"
            "ALS DIT BESTAND BELANGRIJK IS, MAAK ER DAN EERST EEN KOPIE VAN.\n"
            "\n"
            "U kunt dit programma op elk moment normaal sluiten, veranderingen worden automatisch opgeslagen."
            "\n"
            "\n"
            "\n");
}

static void clearscrn(void)
{
    #ifdef _WIN32
        system("cls");
        print_header();
    #elif defined(POSIX)
        printf("\033[2J\033[1;1H");
        print_header();
    #endif
    // else just do nothing.
}

static void clearscrn_true(void)
{
    #ifdef _WIN32
        system("cls");
    #elif defined(POSIX)
        printf("\033[2J\033[1;1H");
    #endif
    // else just do nothing.
}

static char const *strcasestr(const char *str, const char *pattern) {
    size_t i;

    if (!*pattern)
        return (char const *)str;

    for (; *str; str++)
    {
        if (toupper(*str) == toupper(*pattern))
        {
            for (i = 1;; i++)
            {
                if (!pattern[i])
                    return (char const *)str;
                if (toupper(str[i]) != toupper(pattern[i]))
                    break;
            }
        }
    }
    return NULL;
}

static const size_t TO_FREE_BLOCK_SIZE = 256;
static size_t to_free_size;
static size_t to_free_max_size;
static void **to_free_storage;

static bool to_free_add(void *ptr)
{
    if(to_free_storage == NULL)
    {
        to_free_storage = malloc(TO_FREE_BLOCK_SIZE * sizeof(void *));
        if(to_free_storage == NULL) return false;
        to_free_size = 0;
        to_free_max_size = TO_FREE_BLOCK_SIZE;
    }
    else if(to_free_size >= to_free_max_size)
    {
        void *tmp = realloc(to_free_storage, to_free_max_size * sizeof(void *) + TO_FREE_BLOCK_SIZE * sizeof(void *));
        if(tmp == NULL) return false;
        to_free_storage = tmp;
        to_free_max_size = to_free_max_size + TO_FREE_BLOCK_SIZE;
    }
    to_free_storage[to_free_size] = ptr;
    to_free_size++;
    return true;
}

static void to_free_free(void)
{
    for(size_t i = 0; i < to_free_size; i++) free(to_free_storage[i]);
}


struct record
{
    size_t column_count;
    char **columns;
};

static size_t const RECORDS_CHUNK_SIZE = 64;
static size_t records_max_size;
static size_t records_size = 0;
static struct record *records;
static struct record header;

static size_t amount_column_index;
static size_t barcode_column_index;
static int delim;


static void end_of_field_callback(void *parsed_data, size_t len, void *callback_data)
{
    struct record *record = records + records_size;

    if(record->column_count == 0)
    {
        record->columns = malloc(sizeof(char *));
        if(record->columns == NULL) { printf("Fout: kon geen extra geheugen-ruimte aanvragen. (%s) (main.c:%i)\n", strerror(errno), __LINE__); exit(EXIT_FAILURE); }
    }
    else
    {
        size_t size;
        if(!psnip_safe_mul(&size, record->column_count, sizeof(char *))) { printf("Fout: integer overflow (main.c:%i).\n", __LINE__); exit(EXIT_FAILURE); }
        if(!psnip_safe_add(&size, size, sizeof(char *))) { printf("Fout: integer overflow\n"); exit(EXIT_FAILURE); }
        char **tmp = realloc(record->columns, size);
        if(tmp == NULL) { printf("Fout: kon geen extra geheugen-ruimte aanvragen. (%s) (main.c:%i)\n", strerror(errno), __LINE__); exit(EXIT_FAILURE); }
        record->columns = tmp;
    }

    size_t size;
    if(!psnip_safe_add(&size, len, 1)) { printf("Fout: integer overflow (main.c:%i).\n", __LINE__); exit(EXIT_FAILURE); }
    char *column = malloc(size);
    if(column == NULL) { printf("Fout: kon geen extra geheugen-ruimte aanvragen. (%s) (main.c:%i)\n", strerror(errno), __LINE__); exit(EXIT_FAILURE); }
    memcpy(column, parsed_data, len);
    column[len] = '\0';
    record->columns[record->column_count] = column;
    if(!psnip_safe_add(&(record->column_count), record->column_count, 1)) { printf("Fout: integer overflow (main.c:%i).\n", __LINE__); exit(EXIT_FAILURE); }
}

static void end_of_record_callback(int c, void *callback_data)
{
    static bool init = true;
    if(init) // skip the header, use it to find the column indexes
    {
        struct record *record = records; // = "records", That's not a bug.

        // Save header as header
        size_t size;
        if(!psnip_safe_mul(&size, record->column_count, sizeof(char *))) { printf("Fout: integer overflow (main.c:%i).\n", __LINE__); exit(EXIT_FAILURE); }
        header.columns = malloc(size);
        if(record->columns == NULL) { printf("Fout: kon geen extra geheugen-ruimte aanvragen. (%s) (main.c:%i)\n", strerror(errno), __LINE__); exit(EXIT_FAILURE); }
        header.column_count = record->column_count;
        memcpy(header.columns, record->columns, size);

        record->column_count = 0;
        free(record->columns);
        init = false;
    }
    else
    {
        if(!psnip_safe_add(&records_size, records_size, 1)) { printf("Fout: integer overflow (main.c:%i).\n", __LINE__); exit(EXIT_FAILURE); }
        if(records_size >= records_max_size) // Grow if needed.
        {
            if(!psnip_safe_add(&records_max_size, records_max_size, RECORDS_CHUNK_SIZE)) { printf("Fout: integer overflow (main.c:%i).\n", __LINE__); exit(EXIT_FAILURE); }
            size_t size;
            if(!psnip_safe_mul(&size, records_max_size, sizeof(struct record))) { printf("Fout: integer overflow (main.c:%i).\n", __LINE__); exit(EXIT_FAILURE); }
            void *tmp = realloc(records, size);
            if(tmp == NULL) { printf("Fout: kon geen extra geheugen-ruimte aanvragen. (%s) (main.c:%i)\n", strerror(errno), __LINE__); exit(EXIT_FAILURE); }
            records = tmp;
            for(size_t i = records_size; i < records_max_size; i++) records[i].column_count = 0;
        }
    }
}

/*
 * @returns false on error
 */
static bool save(struct csv_parser *parser, struct record *records, size_t records_size, const char *path)
{
    FILE *f = fopen(path, "w");
    if(f == NULL) { printf("Fout: %s", strerror(errno)); }
    for(size_t i = 0; i < header.column_count; i++)
    {
        if(csv_fwrite(f, header.columns[i], strlen(header.columns[i])) == EOF) return false;
        if(i != header.column_count - 1) fputc(delim, f);
    }
    fputc('\n', f);

    for(size_t i = 0; i < records_size; i++)
    {
        struct record *record = records + i;
        for(size_t j = 0; j < record->column_count; j++)
        {
            if(csv_fwrite(f, record->columns[j], strlen(record->columns[j])) == EOF) return false;
            if(j != record->column_count - 1) fputc(delim, f);
        }
        fputc('\n', f);
    }
    if(fclose(f) == EOF) return false;
    return true;
}

/*
 * @returns true on error.
 */
static bool print_table(struct record *records, size_t n)
{
    if(n == 0) return false;
    size_t max_column_count = 0;
    for(size_t i = 0; i < n; i++)
    {
        if(records[i].column_count > max_column_count) max_column_count = records[i].column_count;
    }
    if(max_column_count == 0) return false;

    size_t max_column_widths[max_column_count];
    for(size_t i = 0; i < max_column_count; i++) max_column_widths[i] = 0; // Initialize

    for(size_t i = 0; i < n; i++) // Calculate max widths
    {
        struct record *record = records + i;
        for(size_t column = 0; column < record->column_count; column++)
        {
            // TODO optimize, you dont need to keep counting the length after it's higher than previous ones already.
            size_t len = strlen(record->columns[column]);
            if(len > max_column_widths[column]) max_column_widths[column] = len;
        }
    }
    size_t total_width = 0;
    for(size_t i = 0; i < max_column_count; i++)
    {
        if(!psnip_safe_add(&total_width, total_width, max_column_widths[i])) return true;
    }

    // overflow-safe version of this formula;
    // size_t separator_len = total_width + max_column_count * 3 + 1;
    size_t separator_len;
    if(!psnip_safe_add(&separator_len, total_width, 1)) { printf("Fout: integer overflow (main.c:%i).\n", __LINE__); return true; }
    size_t tmp;
    if(!psnip_safe_mul(&tmp, max_column_count, 3)) { printf("Fout: integer overflow (main.c:%i).\n", __LINE__); return true; }
    if(!psnip_safe_add(&separator_len, separator_len, tmp)) { printf("Fout: integer overflow (main.c:%i).\n", __LINE__); return true; }

    size_t size;
    if(!psnip_safe_add(&size, separator_len, 1)) { printf("Fout: integer overflow (main.c:%i).\n", __LINE__); return true; }
    char *separator = malloc(size);
    if(separator == NULL) return true;
    separator[0] = '+';
    separator[separator_len - 1] = '+';
    if(separator_len > 2)
    {
        memset(separator + 1, '-', separator_len - 2);
    }
    {
        size_t previous = 0;
        for(size_t i = 0; i < max_column_count; i++)
        {
            size_t max_column_width = max_column_widths[i];
            size_t current;
            if(!psnip_safe_add(&current, previous, max_column_width)) { printf("Fout: integer overflow (main.c:%i).\n", __LINE__); free(separator); return true; }
            if(!psnip_safe_add(&current, current, 3)) { printf("Fout: integer overflow (main.c:%i).\n", __LINE__); free(separator); return true; }
            separator[current] = '+';
            previous = current;
        }
    }
    separator[separator_len] = '\0';
    puts(separator);
    for(size_t i = 0; i < n; i++) // Actually print table
    {
        struct record *record = records + i;
        printf("| ");
        for(size_t j = 0; j < record->column_count; j++)
        {
            printf("%s", record->columns[j]);
            size_t padding = max_column_widths[j] - strlen(record->columns[j]);
            for(size_t k = 0; k < padding; k++) putchar(' ');
            printf(" |");
            if(j != record->column_count - 1) putchar(' ');
        }
        printf("\n");
        puts(separator);
    }
    free(separator);
    return false;
}

struct search_result
{
    struct record *record;
    bool error;
};

static bool ask(char *question)
{
    printf("%s", question);
    printf(" (j/n): ");
    fflush(stdout);
    char *answer;
    while(true)
    {
        answer = fgetline(stdin);
        if(answer == NULL) { printf("Fout: %s", strerror(errno)); exit(EXIT_FAILURE); }
        if(strcmp(answer, "j") != 0 && strcmp(answer, "n") != 0 && strcmp(answer, "ja") != 0 && strcmp(answer, "nee") != 0)
        {
            printf("Ongeldig antwoord '%s'. Voer uw antwoord opnieuw in: ", answer); fflush(stdout);
            free(answer);
            continue;
        }
        else
        {
            break;
        }
    }
    bool retval = (answer[0] == 'j') ? true : false;
    free(answer);
    return retval;
}

static bool ask_scanf(const char *question, const char *format, bool show_format, size_t argcount, ...)
{
    va_list args;
    va_start(args, argcount);

    if (show_format)
    {
        printf("%s: ", question);
    }
    else
    {
        printf("%s (format: %s): ", question, format);
    }
    fflush(stdout);

    while(true)
    {
        clearerr(stdin); // make sure to clear
        int result = vscanf(format, args);
        if ((result == EOF && !ferror(stdin)) || result < argcount)
        {
            printf("Ongeldig antwoord. Voer uw antwoord opnieuw in: ");
            fflush(stdout);
            getchar(); // get rid of newline
            continue;
        }
        else if (result == EOF && ferror(stdin))
        {
          printf("Er is een fout opgetreden: '%s'. Voer uw antwoord opnieuw in: ", strerror(errno));
          fflush(stdout);
          getchar(); // get rid of newline
          continue;
        }
        getchar(); // get rid of newline
        break;
    }

    va_end(args);
    return true;
}

static struct search_result do_manual_search(void)
{
    struct search_result retval;

    while(true)
    {
        upper_loop:
        clearscrn();
        printf("Voer zoekterm in: "); fflush(stdout);
        char *query = fgetline(stdin);
        if(query == NULL) { printf("Fout: %s\n", strerror(errno)); exit(EXIT_FAILURE); }

        const size_t search_results_chunk_size = 128;
        size_t search_results_size = 0;
        size_t search_results_max_size = search_results_chunk_size;

        size_t size;
        if(!psnip_safe_mul(&size, search_results_chunk_size, sizeof(struct record))) { retval.error = true; retval.record = NULL; return retval; }
        struct record *search_results = malloc(size);
        if(search_results == NULL) { retval.error = true; retval.record = NULL; return retval; }

        size_t size2;
        if(!psnip_safe_mul(&size2, search_results_chunk_size, sizeof(struct record *))) { free(search_results); retval.error = true; retval.record = NULL; return retval; }
        struct record **search_results_originals = malloc(size2);
        if(search_results_originals == NULL) { free(search_results); retval.error = true; retval.record = NULL; return retval; }

        search_results[0].column_count = header.column_count + 1;
        size_t size3;
        if(!psnip_safe_add(&size3, header.column_count, 1)) { free(search_results_originals); free(search_results); retval.error = true; retval.record = NULL; return retval; }
        if(!psnip_safe_mul(&size3, size3, sizeof(char *))) { free(search_results_originals); free(search_results); retval.error = true; retval.record = NULL; return retval; }
        search_results[0].columns = malloc(size3);
        if(search_results[0].columns == NULL) { printf("Fout: kon geen extra geheugen-ruimte aanvragen. (%s) (main.c:%i)\n", strerror(errno), __LINE__); exit(EXIT_FAILURE); }
        for(size_t i = 0; i < header.column_count; i++) search_results[0].columns[i + 1] = header.columns[i];
        search_results[0].columns[0] = "Keuzenummer";
        search_results_size++; // That's integer-overflow safe

        for(size_t i = 0; i < records_size; i++)
        {
            struct record *record = records + i;
            for(size_t j = 0; j < record->column_count; j++)
            {
                const char *substr = strcasestr(record->columns[j], query);
                if(substr != NULL) // Found a record which matches, add it to the search results and break to outer loop.
                {
                    if(search_results_size + 1 == SIZE_MAX - 1) goto quit_loops;
                    if(search_results_size == search_results_max_size) // Grow it first
                    {
                        size_t size4;
                        if(!psnip_safe_add(&size4, search_results_max_size, search_results_chunk_size)) { free(search_results_originals); free(search_results); retval.error = true; retval.record = NULL; return retval; }
                        if(!psnip_safe_mul(&size4, size4, sizeof(struct record))) { free(search_results_originals); free(search_results); retval.error = true; retval.record = NULL; return retval; }
                        struct record *tmp = realloc(search_results, size4);
                        if(tmp == NULL) { free(search_results_originals); free(search_results); retval.error = true; retval.record = NULL; return retval; }
                        search_results = tmp;

                        size_t size5;
                        if(!psnip_safe_add(&size5, search_results_max_size, search_results_chunk_size)) { free(search_results_originals); free(search_results); retval.error = true; retval.record = NULL; return retval; }
                        if(!psnip_safe_mul(&size5, size5, sizeof(struct record *))) { free(search_results_originals); free(search_results); retval.error = true; retval.record = NULL; return retval; }
                        struct record **tmp_search_results_originals = realloc(search_results_originals, size5);
                        if(tmp_search_results_originals == NULL) { free(search_results_originals); free(search_results); retval.error = true; retval.record = NULL; return retval; }
                        search_results_originals = tmp_search_results_originals;

                        search_results_max_size += search_results_chunk_size; // Integer-overflow safe.
                    }


                    // Create copy of record
                    struct record *search_record = search_results + search_results_size;
                    search_record->column_count = record->column_count;
                    size_t size6;
                    if(!psnip_safe_add(&size6, record->column_count, 1)) { free(search_results_originals); free(search_results); retval.error = true; retval.record = NULL; return retval; }
                    if(!psnip_safe_mul(&size6, size6, sizeof(char *))) { free(search_results_originals); free(search_results); retval.error = true; retval.record = NULL; return retval; }
                    search_record->columns = malloc(size6);
                    if(search_record->columns == NULL) { printf("Fout: kon geen extra geheugen-ruimte aanvragen. (%s) (main.c:%i)\n", strerror(errno), __LINE__); exit(EXIT_FAILURE); }
                    for(size_t k = 0; k < record->column_count; k++) search_record->columns[k + 1] = record->columns[k];


                    // Insert number into first column of new copy of record.
                    char tmp;
                    // blame bloody Macrosuft for the following abomination, they're still stuck in 1989.
                    #if defined(_WIN32)
                        int required_size = snprintf(NULL, 0, "%Iu", search_results_size);
                    #else
                        int required_size = snprintf(&tmp, 1, "%zu", search_results_size);
                    #endif
                    if(required_size < 0) { printf("Fout: error returned by snprintf\n", strerror(errno)); exit(EXIT_FAILURE); }
                    if((unsigned int) required_size > SIZE_MAX - 1) { printf("Fout: required_size is groter dan SIZE_MAX - 1\n"); exit(EXIT_FAILURE); }
                    char *buf = malloc(required_size + 1);
                    if(buf == NULL) { printf("Fout: kon geen extra geheugen-ruimte aanvragen. (%s) (main.c:%i)\n", strerror(errno), __LINE__); exit(EXIT_FAILURE); }
                    // blame bloody Macrosuft for the following abomination, they're still stuck in 1989.
                    #if defined(_WIN32)
                        if(sprintf(buf, "%Iu", search_results_size) < 0) { printf("Fout: error returned by sprintf\n"); exit(EXIT_FAILURE); }
                    #else
                        if(sprintf(buf, "%zu", search_results_size) < 0) { printf("Fout: error returned by sprintf\n"); exit(EXIT_FAILURE); }
                    #endif
                    search_record->columns[0] = buf;
                    // same as: search_record->column_count++;
                    if(!psnip_safe_add(&(search_record->column_count), search_record->column_count, 1)) { free(buf); free(search_results_originals); free(search_results); retval.error = true; retval.record = NULL; return retval; }
                    search_results_originals[search_results_size - 1] = record;
                    // same as: search_results_size++;
                    if(!psnip_safe_add(&search_results_size, search_results_size, 1))
                    {
                        free(buf);
                        free(search_results_originals);
                        free(search_results);
                        retval.error = true;
                        retval.record = NULL;
                        return retval;
                    }
                    break;
                }
            }
        }
        quit_loops:
        if(search_results_size == 1) // the first is the header
        {
            clearscrn();
            printf("Geen resultaten gevonden. "); // no newline on purpose
            if(ask("Wilt u opnieuw zoeken?"))
            {
                continue;
            }
            else
            {
                retval.record = NULL;
                retval.error = false;
                return retval;
            }
        }

        size_t index;
        clearscrn();
        while(true) // Ask number from user
        {
            printf("Resultaten voor \"%s\":\n", query);
            print_table(search_results, search_results_size);
            printf("Kies een keuzenummer, druk op enter om opnieuw te zoeken, of voer 0 in om te stoppen met handmatig zoeken: "); fflush(stdout);
            char *num = fgetline(stdin);
            if(num == NULL)
            {
                printf("Fout: %s", strerror(errno));
                retval.record = NULL;
                retval.error = false;
                free(num);
                free(query);
                for(size_t i = 1; i < search_results_size; i++) free(search_results[i].columns[0]); // Skip the first because that is a string literal.
                free(search_results);
                free(search_results_originals);
                return retval;
            }
            if(*num == '\0') goto upper_loop;
            if(num[0] == '0' && num[1] == '\0')
            {
                retval.record = NULL;
                retval.error = false;
                free(num);
                free(query);
                for(size_t i = 1; i < search_results_size; i++) free(search_results[i].columns[0]); // Skip the first because that is a string literal.
                free(search_results);
                free(search_results_originals);
                return retval;
            }
            // TODO don't use atoll
            long long llindex = atoll(num); // This index starts at 1 because we skip the header
            free(num);
            if(llindex <= 0 || (unsigned long long) llindex > search_results_size-1) { clearscrn(); printf("Fout: ongeldig nummer %lld\n", llindex); continue; }
            if((unsigned long long) llindex > SIZE_MAX) { printf("Technische fout: nummer past niet in size_t."); exit(EXIT_FAILURE); }
            index = (size_t) llindex;
            break;
        }
        retval.record = search_results_originals[index - 1];
        retval.error = false;

        free(query);
        for(size_t i = 1; i < search_results_size; i++) free(search_results[i].columns[0]); // Skip the first because that is a string literal.
        free(search_results);
        free(search_results_originals);
        return retval;
    }
}

static struct search_result do_barcode_search(char *barcode)
{
    struct record *record = NULL;
    for(size_t i = 0; i < records_size; i++)
    {
        struct record *current_record = records + i;
        if(current_record->column_count < barcode_column_index) continue;
        if(strcmp(current_record->columns[barcode_column_index], barcode) == 0)
        {
            record = records + i;
            break;
        }
    }
    struct search_result retval;
    retval.record = record;
    retval.error = false;
    return retval;
}

void at_exit_callback(void)
{
    printf("Druk op enter om het programma te sluiten..\n");
    getchar();
}

int main(void)
{
    atexit(at_exit_callback);
    clearscrn_true();
    print_welcome();
    printf("Voer lijstscheidingsteken in (meestal een komma of puntkomma): "); fflush(stdout);
    delim = fgetc(stdin);
    if(delim == EOF) { printf("Fout.\n"); exit(EXIT_FAILURE); }
    fgetc(stdin);


    FILE *infile;
    char *msg1 = "Voer pad naar CSV bestand in (bijvoorbeeld: C:\\Users\\Jan\\Desktop\\artikelen.csv): ";
    printf("%s", msg1); fflush(stdout);
    char *inpath;
    while (true)
    {
        inpath = fgetline(stdin);
        if(inpath == NULL) { printf("Fout: %s\n", strerror(errno)); exit(EXIT_FAILURE); }

        infile = fopen(inpath, "r+");
        if(infile == NULL)
        {
            clearscrn();
            printf("Fout: kon bestand niet openen. (%s)\n", strerror(errno));
            printf("%s", msg1); fflush(stdout);
            free(inpath);
        }
        else
        {
            break;
        }
    }
    fseek(infile, 0, SEEK_END);
    long fsize = ftell(infile);
    if(fsize == -1) { printf("Fout: %s\n", strerror(errno)); exit(EXIT_FAILURE); }
    if(fsize < 0) { printf("Fout: invalid value for fsize.\n"); exit(EXIT_FAILURE); }
    fseek(infile, 0, SEEK_SET);
    if((unsigned long) fsize > SIZE_MAX) { printf("Fout: value exceeded SIZE_MAX, could not store variable.\n"); exit(EXIT_FAILURE); }
    char *buf = malloc(fsize);
    if(buf == NULL) { printf("Fout: kon geen extra geheugen-ruimte aanvragen. (%s) (main.c:%i)\n", strerror(errno), __LINE__); }
    size_t buf_used = fread(buf, 1, fsize, infile);
    if(buf_used == 0) { printf("Fout: kon data niet lezen uit bestand.\n"); exit(EXIT_FAILURE); }
    fclose(infile);


    char *outpath;
    FILE *outfile;
    if(ask("Wilt u het bijgewerkte bestand in een nieuw bestand opslaan?\n  Dit kan veiliger zijn i.v.m. gegevensverlies terwijl de wijzigingen worden opgeslagen."))
    {
        char *msg2 = "Voer pad naar CSV bestand voor wijzigingen in (bijvoorbeeld: C:\\Users\\Jan\\Desktop\\bijgewerkt.csv): ";
        printf("%s", msg2); fflush(stdout);
        while (true)
        {
            outpath = fgetline(stdin);
            if (outpath == NULL) { printf("Fout: %s\n", strerror(errno)); exit(EXIT_FAILURE); }
            if (strcmp(outpath, inpath) == 0) { free(outpath); outpath = inpath; break; }
            outfile = fopen(outpath, "w");
            if (outfile == NULL)
            {
                clearscrn();
                printf("Fout: kon bestand niet openen. (%s)\n", strerror(errno));
                printf("%s", msg2); fflush(stdout);
                free(outpath);
            }
            else
            {
                break;
            }
        }
    }
    else
    {
        outpath = inpath;
    }

    clearscrn();

    printf("CSV bestand inladen..\n");
    size_t size;
    if(!psnip_safe_mul(&size, RECORDS_CHUNK_SIZE, sizeof(struct record))) { printf("Fout: integer overflow\n"); exit(EXIT_FAILURE); }
    records = malloc(size);
    if(records == NULL) { printf("Fout: kon geen extra geheugen-ruimte aanvragen. (%s) (main.c:%i)\n", strerror(errno), __LINE__); exit(EXIT_FAILURE); }
    records_max_size = RECORDS_CHUNK_SIZE;
    for(size_t i = 0; i < records_max_size; i++) records[i].column_count = 0;

    struct csv_parser parser;
    if(csv_init(&parser, 0) != 0) { printf("Fout: kon parser niet initialiseren.\n"); free(records); exit(EXIT_FAILURE); }
    csv_set_delim(&parser, delim);
    size_t bytes_processed = csv_parse(&parser, buf, buf_used, end_of_field_callback, end_of_record_callback, NULL); // record is the line, field is an entry
    if(bytes_processed < buf_used) { printf("Fout: fout tijdens het lezen van CSV bestand. (%s)\n", csv_strerror(csv_error(&parser))); free(records); exit(EXIT_FAILURE); }
    csv_fini(&parser, end_of_field_callback, end_of_record_callback, NULL); // TODO do we want both callbacks to be called here?

    clearscrn();

    size_t preview_length = 5; // preview_length may not be bigger than SIZE_MAX - 2
    struct record choose_columns_table[preview_length + 3];
    choose_columns_table[0].column_count = header.column_count;
    char *choose_columns_table_header_columns[header.column_count];
    choose_columns_table[0].columns = choose_columns_table_header_columns;

    // Add column numbers header
    for (size_t i = 0; i < choose_columns_table[0].column_count; i++)
    {
        size_t human_index; // Starts at 1 instead of 0
        if(!psnip_safe_add(&human_index, i, 1)) { printf("Fout: integer overflow. (main.c:%i)\n", __LINE__); exit(EXIT_FAILURE); }

        char tmp;
        // blame bloody Macrosuft for the following abomination, they're still stuck in 1989.
        #if defined(_WIN32)
            int required_size = snprintf(NULL, 0, "%Iu", human_index);
        #else
            int required_size = snprintf(&tmp, 1, "%zu", human_index);
        #endif
        if(required_size < 0) { printf("Fout: error returned by snprintf\n", strerror(errno)); exit(EXIT_FAILURE); }
        if((unsigned int) required_size > SIZE_MAX - 1) { printf("Fout: required_size is groter dan SIZE_MAX - 1\n"); exit(EXIT_FAILURE); }
        char *buf = malloc(required_size + 1);
        if(buf == NULL) { printf("Fout: kon geen extra geheugen-ruimte aanvragen. (%s) (main.c:%i)\n", strerror(errno), __LINE__); exit(EXIT_FAILURE); }
        // blame bloody Macrosuft for the following abomination, they're still stuck in 1989.
        #if defined(_WIN32)
            if(sprintf(buf, "%Iu", human_index) < 0) { printf("Fout: error returned by sprintf\n"); exit(EXIT_FAILURE); }
        #else
            if(sprintf(buf, "%zu", human_index) < 0) { printf("Fout: error returned by sprintf\n"); exit(EXIT_FAILURE); }
        #endif
        choose_columns_table[0].columns[i] = buf; // buf contains the string representation of the human index.
    }
    size_t max_choose_columns_table_col_count = 0;
    choose_columns_table[1] = header; // Second row becomes the header
    for (size_t i = 0; i < preview_length; i++) // Remaining preview_length rows become preview rows
    {
        choose_columns_table[i + 2] = records[i];
        if (max_choose_columns_table_col_count < records[i].column_count) max_choose_columns_table_col_count = records[i].column_count;
    }

    // Produce a row containing only dots in each cell as the final row.
    struct record sniff_and_so_on;
    sniff_and_so_on.column_count = max_choose_columns_table_col_count;

    char *sniff_and_so_on_columns[sniff_and_so_on.column_count];
    for (size_t i = 0; i < sniff_and_so_on.column_count; i++)
    {
        sniff_and_so_on_columns[i] = "...";
    }
    sniff_and_so_on.columns = sniff_and_so_on_columns;
    choose_columns_table[preview_length + 2] = sniff_and_so_on;


    // blame bloody Macrosuft for the following abomination, they're still stuck in 1989.
    #if defined(_WIN32)
        char *human_index_format = "%Iu";
    #else
        char *human_index_format = "%zu";
    #endif

    print_table(choose_columns_table, preview_length + 3);
    printf("\nAls deze voorbeeld tabel er vreemd uit ziet, kan het zijn dat u het verkeerde lijstscheidingsteken heeft ingevoerd.\n"
            "Sluit dan het programma en start het opnieuw om een ander lijstscheidingsteken te proberen.\n\n");


    // Select barcode column index
    while (true)
    {
        size_t chosen_human_index;
        if(!ask_scanf("Voer keuzenummer van de barcode-kolom in", human_index_format, true, 1, &chosen_human_index))
        {
            printf("ask_scanf error.\n");
            exit(EXIT_FAILURE);
        }
        if (chosen_human_index == 0 || chosen_human_index > header.column_count)
        {
            printf("Ongeldig keuzenummer '%zu'. Voer uw antwoord opnieuw in.\n", chosen_human_index);
            continue;
        }

        barcode_column_index = chosen_human_index - 1;
        break;
    }

    // Select amount column index
    while (true)
    {
        size_t chosen_human_index;
        if(!ask_scanf("Voer keuzenummer van de aantal/voorraad-kolom in", human_index_format, true, 1, &chosen_human_index))
        {
            printf("ask_scanf error.\n");
            exit(EXIT_FAILURE);
        }
        if (chosen_human_index == 0 || chosen_human_index > header.column_count)
        {
            printf("Ongeldig keuzenummer '%zu'. Voer uw antwoord opnieuw in.\n", chosen_human_index);
            continue;
        }

        amount_column_index = chosen_human_index - 1;
        break;
    }

    for(size_t i = 0; i < choose_columns_table[0].column_count; i++)
    {
      free(choose_columns_table[0].columns[i]);
    }
    clearscrn();

    while(true)
    {
        bool save_error = false;

        clearscrn();
        if(save_error) { printf("Fout: kon bestand niet opslaan. (%s ?)\n", strerror(errno)); save_error = false; }
        printf("Voer barcode in (druk op enter om meteen handmatig te zoeken):\a "); fflush(stdout);
        char *barcode = fgetline(stdin);
        struct search_result result;
        struct record *record;
        if(barcode == NULL)
        {
            printf("Fout: %s\n", strerror(errno));
            continue;
        }
        else if(barcode[0] == '\0')
        {
            result = do_manual_search();
            if(result.error) { printf("Fout: %s", strerror(errno)); free(barcode); continue; }
            record = result.record;
            if(record == NULL) { free(barcode); continue; }
        }
        else
        {
            result = do_barcode_search(barcode);
            if(result.error) { printf("Fout: %s", strerror(errno)); free(barcode); continue; }
            record = result.record;
            if(record == NULL)
            {
                clearscrn();
                printf("Kon geen product met barcode %s vinden. ", barcode); // no newline and purpose
                if(!ask("Wilt u handmatig zoeken?")) { free(barcode); continue; }
                result = do_manual_search();
                if(result.error) { printf("Fout: %s", strerror(errno)); free(barcode); continue; }
                record = result.record;
                if(record == NULL) { free(barcode); continue; }
            }
        }
        free(barcode);

        clearscrn();
        printf("Dit product is gevonden:\n");
        struct record table_records[2];
        table_records[0] = header;
        table_records[1] = *record;
        print_table(table_records, 2);

        printf("Voer aantal in (of druk op enter om niks te veranderen en opnieuw te zoeken): "); fflush(stdout);
        char *amount = fgetline(stdin);
        if(amount == NULL) { printf("Fout: kon ingevoerd aantal niet lezen (%s). Kon aantal hierdoor niet opslaan.\n", strerror(errno)); continue; }
        if(*amount == '\0') continue;
        free(record->columns[amount_column_index]);
        record->columns[amount_column_index] = amount;
        to_free_add(amount);
        if(!save(&parser, records, records_size, outpath)) save_error = true;
    }
    // TODO free all columns in records, remove to_free construct as it is no longer needed
    csv_free(&parser);
    to_free_free();
    if (outpath != inpath) free(outpath);
    free(inpath);
    return EXIT_SUCCESS;
}
