#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <unistd.h>
#include "MessagePacket.h"

#define MSGLEN (1024)
#define TIMEOUT (2)

/* キーボードからの文字列入力・エコーサーバへの送信処理関数 */
int SendEchoMessage(int sock, struct sockaddr_in *pServAddr);

/* ソケットからのメッセージ受信・表示処理関数 */
int ReceiveEchoMessage(int sock, struct sockaddr_in *pServAddr);

/* マルチキャスト通信用/ユニキャスト通信用の各種データ */
int sock_recv, sock;
char *mcastIP, *servIP;
unsigned int mcastPort, servPort;
struct sockaddr_in mcastAddr, servAddr;
struct ip_mreq mcastReq;

/* 接続するユーザ名 */
char* userName;

/* ソケットオプション設定用変数 */
int optVal;                       
int maxDescriptor;
fd_set fdSet;
struct timeval tout;

/* プライベートチャット用 */
char privateChat[MSGLEN];
char* chatStringSplit[3];
char* privateSplit[2];

/* パケット生成関数 */
int Packetize(short msgID, char *msgBuf, short msgLen, char *pktBuf, int pktBufSize){
  if(msgLen > MSGLEN - 4){
    return -1;
  }

  memcpy(&pktBuf[0], &msgID, 2);
  memcpy(&pktBuf[2], &msgLen, 2);
  memcpy(&pktBuf[4], msgBuf, msgLen);

  return msgLen + 4;
}

/* 受信メッセージ生成関数 */
int Depacketize(char *pktBuf, int pktLen, short *msgID, char *msgBuf, short msgBufSize){
  if(msgBufSize != MSGLEN){
    return -1;
  }

  memcpy(msgID, &pktBuf[0], 2);
  memcpy(&msgBufSize, &pktBuf[2], 2);
  memcpy(msgBuf, &pktBuf[4], msgBufSize);

  return msgBufSize;
}

/* 使用方法の表示 */
void usage(char *userName){
  printf("\n");
  printf("*****************************************************\n");
  printf("*** Practice of Information Network | Chat System ***\n");
  printf("*****************************************************\n");
  printf("\n");
  printf("[*] Hello %s.\n", userName);
  printf("\n");
  printf("[*] Usage\n");
  printf("[*] text             : Send a chat to other chat group members.\n");
  printf("[*] /p-<name>-<body> : Send a private chat。\n");
  printf("[*] /info            : Get the chat group information.\n");
  printf("[*] /member          : See members of this chat group.\n");
  printf("[*] /quit            : Quit from this chat group.\n");
  printf("\n");
}

int isDelimiter(char p, char delim){
  return p == delim;
}

/* 文字列srcをdelimで区切りdstに格納する */
/* https://qiita.com/Tsutajiro/items/a5620b17ac530cc96e87 より */
int split(char *dst[], char *src, char delim){
  int count = 0;

  for(;;) {
    while (isDelimiter(*src, delim)){
      src++;
    }

    if (*src == '\0') break;
    dst[count++] = src;
    while (*src && !isDelimiter(*src, delim)) {
      src++;
    }
    if (*src == '\0') break;
    *src++ = '\0';
  }
  return count;
}

