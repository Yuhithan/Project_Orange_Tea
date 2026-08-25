#include "storage.h"

#define STORAGE_MAX_ENTRIES 32
#define STORAGE_NAME_LEN 16
#define STORAGE_CONTENT_LEN 64
#define STORAGE_PATH_LEN 64

struct storage_entry {
    char path[STORAGE_PATH_LEN];
    char name[STORAGE_NAME_LEN];
    char type;
    char content[STORAGE_CONTENT_LEN];
};

static struct storage_entry storage_entries[STORAGE_MAX_ENTRIES];
static int storage_entry_count = 0;

static int storage_create_entry_internal(const char* path, char type, const char* content);

static int storage_strlen(const char* text)
{
    int len = 0;
    while (text[len] != '\0')
    {
        len++;
    }
    return len;
}

static void storage_copy_string(char* dst, const char* src, int max_len)
{
    int i = 0;
    while (src[i] != '\0' && i < max_len - 1)
    {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

static int storage_streq(const char* a, const char* b)
{
    while (*a != '\0' && *b != '\0')
    {
        if (*a != *b)
        {
            return 0;
        }

        a++;
        b++;
    }

    return *a == *b;
}

static void storage_normalize_path(char* out, const char* path, int max_len)
{
    if (path == 0 || path[0] == '\0')
    {
        out[0] = '/';
        out[1] = '\0';
        return;
    }

    int write = 0;
    if (path[0] != '/')
    {
        if (write < max_len - 1)
        {
            out[write++] = '/';
        }
    }

    for (int i = 0; path[i] != '\0' && write < max_len - 1; i++)
    {
        if (path[i] == '/')
        {
            if (write == 0 || out[write - 1] != '/')
            {
                out[write++] = '/';
            }
        }
        else
        {
            out[write++] = path[i];
        }
    }

    if (write == 0)
    {
        out[0] = '/';
        out[1] = '\0';
        return;
    }

    if (write >= max_len)
    {
        write = max_len - 1;
    }

    out[write] = '\0';
}

static void storage_get_name_from_path(const char* path, char* out, int max_len)
{
    int last_slash = -1;
    int i = 0;
    while (path[i] != '\0')
    {
        if (path[i] == '/')
        {
            last_slash = i;
        }
        i++;
    }

    if (last_slash < 0 || last_slash + 1 >= storage_strlen(path))
    {
        storage_copy_string(out, path, max_len);
        return;
    }

    storage_copy_string(out, path + last_slash + 1, max_len);
}

static int storage_create_parent_directories(const char* path)
{
    if (storage_streq(path, "/"))
    {
        return 1;
    }

    int last_slash = -1;
    for (int i = 0; path[i] != '\0'; i++)
    {
        if (path[i] == '/')
        {
            last_slash = i;
        }
    }

    if (last_slash <= 0)
    {
        return 1;
    }

    char parent[STORAGE_PATH_LEN];
    int parent_len = last_slash;
    if (parent_len >= STORAGE_PATH_LEN)
    {
        parent_len = STORAGE_PATH_LEN - 1;
    }

    for (int i = 0; i < parent_len; i++)
    {
        parent[i] = path[i];
    }
    parent[parent_len] = '\0';

    if (parent[0] == '\0')
    {
        parent[0] = '/';
        parent[1] = '\0';
    }

    int parent_index = storage_find_entry(parent);
    if (parent_index >= 0)
    {
        return storage_entries[parent_index].type == 'd';
    }

    return storage_create_entry_internal(parent, 'd', "");
}

static int storage_create_entry_internal(const char* path, char type, const char* content)
{
    if (storage_entry_count >= STORAGE_MAX_ENTRIES)
    {
        return 0;
    }

    char normalized_path[STORAGE_PATH_LEN];
    storage_normalize_path(normalized_path, path, sizeof(normalized_path));

    if (storage_find_entry(normalized_path) >= 0)
    {
        return 0;
    }

    if (!storage_create_parent_directories(normalized_path))
    {
        return 0;
    }

    int index = storage_entry_count++;
    storage_copy_string(storage_entries[index].path, normalized_path, sizeof(storage_entries[index].path));

    storage_get_name_from_path(normalized_path, storage_entries[index].name, sizeof(storage_entries[index].name));
    storage_entries[index].type = type;

    int j = 0;
    while (content[j] != '\0' && j < STORAGE_CONTENT_LEN - 1)
    {
        storage_entries[index].content[j] = content[j];
        j++;
    }
    storage_entries[index].content[j] = '\0';

    return 1;
}

void storage_init(void)
{
    if (storage_entry_count == 0)
    {
        storage_create_entry_internal("/bin", 'd', "");
        storage_create_entry_internal("/README", 'f', "ORT kernel shell\n");
    }
}

int storage_create_entry(const char* path, char type, const char* content)
{
    return storage_create_entry_internal(path, type, content);
}

int storage_find_entry(const char* path)
{
    char normalized_path[STORAGE_PATH_LEN];
    storage_normalize_path(normalized_path, path, sizeof(normalized_path));

    for (int i = 0; i < storage_entry_count; i++)
    {
        if (storage_streq(storage_entries[i].path, normalized_path))
        {
            return i;
        }
    }

    return -1;
}

int storage_remove_entry(const char* path)
{
    char normalized_path[STORAGE_PATH_LEN];
    storage_normalize_path(normalized_path, path, sizeof(normalized_path));

    int index = storage_find_entry(normalized_path);
    if (index < 0)
    {
        return 0;
    }

    for (int i = index; i + 1 < storage_entry_count; i++)
    {
        storage_entries[i] = storage_entries[i + 1];
    }

    storage_entry_count--;
    return 1;
}

int storage_get_entry_count(void)
{
    return storage_entry_count;
}

const char* storage_get_entry_name(int index)
{
    if (index < 0 || index >= storage_entry_count)
    {
        return "";
    }

    return storage_entries[index].name;
}

const char* storage_get_entry_path(int index)
{
    if (index < 0 || index >= storage_entry_count)
    {
        return "";
    }

    return storage_entries[index].path;
}

char storage_get_entry_type(int index)
{
    if (index < 0 || index >= storage_entry_count)
    {
        return '\0';
    }

    return storage_entries[index].type;
}

const char* storage_get_entry_content(int index)
{
    if (index < 0 || index >= storage_entry_count)
    {
        return "";
    }

    return storage_entries[index].content;
}