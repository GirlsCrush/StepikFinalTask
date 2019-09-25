#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>    
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#include <string>
#include <string.h>
#include <regex>
#include <fcntl.h>
#include <signal.h>
#include <sys/epoll.h>

#define MAX_EVENTS 32

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
}



int main(int argc, char** argv)
{
    signal(SIGHUP, SIG_IGN);
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
    
    daemon(0,0);

    if (chdir(dir.c_str()) < 0) {
        exit(EXIT_FAILURE);
    }
    
	int MasterSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	struct sockaddr_in SockAddr;
	SockAddr.sin_family = AF_INET;
	SockAddr.sin_port = port;
	SockAddr.sin_addr.s_addr = inAddr;
	bind(MasterSocket, (struct sockaddr*)(&SockAddr), 
		sizeof(SockAddr));
	set_nonblock(MasterSocket);
	listen(MasterSocket, SOMAXCONN);
	int EPoll = epoll_create1(0);
	struct epoll_event Event;
	Event.data.fd = MasterSocket;
	Event.events = EPOLLIN;
	epoll_ctl(EPoll, EPOLL_CTL_ADD, MasterSocket, &Event);
	while (true) {
		struct epoll_event Events[MAX_EVENTS];
		int N = epoll_wait(EPoll, Events, MAX_EVENTS, -1);

		for (size_t i = 0; i < N; ++i) {
			if (Events[i].data.fd == MasterSocket) {
				int SlaveSocket = accept(MasterSocket, 0, 0);
				set_nonblock(SlaveSocket);

				struct epoll_event Event;
				Event.data.fd = SlaveSocket;
				Event.events = EPOLLIN;
				epoll_ctl(EPoll, EPOLL_CTL_ADD, SlaveSocket,
					&Event);
			} else {
				handleRequest(Events[i].data.fd);
                shutdown(Events[i].data.fd, SHUT_RDWR);
				close(Events[i].data.fd);
			}
		}
	}
    
    
    return 0;
    
}