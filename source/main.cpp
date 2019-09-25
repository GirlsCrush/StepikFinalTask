#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>    
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#include <string>
#include <set>
#include <string.h>
#include <regex>
#include <fcntl.h>
#include <signal.h>
#include <poll.h>

#define MAX_EVENTS 32
#define POLL_SIZE 1024
int set_nonblock(int fd) {
	int flags;
#if defined(O_NONBLOCK)
	if (-1 == (flags = fcntl(fd, F_GETFL, 0)))
		flags = 0;
	return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
#else
	flags = 1;
	return ioctl(fd, FIOBIO, &flags);
#endif
}

#define BUFFER_SIZE 1024

typedef struct {
	char buf[BUFFER_SIZE];
} url_data;

static const std::string templStart = "HTTP/1.0 200 OK\r\nContent-length: ";


static const std::string templEnd = "\r\nConnection: close\r\nContent-Type: text/html\r\n\r\n";

static const std::string not_found = "HTTP/1.0 404 NOT FOUND\r\nContent-length: 0\r\nContent-Type: text/html\r\n\r\n";

void handleRequest(int fd) {
    static char buf[BUFFER_SIZE];
    bzero(buf, BUFFER_SIZE);
	int r = recv(fd, buf, BUFFER_SIZE, MSG_NOSIGNAL);
    printf(buf);
    std::string fileName = "/index.html";
    
    if (r > 0) {
        std::cmatch result;
	    std::regex reg("(GET)[ ]([^ ?]+)");
        std::regex_search(buf, result, reg);

        if(strlen(result[2].str().c_str()))
            fileName = result[2].str();
    } else return;
    // if (fileName[0] == '/')
    //     fileName = fileName.substr(1);
    FILE *f = fopen(fileName.c_str(), "r");
    if (f) {
    std::string s;
    while (!feof(f)) {
        bzero(buf, BUFFER_SIZE);        
        fgets(buf, BUFFER_SIZE, f);
        s += buf;
    }
    s = templStart + std::to_string(strlen(s.c_str())) + templEnd + s;
    send(fd, s.c_str(), strlen(s.c_str()), MSG_NOSIGNAL);    
    } else {
        send(fd, not_found.c_str(), strlen(not_found.c_str()), MSG_NOSIGNAL);  
    }
    shutdown(fd, SHUT_RDWR);
    close(fd);
}



int main(int argc, char** argv)
{
    // signal(SIGHUP, SIG_IGN);
    uint16_t pid, sid;
    in_addr_t inAddr;
    uint16_t port;
    std::string dir;
    int rez = 0;
    inAddr = inet_addr("127.0.0.1");
    port = htons(12345);
    dir = "/";
    if (argc >= 2)
    while ((rez = getopt(argc,argv,"h:p:d:")) != -1){
		switch (rez){
		case 'h': 
        printf("IP = %s.\n",optarg);
        inAddr = inet_addr(optarg);
        break;
		case 'p': printf("PORT = %s.\n",optarg); 
        port = htons(atoi(optarg));
        break;
		case 'd': printf("DIRECTORY = %s\n", optarg); 
        dir = optarg;
        break;
		case '?': printf("Error found !\n");break;
        };
	};
    
    daemon(1,0);

    if (dir.size() && chdir(dir.c_str()) < 0) {
        exit(EXIT_FAILURE);
    }
    
    printf("dsfsdfsdfsf");
	int MasterSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	std::set<int> SlaveSockets;

	struct sockaddr_in SockAddr;
	SockAddr.sin_family = AF_INET;
	SockAddr.sin_port = port;
	SockAddr.sin_addr.s_addr = inAddr;
	bind(MasterSocket, (struct sockaddr*)(&SockAddr), 
		sizeof(SockAddr));
	set_nonblock(MasterSocket);
	listen(MasterSocket, SOMAXCONN);
	
	struct pollfd Set[POLL_SIZE];
	Set[0].fd = MasterSocket;
	Set[0].events = POLLIN;
	while (true) {
		unsigned int index = 1;
		for (auto iter = SlaveSockets.begin(); 
			iter != SlaveSockets.end(); ++iter, ++index){
			Set[index].fd = *iter;
			Set[index].events = POLLIN;
		}
		size_t setSize = 1 + SlaveSockets.size();

		poll(Set, setSize, -1);
		
		for (size_t i = 0; i < setSize; ++i) {
			if (Set[i].revents & POLLIN) {
				if (i) {
					handleRequest(Set[i].fd);
                    SlaveSockets.erase(Set[i].fd);
				} else {
					int SlaveSocket = accept(MasterSocket, 0, 0);
					set_nonblock(SlaveSocket);
					SlaveSockets.insert(SlaveSocket);
				}
			}
		}
	}
    
    
    return 0;
    
}