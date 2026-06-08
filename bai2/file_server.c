#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <limits.h>

#define PORT 8081
#define BUFFER_SIZE 8192

char ROOT_DIR[PATH_MAX];

void send_error(int client_socket, int status_code, const char *status_text, const char *message) {
    char body[2048];
    char header[2048];

    snprintf(body, sizeof(body),
             "<!DOCTYPE html>"
             "<html><head><meta charset='UTF-8'><title>%d %s</title></head>"
             "<body>"
             "<h1>%d %s</h1>"
             "<p>%s</p>"
             "<a href='/'>Quay về thư mục gốc</a>"
             "</body></html>",
             status_code, status_text, status_code, status_text, message);

    snprintf(header, sizeof(header),
             "HTTP/1.1 %d %s\r\n"
             "Content-Type: text/html; charset=UTF-8\r\n"
             "Content-Length: %ld\r\n"
             "Connection: close\r\n"
             "\r\n",
             status_code, status_text, strlen(body));

    send(client_socket, header, strlen(header), 0);
    send(client_socket, body, strlen(body), 0);
}

int hex_to_int(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return 0;
}

void url_decode(char *dest, const char *src) {
    while (*src) {
        if (*src == '%' &&
            isxdigit((unsigned char)*(src + 1)) &&
            isxdigit((unsigned char)*(src + 2))) {

            *dest = (char)(hex_to_int(*(src + 1)) * 16 + hex_to_int(*(src + 2)));
            src += 3;
        } else if (*src == '+') {
            *dest = ' ';
            src++;
        } else {
            *dest = *src;
            src++;
        }

        dest++;
    }

    *dest = '\0';
}

void url_encode(char *dest, const char *src) {
    const char *hex = "0123456789ABCDEF";

    while (*src) {
        unsigned char c = (unsigned char)*src;

        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '/') {
            *dest++ = c;
        } else if (c == ' ') {
            *dest++ = '%';
            *dest++ = '2';
            *dest++ = '0';
        } else {
            *dest++ = '%';
            *dest++ = hex[c >> 4];
            *dest++ = hex[c & 15];
        }

        src++;
    }

    *dest = '\0';
}

void html_escape(char *dest, const char *src) {
    while (*src) {
        if (*src == '<') {
            strcpy(dest, "&lt;");
            dest += 4;
        } else if (*src == '>') {
            strcpy(dest, "&gt;");
            dest += 4;
        } else if (*src == '&') {
            strcpy(dest, "&amp;");
            dest += 5;
        } else if (*src == '"') {
            strcpy(dest, "&quot;");
            dest += 6;
        } else {
            *dest++ = *src;
        }

        src++;
    }

    *dest = '\0';
}

const char *get_mime_type(const char *path) {
    const char *ext = strrchr(path, '.');

    if (ext == NULL) {
        return "application/octet-stream";
    }

    ext++;

    if (strcasecmp(ext, "txt") == 0) return "text/plain; charset=UTF-8";
    if (strcasecmp(ext, "c") == 0) return "text/plain; charset=UTF-8";
    if (strcasecmp(ext, "h") == 0) return "text/plain; charset=UTF-8";
    if (strcasecmp(ext, "html") == 0) return "text/html; charset=UTF-8";
    if (strcasecmp(ext, "css") == 0) return "text/css; charset=UTF-8";
    if (strcasecmp(ext, "js") == 0) return "application/javascript; charset=UTF-8";

    if (strcasecmp(ext, "jpg") == 0) return "image/jpeg";
    if (strcasecmp(ext, "jpeg") == 0) return "image/jpeg";
    if (strcasecmp(ext, "png") == 0) return "image/png";
    if (strcasecmp(ext, "gif") == 0) return "image/gif";
    if (strcasecmp(ext, "bmp") == 0) return "image/bmp";
    if (strcasecmp(ext, "webp") == 0) return "image/webp";
    if (strcasecmp(ext, "svg") == 0) return "image/svg+xml";

    if (strcasecmp(ext, "mp3") == 0) return "audio/mpeg";
    if (strcasecmp(ext, "wav") == 0) return "audio/wav";
    if (strcasecmp(ext, "ogg") == 0) return "audio/ogg";

    if (strcasecmp(ext, "mp4") == 0) return "video/mp4";
    if (strcasecmp(ext, "webm") == 0) return "video/webm";
    if (strcasecmp(ext, "avi") == 0) return "video/x-msvideo";

    if (strcasecmp(ext, "pdf") == 0) return "application/pdf";

    return "application/octet-stream";
}