int main(int argc, char *argv[])
{
  char pktBuf[MSGLEN];
  int pktLen;
  int sendMsgLen;

  /* 引数の数を確認する．*/
  if (argc != 6) {
    fprintf(stderr, "Usage: %s <Server IP> <Server Port> <Group IP> <Group Port> <Name>\n", argv[0]);
    exit(1);
  }
  
  /* 第1引数からエコーサーバのIPアドレスを取得する．*/
  servIP = argv[1];
  servPort = atoi(argv[2]);

  /* メッセージの送受信に使うソケットを作成する．*/
  sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
  memset(&servAddr, 0, sizeof(servAddr));            /* 構造体をゼロで初期化 */
  servAddr.sin_family        = AF_INET;                /* インターネットアドレスファミリ */
  servAddr.sin_addr.s_addr    = inet_addr(servIP);    /* サーバのIPアドレス */
  servAddr.sin_port            = htons(servPort);        /* サーバのポート番号 */

  /* メッセージの受信に使うソケットを作成する．*/
  mcastIP = argv[3];
  mcastPort = atoi(argv[4]);
  sock_recv = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (sock_recv < 0) {
    fprintf(stderr, "socket() failed\n");
    exit(1);
  }

  /* ユーザ名を取得する */
  userName = argv[5];

  /* ローカル用アドレス構造体へ必要な値を格納する．*/
  memset(&mcastAddr, 0, sizeof(mcastAddr));            /* 構造体をゼロで初期化 */
  mcastAddr.sin_family        = AF_INET;                /* インターネットアドレスファミリ */
  mcastAddr.sin_addr.s_addr    = htonl(INADDR_ANY);    /* ワイルドカード */
  mcastAddr.sin_port        = htons(mcastPort);        /* マルチキャストポート番号 */

  /* 同一アドレスとポートに複数のプロセスが bind() するためのオプションを設定する．*/
  optVal = 1;
  if (setsockopt(sock_recv, SOL_SOCKET, SO_REUSEADDR, (void*)&optVal, sizeof(optVal)) < 0) {
    fprintf(stderr, "setsockopt() failed\n");
    exit(1);
  }
  if (setsockopt(sock_recv, SOL_SOCKET, SO_REUSEPORT, (void*)&optVal, sizeof(optVal)) < 0) {
    fprintf(stderr, "setsockopt() failed\n");
    exit(1);
  }

  /* ソケットとマルチキャスト用アドレス構造体を結び付ける．*/
  if (bind(sock_recv, (struct sockaddr*)&mcastAddr, sizeof(mcastAddr)) < 0) {
    fprintf(stderr, "bind() failed\n");
    exit(1);
  }

  /* マルチキャスト設定要求用の構造体へ必要な値を格納する．*/
  mcastReq.imr_multiaddr.s_addr = inet_addr(mcastIP);    /* マルチキャストIPアドレス */
  mcastReq.imr_interface.s_addr = htonl(INADDR_ANY);    /* ワイルドカード */

  /* マルチキャストグループへ参加する．*/
  if (setsockopt(sock_recv, IPPROTO_IP, IP_ADD_MEMBERSHIP, (void*)&mcastReq, sizeof(mcastReq)) < 0) {
    fprintf(stderr, "setsockopt() failed\n");
    exit(1);
  }

  /* select関数で処理するディスクリプタの最大値として，ソケットの値を設定する．*/
  maxDescriptor = sock_recv;

  /* チャットグループに参加する */
  pktLen = Packetize(MSG_ID_JOIN_REQUEST, 
    userName, strlen(userName), pktBuf, MSGLEN
  );

  sendMsgLen = sendto(sock, pktBuf, pktLen, 0,
    (struct sockaddr *)&servAddr, sizeof(servAddr));

  if (pktLen != sendMsgLen) {
    fprintf(stderr, "sendto() sent a different number of bytes than expected");
    return -1;
  }

  /* 文字列入力・メッセージ送信，およびメッセージ受信・表示処理ループ */
  for (;;) {

    /* ディスクリプタの集合を初期化して，キーボートとソケットを設定する．*/
    FD_ZERO(&fdSet);                /* ゼロクリア */
    FD_SET(STDIN_FILENO, &fdSet);    /* キーボード(標準入力)用ディスクリプタを設定 */
    FD_SET(sock_recv, &fdSet);            /* ソケットディスクリプタを設定 */

    /* タイムアウト値を設定する．*/
    tout.tv_sec  = TIMEOUT; /* 秒 */
    tout.tv_usec = 0;       /* マイクロ秒 */

    /* 各ディスクリプタに対する入力があるまでブロックする．*/
    if (select(maxDescriptor + 1, &fdSet, NULL, NULL, &tout) == 0) {
      /* タイムアウト */
      continue;
    }

    /* キーボードからの入力を確認する．*/
    if (FD_ISSET(STDIN_FILENO, &fdSet)) {
     /* キーボードからの入力があるので，文字列を読み込み，エコーサーバへ送信する．*/
      if (SendEchoMessage(sock, &servAddr) < 0) {
        break;
      }
    }
    
    /* ソケットからの入力を確認する．*/
    if (FD_ISSET(sock_recv, &fdSet)) {
      /* ソケットからの入力があるので，メッセージを受信し，表示する．*/
      if (ReceiveEchoMessage(sock_recv, &servAddr) < 0) {
        break;
      }
    }
  }

  /* ソケットを閉じ，プログラムを終了する．*/
  close(sock);
  close(sock_recv);
  exit(0);
}

/*
 * キーボードからの文字列入力・エコーサーバへのメッセージ送信処理関数
 */
