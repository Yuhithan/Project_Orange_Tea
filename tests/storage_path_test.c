#include <assert.h>
#include <stdio.h>
#include "storage.h"

int main(void)
{
    storage_init();

    int created = storage_create_entry("/docs/guide", 'f', "hello");
    assert(created == 1);

    int index = storage_find_entry("/docs/guide");
    assert(index >= 0);
    assert(storage_get_entry_type(index) == 'f');
    assert(storage_get_entry_name(index)[0] == 'g');
    assert(storage_get_entry_name(index)[1] == 'u');
    assert(storage_get_entry_name(index)[2] == 'i');
    assert(storage_get_entry_name(index)[3] == 'd');
    assert(storage_get_entry_name(index)[4] == 'e');
    assert(storage_get_entry_name(index)[5] == '\0');

    const char* path = storage_get_entry_path(index);
    assert(path[0] == '/');
    assert(path[1] == 'd');
    assert(path[2] == 'o');
    assert(path[3] == 'c');
    assert(path[4] == 's');
    assert(path[5] == '/');
    assert(path[6] == 'g');
    assert(path[7] == 'u');
    assert(path[8] == 'i');
    assert(path[9] == 'd');
    assert(path[10] == 'e');
    assert(path[11] == '\0');

    puts("storage path test passed");
    return 0;
}
