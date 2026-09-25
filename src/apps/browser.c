#include "desktop_apps.h"
#include "ORgui.h"
#include "framebuffer.h"
#include "keyboard.h"
#include "network.h"
#include "net/stack.h"
#include "browser.h"

#define BROWSER_STATUS_SIZE 160
#define BROWSER_TITLE_SIZE 128

typedef struct {
    char url[BROWSER_URL_SIZE];
    char title[BROWSER_TITLE_SIZE];
    char status[BROWSER_STATUS_SIZE];
    char html[BROWSER_TEXT_SIZE];
    char typed_url[BROWSER_URL_SIZE];
    char history[BROWSER_HISTORY_SIZE][BROWSER_URL_SIZE];
    int history_count;
    int history_index;
    int scroll_y;
    int page_width;
    BrowserLinkEntry links[BROWSER_MAX_LINKS];
    int link_count;
    ORWindow *window;
    int is_editing;
    int is_loading;
    int loading_started;
} BrowserWindowState;

static BrowserWindowState browser_state_storage[ORGUI_MAX_WINDOWS];

static int browser_strlen(const char *text)
{
    int length = 0;
    if (text == 0) return 0;
    while (text[length] != '\0') length++;
    return length;
}

static void browser_copy(char *destination, const char *source, int max_length)
{
    int index = 0;
    if (destination == 0 || source == 0 || max_length <= 0) return;
    while (source[index] != '\0' && index + 1 < max_length)
    {
        destination[index] = source[index];
        index++;
    }
    destination[index] = '\0';
}

static int browser_equals(const char *left, const char *right)
{
    int index = 0;
    if (left == 0 || right == 0) return left == right;
    while (left[index] != '\0' && right[index] != '\0')
    {
        if (left[index] != right[index]) return 0;
        index++;
    }
    return left[index] == right[index];
}

static int browser_starts_with(const char *text, const char *prefix)
{
    int index = 0;
    if (text == 0 || prefix == 0) return 0;
    while (prefix[index] != '\0')
    {
        if (text[index] != prefix[index]) return 0;
        index++;
    }
    return 1;
}

static void browser_lower(char *text)
{
    int index = 0;
    if (text == 0) return;
    while (text[index] != '\0')
    {
        if (text[index] >= 'A' && text[index] <= 'Z')
        {
            text[index] = (char)(text[index] + ('a' - 'A'));
        }
        index++;
    }
}

static void browser_trim_right(char *text)
{
    int length = browser_strlen(text);
    while (length > 0 && (text[length - 1] == '\n' || text[length - 1] == '\r' || text[length - 1] == ' ' || text[length - 1] == '\t'))
    {
        text[length - 1] = '\0';
        length--;
    }
}

void browser_resolve_relative_url(const char *base_url, const char *relative, char *out, int out_size)
{
    BrowserUrl base;
    char working[512];
    char prefix[256];
    int index = 0;

    if (out == 0 || out_size <= 0) return;
    out[0] = '\0';
    if (base_url == 0 || relative == 0) return;
    if (browser_parse_url(base_url, &base) != 0)
    {
        browser_copy(out, relative, out_size);
        return;
    }
    if (browser_starts_with(relative, "http://") || browser_starts_with(relative, "https://") || browser_starts_with(relative, "mailto:"))
    {
        browser_copy(out, relative, out_size);
        return;
    }

    browser_copy(prefix, base.scheme, sizeof(prefix));
    browser_copy(prefix + browser_strlen(prefix), "://", sizeof(prefix) - browser_strlen(prefix));
    browser_copy(prefix + browser_strlen(prefix), base.host, sizeof(prefix) - browser_strlen(prefix));

    if (relative[0] == '/')
    {
        browser_copy(out, prefix, out_size);
        browser_copy(out + browser_strlen(out), relative, out_size - browser_strlen(out));
        return;
    }

    browser_copy(working, base.path, sizeof(working));
    while (relative[index] == '.' && relative[index + 1] == '.' && relative[index + 2] == '/')
    {
        int slash = browser_strlen(working) - 1;
        while (slash > 0 && working[slash] != '/') slash--;
        if (slash > 0)
        {
            working[slash] = '\0';
        }
        index += 3;
    }
    if (working[0] == '\0')
    {
        browser_copy(working, "/", sizeof(working));
    }
    if (working[browser_strlen(working) - 1] != '/')
    {
        int last_slash = browser_strlen(working) - 1;
        while (last_slash >= 0 && working[last_slash] != '/') last_slash--;
        if (last_slash >= 0)
        {
            working[last_slash + 1] = '\0';
        }
    }
    browser_copy(out, prefix, out_size);
    browser_copy(out + browser_strlen(out), working, out_size - browser_strlen(out));
    browser_copy(out + browser_strlen(out), relative + index, out_size - browser_strlen(out));
}

