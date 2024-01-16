#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>



/**
 * Project 1 starter code
 * All parts needed to be changed/added are marked with TODO
 */

#define BUFFER_SIZE 1024
#define DEFAULT_SERVER_PORT 8081
#define DEFAULT_REMOTE_HOST "131.179.176.34"
#define DEFAULT_REMOTE_PORT 5001

struct server_app {
    // Parameters of the server
    // Local port of HTTP server
    uint16_t server_port;

    // Remote host and port of remote proxy
    char *remote_host;
    uint16_t remote_port;
};

// The following function is implemented for you and doesn't need
// to be change
void parse_args(int argc, char *argv[], struct server_app *app);

// The following functions need to be updated
void handle_request(struct server_app *app, int client_socket);
void serve_local_file(int client_socket, const char *path);
void proxy_remote_file(struct server_app *app, int client_socket, const char *path);

// The main function is provided and no change is needed
int main(int argc, char *argv[])
{
    struct server_app app;
    int server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len;
    int ret;

    parse_args(argc, argv, &app);

    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(app.server_port);

    // The following allows the program to immediately bind to the port in case
    // previous run exits recently
    int optval = 1;
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

    if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_socket, 10) == -1) {
        perror("listen failed");
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d\n", app.server_port);

    while (1) {
        client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &client_len);
        if (client_socket == -1) {
            perror("accept failed");
            continue;
        }
        
        printf("Accepted connection from %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
        handle_request(&app, client_socket);
        close(client_socket);
    }

    close(server_socket);
    return 0;
}

void parse_args(int argc, char *argv[], struct server_app *app)
{
    int opt;

    app->server_port = DEFAULT_SERVER_PORT;
    app->remote_host = NULL;
    app->remote_port = DEFAULT_REMOTE_PORT;

    while ((opt = getopt(argc, argv, "b:r:p:")) != -1) {
        switch (opt) {
        case 'b':
            app->server_port = atoi(optarg);
            break;
        case 'r':
            app->remote_host = strdup(optarg);
            break;
        case 'p':
            app->remote_port = atoi(optarg);
            break;
        default: /* Unrecognized parameter or "-?" */
            fprintf(stderr, "Usage: server [-b local_port] [-r remote_host] [-p remote_port]\n");
            exit(-1);
            break;
        }
    }

    if (app->remote_host == NULL) {
        app->remote_host = strdup(DEFAULT_REMOTE_HOST);
    }
}

void handle_request(struct server_app *app, int client_socket) {
    char buffer[BUFFER_SIZE];
    ssize_t bytes_read;

    bytes_read = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
    if (bytes_read <= 0) {
        return;  // Connection closed or error
    }

    // Read the request from HTTP client
    // Note: This code is not ideal in the real world because it
    // assumes that the request header is small enough and can be read
    // once as a whole.
    // However, the current version suffices for our testing.
    buffer[bytes_read] = '\0';
    // copy buffer to a new string
    char *request = malloc(strlen(buffer) + 1);
    strcpy(request, buffer);

    // TODO: Parse the header and extract essential fields, e.g. file name
    // Hint: if the requested path is "/" (root), default to index.html
    // <Method> <Path> HTTP/1.1
    char *request_line = strtok(request, "\r\n");
    char *method = strtok(request_line, " ");
    char *path = strtok(NULL, " ");

    // Decode the path containing %
    char *d_path = malloc(strlen(path) + 1);
    char a, b;
    const char *src = path;
    char *dest = d_path;
    while (*src) {
        if (*src == '%') {
            char a = src[1];
            char b = src[2];
            if(isxdigit(a) && isxdigit(b)){
                //converts lowercase to uppercase
                if (a >= 'a') 
                    a -= 'a' - 'A';
                //Adjust uppercase letters to it's hex numbers
                //e.g. A is 10, B is 11 and so on
                if (a >= 'A') 
                    a -= ('A' - 10);
                else a -= '0';
                //Same for b
                if (b >= 'a') 
                    b -= 'a' - 'A';
                if (b >= 'A') 
                    b -= ('A' - 10);
                else 
                    b -= '0';
                //Shifting a to the left by 4 bits (multiplying by 16) 
                //effectively places it in the higher 4 bits of a byte.
                //Adding b to the result effectively sets the lower 4 bits of the byte.
                *dest++ = 16 * a + b;
                src += 3; //skips %ab (total of 3 chars)
            }
        } else {
            *dest++ = *src++;
        }
    }
    *dest = '\0'; // end of string

    // Default to index.html if root path is requested
    if (strcmp(d_path, "/") == 0) {
        d_path = "/index.html";  
    }

    // TODO: Implement proxy and call the function under condition
    // specified in the spec
    // if (need_proxy(...)) {
    //    proxy_remote_file(app, client_socket, file_name);
    // } else {
    if (strstr(d_path, ".ts")) {
        proxy_remote_file(app, client_socket, d_path);
    } else {
        serve_local_file(client_socket, d_path);
    }
    //}
    free(d_path);
}

