#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BYTES_PER_LINE 12
#define PATH_BUFFER_SIZE 4096

static const char *get_basename(const char *path)
{
    const char *name = path;
    const char *slash = strrchr(path, '/');
    const char *backslash = strrchr(path, '\\');

    if (slash != NULL && slash + 1 > name) {
        name = slash + 1;
    }
    if (backslash != NULL && backslash + 1 > name) {
        name = backslash + 1;
    }

    return name;
}

static int build_output_path(const char *input_path, char *output_path, size_t output_size)
{
    int written = snprintf(output_path, output_size, "%s.h", input_path);
    return written >= 0 && (size_t)written < output_size;
}

static void build_identifier(const char *input_path, char *identifier, size_t identifier_size)
{
    const char *base = get_basename(input_path);
    size_t out = 0;
    size_t i;

    if (identifier_size == 0) {
        return;
    }

    if (base[0] == '\0') {
        snprintf(identifier, identifier_size, "embedded_data");
        return;
    }

    if (!isalpha((unsigned char)base[0]) && base[0] != '_') {
        identifier[out++] = '_';
    }

    for (i = 0; base[i] != '\0' && out + 1 < identifier_size; ++i) {
        unsigned char ch = (unsigned char)base[i];
        if (isalnum(ch) || ch == '_') {
            identifier[out++] = (char)ch;
        } else {
            identifier[out++] = '_';
        }
    }

    identifier[out] = '\0';

    if (identifier[0] == '\0') {
        snprintf(identifier, identifier_size, "embedded_data");
    }
}

static void build_include_guard(const char *output_path, char *guard, size_t guard_size)
{
    const char *base = get_basename(output_path);
    size_t out = 0;
    size_t i;

    if (guard_size == 0) {
        return;
    }

    for (i = 0; base[i] != '\0' && out + 1 < guard_size; ++i) {
        unsigned char ch = (unsigned char)base[i];
        if (isalnum(ch)) {
            guard[out++] = (char)toupper(ch);
        } else {
            guard[out++] = '_';
        }
    }

    if (out + 3 < guard_size) {
        guard[out++] = '_';
        guard[out++] = 'H';
    }

    guard[out] = '\0';

    if (guard[0] == '\0') {
        snprintf(guard, guard_size, "BIN2C_OUTPUT_H");
    }
}

int main(int argc, char *argv[])
{
    const char *input_path;
    const char *output_path;
    char default_output[PATH_BUFFER_SIZE];
    char identifier[PATH_BUFFER_SIZE];
    char include_guard[PATH_BUFFER_SIZE];
    unsigned char *buffer = NULL;
    long file_size_long;
    size_t file_size;
    size_t bytes_read;
    size_t i;
    FILE *input_file;
    FILE *output_file;

    if (argc != 2 && argc != 3) {
        fprintf(stderr, "Usage: %s <input_file> [output_header]\n", argv[0]);
        return 1;
    }

    input_path = argv[1];
    if (argc == 3) {
        output_path = argv[2];
    } else {
        if (!build_output_path(input_path, default_output, sizeof(default_output))) {
            fprintf(stderr, "%s: output path is too long\n", argv[0]);
            return 1;
        }
        output_path = default_output;
    }

    build_identifier(input_path, identifier, sizeof(identifier));
    build_include_guard(output_path, include_guard, sizeof(include_guard));

    input_file = fopen(input_path, "rb");
    if (input_file == NULL) {
        fprintf(stderr, "%s: cannot open %s for reading\n", argv[0], input_path);
        return 1;
    }

    if (fseek(input_file, 0, SEEK_END) != 0) {
        fprintf(stderr, "%s: cannot seek %s\n", argv[0], input_path);
        fclose(input_file);
        return 1;
    }

    file_size_long = ftell(input_file);
    if (file_size_long < 0) {
        fprintf(stderr, "%s: cannot determine size of %s\n", argv[0], input_path);
        fclose(input_file);
        return 1;
    }

    if (fseek(input_file, 0, SEEK_SET) != 0) {
        fprintf(stderr, "%s: cannot rewind %s\n", argv[0], input_path);
        fclose(input_file);
        return 1;
    }

    file_size = (size_t)file_size_long;
    if (file_size > 0) {
        buffer = (unsigned char *)malloc(file_size);
        if (buffer == NULL) {
            fprintf(stderr, "%s: out of memory\n", argv[0]);
            fclose(input_file);
            return 1;
        }

        bytes_read = fread(buffer, 1, file_size, input_file);
        if (bytes_read != file_size) {
            fprintf(stderr, "%s: failed to read %s\n", argv[0], input_path);
            free(buffer);
            fclose(input_file);
            return 1;
        }
    }

    fclose(input_file);

    output_file = fopen(output_path, "w");
    if (output_file == NULL) {
        fprintf(stderr, "%s: cannot open %s for writing\n", argv[0], output_path);
        free(buffer);
        return 1;
    }

    fprintf(output_file, "#ifndef %s\n", include_guard);
    fprintf(output_file, "#define %s\n\n", include_guard);
    fprintf(output_file, "static const unsigned int %s_size = %zu;\n", identifier, file_size);
    fprintf(output_file, "static const unsigned char %s[%zu] = {", identifier, file_size);

    for (i = 0; i < file_size; ++i) {
        if ((i % BYTES_PER_LINE) == 0) {
            fprintf(output_file, "\n    ");
        }
        fprintf(output_file, "0x%02X", buffer[i]);
        if (i + 1 != file_size) {
            fprintf(output_file, ", ");
        }
    }

    if (file_size > 0) {
        fprintf(output_file, "\n");
    }

    fprintf(output_file, "};\n\n");
    fprintf(output_file, "#endif\n");

    fclose(output_file);
    free(buffer);
    printf("%s %zu bytes\n", output_path, file_size);
    return 0;
}
