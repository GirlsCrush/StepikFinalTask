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

void handleRequest(int fd) {
    static char buf[BUFFER_SIZE];
    bzero(buf, BUFFER_SIZE);
	int r = recv(fd, buf, BUFFER_SIZE, MSG_NOSIGNAL);
    std::string fileName = "/index.html";
    if (r > 0) {
        std::cmatch result;
	    std::regex reg("^(GET)[ ]([^ ]+)([ ]((HTTP/)[0-9].[0-9]))?$");
        std::regex_search(buf, result, reg);
        if(strlen(result[2].str().c_str()))
            fileName = result[2].str();
    } else return;

    FILE *f = fopen(fileName.c_str(), "r");
    if (f) {
    std::string s;
    while (feof(f)) {
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



static const std::string templStart = "HTTP/1.0 200 OK\r\n"

		           "Content-length: ";


static const std::string templEnd = "\r\nConnection: close\r\n"

		       	   "Content-Type: text/html\r\n"

		       	   "\r\n";

static const std::string not_found = "HTTP/1.0 404 NOT FOUND\r\nContent-Type: text/html\r\n\r\n";

int main(int argc, char** argv)
{
    
    // if (argc < 2)
    // {
    //     printf("Usage: ./my_daemon -h <ip> -p <port> -d <directory>\n");
    //     return -1;
    // }
    // int status;
    uint16_t pid, sid;
    in_addr_t inAddr;
    uint16_t port;
    std::string dir;
    int rez = 0;
    if (argc < 2)
    {
        inAddr = inet_addr("127.0.0.1");
        port = htons(80);
    } else while ((rez = getopt(argc,argv,"h:p:d:")) != -1){
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
    
        
    
    // if (!status) // если произошла ошибка загрузки конфига
    // {
    //     printf("Error: Load config failed\n");
    //     return -1;
    // }
    
    // создаем потомка
    pid = fork();

    if (pid == -1) // если не удалось запустить потомка
    {
        // выведем на экран ошибку и её описание
        printf("Error: Start Daemon failed \n");
        
        return -1;
    }
    else if (!pid) // если это потомок
    {
        // данный код уже выполняется в процессе потомка
        // разрешаем выставлять все биты прав на создаваемые файлы,
        // иначе у нас могут быть проблемы с правами доступа
        umask(0);
        
        // создаём новый сеанс, чтобы не зависеть от родителя
        sid = setsid();
        if (sid < 0) {
            exit(EXIT_FAILURE);
        }
        
        // переходим в корень диска, если мы этого не сделаем, то могут быть проблемы.
        // к примеру с размантированием дисков
        if (chdir(dir.c_str()) < 0) {
            exit(EXIT_FAILURE);
        }
        
        // закрываем дискрипторы ввода/вывода/ошибок, так как нам они больше не понадобятся
        close(STDIN_FILENO);
        close(STDOUT_FILENO);
        close(STDERR_FILENO);

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
    else // если это родитель
    {
        // завершим процес, т.к. основную свою задачу (запуск демона) мы выполнили
        return 0;
    }
}