int browser_parse_url(const char *url, BrowserUrl *out)
{
    int scheme_end = -1;
    int path_start = -1;
    int port_index = -1;
    int index;
    if (url == 0 || out == 0) return -1;
    browser_copy(out->scheme, "http", sizeof(out->scheme));
    out->port = 80;
    out->host[0] = '\0';
    out->path[0] = '/';
    out->path[1] = '\0';
    if (url[0] == '\0') return -1;
    for (index = 0; url[index] != '\0'; index++)
    {
        if (url[index] == ':' && url[index + 1] == '/' && scheme_end < 0)
        {
            scheme_end = index;
            browser_copy(out->scheme, url, scheme_end + 1);
            out->scheme[scheme_end] = '\0';
            browser_lower(out->scheme);
            if (browser_equals(out->scheme, "https")) out->port = 443;
            path_start = index + 3;
            break;
        }
    }
    if (scheme_end < 0)
    {
        path_start = 0;
    }
    if (path_start < 0) path_start = 0;
    for (index = path_start; url[index] != '\0'; index++)
    {
        if (url[index] == '/') break;
        if (url[index] == ':')
        {
            port_index = index;
            break;
        }
        out->host[index - path_start] = url[index];
        out->host[index - path_start + 1] = '\0';
    }
    if (port_index >= 0)
    {
        int port_value = 0;
        int port_index2 = 0;
        const char *cursor = url + port_index + 1;
        while (cursor[port_index2] != '\0' && cursor[port_index2] != '/')
        {
            port_value = port_value * 10 + (cursor[port_index2] - '0');
            port_index2++;
        }
        out->port = port_value;
    }
    if (path_start >= 0)
    {
        int host_length = browser_strlen(out->host);
        int cursor = path_start + host_length;
        if (port_index >= 0) cursor = path_start + port_index - path_start + 1;
        if (url[cursor] == ':') cursor++;
        if (url[cursor] == '/' || url[cursor] == '\0')
        {
            browser_copy(out->path, "/", sizeof(out->path));
        }
        else
        {
            out->path[0] = '/';
            out->path[1] = '\0';
        }
        if (url[cursor] == '/')
        {
            int path_index = 0;
            while (url[cursor] != '\0')
            {
                out->path[path_index++] = url[cursor++];
                if (path_index + 1 >= (int)sizeof(out->path)) break;
            }
            out->path[path_index] = '\0';
        }
    }
    if (out->host[0] == '\0')
    {
        browser_copy(out->host, "localhost", sizeof(out->host));
    }
    return 0;
}

