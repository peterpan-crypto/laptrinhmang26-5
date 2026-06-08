#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define PORT 8080
#define BUFFER_SIZE 8192

void url_decode(char *dest, const char *src) {
    char a, b;

    while (*src) {
        if ((*src == '%') &&
            ((a = src[1]) && (b = src[2])) &&
            isxdigit(a) && isxdigit(b)) {

            if (a >= 'a') a -= 'a' - 'A';
            if (a >= 'A') a = a - 'A' + 10;
            else a -= '0';

            if (b >= 'a') b -= 'a' - 'A';
            if (b >= 'A') b = b - 'A' + 10;
            else b -= '0';

            *dest++ = 16 * a + b;
            src += 3;
        } else if (*src == '+') {
            *dest++ = ' ';
            src++;
        } else {
            *dest++ = *src++;
        }
    }

    *dest = '\0';
}

int get_param(const char *params, const char *key, char *value, int value_size) {
    char temp[2048];
    strncpy(temp, params, sizeof(temp) - 1);
    temp[sizeof(temp) - 1] = '\0';

    char *token = strtok(temp, "&");

    while (token != NULL) {
        char *equal_sign = strchr(token, '=');

        if (equal_sign != NULL) {
            *equal_sign = '\0';

            char *param_key = token;
            char *param_value = equal_sign + 1;

            if (strcmp(param_key, key) == 0) {
                char decoded[512];
                url_decode(decoded, param_value);

                strncpy(value, decoded, value_size - 1);
                value[value_size - 1] = '\0';

                return 1;
            }
        }

        token = strtok(NULL, "&");
    }

    return 0;
}

void send_response(int client_socket, const char *html) {
    char response[BUFFER_SIZE];

    sprintf(response,
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/html; charset=UTF-8\r\n"
            "Content-Length: %ld\r\n"
            "Connection: close\r\n"
            "\r\n"
            "%s",
            strlen(html), html);

    send(client_socket, response, strlen(response), 0);
}

void create_home_page(char *html, const char *result_html) {
    sprintf(html,
            "<!DOCTYPE html>"
            "<html>"
            "<head>"
            "<meta charset='UTF-8'>"
            "<title>HTTP Calculator</title>"
            "<style>"
            "body { font-family: Arial; background: #f2f2f2; padding: 40px; }"
            ".container { background: white; padding: 30px; width: 600px; margin: auto; border-radius: 10px; box-shadow: 0 0 10px #ccc; }"
            "h1 { color: #333; text-align: center; }"
            "form { margin: 20px 0; padding: 15px; border: 1px solid #ddd; border-radius: 8px; }"
            "input, select, button { padding: 8px; margin: 5px; }"
            "button { cursor: pointer; }"
            ".result { background: #e8f5e9; padding: 15px; border-radius: 8px; margin-bottom: 20px; }"
            ".error { background: #ffebee; color: #b71c1c; padding: 15px; border-radius: 8px; margin-bottom: 20px; }"
            "</style>"
            "</head>"
            "<body>"
            "<div class='container'>"
            "<h1>HTTP Calculator</h1>"

            "%s"

            "<h2>1. Gửi dữ liệu bằng GET</h2>"
            "<form method='GET' action='/calc'>"
            "<input type='number' step='any' name='a' placeholder='Số thứ nhất' required>"
            "<select name='op'>"
            "<option value='add'>Cộng</option>"
            "<option value='sub'>Trừ</option>"
            "<option value='mul'>Nhân</option>"
            "<option value='div'>Chia</option>"
            "</select>"
            "<input type='number' step='any' name='b' placeholder='Số thứ hai' required>"
            "<button type='submit'>Tính bằng GET</button>"
            "</form>"

            "<h2>2. Gửi dữ liệu bằng POST</h2>"
            "<form method='POST' action='/calc'>"
            "<input type='number' step='any' name='a' placeholder='Số thứ nhất' required>"
            "<select name='op'>"
            "<option value='add'>Cộng</option>"
            "<option value='sub'>Trừ</option>"
            "<option value='mul'>Nhân</option>"
            "<option value='div'>Chia</option>"
            "</select>"
            "<input type='number' step='any' name='b' placeholder='Số thứ hai' required>"
            "<button type='submit'>Tính bằng POST</button>"
            "</form>"

            "</div>"
            "</body>"
            "</html>",
            result_html);
}

void calculate(const char *params, char *result_html) {
    char a_str[100], b_str[100], op[100];

    int has_a = get_param(params, "a", a_str, sizeof(a_str));
    int has_b = get_param(params, "b", b_str, sizeof(b_str));
    int has_op = get_param(params, "op", op, sizeof(op));

    if (!has_a || !has_b || !has_op) {
        sprintf(result_html,
                "<div class='error'>Thiếu tham số. Cần nhập đủ a, b và toán tử.</div>");
        return;
    }

    double a = atof(a_str);
    double b = atof(b_str);
    double result = 0;
    char symbol;

    if (strcmp(op, "add") == 0) {
        result = a + b;
        symbol = '+';
    } else if (strcmp(op, "sub") == 0) {
        result = a - b;
        symbol = '-';
    } else if (strcmp(op, "mul") == 0) {
        result = a * b;
        symbol = '*';
    } else if (strcmp(op, "div") == 0) {
        if (b == 0) {
            sprintf(result_html,
                    "<div class='error'>Lỗi: Không thể chia cho 0.</div>");
            return;
        }

        result = a / b;
        symbol = '/';
    } else {
        sprintf(result_html,
                "<div class='error'>Toán tử không hợp lệ.</div>");
        return;
    }

    sprintf(result_html,
            "<div class='result'>"
            "<h2>Kết quả</h2>"
            "<p>Phép tính: <b>%.2f %c %.2f</b></p>"
            "<p>Kết quả: <b>%.2f</b></p>"
            "</div>",
            a, symbol, b, result);
}

void handle_client(int client_socket) {
    char buffer[BUFFER_SIZE];
    char method[10];
    char url[1024];

    memset(buffer, 0, sizeof(buffer));

    int bytes_received = recv(client_socket, buffer, sizeof(buffer) - 1, 0);

    if (bytes_received <= 0) {
        close(client_socket);
        return;
    }

    sscanf(buffer, "%s %s", method, url);

    char result_html[2048] = "";
    char html[BUFFER_SIZE];

    if (strcmp(method, "GET") == 0) {
        char *query = strchr(url, '?');

        if (query != NULL) {
            query++;
            calculate(query, result_html);
        }

    } else if (strcmp(method, "POST") == 0) {
        char *body = strstr(buffer, "\r\n\r\n");

        if (body != NULL) {
            body += 4;
            calculate(body, result_html);
        } else {
            sprintf(result_html,
                    "<div class='error'>Không đọc được dữ liệu POST.</div>");
        }
    }

    create_home_page(html, result_html);
    send_response(client_socket, html);

    close(client_socket);
}

int main() {
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

    if (listen(server_socket, 5) < 0) {
        perror("Listen thất bại");
        close(server_socket);
        exit(1);
    }

    printf("Server đang chạy tại: http://localhost:%d\n", PORT);
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
