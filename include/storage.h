#pragma once

void storage_init(void);
int storage_create_entry(const char* path, char type, const char* content);
int storage_find_entry(const char* path);
int storage_remove_entry(const char* path);
int storage_get_entry_count(void);
const char* storage_get_entry_name(int index);
const char* storage_get_entry_path(int index);
char storage_get_entry_type(int index);
const char* storage_get_entry_content(int index);