static void browser_find_header_value(const char *response, const char *name, char *out, int out_size)
{
    int name_len = browser_strlen(name);
    int index = 0;
    int header_start = 0;
    int cursor = 0;
    if (response == 0 || out == 0 || out_size <= 0) return;
    out[0] = '\0';
    while (response[index] != '\0')
    {
        if (response[index] == '\n')
        {
            char line[256];
            int line_len = 0;
            int scan = header_start;
            while (scan < index && line_len + 1 < (int)sizeof(line))
            {
                line[line_len++] = response[scan++];
            }
            line[line_len] = '\0';
            browser_trim_right(line);
            if (browser_starts_with(line, name))
            {
                cursor = name_len;
                while (line[cursor] == ' ' || line[cursor] == '\t') cursor++;
                browser_copy(out, line + cursor, out_size);
                return;
            }
            header_start = index + 1;
        }
        index++;
    }
}
int browser_parse_http_response(const char *response, BrowserHttpResponse *out)
{
    int index = 0;
    int status_line_end = 0;
    int header_end = -1;
    int body_start = 0;
    int content_len = 0;
    char status_line[256];
    if (response == 0 || out == 0) return -1;
    out->status_code = 0;
    out->reason[0] = '\0';
    out->location[0] = '\0';
    out->has_content_length = 0;
    out->content_length = 0;
    out->chunked = 0;
    out->mime[0] = '\0';
    out->body[0] = '\0';
    while (response[index] != '\0' && response[index] != '\n') index++;
    status_line_end = index;
    if (status_line_end <= 0) return -1;
    browser_copy(status_line, response, sizeof(status_line));
    if (browser_starts_with(status_line, "HTTP/"))
    {
        int code_start = 0;
        while (status_line[code_start] != ' ' && status_line[code_start] != '\0') code_start++;
        while (status_line[code_start] == ' ' && status_line[code_start] != '\0') code_start++;
        if (status_line[code_start] >= '0' && status_line[code_start] <= '9')
        {
            out->status_code = (status_line[code_start] - '0') * 100;
            if (status_line[code_start + 1] >= '0' && status_line[code_start + 1] <= '9')
            {
                out->status_code += (status_line[code_start + 1] - '0') * 10;
            }
            if (status_line[code_start + 2] >= '0' && status_line[code_start + 2] <= '9')
            {
                out->status_code += (status_line[code_start + 2] - '0');
            }
        }
        if (status_line[code_start + 3] != '\0')
        {
            int reason_cursor = code_start + 3;
            while (status_line[reason_cursor] == ' ') reason_cursor++;
            browser_copy(out->reason, status_line + reason_cursor, sizeof(out->reason));
        }
    }
    while (response[index] != '\0')
    {
        if (response[index] == '\n' && response[index + 1] == '\n')
        {
            header_end = index + 2;
            break;
        }
        if (response[index] == '\r' && response[index + 1] == '\n' && response[index + 2] == '\r' && response[index + 3] == '\n')
        {
            header_end = index + 4;
            break;
        }
        index++;
    }
    if (header_end < 0) return -2;
    body_start = header_end;
    if (response[body_start] != '\0')
    {
        int write_index = 0;
        while (response[body_start + write_index] != '\0' && write_index + 1 < (int)sizeof(out->body))
        {
            out->body[write_index] = response[body_start + write_index];
            write_index++;
        }
        out->body[write_index] = '\0';
    }

    {
        char length_value[32];
        browser_find_header_value(response, "Content-Length:", length_value, sizeof(length_value));
        if (length_value[0] != '\0')
        {
            int digits = 0;
            if (length_value[0] >= '0' && length_value[0] <= '9')
            {
                content_len = 0;
                while (length_value[digits] >= '0' && length_value[digits] <= '9')
                {
                    content_len = content_len * 10 + (length_value[digits] - '0');
                    digits++;
                }
                out->content_length = content_len;
                out->has_content_length = 1;
            }
        }
    }
    {
        char chunked_value[32];
        browser_find_header_value(response, "Transfer-Encoding:", chunked_value, sizeof(chunked_value));
        if (browser_starts_with(chunked_value, "chunked")) out->chunked = 1;
    }
    {
        char location_value[256];
        browser_find_header_value(response, "Location:", location_value, sizeof(location_value));
        if (location_value[0] != '\0') browser_copy(out->location, location_value, sizeof(out->location));
    }
    {
        char content_type[128];
        browser_find_header_value(response, "Content-Type:", content_type, sizeof(content_type));
        if (content_type[0] != '\0') browser_copy(out->mime, content_type, sizeof(out->mime));
    }
    return 0;
}