int SendEchoMessage(int sock, struct sockaddr_in *pServAddr)
{
  char chatString[MSGLEN];
  int chatStringLen;

  char msgString[MSGLEN];
  int msgStringLen;

  char pktBuf[MSGLEN];
  int pktLen;

  int sendMsgLen;

  /* キーボードからの入力を読み込む．(※改行コードも含まれる．) */
  if (fgets(chatString, MSGLEN, stdin) == NULL) {
    /*「Control + D」が入力された．またはエラー発生．*/
    return -1;
  }

  /* プライベートチャット */
  if(strncmp(chatString, "/p", 2) == 0){
    chatString[strlen(chatString)-1] = '\0';
    split(chatStringSplit, chatString, '-');

    /* 書式の設定 */
    snprintf(msgString, MSGLEN, "[%s] %s", userName, chatStringSplit[2]);
    snprintf(msgString, MSGLEN, "%s$%s\n", msgString, chatStringSplit[1]);

    /* 入力文字列の長さを確認する．*/
    msgStringLen = strlen(msgString);
    if (msgStringLen < 1) {
      fprintf(stderr,"No input string.\n");
      return -1;
    }

    pktLen = Packetize(MSG_ID_PRIVATE_CHAT_TEXT, 
      msgString, msgStringLen, pktBuf, MSGLEN
    );

    sendMsgLen = sendto(sock, pktBuf, pktLen, 0,
      (struct sockaddr*)pServAddr, sizeof(*pServAddr));

    if (pktLen != sendMsgLen) {
      fprintf(stderr, "sendto() sent a different number of bytes than expected");
      return -1;
    }
  }

  // チャットグループ情報取得
  else if(strcmp(chatString, "/info\n") == 0){
    pktLen = Packetize(MSG_ID_GROUP_INFO_REQUEST,
      userName, strlen(userName), pktBuf, MSGLEN
    );

    sendMsgLen = sendto(sock, pktBuf, pktLen, 0,
      (struct sockaddr*)pServAddr, sizeof(*pServAddr));
  }

  // メンバー情報取得
  else if(strcmp(chatString, "/member\n") == 0){
    pktLen = Packetize(MSG_ID_USER_LIST_REQUEST,
      userName, strlen(userName), pktBuf, MSGLEN
    );

    sendMsgLen = sendto(sock, pktBuf, pktLen, 0,
      (struct sockaddr*)pServAddr, sizeof(*pServAddr));
  }

  // グループ退出
  else if(strcmp(chatString, "/quit\n") == 0){
    pktLen = Packetize(MSG_ID_LEAVE_REQUEST,
      userName, strlen(userName), pktBuf, MSGLEN
    );

    sendMsgLen = sendto(sock, pktBuf, pktLen, 0,
      (struct sockaddr*)pServAddr, sizeof(*pServAddr));
  }

  // チャット送信
  else{
    /* 書式の設定 */
    snprintf(msgString, MSGLEN, "[%s] %s", userName, chatString);

    /* 入力文字列の長さを確認する．*/
    msgStringLen = strlen(msgString);
    if (msgStringLen < 1) {
      fprintf(stderr,"No input string.\n");
      return -1;
    }

    pktLen = Packetize(MSG_ID_CHAT_TEXT, 
      msgString, msgStringLen, pktBuf, MSGLEN
    );

    sendMsgLen = sendto(sock, pktBuf, pktLen, 0,
      (struct sockaddr*)pServAddr, sizeof(*pServAddr));

    if (pktLen != sendMsgLen) {
      fprintf(stderr, "sendto() sent a different number of bytes than expected");
      return -1;
    }
  }
  
  return 0;
}

/*
 * ソケットからのメッセージ受信・表示処理関数
 */
int ReceiveEchoMessage(int sock, struct sockaddr_in *pServAddr)
{
  struct sockaddr_in fromAddr;
  unsigned int fromAddrLen;
  char pktBuffer[MSGLEN];
  int recvPktLen;
  int msgLen;
  short msgID;
  char msgBuf[MSGLEN];
  char msg[MSGLEN];

  /* エコーメッセージ送信元用アドレス構造体の長さを初期化する．*/
  fromAddrLen = sizeof(fromAddr);

  /* メッセージを受信する．*/
  recvPktLen = recvfrom(sock, pktBuffer, MSGLEN, 0,
    (struct sockaddr*)&fromAddr, &fromAddrLen);

  if (recvPktLen < 0) {
    fprintf(stderr, "recvfrom() failed");
    return -1;
  }

  msgLen = Depacketize(
    pktBuffer, recvPktLen, &msgID, msgBuf, MSGLEN
  );
  msgBuf[msgLen] = '\0';

  /* プライベートチャット受信 */
  if(msgID == (short)MSG_ID_PRIVATE_CHAT_TEXT){
    split(privateSplit, msgBuf, '$');

    privateSplit[0][strlen(privateSplit[0])] = '\0';
    privateSplit[1][strlen(privateSplit[1])-1] = '\0';

    /* 自身に向けたものであれば表示する */
    if(strcmp(privateSplit[1], userName) == 0){
      printf("(private) %s\n", privateSplit[0]);
      return 0;
    }
  }

  /* チャットテキスト受信 */
  else if(msgID == MSG_ID_CHAT_TEXT){
    printf("%s", msgBuf);
    return 0;
  }

  /* グループ情報応答受信 */
  else if(msgID == (short)MSG_ID_GROUP_INFO_RESPONSE){
    printf("\n[*] Chat group multicast address is %s\n\n", msgBuf);
    return 0;
  }

  /* グループ参加応答受信 */
  else if(msgID == MSG_ID_JOIN_RESPONSE){
    if(strcmp(msgBuf, userName) == 0){
      usage(userName);
    }else{
      printf("[*] %s joined chat group.\n", msgBuf);
    }
    return 0;
  }

  /* グループメンバー情報応答受信 */
  else if(msgID == (short)MSG_ID_USER_LIST_RESPONSE){
    printf("%s\n", msgBuf);
    return 0;
  }

  /* グループ離脱応答受信 */
  else if(msgID == MSG_ID_LEAVE_RESPONSE){
    printf("[*] %s left chat group.\n", msgBuf);

    /* 自身の離脱要求が許可されれば終了 */
    if(strcmp(msgBuf, userName) == 0){
      exit(0);
    }

    return 0;
  }

  return 0;
}