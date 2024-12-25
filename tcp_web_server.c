// PA2 - tcp_web_server

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <errno.h>
#include <netdb.h>
#include <sys/stat.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define BUFSIZE 4096
#define HEADERSIZE 1024
#define MAX_QUEUE_SIZE 5
#define PACKET_TRANSFER_TIMEOUT_S 1

// Wrapper for perror
void error(char* msg) 
{
    perror(msg);
    exit(1);
}
void warning(char* msg) 
{
    perror(msg);
}

// Helper functions

void send_response(int sock, const char* response_code, const char* version, const char* content_type, unsigned long content_size, const char* contents)
{
    char header[HEADERSIZE];

    // ---- Generate header ----

    // Put response code
    strcpy(header, version);
    strcat(header, " ");
    strcat(header, response_code);
    strcat(header, "\r\n");

    // Put content type
    strcat(header, "Content-Type: ");
    strcat(header, content_type);
    strcat(header, "\r\n");

    // Put content length
    strcat(header, "Content-Length: ");
    
    char content_size_str[10];
    snprintf(content_size_str, 10, "%ld", content_size);

    strcat(header, content_size_str);
    strcat(header, "\r\n\r\n");

    // ---- File Contents ----

    char response[HEADERSIZE + content_size + 1];

    memcpy(response, header, HEADERSIZE); // Copy the header
    memcpy(response + strlen(header), contents, content_size); // Append the body

    send(sock, response, HEADERSIZE + content_size + 1, 0);
}

