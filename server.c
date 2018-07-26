#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/file.h>
#include <errno.h>
#include <signal.h>

#include "MessagePacket.h"

#define MSGLEN (1024)

int sock, sock_send; /* ソケットディスクリプタ */
struct sockaddr_in mcastAddr;        /* マルチキャスト用アドレス構造体 */

unsigned short servPort; /* エコーサーバ(ローカル)のポート番号 */
char* servIP;
struct sockaddr_in servAddr; /* エコーサーバ(ローカル)用アドレス構造体 */
struct sigaction sigAction; /* シグナルハンドラ設定用構造体 */

/* マルチキャスト用 */
char *mcastIP;                    /* マルチキャストIPアドレス */
unsigned int mcastPort;            /* マルチキャストポート番号 */
unsigned char mcastTTL;            /* マルチキャストパケットのTTL */

/* グループメンバ管理用 */
char members[100][100];
int is_connect[100];
int id = 0;

/* チャットグループ情報用 */
char groupInfoMsg[MSGLEN];

/* グループメンバー情報用 */
char groupMemberStrRow[MSGLEN];
char groupMemberStr[MSGLEN * 100];

 /* SIGIO 発生時のシグナルハンドラ */
void IOSignalHandler(int signo);

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

/* グループ退出処理 */
int leave_member(char *targetUserName){
  int i;
  for(i = 0; i <= 99; i++){
    if(strcmp(members[i], targetUserName) == 0){
      is_connect[i] = 0;
      return 0;
    }
  }

  return -1;
}


int main(int argc, char * argv[]) {
  /* 引数の数を確認する．*/
  if (argc != 5) {
    fprintf(stderr, "Usage: %s<Listen IP> <Listen Port> <Group IP> <Group Port>\n", argv[0]);
    exit(1);
  }

  int i;
  for(i = 0; i <= 99; i++){
    is_connect[i] = 0;
    strcpy(members[i], "");
  }

  servIP = argv[1];
  servPort = atoi(argv[2]);

  /* メッセージの送受信に使うソケットを作成する．*/
  sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (sock < 0) {
    fprintf(stderr, "socket() failed\n");
    exit(1);
  }

  /* エコーサーバ(ローカル)用アドレス構造体へ必要な値を格納する．*/
  memset( & servAddr, 0, sizeof(servAddr)); /* 構造体をゼロで初期化 */
  servAddr.sin_family = AF_INET; /* インターネットアドレスファミリ */
  servAddr.sin_addr.s_addr = inet_addr(servIP); /* ワイルドカード */
  servAddr.sin_port = htons(servPort);

  /* ソケットとエコーサーバ(ローカル)用アドレス構造体を結び付ける．*/
  if (bind(sock, (struct sockaddr *)&servAddr, sizeof(servAddr)) < 0) {
    fprintf(stderr, "bind() failed\n");
    exit(1);
  }

  /* マルチキャスト用 */
  mcastIP = argv[3];
  mcastPort = atoi(argv[4]);
  mcastTTL = 1;

  /* メッセージの送信に使うソケットを作成する．*/
  sock_send = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (setsockopt(sock_send, IPPROTO_IP, IP_MULTICAST_TTL, (void*)&mcastTTL, sizeof(mcastTTL)) < 0) {
    fprintf(stderr, "setsockopt() failed\n");
    exit(1);
  }

  /* マルチキャスト用アドレス構造体へ必要な値を格納する．*/
  memset(&mcastAddr, 0, sizeof(mcastAddr));            /* 構造体をゼロで初期化 */
  mcastAddr.sin_family        = AF_INET;                /* インターネットアドレスファミリ */
  mcastAddr.sin_addr.s_addr    = inet_addr(mcastIP);    /* マルチキャストIPアドレス */
  mcastAddr.sin_port        = htons(mcastPort);        /* マルチキャストポート番号 */

  /* シグナルハンドラを設定する．*/
  sigAction.sa_handler = IOSignalHandler;

  /* ハンドラ内でブロックするシグナルを設定する(全てのシグナルをブロックする)．*/
  if (sigfillset( &sigAction.sa_mask) < 0) {
    fprintf(stderr, "sigfillset() failed\n");
    exit(1);
  }

  /* シグナルハンドラに関するオプション指定は無し．*/
  sigAction.sa_flags = 0;

  /* シグナルハンドラ設定用構造体を使って，シグナルハンドラを登録する．*/
  if (sigaction(SIGIO, &sigAction, 0) < 0) {
    fprintf(stderr, "sigaction() failed\n");
    exit(1);
  }

  /* このプロセスがソケットに関するシグナルを受け取るための設定を行う．*/
  if (fcntl(sock, F_SETOWN, getpid()) < 0) {
    fprintf(stderr, "Unable to set process owner to us\n");
    exit(1);
  }

  /* ソケットに対してノンブロッキングと非同期I/Oの設定を行う．*/
  if (fcntl(sock, F_SETFL, O_NONBLOCK | FASYNC) < 0) {
    fprintf(stderr, "Unable to put the sock into nonblocking/async mode\n");
    exit(1);
  }

  printf("[*] Server Activated.\n");

  /* エコーメッセージの受信と送信以外の処理をする．*/
  for (;;) {
    sleep(1);
  }
}

