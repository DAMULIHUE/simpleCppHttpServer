#include <iostream>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <filesystem>
#include <string>
#include <cstring>
#include <thread>
#include <functional>

#define CHUNK 512

// function to handle errors 
void errorMessage(const std::string message){
        std::cout << message
        << errno << std::endl;
        exit(EXIT_FAILURE);
}

// send headers with its respective HTTP codes
void handleHeader(int &client_fd, const int index_length, const int HTTPcode){
	
	// initialize the variable and builds the header
	std::string header;
	header = "HTTP/1.1 " + std::to_string(HTTPcode);
	if(HTTPcode == 200)	
		header += " OK\r\n";
	else if (HTTPcode == 404)
		header += " Not Found\r\n";

	header += "Content-Type: text/html\r\n";
	header += "Content-Length: " + std::to_string(index_length) + "\r\n";
	header += "Accept-Charset: UTF-8\r\n";
	header += "\r\n";

	// send the header to the client
	send(client_fd, header.c_str(), header.size(), 0);
}

// send body right after the headers
void handleRes(int &client_fd, const char *filePath, const int HTTPcode){

	// open the .html file 
	const int file = open(filePath, O_RDONLY);
	if(file < 0)
		errorMessage("on open file: ");
	
	// get the lenght of the file from the path (./filename)
	const int index_length = std::filesystem::file_size(filePath);

	// send the header first (only called on handle res)
	handleHeader(client_fd, index_length, HTTPcode);

	// send the body by chunks of 512 bytes to the client
	char buffer[CHUNK];
	int valread;
	while((valread = read(file, buffer, sizeof(buffer))) > 0){
		send(client_fd, buffer, valread, 0);
	}

	close(file);
}

// return the protocol sent by the browser
const char *returnProtocolHTTP(int &client_fd){
	
	// get the right protocol by reading the client socket
	char buffer[CHUNK];
	while(true){
		
		if(read(client_fd, buffer, sizeof(buffer)) > 0){
			
			if(strstr(buffer, "GET / HTTP") != nullptr){
				return "GET /";
				break;
			} else {
				return NULL; 
			}
		}
	}
}

void threadFunc(int client_fd){

	// wait and process requests from server
	// handleRequest(client_fd);
	while(true){
		if(returnProtocolHTTP(client_fd) == "GET /"){
			handleRes(client_fd, "./index.html", 200);

		} else {
			
			handleRes(client_fd, "./404.html", 404);
		}
	}
}

int main(){
	
	int server_fd, client_fd, pid;
	sockaddr_in server_addr;
	int server_addr_len = sizeof(server_addr);

	// IPV4 TCP SOCKET
	if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
		errorMessage("on server_fd: ");

	// IPV4 ADDR
	server_addr.sin_family = AF_INET;
	// ANY IP CAN CONNECT
	server_addr.sin_addr.s_addr = INADDR_ANY;
	// PORT 6969 - HTONS MAKE IT A NETWORK BYTE
	server_addr.sin_port = htons(6969);
	
	// bind socket to port (socker file, server_addr and size S.A.)
	if((bind(server_fd,
		(struct sockaddr*)&server_addr,
		sizeof(server_addr))) < 0)
		errorMessage("on bind: ");

	// listen on port
	if(listen(server_fd, 5) < 0)
		errorMessage("on listening: ");
	
	// handle client connection
	while(true){
		client_fd = accept(server_fd,
				(struct sockaddr*)&server_addr,
				(socklen_t*)&server_addr_len);
		
		if(client_fd < 0)
			errorMessage("client fd error: ");
	
		// the thread must be able to copy the params, but were passign them by
		// dereference (theyr memory addresses) so intead of
		//
		//	std::thread t(threadFunc, client_fd, server_fd).join();
		//
		// we do
		// (#include <functional> for std::ref)
		std::thread t{threadFunc, client_fd, std::ref(server_fd)};
		t.detach();
	}

	close(server_fd);
	return 0;
}