void* handle_connection(void* sock_desc)
{
    char buf[BUFSIZE];

    int totalSize = 0;

    int sock = *(int *) sock_desc;

    // Ensure all bytes of the message are received and placed in the buffer
    while (1)
    {
        char tempBuf[BUFSIZE];

        int received = recv(sock, buf, BUFSIZE - 1, 0);

        // Leave if we've finished reading
        if (received == 0)
            break;
        
        // This is necessary or else strtok and strncat won't work
        tempBuf[received] = '\0';

        strncat(buf, tempBuf, BUFSIZE - totalSize);

        totalSize += received;

        // Check if we got everything
        if (strstr(buf, "\r\n\r\n") != NULL)
            break;
    }

    // Parse the http request
    char* request = strtok(buf, "\r\n");
    char* method = strtok(request, " ");
    char* url =  strtok(NULL, " ");
    char* version = strtok(NULL, " ");

    // Lowercase the url
    for (int i = 0; url[i]; i++) url[i] = tolower( (unsigned char) url[i] );

    printf("Server received the following request:\n%s %s %s\n", method, url, version);

    // Check for version error
    if (strcmp(version, "HTTP/1.0") != 0 && strcmp(version, "HTTP/1.1") != 0)
    {
        printf("An HTTP version other than 1.0 or 1.1 was requested\n");

        const char* errmsg = "<!DOCTYPE html><html><body><h1>505 HTTP Version Not Supported</h1></body></html>";

        send_response(sock, "505 HTTP Version Not Supported", "HTTP/1.1", "text/html", strlen(errmsg), errmsg);
        close(sock);
        pthread_exit(NULL);
    }

    // Check for method error
    if (strcmp(method, "GET") != 0)
    {
        printf("A method other than GET was requested\n");

        const char* errmsg = "<!DOCTYPE html><html><body><h1>405 Method Not Allowed</h1></body></html>";

        send_response(sock, "405 Method Not Allowed", version, "text/html", strlen(errmsg), errmsg);
        close(sock);
        pthread_exit(NULL);
    }

    // Check the filepath
    char filepath[100] = "www";
    strncat(filepath, url, 86);

    struct stat st;

    if (stat(filepath, &st) != 0)
    {
        // An error occurred, check if it's because the file doesn't exist
        if (errno == ENOENT) 
        {
            printf("The requested file can not be found in the document tree\n");

            const char* errmsg = "<!DOCTYPE html><html><body><h1>404 Not Found</h1></body></html>";

            send_response(sock, "404 Not Found", version, "text/html", strlen(errmsg), errmsg);
            close(sock);
            pthread_exit(NULL);
        }
        // Check if it's a permission issue
        else if (errno == EACCES)
        {
            printf("The requested file can not be accessed due to a file permission issue\n");

            const char* errmsg = "<!DOCTYPE html><html><body><h1>403 Forbidden</h1></body></html>";

            send_response(sock, "403 Forbidden", version, "text/html", strlen(errmsg), errmsg);
            close(sock);
            pthread_exit(NULL);
        }
        // Must be a bad request then
        else
        {
            printf("The request could not be parsed or is malformed\n");

            const char* errmsg = "<!DOCTYPE html><html><body><h1>400 Bad Request</h1></body></html>";

            send_response(sock, "400 Bad Request", version, "text/html", strlen(errmsg), errmsg);
            close(sock);
            pthread_exit(NULL);
        }
    }

    // If url is directory, check for index.html and index.htm
    if (S_ISDIR(st.st_mode))
    {
        struct stat st_index_html;
        struct stat st_index_htm;

        char filepath_w_index_html[100];
        char filepath_w_index_htm[100];

        strcpy(filepath_w_index_html, filepath);
        strcpy(filepath_w_index_htm, filepath);

        strcat(filepath_w_index_html, "index.html");
        strcat(filepath_w_index_htm, "index.htm");
        
        if (stat(filepath_w_index_html, &st_index_html) == 0)
        {
            strcat(filepath, "index.html");
        }
        else if (stat(filepath_w_index_htm, &st_index_htm) == 0)
        {
            strcat(filepath, "index.htm");
        }
        // An error occurred, check if it's because the file doesn't exist
        else if (errno == ENOENT) 
        {
            printf("The requested file can not be found in the document tree\n");

            const char* errmsg = "<!DOCTYPE html><html><body><h1>404 Not Found</h1></body></html>";

            send_response(sock, "404 Not Found", version, "text/html", strlen(errmsg), errmsg);
            close(sock);
            pthread_exit(NULL);
        }
        // Check if it's a permission issue
        else if (errno == EACCES)
        {
            printf("The requested file can not be accessed due to a file permission issue\n");

            const char* errmsg = "<!DOCTYPE html><html><body><h1>403 Forbidden</h1></body></html>";

            send_response(sock, "403 Forbidden", version, "text/html", strlen(errmsg), errmsg);
            close(sock);
            pthread_exit(NULL);
        }
        // Must be a bad request then
        else
        {
            printf("The request could not be parsed or is malformed\n");

            const char* errmsg = "<!DOCTYPE html><html><body><h1>400 Bad Request</h1></body></html>";

            send_response(sock, "400 Bad Request", version, "text/html", strlen(errmsg), errmsg);
            close(sock);
            pthread_exit(NULL);
        }
    }

    // Determine the file type

    char filetype[25];
    char* extension = NULL;

    for (unsigned int i = strlen(filepath) - 1; i > 0; i--) 
    {
        if (filepath[i - 1] == '.') 
        {
            extension = &filepath[i];
            break;
        }
    }

    int read_binary = 0;

    if (strcmp(extension, "html") == 0 || strcmp(extension, "htm") == 0) 
    {
        strcpy(filetype, "text/html");
    }
    else if (strcmp(extension, "txt") == 0) 
    {
        strcpy(filetype, "text/plain");
    }
    else if (strcmp(extension, "png") == 0) 
    {
        strcpy(filetype, "image/png");
        read_binary = 1;
    }
    else if (strcmp(extension, "gif") == 0) 
    {
        strcpy(filetype, "image/gif");
        read_binary = 1;
    }
    else if (strcmp(extension, "jpg") == 0 || strcmp(extension, "jpeg") == 0) 
    {
        strcpy(filetype, "image/jpg");
        read_binary = 1;
    }
    else if (strcmp(extension, "ico") == 0) 
    {
        strcpy(filetype, "image/x-icon");
        read_binary = 1;
    }
    else if (strcmp(extension, "css") == 0) 
    {
        strcpy(filetype, "text/css");
    }
    else if (strcmp(extension, "js") == 0) 
    {
        strcpy(filetype, "application/javascript");
    }

    printf("File type is %s\n", filetype);

    printf("Opening file at %s\n", filepath);

    // Open the file
    FILE *file;

    if (read_binary)
        file = fopen(filepath, "rb");
    else
        file = fopen(filepath, "r");

    // Make sure file exists
    if (file == NULL) 
    {
        // An error occurred, check if it's because the file doesn't exist
        if (errno == ENOENT) 
        {
            printf("The requested file can not be found in the document tree\n");

            const char* errmsg = "<!DOCTYPE html><html><body><h1>404 Not Found</h1></body></html>";

            send_response(sock, "404 Not Found", version, "text/html", strlen(errmsg), errmsg);
            close(sock);
            pthread_exit(NULL);
        }
        // Check if it's a permission issue
        else if (errno == EACCES)
        {
            printf("The requested file can not be accessed due to a file permission issue\n");

            const char* errmsg = "<!DOCTYPE html><html><body><h1>403 Forbidden</h1></body></html>";

            send_response(sock, "403 Forbidden", version, "text/html", strlen(errmsg), errmsg);
            close(sock);
            pthread_exit(NULL);
        }
        // Must be a bad request then
        else
        {
            printf("The request could not be parsed or is malformed\n");

            const char* errmsg = "<!DOCTYPE html><html><body><h1>400 Bad Request</h1></body></html>";

            send_response(sock, "400 Bad Request", version, "text/html", strlen(errmsg), errmsg);
            close(sock);
            pthread_exit(NULL);
        }
    }

    // Get the size of the contents to be read
    fseek(file, 0, SEEK_END);
    long content_size = ftell(file);

    // Reset file position
    fseek(file, 0, SEEK_SET);

    // Read the contents of the file into the buffer
    char file_contents[content_size + 1];
    fread(file_contents, 1, content_size, file);
    
    file_contents[content_size] = '\0';

    fclose(file);

    send_response(sock, "200 OK", version, &filetype[0], content_size, &file_contents[0]);

    close(sock);

    return NULL;
}

