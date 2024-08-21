#include "csapp.h"

int main(int argc, char **argv) {
    int clientfd;
    char *host, *port, buf[MAXLINE];
    rio_t rio;

    if (argc != 3) {
        fprintf(stderr, "usage: %s <host> <port>\n", argv[0]);
        exit(0);
    }
    host = argv[1];
    port = argv[2];

    clientfd = Open_clientfd(host, port); // 자동으로 소켓 주소 구조체를 생성하고 connect를 호출함. socket의 파일 식별자를 반환
    Rio_readinitb(&rio, clientfd);

    while (Fgets(buf, MAXLINE, stdin) != NULL) { // input을 버퍼에 복사
        Rio_writen(clientfd, buf, strlen(buf)); // 버퍼의 내용을 식별자 clientfd의 파일 위치에 쓰기(네트워크로 전송)
        Rio_readlineb(&rio, buf, MAXLINE); // 서버에서 보낸 내용을 버
        Fputs(buf, stdout); // 버퍼의 내용을 출력함
    }
    Close(clientfd);
    exit(0);
}