static BrowserNode *browser_new_node(BrowserDocument *document, const char *tag)
{
    BrowserNode *node;
    if (document == 0 || document->node_count >= 128) return 0;
    node = &document->nodes[document->node_count++];
    node->parent = 0;
    node->child_count = 0;
    node->text[0] = '\0';
    node->href[0] = '\0';
    node->tag[0] = '\0';
    if (tag != 0) browser_copy(node->tag, tag, sizeof(node->tag));
    return node;
}

static void browser_add_child(BrowserDocument *document, BrowserNode *parent, BrowserNode *child)
{
    if (document == 0 || parent == 0 || child == 0) return;
    if (parent->child_count >= BROWSER_ROOT_CHILDREN) return;
    child->parent = parent;
    parent->children[parent->child_count++] = child;
}

static void browser_append_text_node(BrowserDocument *document, BrowserNode *parent, const char *text)
{
    BrowserNode *node;
    if (document == 0 || parent == 0 || text == 0) return;
    node = browser_new_node(document, "text");
    if (node == 0) return;
    browser_copy(node->text, text, sizeof(node->text));
    browser_add_child(document, parent, node);
}

static void browser_parse_tag_attributes(const char *tag_text, char *tag_name, char *href_value)
{
    int index = 0;
    int len = 0;
    int attr_name_index = 0;
    int attr_value_index = 0;
    char attr_name[32];
    char attr_value[256];
    if (tag_name == 0) return;
    tag_name[0] = '\0';
    if (href_value != 0) href_value[0] = '\0';
    while (tag_text[index] == '<' || tag_text[index] == ' ') index++;
    while (tag_text[index] != '\0' && tag_text[index] != ' ' && tag_text[index] != '>' && tag_text[index] != '/')
    {
        tag_name[len++] = tag_text[index++];
        if (len + 1 >= 32) break;
    }
    tag_name[len] = '\0';
    browser_lower(tag_name);
    while (tag_text[index] != '\0')
    {
        if (tag_text[index] == '>' || tag_text[index] == '/') break;
        if (tag_text[index] == ' ' || tag_text[index] == '\n' || tag_text[index] == '\t')
        {
            index++;
            continue;
        }
        attr_name_index = 0;
        attr_name[0] = '\0';
        while (tag_text[index] != '\0' && tag_text[index] != '=' && tag_text[index] != ' ' && tag_text[index] != '>' && tag_text[index] != '/')
        {
            attr_name[attr_name_index++] = tag_text[index++];
            if (attr_name_index + 1 >= (int)sizeof(attr_name)) break;
        }
        attr_name[attr_name_index] = '\0';
        while (tag_text[index] == ' ' || tag_text[index] == '\t') index++;
        if (tag_text[index] == '=')
        {
            index++;
            while (tag_text[index] == ' ' || tag_text[index] == '\t') index++;
            attr_value_index = 0;
            attr_value[0] = '\0';
            if (tag_text[index] == '"' || tag_text[index] == '\'') index++;
            while (tag_text[index] != '\0' && tag_text[index] != '"' && tag_text[index] != '\'' && tag_text[index] != '>' && tag_text[index] != ' ')
            {
                attr_value[attr_value_index++] = tag_text[index++];
                if (attr_value_index + 1 >= (int)sizeof(attr_value)) break;
            }
            attr_value[attr_value_index] = '\0';
            if (href_value != 0 && browser_equals(attr_name, "href"))
            {
                browser_copy(href_value, attr_value, 256);
            }
            if (tag_text[index] == '"' || tag_text[index] == '\'') index++;
        }
        while (tag_text[index] != '\0' && tag_text[index] != '>' && tag_text[index] != '/') index++;
    }
}