int main(int argc, char **argv)
{
    int sockfd;                         // Socket
    int portnum;                        // Port to listen on
    int clientlen;                      // Byte size of client's address

    struct sockaddr_in serveraddr;      // Server addr
    struct sockaddr_in clientaddr;      // Client addr

    char *clientaddr_str;               // Dotted decimal host addr string
    int optval;                         // Flag value for setsockopt
    
    char buf[BUFSIZE];                  // Message buf
    int n;                              // Message byte size

    // Check command line arguments
    if (argc != 2)
    {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(1);
    }

    portnum = atoi(argv[1]);

    // socket: create the parent socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);

    if (sockfd < 0) 
        error("ERROR opening socket");

    /* setsockopt: Handy debugging trick that lets us rerun the server immediately after we kill it; 
    * otherwise we have to wait about 20 secs. Eliminates "ERROR on binding: Address already in use" error. */
    optval = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (const void *) &optval, sizeof(int)) < 0)
    {
        close(sockfd);
        error("ERROR in setsockopt");
    }

    // Set timeout value in seconds
    /*struct timespec timeout;
    timeout.tv_sec = PACKET_TRANSFER_TIMEOUT_S;
    timeout.tv_nsec = 0;

    // Tell socket to timeout after given time
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(struct timespec)) < 0)
    {
        close(sockfd);
        error("ERROR in setsockopt");
    }*/

    // Build the server's Internet address
    memset(&serveraddr, 0, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serveraddr.sin_port = htons((unsigned short) portnum);

    // Bind socket to specified port
    if (bind(sockfd, (struct sockaddr *) &serveraddr, sizeof(serveraddr)) < 0)
    {
        close(sockfd);
        error("ERROR on binding");
    }

    printf("Listening on port %d...\n", portnum);

    listen(sockfd, MAX_QUEUE_SIZE);

    clientlen = sizeof(clientaddr);
  
    // Receive HTTP requests in infinite loop
    while (1)
    {
        int* new_sock = malloc(sizeof(int));
        
        *new_sock = accept(sockfd, (struct sockaddr *) &clientaddr, &clientlen);

        if (*new_sock < 0)
        {
            warning("ERROR on accept");
            continue;
        }
    
        // Convert sockaddr to IPv4 string
        clientaddr_str = inet_ntoa(clientaddr.sin_addr);

        if (clientaddr_str == NULL)
        {
            warning("ERROR on inet_ntoa");
            continue;
        }

        printf("Connected to client at %s through port %d\n", clientaddr_str, portnum);

        pthread_t thread;
        if (pthread_create(&thread, NULL, handle_connection, (void *) new_sock) != 0)
        {
            warning("Error on pthread_create");
            close(*new_sock);
            free(new_sock);
        }

        pthread_detach(thread);

        printf("Connection being handled by thread %u\n", (int) thread);

        printf("\n");
    }

    printf("Closing socket connection...\n");
    close(sockfd);
    return 0;
}