void serve_local_file(int client_socket, const char *path) {
    // TODO: Properly implement serving of local files
    // The following code returns a dummy response for all requests
    // but it should give you a rough idea about what a proper response looks like
    // What you need to do 
    // (when the requested file exists):
    // * Open the requested file
    // * Build proper response headers (see details in the spec), and send them
    // * Also send file content
    // (When the requested file does not exist):
    // * Generate a correct response

    // char response[] = "HTTP/1.0 200 OK\r\n"
    //                   "Content-Type: text/plain; charset=UTF-8\r\n"
    //                   "Content-Length: 15\r\n"
    //                   "\r\n"
    //                   "Sample response";

    // send(client_socket, response, strlen(response), 0);
    struct stat file_info;
    int file = open(path + 1, O_RDONLY);  // path + 1 to skip the leading '/'
    
    //file=-1: file could not be opened
    //E.C: send it back to the client if the client requests a file that doesn’t exists
    if (file == -1) {
        char response[] = "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n\r\n";
        send(client_socket, response, strlen(response), 0);
        return;
    }

    //determining the appropriate Content-Type of the HTTP response based on the file extension.
    fstat(file, &file_info);
    char header[BUFFER_SIZE];
    char *content_type = "application/octet-stream";
    if (strstr(path, ".txt")){
        content_type = "text/plain";
    } else if (strstr(path, ".jpg")) {
        content_type = "image/jpeg";
    } else if (strstr(path, ".html")) {
        content_type = "text/html";
    } 

    //create the HTTP response header
    snprintf(header, BUFFER_SIZE,
             "HTTP/1.1 200 OK\r\n"
             "Content-Length: %lld\r\n"
             "Content-Type: %s\r\n\r\n",
             file_info.st_size,
             content_type);

    //sends the HTTP response header to the client
    send(client_socket, header, strlen(header), 0);
    
    //send file content
    off_t offset = 0;
    ssize_t sent_bytes;
    while (offset < file_info.st_size) {
        sent_bytes = sendfile(client_socket, file, &offset, file_info.st_size - offset);
        if (sent_bytes <= 0) {
            break;
        }
    }

    close(file);
}

//Acts as a proxy to forward an HTTP request to a remote server and send the response back to the client
void proxy_remote_file(struct server_app *app, int client_socket, const char *request) {
    // TODO: Implement proxy request and replace the following code
    // What's needed:
    // * Connect to remote server (app->remote_server/app->remote_port)
    // * Forward the original request to the remote server
    // * Pass the response from remote server back
    // Bonus:
    // * When connection to the remote server fail, properly generate
    // HTTP 502 "Bad Gateway" response

    // char response[] = "HTTP/1.0 501 Not Implemented\r\n\r\n";
    // send(client_socket, response, strlen(response), 0);

    //creates a socket for communication with the remote server.
    int remote_socket = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in remote_addr;

    //Sets address and port for remote server
    remote_addr.sin_family = AF_INET;
    remote_addr.sin_port = htons(app->remote_port);
    inet_pton(AF_INET, app->remote_host, &(remote_addr.sin_addr));

    // Check if remote server is unavailable
    //E.C: 502 Bad Gateway for reverse proxy
    if (connect(remote_socket, (struct sockaddr*)&remote_addr, sizeof(remote_addr)) == -1) {
        char response[] = "HTTP/1.1 502 Bad Gateway\r\nContent-Length: 0\r\n\r\n";
        send(client_socket, response, strlen(response), 0);
        return;
    }

    // request that will be sent to the remote server
    char _request[BUFFER_SIZE];
    snprintf(_request, BUFFER_SIZE, "GET %s HTTP/1.1\r\nHost: %s\r\n\r\n", request, app->remote_host);
    send(remote_socket, _request, strlen(_request), 0);

    // buffer to receive the response from the remote server
    char buffer[BUFFER_SIZE];
    ssize_t bytes_num;
    while ((bytes_num = recv(remote_socket, buffer, BUFFER_SIZE, 0)) > 0) {
        send(client_socket, buffer, bytes_num, 0);
    }

    close(remote_socket);
}
