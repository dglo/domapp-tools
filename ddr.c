#include <stdio.h>
#include <fcntl.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <errno.h>
#include <unistd.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h> /* for EAGAIN */
#define _GNU_SOURCE
#include <getopt.h>
#include <sys/poll.h>

#include "domapp.h"

main() {
  char filen[] = "/dev/dhc0w0dA";
  int charlen = 5;
  int bufsiz=4092;
  int fd = open(filen, O_RDWR);
  DOMMSG * echoMsg0 = newEchoMsg();
  DOMMSG * echoMsg1 = newEchoMsg();
  DOMMSG * echoMsg2 = newEchoMsg();
  int msglen = 8+charlen;

  DOMMSG * msgReply = newMsg();
  setMsgDataLen(echoMsg0, charlen);
  setMsgDataLen(echoMsg1, charlen);
  setMsgDataLen(echoMsg2, charlen);
  int i;
  for(i=0; i<charlen; i++) echoMsg0->data[i] = '0'+i;

  setMsgID(echoMsg0, 55);
  setMsgID(echoMsg1, 66);
  setMsgID(echoMsg2, 77);

  char * doublebuf = malloc(2*(msglen));
  if(!doublebuf) return -1;
  for(i=0; i<msglen; i++) {
    doublebuf[i] = ((char *) echoMsg0)[i];
    doublebuf[i+msglen] = ((char *) echoMsg1)[i];
  }

  int len = write(fd, doublebuf, 2*(msglen));
  printf("TX %d bytes.\n", len);
  len = getMsg(fd, msgReply, bufsiz, 1000);
  printf("RX %d bytes: ", len); len<=0?printf("\n"):dumpMsg(stdout, "reply", msgReply);
  len = getMsg(fd, msgReply, bufsiz, 1000);
  printf("RX %d bytes: ", len); len<=0?printf("\n"):dumpMsg(stdout, "reply", msgReply);
  len = getMsg(fd, msgReply, bufsiz, 1000);
  printf("RX %d bytes: ", len); len<=0?printf("\n"):dumpMsg(stdout, "reply", msgReply);

  len = sendMsg(fd, echoMsg2, 100);
  printf("TX %d bytes.\n", len);
  len = getMsg(fd, msgReply, bufsiz, 1000);
  printf("RX %d bytes: ", len); len<=0?printf("\n"):dumpMsg(stdout, "reply", msgReply);
  len = getMsg(fd, msgReply, bufsiz, 1000);
  printf("RX %d bytes: ", len); len<=0?printf("\n"):dumpMsg(stdout, "reply", msgReply);
  len = getMsg(fd, msgReply, bufsiz, 1000);
  printf("RX %d bytes: ", len); len<=0?printf("\n"):dumpMsg(stdout, "reply", msgReply);

  close(fd);
}