int is_safe_path(const char *real_path) {
    return strncmp(real_path, ROOT_DIR, strlen(ROOT_DIR)) == 0;
}

void send_file(int client_socket, const char *real_path) {
    FILE *file = fopen(real_path, "rb");

    if (file == NULL) {
        send_error(client_socket, 403, "Forbidden", "Không thể mở file.");
        return;
    }

    struct stat file_stat;

    if (stat(real_path, &file_stat) < 0) {
        fclose(file);
        send_error(client_socket, 500, "Internal Server Error", "Không thể lấy thông tin file.");
        return;
    }

    const char *mime_type = get_mime_type(real_path);

    char header[2048];

    snprintf(header, sizeof(header),
             "HTTP/1.1 200 OK\r\n"
             "Content-Type: %s\r\n"
             "Content-Length: %ld\r\n"
             "Connection: close\r\n"
             "\r\n",
             mime_type, file_stat.st_size);

    send(client_socket, header, strlen(header), 0);

    char buffer[BUFFER_SIZE];
    size_t bytes_read;

    while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        send(client_socket, buffer, bytes_read, 0);
    }

    fclose(file);
}

void send_directory_listing(int client_socket, const char *real_path, const char *url_path) {
    DIR *dir = opendir(real_path);

    if (dir == NULL) {
        send_error(client_socket, 403, "Forbidden", "Không thể mở thư mục.");
        return;
    }

    char body[65536];

    snprintf(body, sizeof(body),
             "<!DOCTYPE html>"
             "<html>"
             "<head>"
             "<meta charset='UTF-8'>"
             "<title>HTTP File Server</title>"
             "<style>"
             "body { font-family: Arial; background: #f4f4f4; padding: 30px; }"
             ".box { background: white; padding: 25px; border-radius: 10px; max-width: 800px; margin: auto; box-shadow: 0 0 10px #ccc; }"
             "h1 { color: #333; }"
             "ul { line-height: 2; }"
             "a { text-decoration: none; color: #0066cc; }"
             "a:hover { text-decoration: underline; }"
             ".folder { font-weight: bold; }"
             ".file { font-style: italic; }"
             "</style>"
             "</head>"
             "<body>"
             "<div class='box'>"
             "<h1>Danh sách thư mục và tập tin</h1>"
             "<p>Đường dẫn hiện tại: <b>%s</b></p>"
             "<ul>",
             url_path);

    if (strcmp(url_path, "/") != 0) {
        strcat(body, "<li><a class='folder' href='../'>.. Quay lại thư mục cha</a></li>");
    }

    struct dirent *entry;

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        char item_real_path[PATH_MAX];
        snprintf(item_real_path, sizeof(item_real_path), "%s/%s", real_path, entry->d_name);

        struct stat item_stat;

        if (stat(item_real_path, &item_stat) < 0) {
            continue;
        }

        char escaped_name[2048];
        char encoded_name[2048];

        html_escape(escaped_name, entry->d_name);
        url_encode(encoded_name, entry->d_name);

        char link[4096];

        if (S_ISDIR(item_stat.st_mode)) {
            snprintf(link, sizeof(link),
                     "<li><a class='folder' href='%s/'>%s/</a></li>",
                     encoded_name, escaped_name);
        } else {
            snprintf(link, sizeof(link),
                     "<li><a class='file' href='%s'>%s</a></li>",
                     encoded_name, escaped_name);
        }

        if (strlen(body) + strlen(link) < sizeof(body) - 100) {
            strcat(body, link);
        }
    }

    closedir(dir);

    strcat(body,
           "</ul>"
           "</div>"
           "</body>"
           "</html>");

    char header[2048];

    snprintf(header, sizeof(header),
             "HTTP/1.1 200 OK\r\n"
             "Content-Type: text/html; charset=UTF-8\r\n"
             "Content-Length: %ld\r\n"
             "Connection: close\r\n"
             "\r\n",
             strlen(body));

    send(client_socket, header, strlen(header), 0);
    send(client_socket, body, strlen(body), 0);
}

