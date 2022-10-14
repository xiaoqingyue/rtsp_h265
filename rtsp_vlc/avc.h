#ifndef AVC_H
#define AVC_H


int startCode3(char* buf);
int startCode4(char* buf);
char* findNextStartCode(char* buf, int len);
int getFrameFromH264File(int fd, char* frame, int size);


#endif // AVC_H