/* SIGIO 発生時のシグナルハンドラ */
void IOSignalHandler(int signo) {
  struct sockaddr_in clntAddr; /* クライアント用アドレス構造体 */
  unsigned int clntAddrLen; /* クライアント用アドレス構造体の長さ */
  char pktBuf[MSGLEN]; /* メッセージ送受信バッファ */
  int recvPktLen; /* 受信メッセージの長さ */
  int sendMsgLen; /* 送信メッセージの長さ */

  int msgID, pktLen;
  char msgBuf[MSGLEN];
  int msgBufSize;

  /* 受信データがなくなるまで，受信と送信を繰り返す．*/
  do {
    /* クライアント用アドレス構造体の長さを初期化する．*/
    clntAddrLen = sizeof(clntAddr);

    /* クライアントからメッセージを受信する．(※この呼び出しはブロックしない) */
    recvPktLen = recvfrom(sock, pktBuf, MSGLEN, 0, (struct sockaddr *)&clntAddr, &clntAddrLen);
    
    /* 受信メッセージの長さを確認する．*/
    if (recvPktLen < 0) {
      /* errono が EWOULDBLOCK である場合，受信データが無くなったことを示す．*/
      /* EWOULDBLOCK は，許容できる唯一のエラー．*/
      if (errno != EWOULDBLOCK) {
        fprintf(stderr, "recvfrom() failed\n");
        exit(1);
      }
    } else {
      /* パケットを受信する */
      msgBufSize = Depacketize(pktBuf, recvPktLen, (short *)&msgID, msgBuf, MSGLEN);
      msgBuf[msgBufSize] = '\0';

      /* グループ参加要求 */
      if(msgID == MSG_ID_JOIN_REQUEST){
        printf("[*] JOIN REQUEST from %s, assigned ID %d\n", msgBuf, id);

        strcpy(members[id], msgBuf);
        is_connect[id++] = 1;

        pktLen = Packetize(MSG_ID_JOIN_RESPONSE, 
          msgBuf, strlen(msgBuf), pktBuf, sizeof(pktBuf)
        );

        /* 受信メッセージをそのままマルチキャストグループに送信する．*/
        sendMsgLen = sendto(sock_send, pktBuf, pktLen, 0,
            (struct sockaddr*)&mcastAddr, sizeof(mcastAddr));
      }

      /* チャットグループ情報要求 */
      else if(msgID == MSG_ID_GROUP_INFO_REQUEST){
        printf("[*] GROUP INFO REQUEST from %s\n", msgBuf);

        snprintf(groupInfoMsg, MSGLEN, "%s:%d", mcastIP, mcastPort);
        snprintf(groupInfoMsg, MSGLEN, "%s, requested by %s", groupInfoMsg, msgBuf);

        pktLen = Packetize(
          MSG_ID_GROUP_INFO_RESPONSE, 
          groupInfoMsg, 
          strlen(groupInfoMsg), 
          pktBuf, 
          sizeof(pktBuf)
        );

        sendMsgLen = sendto(sock_send, pktBuf, pktLen, 0,
            (struct sockaddr*)&mcastAddr, sizeof(mcastAddr));
      }

      /* メンバー情報要求 */
      else if(msgID == MSG_ID_USER_LIST_REQUEST){
        printf("[*] USER LIST REQUEST from %s\n", msgBuf);

        snprintf(
          groupMemberStr, 
          MSGLEN*100, 
          "\n\n[*] Group member request from %s%s\n",
          "",
          msgBuf
        );

        snprintf(
          groupMemberStr, 
          MSGLEN*100,
          "%s%s\n",
          groupMemberStr,
          "[*] Members"
        );
        
        int i;
        for(i = 0; i < id; i++){
          if(is_connect[i]){
            snprintf(
              groupMemberStr, MSGLEN*100, "\t%s%s\n", groupMemberStr, members[i]
            );
          }else{
            snprintf(
              groupMemberStr, MSGLEN*100, "\t%s%s (Disconnected)\n", groupMemberStr, members[i]
            );
          }
        }

        pktLen = Packetize(
          MSG_ID_USER_LIST_RESPONSE, 
          groupMemberStr, 
          strlen(groupMemberStr), 
          pktBuf, 
          sizeof(pktBuf)
        );

        sendMsgLen = sendto(sock_send, pktBuf, pktLen, 0,
            (struct sockaddr*)&mcastAddr, sizeof(mcastAddr));
      }

      /* グループ離脱要求 */
      else if(msgID == MSG_ID_LEAVE_REQUEST){
        printf("[*] LEAVE REQUEST from %s\n", msgBuf);

        if(leave_member(msgBuf) == 0){
          pktLen = Packetize(
            MSG_ID_LEAVE_RESPONSE,  
            msgBuf, 
            strlen(msgBuf), 
            pktBuf, 
            sizeof(pktBuf)
          );

          /* 受信メッセージをそのままマルチキャストグループに送信する．*/
          sendMsgLen = sendto(sock_send, pktBuf, pktLen, 0,
              (struct sockaddr*)&mcastAddr, sizeof(mcastAddr));
        }else{
          printf("Error!\n");
          return;
        }

      }

      /* プライベートチャットテキストの受信 */
      else if(msgID == MSG_ID_PRIVATE_CHAT_TEXT){
        printf("[*] %s", msgBuf);
        pktLen = Packetize(MSG_ID_PRIVATE_CHAT_TEXT, 
          msgBuf, msgBufSize, pktBuf, sizeof(pktBuf)
        );

        /* 受信メッセージをそのままマルチキャストグループに送信する．*/
        sendMsgLen = sendto(sock_send, pktBuf, pktLen, 0,
            (struct sockaddr*)&mcastAddr, sizeof(mcastAddr));
      }

      /* チャットテキストの受信 */
      else if(msgID == MSG_ID_CHAT_TEXT){
        printf("[*] %s", msgBuf);
        pktLen = Packetize(msgID, msgBuf, msgBufSize, pktBuf, sizeof(pktBuf));

        /* 受信メッセージをそのままマルチキャストグループに送信する．*/
        sendMsgLen = sendto(sock_send, pktBuf, pktLen, 0,
            (struct sockaddr*)&mcastAddr, sizeof(mcastAddr));
      }
      
    }
  } while (recvPktLen >= 0);
}