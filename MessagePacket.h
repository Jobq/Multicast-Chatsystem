
#ifndef _MESSAGE_PACKET_H_
#define _MESSAGE_PACKET_H_
#define MSG_ID_NONE						(0x0000)

/* チャットテキスト */
#define MSG_ID_CHAT_TEXT				(0x1010)	/* クライアント <--> サーバ */

/* グループへの参加要求と応答 */
#define MSG_ID_JOIN_REQUEST				(0x1021)	/* クライアント ---> サーバ */
#define MSG_ID_JOIN_RESPONSE			(0x1022)	/* クライアント <--- サーバ */

/* グループからの退出要求と応答 */
#define MSG_ID_LEAVE_REQUEST			(0x1031)	/* クライアント ---> サーバ */
#define MSG_ID_LEAVE_RESPONSE			(0x1032)	/* クライアント <--- サーバ */

/* 特定の参加者に対するチャットテキスト */
#define MSG_ID_PRIVATE_CHAT_TEXT		(0xa010)	/* クライアント <--> サーバ */

/* チャットグループ情報の要求と応答 */
#define MSG_ID_GROUP_INFO_REQUEST		(0xa021)	/* クライアント ---> サーバ */
#define MSG_ID_GROUP_INFO_RESPONSE		(0xa022)	/* クライアント <--- サーバ */

/* チャットグループ内の参加者リスト要求と応答 */
#define MSG_ID_USER_LIST_REQUEST		(0xa031)	/* クライアント ---> サーバ */
#define MSG_ID_USER_LIST_RESPONSE		(0xa032)	/* クライアント <--- サーバ */

/* 一定時間応答の無いクライアントに対しての接続確認要求と応答 */
#define MSG_ID_CONFIRMATION_REQUEST		(0xa041)	/* サーバ ---> クライアント */
#define MSG_ID_CONFIRMATION_RESPONSE	(0xa042)	/* サーバ <--- クライアント */

/*------------------------------------------------------------------------------
 *
 * ■送信パケット生成関数
 *
 * ［関数名］
 *   Packetize
 *
 * ［機能］
 *   送信メッセージを送信パケットバッファに格納する．
 *   この関数呼び出しの後，send()/sendto() 関数を用いてパケットを送信する．
 *
 * ［引数］
 *   ・msgID     ：[IN ] メッセージID
 *   ・msgBuf    ：[IN ] メッセージバッファの先頭番地
 *   ・msgLen    ：[IN ] メッセージバッファに含まれるメッセージ長
 *   ・pktBuf    ：[OUT] 送信用パケットバッファの先頭番地
 *   ・pktBufSize：[IN ] 送信用パケットバッファのサイズ
 *
 * ［戻り値］
 *   ・送信パケットバッファ内のデータ長
 *   ※エラー発生時はマイナスの値(各自定義)を戻り値とする．
 *
 */
extern int Packetize(
	short msgID, char *msgBuf, short msgLen,
	char *pktBuf, int pktBufSize
);

/*------------------------------------------------------------------------------
 *
 * ■受信メッセージ生成関数
 *
 * ［関数名］
 *   Depacketize
 *
 * ［機能］
 *   受信パケットバッファからメッセージを取得する．
 *   recv()/recvfrom() 関数を用いてパケットを 1つ分受信した後に，
 *   この関数を呼び出してメッセージを取得する．
 *
 * ［引数］
 *   ・pktBuf    ：[IN ] 受信パケットバッファの先頭番地
 *   ・pktLen    ：[IN ] 受信パケットバッファに含まれる受信パケット長
 *   ・msgID     ：[OUT] メッセージID
 *   ・msgBuf    ：[OUT] メッセージバッファの先頭番地
 *   ・msgBufSize：[IN ] メッセージバッファのサイズ
 *
 * ［戻り値］
 *   ・メッセージバッファ内のメッセージ長
 *   ※エラー発生時はマイナスの値(各自定義)を戻り値とする．
 *
 */
extern int Depacketize(
	char *pktBuf, int pktLen,
	short *msgID, char *msgBuf, short msgBufSize
);

#endif /* _MESSAGE_PACKET_H_ */