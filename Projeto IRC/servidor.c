#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/sendfile.h>
#include <sys/ipc.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <netdb.h>
#include <string.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <dirent.h>
#include <sodium.h>

#define BUF_SIZE 1024
#define SERVER_PATH "./server_files/"

void erro(char* tipo);
void process_proxy(int proxy_socket_tcp);
int verifica_comandos(const char* porta, const char* max_clients);
void sendFileTcp(char* nomeFicheiro,int proxy_socket_tcp);

int main(int argc, char const *argv[]) {
    int socket_proxy_tcp, socket_proxy_udp, proxy;
    struct sockaddr_in server_addr, proxy_addr;
    int client_addr_size; 
    

    if (argc!=3) erro("ERROR: Wrong format:\n<server port> <max clients>");
    else {
        
        if (verifica_comandos(argv[1], argv[2])==1) {
            bzero((void *) &server_addr, sizeof(server_addr));
            server_addr.sin_family = AF_INET;
            server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
            server_addr.sin_port = htons(atoi(argv[1]));

            if ((socket_proxy_tcp = socket(AF_INET, SOCK_STREAM, 0)) < 0) erro("socket error"); 
            if ((socket_proxy_udp = socket(AF_INET, SOCK_STREAM, 0)) < 0) erro("socket error"); 
            if (bind(socket_proxy_tcp,(struct sockaddr*)&server_addr,sizeof(server_addr)) < 0) erro("bind error");
            if(listen(socket_proxy_tcp, atoi(argv[2])) < 0) erro("listen error");
            
            client_addr_size = sizeof(proxy_addr);
            
            printf("\t-- Waiting Connections -- \n");
            while (1) {
                while(waitpid(-1,NULL,WNOHANG)>0);
                
                proxy = accept(socket_proxy_tcp,(struct sockaddr *)&proxy_addr, (socklen_t *)&client_addr_size);

                if (proxy > 0) {
                    printf("\t -- Connection Created! -- \n\n");
                    if (fork() == 0) {
                        close(socket_proxy_tcp);
                        process_proxy(proxy);
                        printf("\t -- Connection Lost! -- \n\n");
                        exit(0);
                    }
                    close(proxy);
                }
            }
        } else printf("ERROR: Wrong format:\n<server port> <max clients>\n");
    }
    return 0;
}

void process_proxy(int proxy_socket_tcp) {
    int size_read;
    char files_list[BUF_SIZE], buffer_recv[BUF_SIZE];
    DIR *dp;
    struct dirent *ep;     
    char protocolo[BUF_SIZE], is_encriptado[BUF_SIZE];
    
    while (1) {
        strcpy(files_list, "");
        size_read=read(proxy_socket_tcp, buffer_recv, BUF_SIZE);
        buffer_recv[size_read-1]='\0';
        if (strcasecmp("LIST", buffer_recv)==0) {
            dp = opendir ("./server_files/");
            if (dp != NULL) {
                while ((ep = readdir (dp))!=NULL) {
                    if (strlen(ep->d_name)<3) continue;
                    else {
                        strcat(files_list, ep->d_name);
                        strcat(files_list, "\n");
                    }
                }
            } else erro ("Couldn't open the directory");
            closedir (dp);
            write(proxy_socket_tcp, files_list, BUF_SIZE);
        }
        else if (strcasecmp("QUIT", buffer_recv)==0) erro("\t-- Connection Lost --\n");
        else {
            strcpy(protocolo, buffer_recv);
            size_read=read(proxy_socket_tcp, is_encriptado, BUF_SIZE);
            is_encriptado[size_read-1]='\0';
            size_read=read(proxy_socket_tcp, buffer_recv, BUF_SIZE);
            buffer_recv[size_read-1]='\0';

            sendFileTcp(buffer_recv, proxy_socket_tcp);
        }
    }
}

int verifica_comandos(const char* string1, const char* string2) {
    for (int i=0; i<strlen(string1)-1; i++) {
        if (string1[i]>='0' && string1[i]<='9') continue;
        else return 0;
    }
    for (int i=0; i<strlen(string2)-1; i++) {
        if (string2[i]>='0' && string2[i]<='9') continue;
        else return 0;
    }
    return 1;
}

void erro(char* tipo) {
    printf("%s\n", tipo);
    exit(0);
}

void sendFileTcp(char* nomeFicheiro,int proxy_socket_tcp) {
    char buffer[BUF_SIZE],directoria[BUF_SIZE];
    struct stat file_stats;
    FILE* f;
    int aux, id_file;
    strcpy(directoria,SERVER_PATH);
    strcat(directoria,nomeFicheiro);
    
    if ((id_file = open(directoria, O_RDONLY))<0) erro("File not openned");
    if (fstat(id_file, &file_stats)<0) erro("Error taking stats from file");
    
    f=fopen(directoria,"rb");
    write(proxy_socket_tcp, &file_stats.st_size, sizeof(long int));

    if(f==NULL) erro("ficheiro");
    while((aux=fread(buffer,1,sizeof(buffer),f))>0){ write(proxy_socket_tcp, buffer, aux);}

    fclose(f);    
}