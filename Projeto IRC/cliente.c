#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <netdb.h>
#include <string.h>
#include <arpa/inet.h>
#include <time.h>

#define BUF_SIZE 1024
#define CLIENT_PATH "./downloads/"

void just_process(int proxy_socket_tcp, int proxy_socket_udp, struct sockaddr_in proxy_addr);
void erro(char *string);
void receiveFileTcp(char* nomeFicheiro,int proxy_socket_tcp);

int main(int argc, char const *argv[]) {
    int proxy_socket_tcp, proxy_socket_udp;
    struct sockaddr_in proxy_addr;
    
    if (argc!=5) erro("ERROR: Wrong format:\n<proxy address> <server address> <port> <protocol>");
    else {
        if (strcasecmp("TCP", argv[4])!=0) erro("Wrong Protocolo\n");
        // CONNECTION PROXY CODE
        bzero((void *) &proxy_addr, sizeof(proxy_addr));
        proxy_addr.sin_family = AF_INET;
        inet_pton(AF_INET, argv[1], &(proxy_addr.sin_addr));
        proxy_addr.sin_port = htons(atoi(argv[3]));
        
        if((proxy_socket_tcp = socket(AF_INET, SOCK_STREAM, 0)) == -1) erro("Socket TCP.");
        if((proxy_socket_udp = socket(AF_INET, SOCK_DGRAM, 0)) == -1) erro("Socket UDP.");
        if (bind(proxy_socket_udp,(struct sockaddr*)&proxy_addr,sizeof(proxy_addr)) < 0) erro("bind UDP error");
        if( connect(proxy_socket_tcp,(struct sockaddr *)&proxy_addr,sizeof (proxy_addr)) < 0) erro("Connect");
        // ==========================

        // CONNECTED USER
        write(proxy_socket_tcp, argv[2], sizeof(argv[2]));
        printf("\t -- Connected to Proxy --\n\n");
        just_process(proxy_socket_tcp, proxy_socket_udp, proxy_addr);
    }
    return 0;
}

void just_process(int proxy_socket_tcp, int proxy_socket_udp, struct sockaddr_in proxy_addr) {
    int size_buf;
    char comando[BUF_SIZE], protocolo[5], encriptado[5], files_list[BUF_SIZE], nomeFicheiro[BUF_SIZE];
    

    while (1) {
        scanf(" %[^\n]", comando);
        if (strcasecmp(comando, "LIST")==0) {
            write(proxy_socket_tcp, comando, BUF_SIZE);
            size_buf=read(proxy_socket_tcp, files_list, BUF_SIZE);
            files_list[size_buf-1]='\0';
            printf("\n%s", files_list);
        } else if (strcasecmp(comando, "QUIT")==0) {
            write(proxy_socket_tcp, comando, BUF_SIZE);
            printf("\t-- Connection Lost --\n");
        } else if (strcasecmp("DOWNLOAD", strtok(comando, " "))==0) {
            if (strcasecmp("TCP", (strcpy(protocolo,strtok(NULL, " "))))==0 || strcasecmp("UDP", protocolo)==0) {
                if (strcasecmp("ENC", (strcpy(encriptado,strtok(NULL, " "))))==0 || strcasecmp("NOR", encriptado)==0) {

                    write(proxy_socket_tcp, protocolo, BUF_SIZE);
                    write(proxy_socket_tcp, encriptado, BUF_SIZE);
                    strcpy(nomeFicheiro, strtok(NULL, "\n"));
                    write(proxy_socket_tcp, nomeFicheiro, BUF_SIZE);
                    receiveFileTcp(nomeFicheiro, proxy_socket_tcp);

                } else printf("Wrong encriptation\n");
            } else printf("Wrong Protocol\n");
        } else printf("ERROR: Wrong command.\n");   
    }  
}

void erro(char* tipo) {
    printf("%s\n", tipo);
    exit(0);
}

void receiveFileTcp(char* nomeFicheiro,int proxy_socket_tcp) {
    long int tamanho=0, total=0, aux=0;
    char buffer[BUF_SIZE];
    char directoria[BUF_SIZE];
    strcpy(directoria, CLIENT_PATH);
    strcat(directoria,nomeFicheiro);
    read(proxy_socket_tcp,&tamanho,sizeof(long int));
    FILE* f = fopen(directoria,"wb");
    printf("\n\n");
    if(f!=NULL){
        while(total<tamanho){
            aux=read(proxy_socket_tcp,buffer,sizeof(buffer));
            total+=aux;
            fwrite(buffer, 1, aux, f);
        }
        printf("Ficheiro Recebido:%s\nTotal de bytes recebidos:%ld\n\n",nomeFicheiro, total);
        fclose(f);
    }else{
        printf("Erro a abrir directoria");
    }
}