void handle_client(int client_socket) {
    char buffer[BUFFER_SIZE];
    char method[16];
    char request_path[2048];

    memset(buffer, 0, sizeof(buffer));

    int bytes_received = recv(client_socket, buffer, sizeof(buffer) - 1, 0);

    if (bytes_received <= 0) {
        close(client_socket);
        return;
    }

    sscanf(buffer, "%15s %2047s", method, request_path);

    if (strcmp(method, "GET") != 0) {
        send_error(client_socket, 405, "Method Not Allowed", "Server chỉ hỗ trợ phương thức GET.");
        close(client_socket);
        return;
    }

    char *query = strchr(request_path, '?');

    if (query != NULL) {
        *query = '\0';
    }

    char decoded_path[2048];
    url_decode(decoded_path, request_path);

    if (strstr(decoded_path, "..") != NULL) {
        send_error(client_socket, 403, "Forbidden", "Không được truy cập đường dẫn chứa '..'.");
        close(client_socket);
        return;
    }

    char full_path[PATH_MAX];

    if (strcmp(decoded_path, "/") == 0) {
        snprintf(full_path, sizeof(full_path), "%s", ROOT_DIR);
    } else {
        snprintf(full_path, sizeof(full_path), "%s%s", ROOT_DIR, decoded_path);
    }

    char real_path[PATH_MAX];

    if (realpath(full_path, real_path) == NULL) {
        send_error(client_socket, 404, "Not Found", "Không tìm thấy file hoặc thư mục.");
        close(client_socket);
        return;
    }

    if (!is_safe_path(real_path)) {
        send_error(client_socket, 403, "Forbidden", "Không được truy cập bên ngoài thư mục server.");
        close(client_socket);
        return;
    }

    struct stat path_stat;

    if (stat(real_path, &path_stat) < 0) {
        send_error(client_socket, 500, "Internal Server Error", "Không thể đọc thông tin đường dẫn.");
        close(client_socket);
        return;
    }

    if (S_ISDIR(path_stat.st_mode)) {
        send_directory_listing(client_socket, real_path, decoded_path);
    } else if (S_ISREG(path_stat.st_mode)) {
        send_file(client_socket, real_path);
    } else {
        send_error(client_socket, 403, "Forbidden", "Không hỗ trợ kiểu file này.");
    }

    close(client_socket);
}

int main() {
    if (getcwd(ROOT_DIR, sizeof(ROOT_DIR)) == NULL) {
        perror("Không lấy được thư mục hiện tại");
        exit(1);
    }

    int server_socket;
    int client_socket;

    struct sockaddr_in server_addr;
    struct sockaddr_in client_addr;

    socklen_t client_len = sizeof(client_addr);

    server_socket = socket(AF_INET, SOCK_STREAM, 0);

    if (server_socket < 0) {
        perror("Không tạo được socket");
        exit(1);
    }

    int opt = 1;
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind thất bại");
        close(server_socket);
        exit(1);
    }

    if (listen(server_socket, 10) < 0) {
        perror("Listen thất bại");
        close(server_socket);
        exit(1);
    }

    printf("HTTP File Server đang chạy tại: http://localhost:%d\n", PORT);
    printf("Thư mục gốc của server: %s\n", ROOT_DIR);
    printf("Nhấn Ctrl + C để dừng server.\n");

    while (1) {
        client_socket = accept(server_socket,
                               (struct sockaddr *)&client_addr,
                               &client_len);

        if (client_socket < 0) {
            perror("Accept thất bại");
            continue;
        }

        handle_client(client_socket);
    }

    close(server_socket);

    return 0;
}