int browser_parse_html_document(const char *html, BrowserDocument *document)
{
    int index = 0;
    BrowserNode *current = 0;
    char tag_name[32];
    char href_value[256];
    if (html == 0 || document == 0) return -1;
    document->root.tag[0] = '\0';
    document->node_count = 0;
    document->error = 0;
    current = browser_new_node(document, "document");
    if (current == 0) return -1;
    while (html[index] != '\0')
    {
        if (html[index] == '<')
        {
            int start = index + 1;
            int end = start;
            while (html[end] != '\0' && html[end] != '>') end++;
            if (html[end] == '>')
            {
                char tag_text[256];
                int tag_len = 0;
                int tag_index = start;
                while (tag_index < end && tag_len + 1 < (int)sizeof(tag_text))
                {
                    tag_text[tag_len++] = html[tag_index++];
                }
                tag_text[tag_len] = '\0';
                if (tag_text[0] == '/' && tag_text[1] != '\0')
                {
                    char close_name[32];
                    browser_copy(close_name, tag_text + 1, sizeof(close_name));
                    browser_lower(close_name);
                    if (current != 0 && current->parent != 0 && browser_equals(current->tag, close_name))
                    {
                        current = current->parent;
                    }
                    index = end + 1;
                    continue;
                }
                if (tag_text[0] == '!')
                {
                    index = end + 1;
                    continue;
                }
                tag_name[0] = '\0';
                href_value[0] = '\0';
                browser_parse_tag_attributes(tag_text, tag_name, href_value);
                if (tag_name[0] != '\0')
                {
                    BrowserNode *child = browser_new_node(document, tag_name);
                    if (child != 0)
                    {
                        browser_add_child(document, current, child);
                        if (href_value[0] != '\0') browser_copy(child->href, href_value, sizeof(child->href));
                        current = child;
                    }
                }
                index = end + 1;
                continue;
            }
        }
        else
        {
            int text_start = index;
            while (html[index] != '\0' && html[index] != '<') index++;
            if (index > text_start)
            {
                char chunk[256];
                int chunk_len = 0;
                int scan = text_start;
                while (scan < index && chunk_len + 1 < (int)sizeof(chunk))
                {
                    chunk[chunk_len++] = html[scan++];
                }
                chunk[chunk_len] = '\0';
                if (current != 0 && chunk[0] != '\0')
                {
                    browser_append_text_node(document, current, chunk);
                }
            }
            continue;
        }
        index++;
    }
    document->root = document->nodes[0];
    return 0;
}

static BrowserWindowState *browser_window_state_for(ORWindow *window)
{
    int index;
    for (index = 0; index < ORGUI_MAX_WINDOWS; index++)
    {
        if (browser_state_storage[index].window == window) return &browser_state_storage[index];
    }
    for (index = 0; index < ORGUI_MAX_WINDOWS; index++)
    {
        if (browser_state_storage[index].window == 0)
        {
            browser_state_storage[index].window = window;
            return &browser_state_storage[index];
        }
    }
    return 0;
}

static void browser_draw_status(ORWindow *window, BrowserWindowState *state)
{
    int status_y = window->y + window->height - 22;
    fb_fill_rect(window->x + 4, status_y, window->width - 8, 16, OR_COLOR_PANEL);
    fb_draw_rect(window->x + 4, status_y, window->width - 8, 16, OR_COLOR_BORDER);
    ORgui_draw_text(window->x + 10, status_y + 3, state->status, OR_COLOR_FIRE_RED);
}

