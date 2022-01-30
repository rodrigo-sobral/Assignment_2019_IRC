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

#define BUF_SIZE 1024
#define SERVER_PORT 5000
#define PROXY_IP "10.0.2.15"
#define PROXY_PATH "./buffer_folder/"

typedef struct {
    int loss,save,tempo,lastPorta,lastPortaCliente;
    long int tamanho;
    char nomeFicheiro[BUF_SIZE];
    char directoriaAnterior[BUF_SIZE];
    char protocolo[5];
    char lastClienteIp[BUF_SIZE],lastServerIp[BUF_SIZE];
}MemPartilhada;

void erro(char* tipo);
void process_client_server(int socket_client_tcp, int socket_client_udp, struct sockaddr_in proxy_addr);
int verifica_comandos(const char *porta);
void receiveFileTcp(char* nomeFicheiro,int proxy_socket_tcp,int socketCliente);
void sendFileTcp(char* nomeFicheiro,int proxy_socket_tcp);
void atualizaMemoria(char* nomeFicheiro,char* protocolo,long int tamanho);
void processaComandos();

MemPartilhada *memPartilhada;

int main(int argc, char const *argv[]) {
    int socket_client_tcp, client_socket_udp, client;
    struct sockaddr_in client_addr, proxy_addr;
    int client_addr_size;

    int shmid = shmget(IPC_PRIVATE,sizeof(MemPartilhada),IPC_CREAT|0777);
    memPartilhada = (MemPartilhada*) shmat(shmid,NULL,0);
    memPartilhada->loss=20;
    memPartilhada->tamanho=0;

    if(fork()==0) processaComandos();

    if (argc != 2) erro("ERROR: Wrong format:\n<proxy port>");
    else if (verifica_comandos(argv[1]) == 0) erro("ERROR: Wrong format:\n<proxy port>");
    else {
        inet_pton(AF_INET, PROXY_IP, &(proxy_addr.sin_addr));
        bzero((void *)&proxy_addr, sizeof(proxy_addr));
        proxy_addr.sin_family = AF_INET;
        proxy_addr.sin_port = htons(atoi(argv[1]));

        if ((socket_client_tcp = socket(AF_INET, SOCK_STREAM, 0)) < 0) erro("Socket TCP");
        if ((client_socket_udp = socket(AF_INET, SOCK_DGRAM, 0)) < 0) erro("Socket UDP");
        if (bind(socket_client_tcp, (struct sockaddr *)&proxy_addr, sizeof(proxy_addr)) < 0) erro("na funcao bind");
        if (listen(socket_client_tcp, SOMAXCONN) < 0) erro("na funcao listen");

        client_addr_size = sizeof(client_addr);
        printf("\t-- Waiting Connections -- \n");
        while (1) {
            while (waitpid(-1, NULL, WNOHANG) > 0);
            client = accept(socket_client_tcp,(struct sockaddr *)&client_addr, (socklen_t *)&client_addr_size);
            
            inet_ntop(AF_INET,&client_addr,memPartilhada->lastClienteIp,BUF_SIZE);
            memPartilhada->lastPortaCliente=client_addr.sin_port;

            if (client > 0) {
                // WHEN PROXY RECEIVES A CLIENT, IT CONNECTS TO SERVER
                if (fork() == 0) {
                    printf("\t -- Connection Created! -- \n\n");
                    close(socket_client_tcp);
                    process_client_server(client, client_socket_udp, proxy_addr);
                    exit(0);
                }
                close(client);
            }
        }
    }
    return 0;
}


