#include <assert.h>
#include <string.h>

#include "browser.h"

int main(void)
{
    BrowserUrl url;
    BrowserHttpResponse response;
    BrowserDocument document;
    char resolved[256];

    assert(browser_parse_url("http://example.com/path?a=1", &url) == 0);
    assert(strcmp(url.scheme, "http") == 0);
    assert(strcmp(url.host, "example.com") == 0);
    assert(strstr(url.path, "/path") != 0);

    assert(browser_parse_http_response("HTTP/1.1 200 OK\r\nContent-Length: 5\r\n\r\nhello", &response) == 0);
    assert(response.status_code == 200);
    assert(response.content_length == 5);
    assert(strcmp(response.body, "hello") == 0);

    assert(browser_parse_html_document("<html><body><h1>Title</h1><p>Hello <a href=\"/x\">link</a></p></body></html>", &document) == 0);
    assert(document.node_count > 0);

    browser_resolve_relative_url("http://example.com/docs/page.html", "../about.html", resolved, sizeof(resolved));
    assert(strstr(resolved, "example.com") != 0);
    assert(strstr(resolved, "about.html") != 0);

    return 0;
}
