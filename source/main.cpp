#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>    
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#include <string>

int main(int argc, char** argv)
{
    if (argc != 2)
    {
        printf("Usage: ./my_daemon -h <ip> -p <port> -d <directory>\n");
        return -1;
    }

    int status;
    int pid, sid;
    in_addr_t inAddr;
    int port;
    std::string dir;
    int rez = 0;

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
    

    
    if (!status) // если произошла ошибка загрузки конфига
    {
        printf("Error: Load config failed\n");
        return -1;
    }
    
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
            printf("Error: SID is not given.\n");
            exit(EXIT_FAILURE);
        }
        
        // переходим в корень диска, если мы этого не сделаем, то могут быть проблемы.
        // к примеру с размантированием дисков
        if ((chdir("/")) < 0) {
            printf("Error: Working dir is not changed.\n");
            exit(EXIT_FAILURE);
        }
        
        // закрываем дискрипторы ввода/вывода/ошибок, так как нам они больше не понадобятся
        close(STDIN_FILENO);
        close(STDOUT_FILENO);
        close(STDERR_FILENO);
        
        // Данная функция будет осуществлять слежение за процессом
        
        
        return status;
    }
    else // если это родитель
    {
        // завершим процес, т.к. основную свою задачу (запуск демона) мы выполнили
        return 0;
    }
}