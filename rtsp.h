#ifndef RTSP_H
#define RTSP_H




char* getLineFromBuf(char* buf, char* line);
int handleCmd_OPTIONS(char* result, int cseq);
int handleCmd_DESCRIBE(char* result, int cseq, char* url);
int handleCmd_SETUP(char* result, int cseq, int clientRtpPort);
int handleCmd_PLAY(char* result, int cseq);


#endif // RTSP_H
