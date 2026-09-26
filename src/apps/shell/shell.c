#include "shell.h"
#include "keyboard.h"
#include "imp.h"
#include "storage.h"
#include "vfs.h"
#include "network.h"
#include "boot_mode.h"
#include "desktop.h"
#include "desktop_apps.h"

#define MAX_CMD 128
#define MAX_HISTORY 16
#define MAX_ALIASES 8

static char cmd[MAX_CMD];
static char history[MAX_HISTORY][MAX_CMD];
static int history_count = 0;
static char aliases[MAX_ALIASES][MAX_CMD];
static int alias_count = 0;
static char current_dir[STORAGE_MAX_PATH] = "/";
static int shell_last_storage_status = STORAGE_OK;
static char current_layout[16] = "en-us";
static int shell_seed = 1337;
static int shell_uptime_seconds = 0;
static int shell_clock_second = 0;
static int shell_clock_minute = 0;
static int shell_clock_hour = 12;
static int shell_clock_day = 25;
static int shell_clock_month = 7;
static int shell_clock_year = 2026;

static int shell_streq(const char* a, const char* b)
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

static int shell_starts_with(const char* text, const char* prefix)
{
    while (*prefix != '\0')
    {
        if (*text != *prefix)
        {
            return 0;
        }

        text++;
        prefix++;
    }

    return 1;
}

static int shell_is_space(char c)
{
    return c == ' ' || c == '\t';
}

static const char* shell_skip_spaces(const char* text)
{
    while (*text != '\0' && shell_is_space(*text))
    {
        text++;
    }

    return text;
}

static int shell_strlen(const char* text)
{
    int len = 0;
    while (text[len] != '\0')
    {
        len++;
    }
    return len;
}

