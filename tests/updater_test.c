#define _GNU_SOURCE

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "update.h"

extern char *mkdtemp(char *template_name);

int main(void)
{
    char root[] = "/tmp/ortos-update-XXXXXX";
    char manifest[256], package[256], current[256], backup[256], version[256], error[128];
    FILE *file;
    assert(mkdtemp(root) != NULL);
    snprintf(manifest, sizeof(manifest), "%s/update.manifest", root);
    snprintf(package, sizeof(package), "%s/new.bin", root);
    snprintf(current, sizeof(current), "%s/kernel.bin", root);
    snprintf(backup, sizeof(backup), "%s/kernel.bin.previous", root);
    snprintf(version, sizeof(version), "%s/ortos.version", root);
    file = fopen(package, "wb"); assert(file); fputs("updated kernel\n", file); fclose(file);
    file = fopen(current, "wb"); assert(file); fputs("working kernel\n", file); fclose(file);
    file = fopen(version, "w"); assert(file); fputs("1.0.0\n", file); fclose(file);
    file = fopen(manifest, "w"); assert(file);
    fprintf(file, "version=1.1.0\narchitecture=x86_64\nmin_version=1.0.0\npackage=new.bin\nsize=15\nsha256=5049b575f2db7ab9b09f7f6f518318c2c59c145f6332012f52bd4c9c7867479c\n");
    fclose(file);
    assert(ortos_update_apply(manifest, root, error, sizeof(error)) == 0);
    file = fopen(current, "rb"); assert(file); char content[32] = {0}; fread(content, 1, sizeof(content) - 1, file); fclose(file);
    assert(strcmp(content, "updated kernel\n") == 0);
    file = fopen(backup, "rb"); assert(file); memset(content, 0, sizeof(content)); fread(content, 1, sizeof(content) - 1, file); fclose(file);
    assert(strcmp(content, "working kernel\n") == 0);
    remove(manifest); remove(package); remove(current); remove(backup); remove(version); rmdir(root);
    return 0;
}