void process_client_server(int socket_client_tcp, int socket_client_udp, struct sockaddr_in proxy_addr) {
    int socket_server_tcp,socket_server_udp, size_buf;
    char buffer_recv[BUF_SIZE], files_list[BUF_SIZE], protocolo[BUF_SIZE];
    struct sockaddr_in server_addr;

    // WHEN IT RECEIVES THE SERVER IP, IT CONNECTS TO THE SERVER
    size_buf= read(socket_client_tcp, buffer_recv, sizeof(buffer_recv));
    buffer_recv[size_buf-1]='\0';

    strcpy(memPartilhada->lastServerIp,buffer_recv);

    inet_pton(AF_INET, buffer_recv, &(server_addr.sin_addr));
    bzero((void *)&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);

    memPartilhada->lastPorta=SERVER_PORT;
    strcpy(memPartilhada->protocolo,"TCP");

    // CREATION OF COMUNICATION SOCKETS
    if((socket_server_tcp = socket(AF_INET, SOCK_STREAM, 0)) == -1) erro("ERROR: Socket TCP.");
    if((socket_server_udp = socket(AF_INET, SOCK_DGRAM, 0)) == -1) erro("ERROR: Socket UDP.");
    if (bind(socket_server_udp,(struct sockaddr*)&server_addr,sizeof(server_addr)) < 0) erro("bind UDP error");
    if(connect(socket_server_tcp, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) erro("Connect.");
    
    // STARTS RECEIVING COMMANDS
    while (1) {
        size_buf= read(socket_client_tcp, buffer_recv, sizeof(buffer_recv));
        buffer_recv[size_buf-1]='\0';
        

        if (strcasecmp("LIST", buffer_recv)==0) {
            write(socket_server_tcp, buffer_recv, sizeof(buffer_recv));
            read(socket_server_tcp, files_list, BUF_SIZE);
            write(socket_client_tcp, files_list, BUF_SIZE);
        }
        else if (strcasecmp("QUIT", buffer_recv)==0) {
            write(socket_client_tcp, "quit", sizeof("quit"));
            erro("\t-- Connection Lost --\n");
        } else {
            //protocolo
            remove(memPartilhada->directoriaAnterior);
            strcpy(protocolo, buffer_recv);
            write(socket_server_tcp, protocolo, BUF_SIZE);
            //encripado
            size_buf=read(socket_client_tcp, buffer_recv, BUF_SIZE);
            buffer_recv[size_buf-1]='\0';
            write(socket_server_tcp, buffer_recv, BUF_SIZE);
            //ficheiro
            size_buf=read(socket_client_tcp, buffer_recv, BUF_SIZE);
            buffer_recv[size_buf-1]='\0';
            
            write(socket_server_tcp, buffer_recv, BUF_SIZE);
            
            receiveFileTcp(buffer_recv, socket_server_tcp, socket_client_tcp);
            printf("Ficheiro recebido do server, a enviar\n");
            sendFileTcp(buffer_recv, socket_client_tcp);
            printf("Ficheiro enviado ao cliente\n");

        }
    }
}

int verifica_comandos(const char *string1) {
    for (int i = 0; i < strlen(string1) - 1; i++) {
        if (string1[i] >= '0' && string1[i] <= '9') continue;
        else return 0;
    }
    return 1;
}

void erro(char* tipo) {
    printf("%s\n", tipo);
    exit(0);
}

void receiveFileTcp(char* nomeFicheiro,int proxy_socket_tcp,int socketCliente) {
    long int tamanho, total=0, aux;
    char buffer[BUF_SIZE], diretoria[BUF_SIZE];
    FILE* f;

    strcpy(diretoria, PROXY_PATH);
    strcat(diretoria, nomeFicheiro);

    read(proxy_socket_tcp,&tamanho,sizeof(long int));
    write(socketCliente,&tamanho,sizeof(long int));
    printf("%s\n",diretoria);
    f = fopen(diretoria,"wb");
    if(f!=NULL){
        while(total<tamanho){
            aux=read(proxy_socket_tcp,buffer,sizeof(buffer));
            total+=aux;
            fwrite(buffer,1,aux,f);
        }
        printf("Total de bytes recebidos:%ld\n",total);
        fclose(f);
    }
}

void sendFileTcp(char* nomeFicheiro,int proxy_socket_tcp) {
    char buffer[BUF_SIZE], directoria[BUF_SIZE];
    int aux;

    strcpy(directoria, PROXY_PATH);
    strcat(directoria, nomeFicheiro);
    FILE* f= fopen(directoria,"rb");
    if(f==NULL)erro("ficheiro");

    while((aux=fread(buffer,1,sizeof(buffer),f))>0){
        write(proxy_socket_tcp,buffer,aux);
    }
    fclose(f);
    if(memPartilhada->save==0){
        remove(directoria);
    }
    strcpy(memPartilhada->directoriaAnterior,directoria);
}


void processaComandos(){
    char buffer[BUF_SIZE];
    int loss;
    while(1){
        scanf(" %[^\n]",buffer);
        if(strcasecmp(buffer,"SAVE")==0){
            if(memPartilhada->save==1){
                memPartilhada->save=0;
                printf("Função save desativada\n");
            }
            else if(memPartilhada->save==0){
                memPartilhada->save=1;
                printf("Função save ativada\n");
            }
        }else if(strcasecmp(buffer,"SHOW")==0){
            printf("Informações da última ligação\n\n");
            printf("Endereço de origem:%s\n",memPartilhada->lastClienteIp);
            printf("Porta de origem:%d\n",memPartilhada->lastPortaCliente);
            printf("Endereço destino:%s\n",memPartilhada->lastServerIp);
            printf("Porta destino:%d\n",memPartilhada->lastPorta);
            printf("Protocolo:%s\n",memPartilhada->protocolo);

        }
        else if(strcasecmp(strtok(buffer," "),"LOSSES")==0){
            loss=atoi(strtok(NULL,"\n"));

            memPartilhada->loss=loss;
            printf("Percentagem de bytes perdidos em UDP alterada para %d%%\n",loss);
        }
    }
}






;