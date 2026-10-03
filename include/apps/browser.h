#pragma once

#define BROWSER_URL_SIZE 256
#define BROWSER_STATUS_SIZE 160
#define BROWSER_TITLE_SIZE 128
#define BROWSER_TEXT_SIZE 2048
#define BROWSER_HISTORY_SIZE 8
#define BROWSER_MAX_LINKS 32
#define BROWSER_ROOT_CHILDREN 32
#define BROWSER_TEXT_LINE_SIZE 256

typedef struct {
    char scheme[16];
    char host[128];
    char path[256];
    int port;
} BrowserUrl;

typedef struct {
    int status_code;
    char reason[64];
    char location[256];
    int has_content_length;
    int content_length;
    int chunked;
    char mime[64];
    char body[BROWSER_TEXT_SIZE];
} BrowserHttpResponse;

typedef struct BrowserNode {
    char tag[32];
    char text[256];
    char href[256];
    int child_count;
    struct BrowserNode *children[BROWSER_ROOT_CHILDREN];
    struct BrowserNode *parent;
} BrowserNode;

typedef struct {
    BrowserNode root;
    BrowserNode nodes[128];
    int node_count;
    int error;
} BrowserDocument;

typedef struct {
    int y;
    int end_y;
    char href[256];
} BrowserLinkEntry;

int browser_parse_url(const char *url, BrowserUrl *out);
void browser_resolve_relative_url(const char *base_url, const char *relative, char *out, int out_size);
int browser_parse_http_response(const char *response, BrowserHttpResponse *out);
int browser_parse_html_document(const char *html, BrowserDocument *document);