static void browser_render_document(BrowserWindowState *state, ORWindow *window)
{
    BrowserDocument document;
    char page_text[BROWSER_TEXT_SIZE];
    int line_y = window->y + 68;
    int page_right = window->x + window->width - 18;
    int item_count = 0;
    int page_left = window->x + 12;
    BrowserNode *node = 0;
    int visible_lines = 0;
    state->link_count = 0;
    browser_parse_html_document(state->html, &document);
    browser_copy(page_text, state->html, sizeof(page_text));
    if (page_text[0] == '\0')
    {
        browser_copy(state->status, "No page loaded", sizeof(state->status));
    }
    if (document.node_count == 0)
    {
        ORgui_draw_text(page_left, line_y, "Browser error: unable to parse HTML.", OR_COLOR_FIRE_RED);
        return;
    }
    node = &document.nodes[0];
    if (node->child_count > 0 && browser_equals(node->children[0]->tag, "document"))
    {
        node = node->children[0];
    }
    for (item_count = 0; item_count < node->child_count && line_y < window->y + window->height - 38; item_count++)
    {
        BrowserNode *child = node->children[item_count];
        if (browser_equals(child->tag, "text") && child->text[0] != '\0')
        {
            char text_line[BROWSER_TEXT_LINE_SIZE];
            char *cursor = child->text;
            while (cursor != 0 && *cursor != '\0' && visible_lines < 90)
            {
                int draw_len = 0;
                while (cursor[draw_len] != '\0' && cursor[draw_len] != '\n' && draw_len + 1 < (int)sizeof(text_line))
                {
                    char character = cursor[draw_len];
                    text_line[draw_len++] = character;
                }
                text_line[draw_len] = '\0';
                if (text_line[0] != '\0')
                {
                    if (browser_starts_with(text_line, " ") || browser_starts_with(text_line, "\t"))
                    {
                        int remap = 0;
                        while (text_line[remap] == ' ' || text_line[remap] == '\t') remap++;
                        browser_copy(text_line, text_line + remap, sizeof(text_line));
                    }
                    if (line_y + 20 > window->y + window->height - 38) break;
                    if (state->link_count < BROWSER_MAX_LINKS && child->href[0] != '\0')
                    {
                        browser_copy(state->links[state->link_count].href, child->href, sizeof(state->links[state->link_count].href));
                        state->links[state->link_count].y = line_y;
                        state->links[state->link_count].end_y = line_y + 16;
                        state->link_count++;
                    }
                    ORgui_draw_text(page_left, line_y, text_line, OR_COLOR_FIRE_RED);
                    line_y += 16;
                    visible_lines++;
                }
                if (*cursor == '\n') cursor++;
                cursor += draw_len;
                if (draw_len == 0 && *cursor == '\0') break;
                if (cursor[0] == '\0') break;
                if (cursor[0] == '\n') cursor++;
                if (visible_lines >= 90) break;
            }
        }
        else if (browser_equals(child->tag, "h1") || browser_equals(child->tag, "h2") || browser_equals(child->tag, "h3") || browser_equals(child->tag, "h4") || browser_equals(child->tag, "h5") || browser_equals(child->tag, "p") || browser_equals(child->tag, "div") || browser_equals(child->tag, "li"))
        {
            if (child->text[0] != '\0')
            {
                ORgui_draw_text(page_left, line_y, child->text, OR_COLOR_FIRE_RED);
                line_y += 16;
            }
            if (child->href[0] != '\0')
            {
                browser_copy(state->links[state->link_count].href, child->href, sizeof(state->links[state->link_count].href));
                state->links[state->link_count].y = line_y - 16;
                state->links[state->link_count].end_y = line_y;
                state->link_count++;
            }
        }
        else if (browser_equals(child->tag, "a") && child->href[0] != '\0')
        {
            ORgui_draw_text(page_left, line_y, child->text, OR_COLOR_FIRE_RED);
            browser_copy(state->links[state->link_count].href, child->href, sizeof(state->links[state->link_count].href));
            state->links[state->link_count].y = line_y;
            state->links[state->link_count].end_y = line_y + 16;
            state->link_count++;
            line_y += 16;
        }
    }
    if (document.node_count == 0)
    {
        ORgui_draw_text(page_left, line_y, "No content available.", OR_COLOR_FIRE_RED);
    }
    if (state->scroll_y > 0)
    {
        ORgui_draw_text(page_right - 20, window->y + 44, "^", OR_COLOR_FIRE_ORANGE);
    }
    else
    {
        ORgui_draw_text(page_right - 20, window->y + 44, "-", OR_COLOR_FIRE_ORANGE);
    }
    if (state->is_loading)
    {
        ORgui_draw_text(page_right - 26, window->y + 82, "loading", OR_COLOR_FIRE_ORANGE);
    }
}

