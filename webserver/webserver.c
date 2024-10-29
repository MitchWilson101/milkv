

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

#define PORT 8080

//just coz

/*void handleClient(int new_socket) {

	
    char buffer[30000] = {0};
    read(new_socket, buffer, 30000);

    printf("Received request:\n%s\n", buffer);

    // Prepare a simple HTTP response
    char *httpResponse = "HTTP/1.1 200 OK\n"
                         "Content-Type: text/html\n"
                         "Content-Length: 56\n\n" 

                         "<html><body><h1>Welcome to Milkv Duo Web Server</h1></body></html>";

    // Send the HTTP response
    send(new_socket, httpResponse, strlen(httpResponse), 0);
    printf("Response sent\n");

    // Close the socket
    close(new_socket);
}*/

int main(int argc, char const* argv[]) {

	 // Buffers to store the two input strings
    char input1[128];
    char input2[128];
    char input3[128];

     //Read the first string from stdin
    if (fgets(input1, sizeof(input1), stdin) != NULL) {
        // Remove trailing newline if present
        input1[strcspn(input1, "\n")] = '\0';
    } else {
        fprintf(stderr, "Failed to read first input string.\n");
        return 1;
    }

    // Read the second string from stdin
    if (fgets(input2, sizeof(input2), stdin) != NULL) {
        // Remove trailing newline if present
        input2[strcspn(input2, "\n")] = '\0';
    } else {
        fprintf(stderr, "Failed to read second input string.\n");
        return 1;
    }
   // Read the third string from stdin
    if (fgets(input3, sizeof(input2), stdin) != NULL) {
        // Remove trailing newline if present
        input2[strcspn(input3, "\n")] = '\0';
    } else {
        fprintf(stderr, "Failed to read second input string.\n");
        return 1;
    }
    // Debug: Print the received strings
    printf("RS1: %s\n", input1);
    printf("RS2: %s\n", input2);
    printf("RS2: %s\n", input3); 

	// New code ends here 
    int server_fd, new_socket;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);

    // Create socket file descriptor
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // Attach socket to port 8080
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY; // Bind to all available interfaces
    address.sin_port = htons(PORT);

    // Bind the socket to the port
    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    // Listen for incoming connections
    if (listen(server_fd, 3) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    printf("Server is listening on port %d\n", PORT);

    // Accept and handle incoming connections
    while (1) {
        if ((new_socket = accept(server_fd, (struct sockaddr*)&address, (socklen_t*)&addrlen)) < 0) {
            perror("accept");
            exit(EXIT_FAILURE);
        }

        // Handle the client in a separate function
        //handleClient(new_socket);

	char buffer[30000] = {0};
    read(new_socket, buffer, 30000);

    printf("Received request:\n%s\n", buffer);

    // Prepare a simple HTTP response
    char *httpResponse = "HTTP/1.1 200 OK\n"
             "Content-Type: text/html\n\n"
             "<html><body>"
             "<h1>Welcome to Milkv Duo Web Server</h1>"
             "<p>String 1: %s</p>"
             "<p>String 2: %s</p>"
             "<p>String 3: %s</p>"
             "</body></html>", input1, input2, input3;

    // Send the HTTP response
    send(new_socket, httpResponse, strlen(httpResponse), 0);
    printf("Response sent\n");

    // Close the socket
    close(new_socket);
    }

    return 0;
}