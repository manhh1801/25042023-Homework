#include <arpa/inet.h>
#include <sys/socket.h>
#include <stdio.h>
#include <sys/unistd.h>
#include <stdlib.h>
#include <string.h>

int main()
{

    /*<Setting up socket>*/

    printf("\n");

    int server=socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    struct sockaddr_in sv_addr;
    sv_addr.sin_family=AF_INET;
    sv_addr.sin_addr.s_addr=htonl(INADDR_ANY);
    sv_addr.sin_port=htons(9000);

    if(bind(server, (struct sockaddr*)&sv_addr, sizeof(sv_addr)))
    {
        perror("Bind address to socket failed.\n");
        return 1;
    }

    /*<>*/


    /*<Waiting and accepting connection>*/

    if(listen(server, 5))
    {
        perror("Connect failed.\n");
        return 1;
    }

    /*<>*/


    /*<Communicating>*/

    fd_set fd_read;

    typedef struct client
    {
        int client;
        struct sockaddr_in cl_addr;
        char* name;
        char state;
        struct client *next;
    }
    client;
    client** clients=malloc(sizeof(client*));
    *clients=NULL;

    while(1)
    {
        FD_SET(server, &fd_read);
        client* temp=*clients;
        while(temp!=NULL)
        {
            FD_SET(temp->client, &fd_read);
            temp=temp->next;
        }
        FD_SET(STDIN_FILENO, &fd_read);

        if(select(FD_SETSIZE, &fd_read, NULL, NULL, NULL)==-1)
        {
            printf("Exception occured!\n");
            break;
        }

        if(FD_ISSET(server, &fd_read))
        {
            client* new_client=malloc(sizeof(client));
            int cl_addr_length=sizeof(new_client->cl_addr);
            new_client->client=accept(server, (struct sockaddr*)&(new_client->cl_addr), &cl_addr_length);
            new_client->name=NULL;
            new_client->state=0;
            new_client->next=*clients;
            *clients=new_client;
            printf("Accepted connection: %s:%d\n", inet_ntoa((new_client->cl_addr).sin_addr), ntohs((new_client->cl_addr).sin_port));
        }

        temp=*clients;
        client* prev=NULL;
        while(temp!=NULL)
        {
            if(FD_ISSET(temp->client, &fd_read))
            {
                unsigned int buf_count=0;
                char **buf_storage=NULL;

                if(read(temp->client, &buf_count, 4)<=0)
                {
                    printf("%s:%d - %s disconnected.\n", inet_ntoa((temp->cl_addr).sin_addr), ntohs((temp->cl_addr).sin_port), temp->name);
                    client* delete=temp;
                    temp=temp->next;
                    free(delete);
                    if(prev!=NULL) {prev->next=temp;}
                    else {*clients=temp;}
                }
                else
                {
                    char message_state=0;

                    for(int index=0; index<buf_count; index++)
                    {
                        char *buf=malloc(64);
                        read(temp->client, buf, 64);
                        buf_storage=(char**)realloc(buf_storage, 8*(buf_count+1));
                        buf_storage[buf_count++]=buf;
                    }

                    if(temp->state==0)
                    {
                        int buf_count_index=0, buf_index=0;
                        char found=0;
                        for(; buf_count_index<buf_count; buf_count_index++)
                        {
                            for(buf_index=0; buf_index<64; buf_index++)
                            {
                                if(buf_storage[buf_count_index][buf_index]==':')
                                {
                                    found=1;
                                    break;
                                }
                            }
                            if(found==1) break;
                        }
                        if(buf_count_index!=0&&buf_index!=0)
                        {
                            if(buf_count_index!=(buf_count-1)&&buf_index!=63)
                            {
                                char* temp_str=malloc(1);
                                for(int index=0; index<=buf_count_index; index++)
                                {
                                    if(index==buf_count_index)
                                    {
                                        for(int index2=0; index2<=buf_index; index2++) {strncat(temp_str, &buf_storage[index][index2], 1);}
                                        char null='\0';
                                        strncat(temp_str, &null, 1);
                                        continue;
                                    }
                                    for(int index2=0; index2<64; index2++) {strncat(temp_str, &buf_storage[index][index2], 1);}
                                }
                                temp->name=malloc(strlen(temp_str));
                                memcpy((char*)temp->name, temp_str, strlen(temp_str));

                                message_state=1;
                            }
                        }
                        write(temp->client, &message_state, 1);
                    }
                    else
                    {
                        message_state=2;

                        printf("Received from %s:%d - %s: ", inet_ntoa((temp->cl_addr).sin_addr), ntohs((temp->cl_addr).sin_port), temp->name);
                        for(int index=0; index<buf_count; index++) {printf("%s", buf_storage[index]);}

                        client* temp2=*clients;
                        while(temp2!=NULL)
                        {
                            if(temp2!=temp)
                            {
                                write(temp2->client, &message_state, 1);

                                int byte_sent=strlen(temp->name);
                                write(temp2->client, &byte_sent, 4);
                                write(temp2->client, &(temp->name), byte_sent);

                                write(temp2->client, &buf_count, 4);
                                for(int index=0; index<buf_count; index++) { write(temp2->client, buf_storage[index], 64);}
                            }
                            temp2=temp2->next;
                        }
                    }

                    for(int index=0; index<buf_count; index++) {free(buf_storage[index]);}
                    free(buf_storage);

                    prev=temp;
                    temp=temp->next;
                }
            }
        }

        if(FD_ISSET(STDIN_FILENO, &fd_read))
        {
            int message_state=3;
            unsigned int buf_count=0;
            char **buf_storage=NULL;

            while(1)
            {
                char* input_recv=malloc(65);
                fgets(input_recv, 64, stdin);
                int buf_length=strlen(input_recv);
                if(buf_length==64)
                {
                    char* buf=malloc(64);
                    memcpy(buf, input_recv, 64);
                    buf_storage=(char**)realloc(buf_storage, 8*(buf_count+1));
                    buf_storage[buf_count++]=buf;
                    if(buf[63]==10)
                    {
                        buf[63]='\0';
                        break;
                    }
                }
                else
                {
                    if(buf_length!=1)
                    {
                        char* buf=malloc(64);
                        memcpy(buf, input_recv, 64);
                        buf_storage=(char**)realloc(buf_storage, 8*(buf_count+1));
                        buf_storage[buf_count++]=buf;
                        for(int index=buf_length-1; index<64; index++) {buf[index]='\0';}
                    }
                    break;
                }
            }

            temp=*clients;
            while(temp!=NULL)
            {
                write(temp->client, &message_state, 4);
                for(int index=0; index<buf_count; index++) {write(temp->client, buf_storage[index], 64);}
                temp=temp->next;
            }
            printf("Sent: ");
            for(int index=0; index<buf_count; index++) {printf("%s", buf_storage[index]);}
            printf("\n");
        }

        FD_ZERO(&fd_read);
    }

    printf("\n");

    /*<>*/


    /*<Close connection>*/

    close(server);
    printf("Server closed.\n");
    printf("\n");

    return 1;

    /*<>*/

}