static void browser_window_draw(ORWindow *window)
{
    BrowserWindowState *state = browser_window_state_for(window);
    int content_top = window->y + 58;
    int content_left = window->x + 6;
    int content_width = window->width - 12;
    int content_height = window->height - 90;

    if (state == 0) return;
    fb_fill_rect(window->x + 4, window->y + 24, window->width - 8, 28, OR_COLOR_PANEL);
    fb_draw_rect(window->x + 4, window->y + 24, window->width - 8, 28, OR_COLOR_BORDER);

    ORgui_draw_button(window->x + 10, window->y + 30, 42, 18, "<", 0);
    ORgui_draw_button(window->x + 56, window->y + 30, 42, 18, ">", 0);
    ORgui_draw_button(window->x + 102, window->y + 30, 56, 18, "Reload", 0);
    ORgui_draw_button(window->x + 164, window->y + 30, 42, 18, "Stop", 0);

    fb_fill_rect(window->x + 212, window->y + 28, window->width - 224, 22, OR_COLOR_WINDOW);
    fb_draw_rect(window->x + 212, window->y + 28, window->width - 224, 22, OR_COLOR_BORDER);
    ORgui_draw_text(window->x + 220, window->y + 34, state->typed_url[0] == '\0' ? state->url : state->typed_url, OR_COLOR_FIRE_RED);

    fb_fill_rect(content_left, content_top, content_width, content_height, OR_COLOR_WINDOW);
    fb_draw_rect(content_left, content_top, content_width, content_height, OR_COLOR_BORDER);
    ORgui_draw_text(content_left + 8, content_top + 8, state->title, OR_COLOR_FIRE_RED);
    ORgui_draw_text(content_left + 8, content_top + 28, state->status, OR_COLOR_FIRE_ORANGE);
    browser_render_document(state, window);
    browser_draw_status(window, state);
    ORgui_draw_text(window->x + 8, window->y + 6, state->title[0] != '\0' ? state->title : "browser", OR_COLOR_TEXT);
}