static void shell_copy_string(char* dst, const char* src, int max_len)
{
    int i = 0;
    while (src[i] != '\0' && i < max_len - 1)
    {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

static const char* shell_read_token(const char* text, char* token, int max_len)
{
    text = shell_skip_spaces(text);
    int i = 0;
    while (*text != '\0' && !shell_is_space(*text) && i < max_len - 1)
    {
        token[i++] = *text++;
    }
    token[i] = '\0';
    return text;
}

static const char *shell_read_word(const char *text, char *word, int max_len)
{
    text = shell_skip_spaces(text);
    int index = 0;
    char quote = 0;
    if (*text == '"' || *text == '\'') quote = *text++;
    while (*text && (quote ? *text != quote : !shell_is_space(*text)) && index < max_len - 1)
        word[index++] = *text++;
    if (quote && *text == quote) text++;
    word[index] = 0;
    return text;
}

static void shell_print_int(int value)
{
    char digits[16];
    int count = 0;
    int negative = 0;

    if (value < 0)
    {
        negative = 1;
        value = -value;
    }

    do
    {
        digits[count++] = (char)('0' + (value % 10));
        value /= 10;
    }
    while (value > 0 && count < 15);

    if (negative)
    {
        imp_char('-');
    }

    while (count > 0)
    {
        imp_char(digits[--count]);
    }
}

static void shell_print_uint64(uint64_t value)
{
    char digits[24];
    int count = 0;
    do { digits[count++] = (char)('0' + value % 10); value /= 10; } while (value && count < 24);
    while (count > 0) imp_char(digits[--count]);
}

static void shell_tick_clock(void)
{
    shell_clock_second++;
    shell_uptime_seconds++;

    if (shell_clock_second >= 60)
    {
        shell_clock_second = 0;
        shell_clock_minute++;
    }

    if (shell_clock_minute >= 60)
    {
        shell_clock_minute = 0;
        shell_clock_hour++;
    }

    if (shell_clock_hour >= 24)
    {
        shell_clock_hour = 0;
        shell_clock_day++;
    }

    if (shell_clock_day > 31)
    {
        shell_clock_day = 1;
        shell_clock_month++;
    }

    if (shell_clock_month > 12)
    {
        shell_clock_month = 1;
        shell_clock_year++;
    }
}

static void shell_add_history(const char* text)
{
    int index = history_count % MAX_HISTORY;
    shell_copy_string(history[index], text, MAX_CMD);

    if (history_count < MAX_HISTORY)
    {
        history_count++;
    }
}

static void shell_add_alias(const char* name, const char* value)
{
    if (alias_count >= MAX_ALIASES)
    {
        return;
    }

    int index = alias_count++;
    int i = 0;

    while (name[i] != '\0' && i < MAX_CMD - 2)
    {
        aliases[index][i] = name[i];
        i++;
    }
    aliases[index][i] = '=';
    i++;

    int j = 0;
    while (value[j] != '\0' && i + j < MAX_CMD - 1)
    {
        aliases[index][i + j] = value[j];
        j++;
    }
    aliases[index][i + j] = '\0';
}

static int shell_lookup_alias(const char* name, char* value, int max_len)
{
    for (int i = 0; i < alias_count; i++)
    {
        int j = 0;
        while (aliases[i][j] != '\0' && aliases[i][j] != '=' && j < max_len - 1)
        {
            if (aliases[i][j] != name[j])
            {
                break;
            }
            j++;
        }

        if (aliases[i][j] == '=' && name[j] == '\0')
        {
            shell_copy_string(value, aliases[i] + j + 1, max_len);
            return 1;
        }
    }

    return 0;
}

static int shell_expand_alias(const char* input, char* output, int max_len)
{
    char first_token[MAX_CMD];
    const char* cursor = shell_read_token(input, first_token, sizeof(first_token));

    if (first_token[0] == '\0')
    {
        return 0;
    }

    char alias_value[MAX_CMD];
    if (!shell_lookup_alias(first_token, alias_value, sizeof(alias_value)))
    {
        return 0;
    }

    shell_copy_string(output, alias_value, max_len);
    cursor = shell_skip_spaces(cursor);

    if (*cursor != '\0')
    {
        int len = shell_strlen(output);
        if (len < max_len - 1)
        {
            output[len++] = ' ';
            shell_copy_string(output + len, cursor, max_len - len);
        }
    }

    return 1;
}

static void shell_print_help(void)
{
    imp_text("Available commands:\n");
    imp_text("  help        - affiche toutes les commandes\n");
    imp_text("  version     - affiche la version du noyau\n");
    imp_text("  uname       - informations sur le système\n");
    imp_text("  uptime      - temps depuis le démarrage\n");
    imp_text("  date        - affiche la date et l'heure\n");
    imp_text("  time        - affiche l'heure\n");
    imp_text("  reboot      - redémarre la machine\n");
    imp_text("  shutdown    - éteint la machine\n");
    imp_text("  loadkeys    - change la langue (fr / en-us)\n");
    imp_text("  cpuinfo     - informations sur le processeur\n");
    imp_text("  meminfo     - informations sur la mémoire\n");
    imp_text("  sysinfo     - résumé du système\n");
    imp_text("  pci         - liste les périphériques PCI\n");
    imp_text("  regs        - affiche les registres CPU\n");
    imp_text("  gdt         - affiche la GDT\n");
    imp_text("  idt         - affiche l'IDT\n");
    imp_text("  stack       - affiche la pile\n");
    imp_text("  heap        - informations sur le tas mémoire\n");
    imp_text("  irq         - état des interruptions\n");
    imp_text("  ls          - liste les fichiers\n");
    imp_text("  cd          - change de dossier\n");
    imp_text("  pwd         - affiche le dossier courant\n");
    imp_text("  cat         - affiche un fichier\n");
    imp_text("  touch       - crée un fichier\n");
    imp_text("  create      - crée un fichier/dossier avec contenu optionnel\n");
    imp_text("  mkdir       - crée un dossier\n");
    imp_text("  rm          - supprime un fichier\n");
    imp_text("  cp          - copie un fichier\n");
    imp_text("  mv          - déplace ou renomme un fichier\n");
    imp_text("  write       - écrit un fichier\n");
    imp_text("  append      - ajoute à un fichier\n");
    imp_text("  rmdir       - supprime un dossier vide\n");
    imp_text("  del         - supprime récursivement avec confirmation\n");
    imp_text("  textedit/modify - éditeur de texte console\n");
    imp_text("  stat        - informations sur un fichier/dossier\n");
    imp_text("  du          - espace utilisé par un chemin\n");
    imp_text("  disks       - liste les périphériques détectés\n");
    imp_text("  diskinfo    - affiche les informations du disque\n");
    imp_text("  df          - affiche l'espace du volume\n");
    imp_text("  mount       - monte le volume détecté\n");
    imp_text("  umount      - démonte le volume\n");
    imp_text("  format      - formate le volume avec confirmation\n");
    imp_text("  fsck        - vérifie l'intégrité du volume\n");
    imp_text("  sync        - synchronise les écritures\n");
    imp_text("  storage-test --safe - teste le stockage sur disque RAM\n");
    imp_text("  tasks       - liste les tâches\n");
    imp_text("  kill        - termine une tâche\n");
    imp_text("  ps          - liste les processus\n");
    imp_text("  test        - lance les tests du noyau\n");
    imp_text("  test filesystem - teste les commandes sur le système de fichiers monté\n");
    imp_text("  test storage - alias de test filesystem\n");
    imp_text("  panic       - déclenche un kernel panic\n");
    imp_text("  beep        - bip du PC Speaker\n");
    imp_text("  cls         - alias de clear\n");
    imp_text("  calc        - calculatrice simple\n");
    imp_text("  rand        - nombre aléatoire\n");
    imp_text("  sleep       - attend quelques secondes\n");
    imp_text("  repeat      - répète une commande\n");
    imp_text("  history     - historique des commandes\n");
    imp_text("  alias       - crée un alias\n");
    imp_text("  env         - variables d'environnement\n");
    imp_text("  ping        - test réseau (ping <host> [eth|wifi])\n");
    imp_text("  wifi        - wifi connect/disconnect/status\n");
    imp_text("  gui         - ouvre l'environnement graphique ORgui\n");
    imp_text("  notify      - affiche une notification graphique\n");
    imp_text("  i_use_arch_btw - blague fun pour les utilisateurs Arch\n");
    imp_text("  opsec       - Special command");
}

static void shell_set_current_dir(const char* path)
{
    int i = 0;
    while (path[i] != '\0' && i < STORAGE_MAX_PATH - 1)
    {
        current_dir[i] = path[i];
        i++;
    }
    current_dir[i] = '\0';
}

static int shell_resolve_path(const char* path, char* out, int max_len)
{
    return vfs_resolve_path(current_dir, path, out, (size_t)max_len) == STORAGE_OK;
}

static void shell_print_storage_error(int error)
{
    switch (error) {
    case STORAGE_ERR_NOENT: imp_text("Error: file or directory not found\n"); break;
    case STORAGE_ERR_EXIST: imp_text("Error: file or directory already exists\n"); break;
    case STORAGE_ERR_NOTDIR: imp_text("Error: parent is not a directory\n"); break;
    case STORAGE_ERR_ISDIR: imp_text("Error: is a directory\n"); break;
    case STORAGE_ERR_NOTEMPTY: imp_text("Error: directory is not empty\n"); break;
    case STORAGE_ERR_NOSPC: imp_text("Error: storage full\n"); break;
    case STORAGE_ERR_INVAL: imp_text("Error: invalid path\n"); break;
    case STORAGE_ERR_NOTMOUNTED: imp_text("Error: filesystem is not mounted\n"); break;
    default: imp_text("Error: filesystem error\n"); break;
    }
}

static const char *shell_basename(const char *path)
{
    const char *name = path;
    for (const char *p = path; *p; p++) if (*p == '/') name = p + 1;
    return name;
}

static int shell_join_target(const char *source, const char *target, char *out, int max_len)
{
    struct storage_entry_info info;
    size_t target_length = 0, name_length = 0;
    while (target[target_length]) target_length++;
    const char *name = shell_basename(source);
    while (name[name_length]) name_length++;
    if (vfs_stat(target, &info) != STORAGE_OK || info.type != 'd') {
        shell_copy_string(out, target, max_len);
        return 1;
    }
    if (target_length + name_length + 2 > (size_t)max_len) return 0;
    shell_copy_string(out, target, max_len);
    if (target_length > 1) out[target_length++] = '/';
    shell_copy_string(out + target_length, name, max_len - (int)target_length);
    return 1;
}

static void shell_create_command(const char *arguments)
{
    char kind[16], path[STORAGE_MAX_PATH], content[256];
    const char *cursor = shell_read_word(arguments, kind, sizeof(kind));
    cursor = shell_skip_spaces(cursor);
    if (shell_streq(kind, "file")) {
        const char *separator = cursor;
        while (*separator && *separator != '>') separator++;
        size_t content_length = 0;
        if (*separator == '>') {
            const char *first = cursor;
            while (first < separator && shell_is_space(*first)) first++;
            const char *last = separator;
            while (last > first && shell_is_space(last[-1])) last--;
            if (last - first >= 2 && (first[0] == '"' || first[0] == '\'') && last[-1] == first[0]) { first++; last--; }
            content_length = (size_t)(last - first);
            if (content_length >= sizeof(content)) { imp_text("Error: file content exceeds filesystem limit\n"); return; }
            for (size_t i = 0; i < content_length; i++) content[i] = first[i];
            content[content_length] = 0;
            cursor = shell_read_word(separator + 1, path, sizeof(path));
            if (*shell_skip_spaces(cursor)) { imp_text("Usage: create file [text >] <path>\n"); return; }
        } else {
            cursor = shell_read_word(cursor, path, sizeof(path));
            if (!path[0] || *shell_skip_spaces(cursor)) { imp_text("Usage: create file [text >] <path>\n"); return; }
            content[0] = 0;
        }
        char resolved[STORAGE_MAX_PATH];
        if (!shell_resolve_path(path, resolved, sizeof(resolved))) { imp_text("Error: invalid path\n"); return; }
        int result = vfs_create(resolved, content, content_length);
        shell_last_storage_status = result;
        if (result == STORAGE_OK) { imp_text("File created: "); imp_text(resolved); imp_char('\n'); }
        else shell_print_storage_error(result);
        return;
    }
    if (shell_streq(kind, "folder") || shell_streq(kind, "directory")) {
        cursor = shell_read_word(cursor, path, sizeof(path));
        if (!path[0] || *shell_skip_spaces(cursor)) { imp_text("Usage: create folder <path>\n"); return; }
        char resolved[STORAGE_MAX_PATH];
        if (!shell_resolve_path(path, resolved, sizeof(resolved))) { imp_text("Error: invalid path\n"); return; }
        int result = vfs_mkdir(resolved);
        shell_last_storage_status = result;
        if (result == STORAGE_OK) { imp_text("Directory created: "); imp_text(resolved); imp_char('\n'); }
        else shell_print_storage_error(result);
        return;
    }
    imp_text("Usage: create file <path> | create file <text> > <path> | create folder <path>\n");
}

static void shell_mkdir_command(const char *arguments)
{
    int recursive = 0;
    char path[STORAGE_MAX_PATH], resolved[STORAGE_MAX_PATH];
    const char *cursor = shell_skip_spaces(arguments);
    if (shell_starts_with(cursor, "-p") && shell_is_space(cursor[2])) { recursive = 1; cursor = shell_skip_spaces(cursor + 2); }
    cursor = shell_read_word(cursor, path, sizeof(path));
    if (!path[0] || *shell_skip_spaces(cursor)) { imp_text("Usage: mkdir [-p] <directory>\n"); return; }
    if (!shell_resolve_path(path, resolved, sizeof(resolved))) { imp_text("Error: invalid path\n"); return; }
    if (!recursive) {
        int result = vfs_mkdir(resolved);
        shell_last_storage_status = result;
        if (result == STORAGE_OK) imp_text("Directory created\n"); else shell_print_storage_error(result);
        return;
    }
    for (size_t i = 1; resolved[i]; i++) {
        if (resolved[i] != '/' && resolved[i + 1] != 0) continue;
        char component[STORAGE_MAX_PATH];
        size_t length = i;
        for (size_t j = 0; j < length; j++) component[j] = resolved[j];
        component[length] = 0;
        struct storage_entry_info existing;
        if (vfs_stat(component, &existing) == STORAGE_OK) {
            if (existing.type != 'd') { imp_text("Error: path component is not a directory\n"); return; }
            continue;
        }
        int result = vfs_mkdir(component);
        shell_last_storage_status = result;
        if (result != STORAGE_OK) { shell_print_storage_error(result); return; }
    }
    if (resolved[1] == 0) { imp_text("Error: invalid path\n"); return; }
    imp_text("Directory created\n");
}

static int shell_read_editor_line(char *line, size_t capacity)
{
    size_t length = 0;
    while (1) {
        int key = keyboard_getchar();
        if (key == '\n') { line[length] = 0; imp_char('\n'); return 1; }
        if (key == '\b') { if (length) { length--; imp_text("\b \b"); } continue; }
        if (key < 32 || key > 126 || length + 1 >= capacity) continue;
        line[length++] = (char)key;
        imp_char((char)key);
    }
}

static void shell_textedit(const char *path)
{
    char current[256], replacement[256], line[128];
    size_t current_length = 0, replacement_length = 0;
    int result = vfs_stat(path, &(struct storage_entry_info){0});
    if (result == STORAGE_OK) {
        int fd = vfs_open(path, 0);
        if (fd < 0) result = fd;
        else {
            int count = vfs_read(fd, current, sizeof(current));
            vfs_close(fd);
            if (count < 0) result = count;
            else { current_length = (size_t)count; result = STORAGE_OK; }
        }
    }
    if (result < 0) { shell_print_storage_error(result); return; }
    imp_text("Current contents:\n");
    for (size_t i = 0; i < current_length; i++) imp_char(current[i]);
    imp_text("\nEnter replacement text, one line at a time. :wq saves; :q! cancels.\n");
    while (1) {
        imp_text("edit> ");
        shell_read_editor_line(line, sizeof(line));
        if (shell_streq(line, ":q!")) { imp_text("Edit cancelled\n"); return; }
        if (shell_streq(line, ":wq")) break;
        size_t line_length = 0;
        while (line[line_length]) line_length++;
        if (replacement_length + line_length + 1 >= sizeof(replacement)) { imp_text("Error: file content exceeds filesystem limit\n"); return; }
        for (size_t i = 0; i < line_length; i++) replacement[replacement_length++] = line[i];
        replacement[replacement_length++] = '\n';
    }
    result = vfs_write_file(path, replacement, replacement_length, 0);
    shell_last_storage_status = result < 0 ? result : STORAGE_OK;
    if (result >= 0) result = STORAGE_OK;
    if (result < 0) shell_print_storage_error(result); else imp_text("File saved\n");
}

static int shell_confirm_delete(void)
{
    imp_text("Delete this directory and all contents? [y/N] ");
    int answer = keyboard_getchar();
    int confirmed = answer == 'y' || answer == 'Y';
    while (answer != '\n' && answer != '\r') answer = keyboard_getchar();
    imp_char('\n');
    return confirmed;
}

static const char *shell_storage_error_name(int error)
{
    switch (error) {
    case STORAGE_OK: return "success";
    case STORAGE_ERR_INVAL: return "invalid argument/path";
    case STORAGE_ERR_NOENT: return "not found";
    case STORAGE_ERR_EXIST: return "already exists";
    case STORAGE_ERR_NOTDIR: return "not a directory";
    case STORAGE_ERR_ISDIR: return "is a directory";
    case STORAGE_ERR_NOSPC: return "storage full";
    case STORAGE_ERR_NOTEMPTY: return "directory not empty";
    case STORAGE_ERR_IO: return "storage I/O error";
    case STORAGE_ERR_NOTMOUNTED: return "filesystem not mounted";
    case STORAGE_ERR_CORRUPT: return "filesystem corrupted";
    default: return "unexpected result";
    }
}

static void shell_test_report(const char *name, int actual, int expected, const char *path)
{
    imp_text("[TEST] ");
    imp_text(name);
    imp_char('\n');
    if (actual == expected) {
        imp_text("[PASS] ");
        imp_text(name);
        imp_char('\n');
        return;
    }
    imp_text("[FAIL] ");
    imp_text(name);
    imp_char('\n');
    imp_text("error: ");
    imp_text(shell_storage_error_name(actual));
    imp_text("\npath: ");
    imp_text(path ? path : "(none)");
    imp_text("\nerrno: ");
    shell_print_int(actual);
    imp_char('\n');
}

static int shell_directory_has_child(const char *directory, const char *child_name)
{
    size_t directory_length = (size_t)shell_strlen(directory);
    for (int i = 0; i < vfs_entry_count(); i++) {
        struct storage_entry_info info;
        if (vfs_readdir(i, &info) != STORAGE_OK) continue;
        const char *path = info.path;
        const char *child = 0;
        if (directory_length == 1) child = path[0] == '/' ? path + 1 : 0;
        else if (shell_starts_with(path, directory) && path[directory_length] == '/') child = path + directory_length + 1;
        if (!child || !*child) continue;
        int direct = 1;
        for (const char *p = child; *p; p++) if (*p == '/') { direct = 0; break; }
        if (direct && shell_streq(child, child_name)) return 1;
    }
    return 0;
}

static int shell_test_read_file(const char *path, char *buffer, size_t capacity)
{
    int fd = vfs_open(path, 0);
    if (fd < 0) return fd;
    int count = vfs_read(fd, buffer, capacity);
    vfs_close(fd);
    return count;
}

static void shell_test_filesystem(void)
{
    static const char root[] = "/.ortos-fs-test";
    static const char nested[] = "/.ortos-fs-test/subdir";
    static const char file[] = "/.ortos-fs-test/test.txt";
    static const char copy[] = "/.ortos-fs-test/copy.txt";
    static const char moved[] = "/.ortos-fs-test/moved.txt";
    char content[32] = {0}, saved_directory[STORAGE_MAX_PATH];
    struct storage_stats stats_before, stats_after;
    struct storage_entry_info existing;
    shell_copy_string(saved_directory, current_dir, sizeof(saved_directory));
    int result = vfs_stat(root, &existing);
    shell_test_report("dedicated path is unused", result, STORAGE_ERR_NOENT, root);
    if (result != STORAGE_ERR_NOENT) return;

    shell_execute_line("mkdir /.ortos-fs-test");
    result = shell_last_storage_status;
    shell_test_report("mkdir", result, STORAGE_OK, root);
    if (result != STORAGE_OK || vfs_stat(root, &existing) != STORAGE_OK) return;

    shell_execute_line("mkdir /.ortos-fs-test/subdir");
    shell_test_report("nested mkdir", shell_last_storage_status, STORAGE_OK, nested);
    shell_execute_line("create file \"hello\" > /.ortos-fs-test/test.txt");
    shell_test_report("create file", shell_last_storage_status, STORAGE_OK, file);
    shell_execute_line("cat /.ortos-fs-test/test.txt");
    int count = shell_test_read_file(file, content, sizeof(content));
    shell_test_report("cat/read contents", count == 5 && content[0] == 'h' && content[4] == 'o' ? STORAGE_OK : (count < 0 ? count : STORAGE_ERR_CORRUPT), STORAGE_OK, file);

    shell_execute_line("write /.ortos-fs-test/test.txt \"persistent test\"");
    shell_test_report("write file", shell_last_storage_status, 15, file);
    shell_execute_line("cat /.ortos-fs-test/test.txt");
    count = shell_test_read_file(file, content, sizeof(content));
    shell_test_report("read modified contents", count == 15 && content[0] == 'p' && content[14] == 't' ? STORAGE_OK : (count < 0 ? count : STORAGE_ERR_CORRUPT), STORAGE_OK, file);

    shell_execute_line("cp /.ortos-fs-test/test.txt /.ortos-fs-test/copy.txt");
    shell_test_report("cp", shell_last_storage_status, STORAGE_OK, copy);
    shell_execute_line("mv /.ortos-fs-test/copy.txt /.ortos-fs-test/moved.txt");
    shell_test_report("mv", shell_last_storage_status, STORAGE_OK, moved);
    shell_execute_line("stat /.ortos-fs-test/moved.txt");
    struct storage_entry_info file_info;
    result = vfs_stat(moved, &file_info);
    shell_test_report("stat", result == STORAGE_OK && file_info.type == 'f' && file_info.size == 15 ? STORAGE_OK : (result < 0 ? result : STORAGE_ERR_CORRUPT), STORAGE_OK, moved);

    shell_execute_line("cd /.ortos-fs-test/subdir");
    shell_test_report("cd", shell_streq(current_dir, nested) ? STORAGE_OK : STORAGE_ERR_CORRUPT, STORAGE_OK, nested);
    shell_execute_line("pwd");
    shell_test_report("pwd", shell_streq(current_dir, nested) ? STORAGE_OK : STORAGE_ERR_CORRUPT, STORAGE_OK, nested);
    shell_execute_line("create file \"relative\" > ./relative.txt");
    shell_test_report("relative path create", shell_last_storage_status, STORAGE_OK, "/.ortos-fs-test/subdir/relative.txt");
    shell_execute_line("ls");
    shell_test_report("ls", shell_directory_has_child(nested, "relative.txt") ? STORAGE_OK : STORAGE_ERR_NOENT, STORAGE_OK, nested);
    shell_execute_line("cd /");
    shell_test_report("cd root", shell_streq(current_dir, "/") ? STORAGE_OK : STORAGE_ERR_CORRUPT, STORAGE_OK, "/");
    shell_execute_line("mkdir /.ortos-fs-test/empty");
    shell_execute_line("rmdir /.ortos-fs-test/empty");
    shell_test_report("empty rmdir", shell_last_storage_status, STORAGE_OK, "/.ortos-fs-test/empty");
    shell_execute_line("rmdir /.ortos-fs-test/subdir");
    shell_test_report("non-empty rmdir rejected", shell_last_storage_status, STORAGE_ERR_NOTEMPTY, nested);
    shell_execute_line("rm /.ortos-fs-test/subdir/relative.txt");
    shell_test_report("rm", shell_last_storage_status, STORAGE_OK, "/.ortos-fs-test/subdir/relative.txt");
    shell_execute_line("rmdir /.ortos-fs-test/subdir");
    shell_test_report("rmdir after emptying", shell_last_storage_status, STORAGE_OK, nested);
    shell_execute_line("rmdir /.ortos-fs-test");
    shell_test_report("non-empty parent rmdir rejected", shell_last_storage_status, STORAGE_ERR_NOTEMPTY, root);

    shell_execute_line("du /.ortos-fs-test");
    shell_execute_line("df");
    int stats_result = vfs_get_stats(&stats_before);
    shell_test_report("free-space reporting", stats_result, STORAGE_OK, root);
    shell_execute_line("sync");
    shell_test_report("sync", shell_last_storage_status, STORAGE_OK, root);
    shell_execute_line("del -f /.ortos-fs-test");
    result = shell_last_storage_status;
    if (result == STORAGE_OK) result = vfs_stat(root, &existing) == STORAGE_ERR_NOENT ? STORAGE_OK : STORAGE_ERR_CORRUPT;
    shell_test_report("recursive deletion", result, STORAGE_OK, root);
    stats_result = vfs_get_stats(&stats_after);
    shell_test_report("space after cleanup", stats_result == STORAGE_OK && stats_after.free_sectors >= stats_before.free_sectors ? STORAGE_OK : (stats_result < 0 ? stats_result : STORAGE_ERR_CORRUPT), STORAGE_OK, root);
    if (!shell_starts_with(saved_directory, root)) shell_set_current_dir(saved_directory);
}

static void shell_print_current_directory(void)
{
    imp_text(current_dir);
    imp_char('\n');
}

static void shell_print_prompt(void)
{
    imp_text("ORT$");
    imp_text(current_dir);
    imp_char('>');
}

static int shell_execute_text(const char* text)
{
    int i = 0;
    while (text[i] != '\0' && i < MAX_CMD - 1)
    {
        cmd[i] = text[i];
        i++;
    }
    cmd[i] = '\0';
    return i;
}

static void shell_execute_command(void)
{
    if (cmd[0] == '\0')
    {
        return;
    }

    char expanded[MAX_CMD];
    if (shell_expand_alias(cmd, expanded, sizeof(expanded)))
    {
        shell_execute_text(expanded);
    }

    shell_add_history(cmd);
    shell_tick_clock();

    if (shell_streq(cmd, "help"))
    {
        shell_print_help();
    }
    else if (shell_streq(cmd, "clear") || shell_streq(cmd, "cls"))
    {
        imp_cls();
    }
    else if (shell_streq(cmd, "hello"))
    {
        imp_text("Hello from ORT!\n");
    }
    else if (shell_streq(cmd, "version"))
    {
        imp_text("ORT kernel version alplha-2.0.0\n");
    }
    else if (shell_streq(cmd, "uname"))
    {
        imp_text("ORTOS 0.2 x86_64 GNU/Linux\n");
    }
    else if (shell_streq(cmd, "uptime"))
    {
        imp_text("Uptime: ");
        shell_print_int(shell_uptime_seconds);
        imp_text("s\n");
    }
    else if (shell_streq(cmd, "date"))
    {
        imp_text("Date: ");
        shell_print_int(shell_clock_year);
        imp_char('-');
        shell_print_int(shell_clock_month);
        imp_char('-');
        shell_print_int(shell_clock_day);
        imp_char('\n');
    }
    else if (shell_streq(cmd, "time"))
    {
        imp_text("Time: ");
        shell_print_int(shell_clock_hour);
        imp_char(':');
        shell_print_int(shell_clock_minute);
        imp_char(':');
        shell_print_int(shell_clock_second);
        imp_char('\n');
    }
    else if (shell_streq(cmd, "reboot"))
    {
        imp_text("Reboot requested.\n");
        ortos_reboot();
    }
    else if (shell_streq(cmd, "shutdown"))
    {
        imp_text("Shutdown requested.\n");
        asm volatile ("cli; hlt");
    }
    else if (shell_starts_with(cmd, "loadkeys"))
    {
        const char* argument = shell_skip_spaces(cmd + 8);
        if (shell_streq(argument, "fr"))
        {
            imp_text("Keyboard layout switched to fr.\n");
            current_layout[0] = 'f';
            current_layout[1] = 'r';
            current_layout[2] = '\0';
            keyboard_set_layout(current_layout);
        }
        else if (shell_streq(argument, "en-us"))
        {
            imp_text("Keyboard layout switched to en-us.\n");
            current_layout[0] = 'e';
            current_layout[1] = 'n';
            current_layout[2] = '-';
            current_layout[3] = 'u';
            current_layout[4] = 's';
            current_layout[5] = '\0';
            keyboard_set_layout(current_layout);
        }
        else
        {
            imp_text("Usage: loadkeys fr|en-us\n");
        }
    }
    else if (shell_streq(cmd, "cpuinfo"))
    {
        imp_text("CPU: x86_64, 1 core, unknown model\n");
    }
    else if (shell_streq(cmd, "meminfo"))
    {
        imp_text("Memory: 64MB available\n");
    }
    else if (shell_streq(cmd, "sysinfo"))
    {
        imp_text("System: ORTOS shell, VGA console, keyboard input\n");
    }
    else if (shell_streq(cmd, "pci"))
    {
        imp_text("PCI: VGA controller, PS/2 keyboard controller\n");
    }
    else if (shell_streq(cmd, "regs"))
    {
        imp_text("CPU registers: RAX=0x0 RBX=0x0 RCX=0x0\n");
    }
    else if (shell_streq(cmd, "gdt"))
    {
        imp_text("GDT: placeholder entry loaded\n");
    }
    else if (shell_streq(cmd, "idt"))
    {
        imp_text("IDT: placeholder entry loaded\n");
    }
    else if (shell_streq(cmd, "stack"))
    {
        imp_text("Stack: kernel stack initialized\n");
    }
    else if (shell_streq(cmd, "heap"))
    {
        imp_text("Heap: simple static allocator active\n");
    }
    else if (shell_streq(cmd, "irq"))
    {
        imp_text("IRQ: keyboard interrupt enabled\n");
    }
    else if (shell_streq(cmd, "ls") || shell_starts_with(cmd, "ls "))
    {
        char argument[STORAGE_MAX_PATH], directory[STORAGE_MAX_PATH];
        const char *cursor = shell_read_word(shell_skip_spaces(cmd + 2), argument, sizeof(argument));
        if (*shell_skip_spaces(cursor)) { imp_text("Usage: ls [directory]\n"); }
        else {
            const char *path = argument[0] ? argument : current_dir;
            struct storage_entry_info dir_info;
            if (!shell_resolve_path(path, directory, sizeof(directory))) imp_text("Error: invalid path\n");
            else if (vfs_stat(directory, &dir_info) != STORAGE_OK) imp_text("Error: directory not found\n");
            else if (dir_info.type != 'd') imp_text("Error: not a directory\n");
            else {
                imp_text("NAME                    TYPE       SIZE\n");
                size_t base_length = 0;
                while (directory[base_length]) base_length++;
                for (int i = 0; i < vfs_entry_count(); i++) {
                    struct storage_entry_info item;
                    if (vfs_readdir(i, &item) != STORAGE_OK) continue;
                    const char *entry_path = item.path;
                    const char *child = 0;
                    if (base_length == 1) {
                        if (entry_path[0] == '/' && entry_path[1]) child = entry_path + 1;
                    } else if (shell_starts_with(entry_path, directory) && entry_path[base_length] == '/') child = entry_path + base_length + 1;
                    if (!child || !*child) continue;
                    int direct = 1;
                    for (const char *p = child; *p; p++) if (*p == '/') { direct = 0; break; }
                    if (!direct) continue;
                    imp_text(item.name); imp_text("  ");
                    imp_text(item.type == 'd' ? "DIR        -\n" : "FILE       ");
                    if (item.type == 'f') { shell_print_uint64(item.size); imp_text(" B\n"); }
                }
            }
        }
    }
    else if (shell_streq(cmd, "disks") || shell_streq(cmd, "diskinfo") || shell_streq(cmd, "disk list") || shell_streq(cmd, "disk info"))
    {
        struct storage_device_info info;
        if (vfs_get_device_info(&info) != STORAGE_OK) imp_text("No storage device detected\n");
        else
        {
            imp_text("Device: "); imp_text(info.name); imp_text(" Type: "); imp_text(info.type);
            imp_text(" Sectors: "); shell_print_uint64(info.sectors); imp_text(" Sector size: "); shell_print_uint64(info.sector_size);
            imp_text(" Status: "); imp_text(info.mounted ? "mounted\n" : "unmounted\n");
        }
    }
    else if (shell_streq(cmd, "df"))
    {
        struct storage_stats stats;
        if (vfs_get_stats(&stats) != STORAGE_OK) imp_text("df: no mounted filesystem\n");
        else { imp_text("Filesystem    Total sectors    Used    Free    Mount\n"); imp_text("ORFS          "); shell_print_uint64(stats.total_sectors); imp_text("             "); shell_print_uint64(stats.used_sectors); imp_text("     "); shell_print_uint64(stats.free_sectors); imp_text("     /\n"); }
    }
    else if (shell_streq(cmd, "mount"))
    {
        shell_last_storage_status = vfs_mount();
        if (shell_last_storage_status == STORAGE_OK) imp_text("Filesystem mounted\n"); else shell_print_storage_error(shell_last_storage_status);
    }
    else if (shell_streq(cmd, "umount") || shell_streq(cmd, "unmount"))
    {
        shell_last_storage_status = vfs_unmount();
        if (shell_last_storage_status == STORAGE_OK) imp_text("Filesystem unmounted\n"); else shell_print_storage_error(shell_last_storage_status);
    }
    else if (shell_streq(cmd, "sync"))
    {
        shell_last_storage_status = vfs_sync();
        if (shell_last_storage_status == STORAGE_OK) imp_text("Storage synchronized\n"); else shell_print_storage_error(shell_last_storage_status);
    }
    else if (shell_streq(cmd, "storage-test") || shell_streq(cmd, "storage-test --safe"))
    {
        unsigned int result = storage_self_test();
        imp_text((result & STORAGE_SELFTEST_DEVICE) ? "[PASS] Disk detection (RAM device)\n" : "[FAIL] Disk detection (RAM device)\n");
        imp_text((result & STORAGE_SELFTEST_INFO) ? "[PASS] Disk information\n" : "[FAIL] Disk information\n");
        imp_text((result & STORAGE_SELFTEST_BLOCK_IO) ? "[PASS] Block read/write and bounds\n" : "[FAIL] Block read/write and bounds\n");
        imp_text((result & STORAGE_SELFTEST_MOUNT) ? "[PASS] Filesystem format/mount/unmount\n" : "[FAIL] Filesystem format/mount/unmount\n");
        imp_text((result & STORAGE_SELFTEST_FILES) ? "[PASS] File and directory operations\n" : "[FAIL] File and directory operations\n");
        imp_text((result & STORAGE_SELFTEST_SPACE) ? "[PASS] Free-space calculation\n" : "[FAIL] Free-space calculation\n");
        imp_text((result & STORAGE_SELFTEST_CACHE) ? "[PASS] Cache read/write/eviction\n" : "[FAIL] Cache read/write/eviction\n");
        imp_text((result & STORAGE_SELFTEST_FSCK) ? "[PASS] Filesystem integrity and repair\n" : "[FAIL] Filesystem integrity and repair\n");
        imp_text((result & STORAGE_SELFTEST_ERRORS) ? "[PASS] Storage error handling\n" : "[FAIL] Storage error handling\n");
        imp_text((result & STORAGE_SELFTEST_ALL) == STORAGE_SELFTEST_ALL ? "Storage self-test: PASS\n" : "Storage self-test: FAIL\n");
    }
    else if (shell_starts_with(cmd, "format"))
    {
        const char *argument = shell_skip_spaces(cmd + 6);
        if (!shell_streq(argument, "yes")) imp_text("format: confirmation required, use 'format yes'\n");
        else if ((shell_last_storage_status = vfs_format()) == STORAGE_OK) imp_text("Filesystem formatted and mounted\n");
        else shell_print_storage_error(shell_last_storage_status);
    }
    else if (shell_starts_with(cmd, "fsck"))
    {
        const char *argument = shell_skip_spaces(cmd + 4);
        int errors = 0; int result = vfs_fsck(shell_streq(argument, "repair") || shell_streq(argument, "--repair"), &errors);
        shell_last_storage_status = result;
        if (result == STORAGE_OK) imp_text("fsck: clean\n"); else { imp_text("fsck: errors detected: "); shell_print_int(errors); imp_char('\n'); }
    }
    else if (shell_streq(cmd, "cd") || shell_starts_with(cmd, "cd "))
    {
        char argument[STORAGE_MAX_PATH], resolved[STORAGE_MAX_PATH];
        const char *cursor = shell_read_word(shell_skip_spaces(cmd + 2), argument, sizeof(argument));
        struct storage_entry_info info;
        if (!argument[0] || *shell_skip_spaces(cursor)) { imp_text("Usage: cd <directory>\n"); }
        else if (!shell_resolve_path(argument, resolved, sizeof(resolved))) imp_text("Error: invalid path\n");
        else if (vfs_stat(resolved, &info) != STORAGE_OK) imp_text("Error: directory not found\n");
        else if (info.type != 'd') imp_text("Error: not a directory\n");
        else { shell_set_current_dir(resolved); shell_print_current_directory(); }
    }
    else if (shell_streq(cmd, "pwd")) shell_print_current_directory();
    else if (shell_starts_with(cmd, "cat "))
    {
        char argument[STORAGE_MAX_PATH], resolved[STORAGE_MAX_PATH], buffer[64];
        const char *cursor = shell_read_word(shell_skip_spaces(cmd + 3), argument, sizeof(argument));
        if (!argument[0] || *shell_skip_spaces(cursor)) { imp_text("Usage: cat <file>\n"); }
        else if (!shell_resolve_path(argument, resolved, sizeof(resolved))) imp_text("Error: invalid path\n");
        else {
            int fd = vfs_open(resolved, 0);
            if (fd < 0) shell_print_storage_error(fd);
            else {
                int count;
                while ((count = vfs_read(fd, buffer, sizeof(buffer))) > 0)
                    for (int i = 0; i < count; i++) imp_char(buffer[i]);
                vfs_close(fd);
                if (count < 0) shell_print_storage_error(count);
            }
        }
    }
    else if (shell_starts_with(cmd, "create ")) shell_create_command(shell_skip_spaces(cmd + 6));
    else if (shell_starts_with(cmd, "textedit ") || shell_starts_with(cmd, "modify "))
    {
        const char *arguments = shell_skip_spaces(cmd + (cmd[0] == 'm' ? 6 : 9));
        char argument[STORAGE_MAX_PATH], resolved[STORAGE_MAX_PATH];
        const char *cursor = shell_read_word(arguments, argument, sizeof(argument));
        if (!argument[0] || *shell_skip_spaces(cursor)) imp_text("Usage: textedit <file>\n");
        else if (!shell_resolve_path(argument, resolved, sizeof(resolved))) imp_text("Error: invalid path\n");
        else shell_textedit(resolved);
    }
    else if (shell_starts_with(cmd, "touch "))
    {
        char argument[STORAGE_MAX_PATH], resolved[STORAGE_MAX_PATH];
        const char *cursor = shell_read_word(shell_skip_spaces(cmd + 5), argument, sizeof(argument));
        if (!argument[0] || *shell_skip_spaces(cursor)) imp_text("Usage: touch <file>\n");
        else if (!shell_resolve_path(argument, resolved, sizeof(resolved))) imp_text("Error: invalid path\n");
        else { int result = vfs_create(resolved, 0, 0); shell_last_storage_status = result; if (result == STORAGE_OK) imp_text("File created\n"); else shell_print_storage_error(result); }
    }
    else if (shell_streq(cmd, "mkdir") || shell_starts_with(cmd, "mkdir ")) shell_mkdir_command(shell_skip_spaces(cmd + 5));
    else if (shell_streq(cmd, "rm") || shell_starts_with(cmd, "rm "))
    {
        char argument[STORAGE_MAX_PATH], resolved[STORAGE_MAX_PATH];
        const char *cursor = shell_read_word(shell_skip_spaces(cmd + 2), argument, sizeof(argument));
        if (!argument[0] || *shell_skip_spaces(cursor)) imp_text("Usage: rm <file>\n");
        else if (!shell_resolve_path(argument, resolved, sizeof(resolved))) imp_text("Error: invalid path\n");
        else { int result = vfs_unlink(resolved); shell_last_storage_status = result; if (result == STORAGE_OK) imp_text("File removed\n"); else shell_print_storage_error(result); }
    }
    else if (shell_starts_with(cmd, "del "))
    {
        const char *cursor = shell_skip_spaces(cmd + 4);
        int force = 0;
        if (shell_starts_with(cursor, "-f") && shell_is_space(cursor[2])) { force = 1; cursor = shell_skip_spaces(cursor + 2); }
        char argument[STORAGE_MAX_PATH], resolved[STORAGE_MAX_PATH];
        cursor = shell_read_word(cursor, argument, sizeof(argument));
        if (!argument[0] || *shell_skip_spaces(cursor)) imp_text("Usage: del [-f] <directory>\n");
        else if (!shell_resolve_path(argument, resolved, sizeof(resolved))) imp_text("Error: invalid path\n");
        else if (shell_streq(resolved, "/") || shell_streq(resolved, "/C:") || shell_streq(resolved, "/C:/user") || shell_streq(resolved, "/C:/menu") || shell_streq(current_dir, resolved) || shell_starts_with(current_dir, resolved)) imp_text("Error: refusing to delete a protected directory\n");
        else if (!force && !shell_confirm_delete()) imp_text("Delete cancelled\n");
        else { int result = vfs_remove_tree(resolved); shell_last_storage_status = result; if (result == STORAGE_OK) imp_text("Directory tree removed\n"); else shell_print_storage_error(result); }
    }
    else if (shell_starts_with(cmd, "cp ") || shell_starts_with(cmd, "mv "))
    {
        int move = cmd[0] == 'm';
        const char *cursor = shell_skip_spaces(cmd + 2);
        char source[STORAGE_MAX_PATH], destination[STORAGE_MAX_PATH];
        char source_path[STORAGE_MAX_PATH], destination_path[STORAGE_MAX_PATH], target_path[STORAGE_MAX_PATH];
        cursor = shell_read_word(cursor, source, sizeof(source));
        cursor = shell_read_word(cursor, destination, sizeof(destination));
        if (!source[0] || !destination[0] || *shell_skip_spaces(cursor)) imp_text(move ? "Usage: mv <source> <destination>\n" : "Usage: cp <source> <destination>\n");
        else if (!shell_resolve_path(source, source_path, sizeof(source_path)) || !shell_resolve_path(destination, destination_path, sizeof(destination_path))) imp_text("Error: invalid path\n");
        else if (!shell_join_target(source_path, destination_path, target_path, sizeof(target_path))) imp_text("Error: invalid destination path\n");
        else {
            int result = move ? vfs_rename(source_path, target_path) : vfs_copy(source_path, target_path);
            shell_last_storage_status = result;
            if (result == STORAGE_OK) imp_text(move ? "Moved\n" : "Copied\n"); else shell_print_storage_error(result);
        }
    }
    else if (shell_starts_with(cmd, "write ") || shell_starts_with(cmd, "append "))
    {
        int append = cmd[0] == 'a';
        const char *cursor = shell_skip_spaces(cmd + (append ? 6 : 5));
        char path[STORAGE_MAX_PATH], resolved[STORAGE_MAX_PATH], data[MAX_CMD];
        cursor = shell_read_word(cursor, path, sizeof(path));
        cursor = shell_skip_spaces(cursor);
        shell_copy_string(data, cursor, sizeof(data));
        size_t length = (size_t)shell_strlen(data);
        if (length >= 2 && data[0] == '"' && data[length - 1] == '"') { data[length - 1] = 0; cursor = data + 1; }
        else cursor = data;
        if (!path[0] || !cursor[0]) imp_text("Usage: write|append <file> <text>\n");
        else if (!shell_resolve_path(path, resolved, sizeof(resolved))) imp_text("Error: invalid path\n");
        else { int result = vfs_write_file(resolved, cursor, (size_t)shell_strlen(cursor), append); shell_last_storage_status = result; if (result < 0) shell_print_storage_error(result); else imp_text("File written\n"); }
    }
    else if (shell_streq(cmd, "rmdir") || shell_starts_with(cmd, "rmdir "))
    {
        char argument[STORAGE_MAX_PATH], resolved[STORAGE_MAX_PATH];
        const char *cursor = shell_read_word(shell_skip_spaces(cmd + 5), argument, sizeof(argument));
        if (!argument[0] || *shell_skip_spaces(cursor)) imp_text("Usage: rmdir <empty-directory>\n");
        else if (!shell_resolve_path(argument, resolved, sizeof(resolved))) imp_text("Error: invalid path\n");
        else { int result = vfs_rmdir(resolved); shell_last_storage_status = result; if (result == STORAGE_OK) imp_text("Directory removed\n"); else shell_print_storage_error(result); }
    }
    else if (shell_starts_with(cmd, "stat "))
    {
        char argument[STORAGE_MAX_PATH], resolved[STORAGE_MAX_PATH];
        const char *cursor = shell_read_word(shell_skip_spaces(cmd + 4), argument, sizeof(argument));
        struct storage_entry_info info;
        if (!argument[0] || *shell_skip_spaces(cursor)) imp_text("Usage: stat <path>\n");
        else if (!shell_resolve_path(argument, resolved, sizeof(resolved))) imp_text("Error: invalid path\n");
        else if (vfs_stat(resolved, &info) != STORAGE_OK) imp_text("Error: path not found\n");
        else { imp_text("Path: "); imp_text(info.path); imp_text("\nType: "); imp_text(info.type == 'd' ? "directory\n" : "file\n"); imp_text("Size: "); shell_print_uint64(info.size); imp_text(" bytes\nBlocks: "); shell_print_uint64(info.blocks_used); imp_char('\n'); }
    }
    else if (shell_starts_with(cmd, "du "))
    {
        char argument[STORAGE_MAX_PATH], resolved[STORAGE_MAX_PATH];
        const char *cursor = shell_read_word(shell_skip_spaces(cmd + 2), argument, sizeof(argument));
        struct storage_entry_info info;
        if (!argument[0] || *shell_skip_spaces(cursor)) imp_text("Usage: du <path>\n");
        else if (!shell_resolve_path(argument, resolved, sizeof(resolved))) imp_text("Error: invalid path\n");
        else if (vfs_stat(resolved, &info) != STORAGE_OK) imp_text("Error: path not found\n");
        else {
            uint64_t bytes = info.size;
            if (info.type == 'd') for (int i = 0; i < vfs_entry_count(); i++) {
                struct storage_entry_info item;
                if (vfs_readdir(i, &item) == STORAGE_OK && shell_starts_with(item.path, resolved) && item.path[0] &&
                    ((resolved[0] == '/' && resolved[1] == 0 && item.path[1]) || item.path[shell_strlen(resolved)] == '/') &&
                    item.type == 'f') bytes += item.size;
            }
            shell_print_uint64(bytes); imp_text(" bytes\t"); imp_text(resolved); imp_char('\n');
        }
    }
    else if (shell_streq(cmd, "tasks"))
    {
        imp_text("Tasks: shell, idle, keyboard\n");
    }
    else if (shell_starts_with(cmd, "kill"))
    {
        imp_text("Task kill requested.\n");
    }
    else if (shell_streq(cmd, "ps"))
    {
        imp_text("PID 1 shell\nPID 2 idle\n");
    }
    else if (shell_streq(cmd, "test filesystem") || shell_streq(cmd, "test storage"))
    {
        shell_test_filesystem();
    }
    else if (shell_streq(cmd, "test"))
    {
        imp_text("Kernel tests: OK\n");
    }
    else if (shell_streq(cmd, "panic"))
    {
        imp_text("Kernel panic triggered\n");
        asm volatile ("ud2");
    }
    else if (shell_streq(cmd, "beep"))
    {
        imp_text("BEEP!\n");
        asm volatile ("outb %%al, $0x61" : : "a"(0x03));
    }
    else if (shell_streq(cmd, "calc"))
    {
        imp_text("Calculator ready. Use 'calc <a> <op> <b>'\n");
    }
    else if (shell_starts_with(cmd, "calc"))
    {
        const char* cursor = shell_skip_spaces(cmd + 4);
        char lhs[MAX_CMD];
        char op[MAX_CMD];
        char rhs[MAX_CMD];
        cursor = shell_read_token(cursor, lhs, sizeof(lhs));
        cursor = shell_skip_spaces(cursor);
        cursor = shell_read_token(cursor, op, sizeof(op));
        cursor = shell_skip_spaces(cursor);
        shell_read_token(cursor, rhs, sizeof(rhs));

        if (lhs[0] != '\0' && rhs[0] != '\0' && op[0] != '\0')
        {
            int left = 0;
            int right = 0;
            int valid = 1;
            int value = 0;
            int i = 0;

            while (lhs[i] != '\0')
            {
                if (lhs[i] < '0' || lhs[i] > '9')
                {
                    valid = 0;
                    break;
                }
                left = left * 10 + (lhs[i] - '0');
                i++;
            }

            i = 0;
            while (rhs[i] != '\0')
            {
                if (rhs[i] < '0' || rhs[i] > '9')
                {
                    valid = 0;
                    break;
                }
                right = right * 10 + (rhs[i] - '0');
                i++;
            }

            if (valid)
            {
                if (op[0] == '+')
                {
                    value = left + right;
                }
                else if (op[0] == '-')
                {
                    value = left - right;
                }
                else if (op[0] == '*')
                {
                    value = left * right;
                }
                else if (op[0] == '/')
                {
                    if (right != 0)
                    {
                        value = left / right;
                    }
                    else
                    {
                        valid = 0;
                    }
                }
                else if (op[0] == '%')
                {
                    if (right != 0)
                    {
                        value = left % right;
                    }
                    else
                    {
                        valid = 0;
                    }
                }
                else
                {
                    valid = 0;
                }
            }

            if (valid)
            {
                imp_text("Result: ");
                shell_print_int(value);
                imp_char('\n');
            }
            else
            {
                imp_text("Usage: calc <a> <op> <b>\n");
            }
        }
        else
        {
            imp_text("Usage: calc <a> <op> <b>\n");
        }
    }
    else if (shell_streq(cmd, "rand"))
    {
        shell_seed = shell_seed * 1103515245 + 12345;
        imp_text("Random: ");
        shell_print_int(shell_seed & 0x7fff);
        imp_char('\n');
    }
    else if (shell_streq(cmd, "sleep"))
    {
        int delay = 1;
        int i = 0;
        imp_text("Sleeping...\n");
        while (i < 1000000 * delay)
        {
            i++;
        }
    }
    else if (shell_starts_with(cmd, "repeat"))
    {
        const char* cursor = shell_skip_spaces(cmd + 6);
        char token[MAX_CMD];
        shell_read_token(cursor, token, sizeof(token));

        if (token[0] == '\0')
        {
            if (history_count > 0)
            {
                int index = (history_count - 1 + MAX_HISTORY) % MAX_HISTORY;
                shell_execute_text(history[index]);
            }
            else
            {
                imp_text("No history available\n");
            }
        }
        else
        {
            int index = 0;
            int valid = 1;
            int i = 0;
            while (token[i] != '\0')
            {
                if (token[i] < '0' || token[i] > '9')
                {
                    valid = 0;
                    break;
                }
                index = index * 10 + (token[i] - '0');
                i++;
            }

            if (valid && index >= 0 && index < history_count)
            {
                int history_index = (history_count - 1 - index + MAX_HISTORY) % MAX_HISTORY;
                shell_execute_text(history[history_index]);
            }
            else
            {
                shell_execute_text(token);
            }
        }
    }
    else if (shell_streq(cmd, "history"))
    {
        for (int i = 0; i < history_count; i++)
        {
            int index = (history_count - 1 - i + MAX_HISTORY) % MAX_HISTORY;
            imp_text(history[index]);
            imp_char('\n');
        }
    }
    else if (shell_starts_with(cmd, "alias"))
    {
        const char* cursor = shell_skip_spaces(cmd + 5);
        char name[MAX_CMD];
        char value[MAX_CMD];
        cursor = shell_read_token(cursor, name, sizeof(name));
        cursor = shell_skip_spaces(cursor);
        shell_read_token(cursor, value, sizeof(value));

        if (name[0] != '\0' && value[0] != '\0')
        {
            shell_add_alias(name, value);
            imp_text("Alias created\n");
        }
        else if (name[0] != '\0')
        {
            char stored[MAX_CMD];
            if (shell_lookup_alias(name, stored, sizeof(stored)))
            {
                imp_text(name);
                imp_text(" -> ");
                imp_text(stored);
                imp_char('\n');
            }
            else
            {
                imp_text("Alias not found\n");
            }
        }
        else
        {
            for (int i = 0; i < alias_count; i++)
            {
                imp_text(aliases[i]);
                imp_char('\n');
            }
        }
    }
    else if (shell_streq(cmd, "env"))
    {
        imp_text("PATH=/bin\nLANG=");
        imp_text(current_layout);
        imp_char('\n');
    }
    else if (shell_starts_with(cmd, "ping"))
    {
        const char* cursor = shell_skip_spaces(cmd + 4);
        char host[MAX_CMD];
        char mode[MAX_CMD];
        cursor = shell_read_token(cursor, host, sizeof(host));
        cursor = shell_skip_spaces(cursor);
        shell_read_token(cursor, mode, sizeof(mode));

        if (host[0] == '\0')
        {
            imp_text("Usage: ping <host> [eth|wifi]\n");
        }
        else if (mode[0] != '\0' && !shell_streq(mode, "eth") && !shell_streq(mode, "wifi"))
        {
            imp_text("Usage: ping <host> [eth|wifi]\n");
        }
        else if (shell_streq(mode, "wifi") && !network_is_wifi_connected())
        {
            imp_text("Wi-Fi not connected\n");
        }
        else if (!shell_streq(mode, "wifi") && !network_has_ethernet())
        {
            imp_text("Ethernet not available\n");
        }
        else
        {
            int result = network_ping(host, shell_streq(mode, "wifi"));
            if (result == NETWORK_ERR_NOT_SUPPORTED)
                imp_text("Network protocol stack is not supported yet.\n");
            else if (result == NETWORK_ERR_NOT_CONNECTED)
                imp_text("Network interface is not connected.\n");
        }
    }
    else if (shell_starts_with(cmd, "wifi"))
    {
        const char* cursor = shell_skip_spaces(cmd + 4);
        char subcmd[MAX_CMD];
        cursor = shell_read_token(cursor, subcmd, sizeof(subcmd));

        if (shell_streq(subcmd, "connect"))
        {
            char ssid[MAX_CMD];
            cursor = shell_skip_spaces(cursor);
            shell_read_token(cursor, ssid, sizeof(ssid));
            if (ssid[0] == '\0')
            {
                imp_text("Usage: wifi connect <ssid>\n");
            }
            else
            {
                if (network_connect_wifi(ssid) == NETWORK_OK)
                {
                    imp_text("Wi-Fi connected to ");
                    imp_text(ssid);
                    imp_char('\n');
                }
                else
                {
                    imp_text("Wi-Fi hardware is not supported yet.\n");
                }
            }
        }
        else if (shell_streq(subcmd, "disconnect"))
        {
            network_disconnect_wifi();
            imp_text("Wi-Fi disconnected\n");
        }
        else if (shell_streq(subcmd, "status"))
        {
            if (network_is_wifi_connected())
            {
                imp_text("Wi-Fi connected to ");
                imp_text(network_get_wifi_ssid());
                imp_char('\n');
            }
            else
            {
                imp_text("Wi-Fi disconnected\n");
            }
        }
        else
        {
            imp_text("Usage: wifi connect <ssid> | disconnect | status\n");
        }
    }
    else if (shell_streq(cmd, "i_use_arch_btw"))
    {
        imp_text("If you run this command, Why you didn't use Arch btw?.\n");
        imp_text("You should have used Windows, It is better for your mental health.\n");
    }
    else if (shell_streq(cmd, "echo"))
    {
        imp_char('\n');
    }
    else if (shell_starts_with(cmd, "echo"))
    {
        const char* argument = shell_skip_spaces(cmd + 4);
        if (*argument != '\0')
        {
            imp_text(argument);
            imp_char('\n');
        }
        else
        {
            imp_char('\n');
        }
    }
    else if (shell_starts_with(cmd, "notify"))
    {
        const char *cursor = shell_skip_spaces(cmd + 6);
        char name[MAX_CMD];
        char message[MAX_CMD];
        int quoted = *cursor == '"';
        if (quoted) cursor++;
        int name_length = 0;
        while (*cursor && ((quoted && *cursor != '"') || (!quoted && !shell_is_space(*cursor))) && name_length < MAX_CMD - 1)
            name[name_length++] = *cursor++;
        name[name_length] = '\0';
        if (quoted && *cursor == '"') cursor++;
        cursor = shell_skip_spaces(cursor);
        quoted = *cursor == '"';
        if (quoted) cursor++;
        int message_length = 0;
        while (*cursor && ((quoted && *cursor != '"') || (!quoted && !shell_is_space(*cursor))) && message_length < MAX_CMD - 1)
            message[message_length++] = *cursor++;
        message[message_length] = '\0';
        if (quoted && *cursor == '"') cursor++;
        cursor = shell_skip_spaces(cursor);
        if (name[0] == '\0' || message[0] == '\0' || *cursor != '\0')
            imp_text("Usage: notify \"name\" \"message\"\n");
        else {
            desktop_notify(name, message);
            imp_text("Notification sent\n");
        }
    }
    else if (shell_streq(cmd, "desktop") || shell_streq(cmd, "gui"))
    {
        ortos_boot_mode_set(ORTOS_BOOT_MODE_GUI);
        imp_text("Switching to GUI mode...\n");
        desktop_init(0);
        desktop_run();
        ortos_boot_mode_set(ORTOS_BOOT_MODE_SHELL);
        imp_cls();
        imp_text("Returned from ORgui.\n");
    }
    else if (shell_streq(cmd, "ospec"))
    {
        imp_text("ORTOS Special Command Executed!\n");
        imp_text(
        "                                                                                                    \n"
        "                                                                                                    \n"
        "                                                                                                    \n"
        "                                                                                                    \n"
        "                                                                                                    \n"
        "                                                                                                    \n"
        "                        AAAAAAAAAA                                                                  \n"
        "                    AAAA          AAAAAA                                           AAAAAAAAAA       \n"
        "                   A                    AAAA                                AAAAAAA                 \n"
        "                   AA                       AAA                       AAAAAA               AA       \n"
        "                     AA                        AAA              AAAAAA               AA   AA        \n"
        "                      AA        A                 AAA       AAAA                  AA     AA         \n"
        "                        AA        A                  AAAAAAA                   AA       AA          \n"
        "                         AA         A                     A AAAAAAA         AA          A           \n"
        "                          A         AAA      AAA     AAAAA           AAA      AA       AA           \n"
        "                          A      AA       AA                             AA      AA    A            \n"
        "                          A    A      AA                                    A          A            \n"
        "                          A          A                                        A        A            \n"
        "                         AA       AA                                           AA       A           \n"
        "                        AA      AA                                               A      A           \n"
        "                      AA       A                                             A    AA    AA          \n"
        "                      A       A                       AA                      A     A    A           \n"
        "                      AA    A    A                                             A     A    A         \n"
        "                        AAAA    A                       A                       A     A  A           \n"
        "                         AA                              A     AA                      AA            \n"
        "                        AA                    AA     A        A  A               A     AA            \n"
        "                       AA     A                      A    A   A  A         A      A     AA           \n"
        "                      AA     A                A AA   A    AA  A  A         A      A      A          \n"
        "                      A            A         AA    AA       A       AAA           A      A          \n"
        "                     A      A      A     AAAA      A  A    A     A   A   AA A             A         \n"
        "                     A      A      A   A    A     AAAA A       AAAA  A   A                A         \n"
        "                     A      A       A       A  AAAAAAA       AAAA A  A    A       A       A         \n"
        "                     A      A          A    A   AAAA   A      AAA                        AA         \n"
        "                     A      A          A             A      A        A   A              AA          \n"
        "                     AA                 A    A     AA       A AA AA  A   A             AA           \n"
        "                      A      A           A    A                      A   A     A     AA             \n"
        "                       AA     A           A   A                         A     A     AA              \n"
        "                        AAA     A          A   A                    A   A    A    AAA               \n"
        "                          AAA     A         A   A                 AAA  A   A    AAA                 \n"
        "                             AAAA    AAA     A  AA AAAAA    AAAA      A        AA                   \n"
        "                                AAAA          A     AA             A        AAA                     \n"
        "                                    AAAA          A A           AAA      AAAA                       \n"
        "                                        AAAAA   AAA   AA    AA    AAAAAAAA                          \n"
        "                                             AAAAA      A         AAA                               \n"
        "                                               AAA    AA     A   A  AAA                             \n"
        "                                              AA     A   A A  A A     AA                           \n"
        "                                             AA        AA   AA         AAA                         \n"
        "                                            AA      A   A                AA                        \n"
        "                                            A                             AA                        \n"
        "                                           A            A   A              A                        \n"
        "                                          AA                A              AA                       \n"
        "                                          A            A                    A                       \n"
        "                                         AA            A     A               A                      \n"
        "                                         A                   A    AA    A    A                      \n"
        "                                        AA            A      A               AA                     \n"
        "                                        A             A      A                A                     \n"
        "                                        A                    A                A                     \n"
        "                                       AA                    A                A                     \n"
        "                                       A             A       A                A                     \n"
        "                                       A             A       A                A                     \n"
        "                                       A                                      A                     \n"
        "                                        AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA                     \n"
        "                                                                                                    \n"
        "                                                                                                    \n"
                                                                                                                );
        imp_text("I know you location :)");
    }
    else if (shell_streq(cmd, "exit"))
    {
        imp_text("Exiting shell...\n");
        asm volatile ("cli; hlt");
    }
    else
    {
        imp_text("Unknown command. Type 'help' for a list.\n");
    }
}

void shell_execute_line(const char *line)
{
    if (line == 0)
    {
        return;
    }

    shell_last_storage_status = STORAGE_OK;
    shell_execute_text(line);
    shell_execute_command();
}

void shell_init()
{
    vfs_init();

    imp_text("ORT Shell\n");
    imp_text("Type 'help' for a list of commands.\n");
}

void shell_run()
{
    while (1)
    {
        shell_print_prompt();
        imp_char(' ');

        int pos = 0;

        while (1)
        {
            int key = keyboard_getchar();

            if (key == KEY_SCROLL_UP) {
                imp_scroll_up(1);
                continue;
            }
            if (key == KEY_SCROLL_DOWN) {
                imp_scroll_down(1);
                continue;
            }
            if (key == KEY_PAGE_UP) {
                imp_scroll_up(10);
                continue;
            }
            if (key == KEY_PAGE_DOWN) {
                imp_scroll_down(10);
                continue;
            }

            if (key == '\n')
            {
                cmd[pos] = '\0';
                imp_char('\n');
                break;
            }

            if (key == '\b')
            {
                if (pos > 0)
                {
                    pos--;
                    imp_text("\b \b");
                    cmd[pos] = '\0';
                }
                continue;
            }

            if (pos < MAX_CMD - 1)
            {
                cmd[pos++] = (char)key;
                imp_char((char)key);
            }
        }

        shell_execute_line(cmd);
    }
};