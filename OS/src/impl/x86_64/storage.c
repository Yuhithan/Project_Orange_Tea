#include "storage.h"

#define STORAGE_MAX_ENTRIES 32
#define STORAGE_NAME_LEN 16
#define STORAGE_CONTENT_LEN 64

struct storage_entry {
    char name[STORAGE_NAME_LEN];
    char type;
    char content[STORAGE_CONTENT_LEN];
};

static struct storage_entry storage_entries[STORAGE_MAX_ENTRIES];
static int storage_entry_count = 0;

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

static int storage_create_entry_internal(const char* name, char type, const char* content)
{
    if (storage_entry_count >= STORAGE_MAX_ENTRIES)
    {
        return 0;
    }

    int index = storage_entry_count++;
    int i = 0;

    while (name[i] != '\0' && i < STORAGE_NAME_LEN - 1)
    {
        storage_entries[index].name[i] = name[i];
        i++;
    }
    storage_entries[index].name[i] = '\0';
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
        storage_create_entry_internal("bin", 'd', "");
        storage_create_entry_internal("README", 'f', "ORT kernel shell\n");
    }
}

int storage_create_entry(const char* name, char type, const char* content)
{
    return storage_create_entry_internal(name, type, content);
}

int storage_find_entry(const char* name)
{
    for (int i = 0; i < storage_entry_count; i++)
    {
        if (storage_streq(storage_entries[i].name, name))
        {
            return i;
        }
    }

    return -1;
}

int storage_remove_entry(const char* name)
{
    int index = storage_find_entry(name);
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