static void browser_window_event(ORWindow *window, const OREvent *event)
{
    BrowserWindowState *state = browser_window_state_for(window);
    if (state == 0) return;
    if (event->type == OR_EVENT_MOUSE_DOWN)
    {
        int x = event->x;
        int y = event->y;
        int index;
        if (x >= window->x + 10 && x < window->x + 52 && y >= window->y + 30 && y <= window->y + 48)
        {
            if (state->history_index > 0)
            {
                state->history_index--;
                browser_copy(state->url, state->history[state->history_index], sizeof(state->url));
                browser_copy(state->typed_url, state->url, sizeof(state->typed_url));
                browser_copy(state->status, "Back", sizeof(state->status));
            }
            return;
        }
        if (x >= window->x + 56 && x < window->x + 98 && y >= window->y + 30 && y <= window->y + 48)
        {
            if (state->history_index + 1 < state->history_count)
            {
                state->history_index++;
                browser_copy(state->url, state->history[state->history_index], sizeof(state->url));
                browser_copy(state->typed_url, state->url, sizeof(state->typed_url));
                browser_copy(state->status, "Forward", sizeof(state->status));
            }
            return;
        }
        if (x >= window->x + 102 && x < window->x + 158 && y >= window->y + 30 && y <= window->y + 48)
        {
            browser_copy(state->status, "Reloading ...", sizeof(state->status));
            state->is_loading = 1;
            state->loading_started = 1;
            return;
        }
        if (x >= window->x + 164 && x < window->x + 206 && y >= window->y + 30 && y <= window->y + 48)
        {
            state->is_loading = 0;
            browser_copy(state->status, "Stopped", sizeof(state->status));
            return;
        }
        if (x >= window->x + 212 && x < window->x + window->width - 12 && y >= window->y + 28 && y <= window->y + 50)
        {
            state->is_editing = 1;
            state->typed_url[0] = '\0';
            return;
        }
        for (index = 0; index < state->link_count; index++)
        {
            if (y >= state->links[index].y && y <= state->links[index].end_y && state->links[index].href[0] != '\0')
            {
                browser_resolve_relative_url(state->url, state->links[index].href, state->typed_url, sizeof(state->typed_url));
                browser_copy(state->url, state->typed_url, sizeof(state->url));
                browser_copy(state->status, "Followed link", sizeof(state->status));
                return;
            }
        }
        return;
    }
    if (event->type == OR_EVENT_KEY_DOWN)
    {
        if (!state->is_editing)
        {
            if (event->key == '\n' || event->key == '\r')
            {
                state->is_editing = 0;
                browser_copy(state->url, state->typed_url, sizeof(state->url));
                browser_copy(state->status, "Loading page", sizeof(state->status));
                state->is_loading = 1;
            }
            return;
        }
        if (event->key == '\b')
        {
            int len = browser_strlen(state->typed_url);
            if (len > 0) state->typed_url[len - 1] = '\0';
            return;
        }
        if (event->key == '\n' || event->key == '\r')
        {
            state->is_editing = 0;
            browser_copy(state->url, state->typed_url, sizeof(state->url));
            browser_copy(state->status, "Loading page", sizeof(state->status));
            state->is_loading = 1;
            return;
        }
        if (event->key >= 32 && event->key < 127 && browser_strlen(state->typed_url) + 1 < (int)sizeof(state->typed_url))
        {
            int len = browser_strlen(state->typed_url);
            state->typed_url[len] = (char)event->key;
            state->typed_url[len + 1] = '\0';
        }
    }
}

void browser_window_open(ORWindow *window)
{
    BrowserWindowState *state = browser_window_state_for(window);
    if (state == 0) return;
    state->window = window;
    browser_copy(state->url, "http://example.com", sizeof(state->url));
    browser_copy(state->typed_url, state->url, sizeof(state->typed_url));
    browser_copy(state->title, "browser", sizeof(state->title));
    browser_copy(state->status, "Ready", sizeof(state->status));
    state->scroll_y = 0;
    state->history_count = 0;
    state->history_index = 0;
    state->is_editing = 0;
    state->is_loading = 0;
    state->loading_started = 0;
    state->html[0] = '\0';
    browser_copy(state->html,
                 "<html><body><h1>browser</h1><p>Network access is not available in this build. The browser window is active and ready for a URL, but HTTP requests require the socket/DNS stack from the current OS build.</p><p><a href=\"https://example.com\">Try https example</a></p></body></html>",
                 sizeof(state->html));
    browser_copy(state->title, "browser", sizeof(state->title));
    window->on_draw = browser_window_draw;
    window->on_event = browser_window_event;
}

int browser_parse_html_document_test(void);
int browser_parse_http_response_test(void);

int browser_parse_html_document_test(void)
{
    BrowserDocument document;
    const char *html = "<html><body><h1>Title</h1><p>Hello <a href=\"/x\">link</a></p></body></html>";
    return browser_parse_html_document(html, &document);
}

int browser_parse_http_response_test(void)
{
    BrowserHttpResponse response;
    const char *text = "HTTP/1.1 200 OK\r\nContent-Length: 5\r\nContent-Type: text/plain\r\n\r\nhello";
    return browser_parse_http_response(text, &response);
}
