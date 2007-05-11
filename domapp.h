/* domapp.h - message information for domapp messages
   Refer to code in domapp project -- should really use that code,
   but a few definitions are put here for convenience.
   jacobsen@npxdesigns.com June 2004
*/

#ifndef __DOMAPP_H__
#define __DOMAPP_H__

#include <stdarg.h>
#include <sys/poll.h>

#define UBYTE unsigned char

#define MSG_HDR_LEN 8
#define MAX_DATA_LEN (4092-MSG_HDR_LEN)

typedef struct {
  union HEAD{
    struct HD {
      UBYTE mt;
      UBYTE mst;
      UBYTE dlenHI;
      UBYTE dlenLO;
      UBYTE res[2];
      UBYTE msgID;
      UBYTE status;
    } hd;
    UBYTE h[MSG_HDR_LEN];
  } head;
  UBYTE data[MAX_DATA_LEN]; /* Max size, will in fact be limited by buffer driver size */
} DOMMSG;

inline int msgType(DOMMSG *m) { return m->head.hd.mt; }
inline int msgSubType(DOMMSG *m) { return m->head.hd.mst; }
inline int msgDataLen(DOMMSG *m) { return (m->head.hd.dlenHI << 8) | m->head.hd.dlenLO; }
inline int msgStatus(DOMMSG *m) { return m->head.hd.status; }
inline int getMsgID(DOMMSG *m) { return m->head.hd.msgID; }

#define MSGHAND_GET_DOM_ID             10
#define MSGHAND_GET_DOM_NAME           11
#define MSGHAND_GET_DOM_VER            12
#define MSGHAND_GET_PKT_STATS          13
#define MSGHAND_GET_MSG_STATS          14
#define MSGHAND_CLR_PKT_STATS          15
#define MSGHAND_CLR_MSG_STATS          16
#define MSGHAND_GET_ATWD_ID            17
#define MSGHAND_ECHO_MSG               18
#define MSGHAND_ACCESS_MEMORY_CONTENTS 20
#define MSGHAND_REBOOT_CPU_FLASH       23
#define MSGHAND_GET_DOMAPP_RELEASE     24
#define DATA_ACC_DATA_AVAIL        10
#define DATA_ACC_GET_DATA          11
#define DATA_ACC_GET_NEXT_MONI_REC 12
#define DATA_ACC_SET_MONI_IVAL     13 /* Set monitoring interval for
                                         hardware and configuration snapshot
                                         records */
#define DATA_ACC_SET_ENG_FMT       14
#define DATA_ACC_TEST_SW_COMP      15 /* Inject software compression test pattern data */
#define DATA_ACC_SET_BASELINE_THRESHOLD 16
#define DATA_ACC_GET_BASELINE_THRESHOLD 17
#define DATA_ACC_SET_SW_DATA_COMPRESSION 18
#define DATA_ACC_GET_SW_DATA_COMPRESSION 19
#define DATA_ACC_SET_SW_DATA_COMPRESSION_FORMAT 20
#define DATA_ACC_GET_SW_DATA_COMPRESSION_FORMAT 21
#define DATA_ACC_RESET_LBM 22
#define DATA_ACC_GET_FB_SERIAL 23
#define DATA_ACC_SET_DATA_FORMAT   24
#define DATA_ACC_GET_DATA_FORMAT   25
#define DATA_ACC_SET_COMP_MODE     26
#define DATA_ACC_GET_COMP_MODE     27
#define DATA_ACC_GET_SN_DATA       28
#define DATA_ACC_RESET_MONI_BUF    29
#define DATA_ACC_MONI_AVAIL        30
#define DSC_READ_ALL_ADCS 10
#define DSC_READ_ONE_ADC 11
#define DSC_READ_ALL_DACS 12
#define DSC_WRITE_ONE_DAC 13
#define DSC_SET_PMT_HV 14
#define DSC_ENABLE_PMT_HV 16
#define DSC_DISABLE_PMT_HV 18
#define DSC_QUERY_PMT_HV 22
#define DSC_READ_ONE_ADC_REPT 27
#define DSC_GET_PMT_HV_LIMIT 28
#define DSC_SET_PMT_HV_LIMIT 29
#define DSC_READ_ONE_DAC 30
#define DSC_SET_TRIG_MODE 31
#define DSC_GET_TRIG_MODE 32
#define DSC_SELECT_ATWD 33
#define DSC_WHICH_ATWD 34
#define DSC_MUX_SELECT 35
#define DSC_WHICH_MUX 36
#define DSC_SET_PULSER_RATE 37
#define DSC_GET_PULSER_RATE 38
#define DSC_SET_PULSER_ON 39
#define DSC_SET_PULSER_OFF 40
#define DSC_PULSER_RUNNING 41
#define DSC_GET_RATE_METERS 42
#define DSC_SET_SCALER_DEADTIME 43
#define DSC_GET_SCALER_DEADTIME 44
#define DSC_SET_LOCAL_COIN_MODE 45
#define DSC_GET_LOCAL_COIN_MODE 46
#define DSC_SET_LOCAL_COIN_WINDOW 47
#define DSC_GET_LOCAL_COIN_WINDOW 48
#define DSC_SET_LC_TYPE 49
#define DSC_GET_LC_TYPE 50
#define DSC_SET_LC_TX 51
#define DSC_GET_LC_TX 52
#define DSC_SET_LC_SRC 53
#define DSC_GET_LC_SRC 54
#define DSC_SET_LC_SPAN 55
#define DSC_GET_LC_SPAN 56
#define DSC_SET_LC_CABLE_LEN 57
#define DSC_GET_LC_CABLE_LEN 58
#define DSC_ENABLE_SN 59
#define DSC_DISABLE_SN 60
#define EXPCONTROL_BEGIN_RUN 12
#define EXPCONTROL_END_RUN 13
#define EXPCONTROL_DO_PEDESTAL_COLLECTION 16
#define EXPCONTROL_GET_NUM_PEDESTALS 19
#define EXPCONTROL_GET_PEDESTAL_AVERAGES 20
#define EXPCONTROL_ZERO_PEDESTALS 25
#define EXPCONTROL_BEGIN_FB_RUN 27
#define EXPCONTROL_END_FB_RUN 28

enum Type {
  MESSAGE_HANDLER=1,
  DOM_SLOW_CONTROL,
  DATA_ACCESS,
  EXPERIMENT_CONTROL,
        TEST_MANAGER
};

int getMsg(int filep, DOMMSG * m, int bufsiz, int maxpoll) {
  /* Return message length if successful; 
     return 0 if timeout 
     return -1 if error 
  */
  char * buf = malloc(bufsiz);
  if(buf == NULL) {
    printf("getMsg: Can't get temporary buffer of %d bytes.\n", bufsiz);
    return -1;
  }

  /* Do first read to get message length */
  struct pollfd fds;
  fds.fd     = filep; 
  fds.events = POLLIN;
  int ret    = poll(&fds, 1, maxpoll);
  if(ret < 0 || (! (fds.revents & POLLIN))) { free(buf); return 0; }

  int nr = read(filep, buf, bufsiz);
  int i;
  if(nr < MSG_HDR_LEN) {
    fprintf(stderr, "getMsg: Short read (%d bytes) of header!\n", nr);
    for(i=0;i<nr;i++)
      fprintf(stderr, "\t%02x %c\n",buf[i],buf[i]>33&&buf[i]<126?buf[i]:'X');
    free(buf); 
    return -1;
  }

  /* Have at least 8 bytes now */
  int databytes = nr-MSG_HDR_LEN;
  memcpy(&(m->head.h[0]), buf, MSG_HDR_LEN);
  int datalen = msgDataLen(m);
  if(datalen > bufsiz) {
    fprintf(stderr,"%d byte message too large, exceeds max=%d bytes.\n"
	    "Header=(", datalen, bufsiz);
    for(i=0;i<8;i++) 
      fprintf(stderr, "%02x ",(int)m->head.h[i]);
    for(i=0;i<8;i++) 
      fprintf(stderr," %c",m->head.h[i]>33&&m->head.h[i]<126?m->head.h[i]:'X');
    fprintf(stderr,").\n");
    free(buf);
    return -1;
  }

  int lentarget = databytes+MSG_HDR_LEN;
  if(nr != lentarget) {
    fprintf(stderr,"getMsg: Odd read length, wanted %d, got %d.\n", lentarget, nr);
    free(buf);
    return -1;
  }

  if(databytes > 0)
    memcpy(m->data, buf+MSG_HDR_LEN, databytes);
  free(buf);
  return nr;
}

void zeroMsg(DOMMSG *m) { bzero(m->head.h, MSG_HDR_LEN); }

void setMsgType(DOMMSG *m, int type) { m->head.hd.mt = type; }

void setMsgSubtype(DOMMSG *m, int subtype) { m->head.hd.mst = subtype; }

void setMsgDataLen(DOMMSG *m, int datalen) { 
  m->head.hd.dlenHI = (datalen>>8)&0xFF;
  m->head.hd.dlenLO = datalen&0xFF;
}

void setMsgStatus(DOMMSG *m, int status) { m->head.hd.status = status; }

void setMsgID(DOMMSG *m, unsigned char id) { m->head.hd.msgID = id; }

int sendMsg(int filep, DOMMSG * m, int maxpoll) {
  /* Send message on filep.  Return number of bytes written, or 0 if timeout,
     or -1 if error */

  /* datalen == length of data portion of message; can be 0; */     
  int datalen = msgDataLen(m);
  int msglen  = datalen + MSG_HDR_LEN;
  struct pollfd fds;
  fds.fd      = filep;
  fds.events  = POLLOUT;

  int ret     = poll(&fds, 1, maxpoll);
  if(ret < 0) return 0;
  if(!fds.revents & POLLOUT) return 0;

  char * buf = malloc(msglen);
  if(buf == NULL) {
    fprintf(stderr,"sendMsg: malloc failed.\n");
    return -1;
  }
  memcpy(buf, &(m->head.h[0]), MSG_HDR_LEN);
  memcpy(buf+MSG_HDR_LEN, m->data, datalen);
  int nw = write(filep, buf, msglen);
  if(nw != MSG_HDR_LEN+datalen) {
    fprintf(stderr,"sendMsg: bad write length: %d != %d.\n", nw, MSG_HDR_LEN+datalen);
    return -1;
  }
  free(buf);
  return nw;
}

DOMMSG * newEchoMsg(void) {
  DOMMSG * m = (DOMMSG *) malloc(sizeof(DOMMSG));
  if(!m) return NULL;
  setMsgType(m, MESSAGE_HANDLER);
  setMsgSubtype(m, MSGHAND_ECHO_MSG);
  setMsgStatus(m, 0);
  setMsgID(m, 0);
  setMsgDataLen(m, 0); /* In case user forgets to fill */
  return m;
}

DOMMSG * newMoniMsg(void) {
  DOMMSG * m = (DOMMSG *) malloc(sizeof(DOMMSG));
  if(!m) return NULL;
  setMsgType(m, DATA_ACCESS);
  setMsgSubtype(m, DATA_ACC_GET_NEXT_MONI_REC);
  setMsgStatus(m, 0);
  setMsgID(m, 0);
  setMsgDataLen(m, 0); /* In case user forgets to fill */
  return m;
}

/* make a big-endian long from a little-endian one */
void formatLong(unsigned long value, unsigned char *buf) {
  *buf++=(UBYTE)((value>>24)&0xff);
  *buf++=(UBYTE)((value>>16)&0xff);
  *buf++=(UBYTE)((value>>8)&0xff);
  *buf++=(UBYTE)(value&0xff);
}

/* make a big-endian short from a little-endian one */
void formatShort(unsigned short value, unsigned char *buf) {
  *buf++=(UBYTE)((value>>8)&0xff);
  *buf++=(UBYTE)(value&0xff);
}

unsigned long unformatLong(unsigned char *buf) {
  unsigned long temp;
  temp=(unsigned long)(*buf++);
  temp=temp<<8;
  temp|=(unsigned long)(*buf++);
  temp=temp<<8;
  temp|=(unsigned long)(*buf++);
  temp=temp<<8;
  temp|=(unsigned long)(*buf++);
  return temp;
}

/* make a little-endian short from a big-endian one */
unsigned short unformatShort(unsigned char *buf) {
  unsigned short temp;
  temp=(unsigned short)(*buf++);
  temp=temp<<8;
  temp|=(unsigned short)(*buf++);
  return temp;
}


DOMMSG * newSetMoniIvalsMsg(unsigned long hwival, unsigned long cfival) {
  DOMMSG * m = (DOMMSG *) malloc(sizeof(DOMMSG));
  if(!m) return NULL;
  setMsgType(m, DATA_ACCESS);
  setMsgSubtype(m, DATA_ACC_SET_MONI_IVAL);
  setMsgStatus(m, 0);
  setMsgID(m, 0);
  setMsgDataLen(m, 8);
  formatLong(hwival, m->data);
  formatLong(cfival, m->data+4);
  return m;
}

DOMMSG * newSetDacMsg(int dacnum, int dacval) {
  DOMMSG * m = (DOMMSG *) malloc(sizeof(DOMMSG));
  if(!m) return NULL;
  setMsgType(m, DOM_SLOW_CONTROL);
  setMsgSubtype(m, DSC_WRITE_ONE_DAC);
  setMsgStatus(m, 0);
  setMsgID(m, 0);
  setMsgDataLen(m, 4);
  m->data[0] = dacnum & 0xFF;
  m->data[1] = 0;
  formatShort((unsigned short) (dacval & 0xFFFF), m->data+2);
  return m;
}


DOMMSG * newGetDomappReleaseMsg(void) {
  DOMMSG * m = (DOMMSG *) malloc(sizeof(DOMMSG));
  if(!m) return NULL;
  setMsgType(m, MESSAGE_HANDLER);
  setMsgSubtype(m, MSGHAND_GET_DOMAPP_RELEASE);
  setMsgStatus(m, 0);
  setMsgID(m, 0);
  setMsgDataLen(m, 0);
  return m;
}

DOMMSG * newMsg(void) {
  return (DOMMSG *) malloc(sizeof(DOMMSG));
}

DOMMSG * newGetSNDataMsg(void) {
  DOMMSG * m = (DOMMSG *) malloc(sizeof(DOMMSG));
  if(!m) return NULL;
  setMsgType(m, DATA_ACCESS);
  setMsgSubtype(m, DATA_ACC_GET_SN_DATA);
  setMsgStatus(m, 0);
  setMsgID(m, 0);
  setMsgDataLen(m, 0);
  return m;
}

DOMMSG * newGetHitDataMsg(void) {
  DOMMSG * m = (DOMMSG *) malloc(sizeof(DOMMSG));
  if(!m) return NULL;
  setMsgType(m, DATA_ACCESS);
  setMsgSubtype(m, DATA_ACC_GET_DATA);
  setMsgStatus(m, 0);
  setMsgID(m, 0);
  setMsgDataLen(m, 0);
  return m;
}

DOMMSG * newSetTrigModeMsg(int trigMode) {
  DOMMSG * m = (DOMMSG *) malloc(sizeof(DOMMSG));
  if(!m) return NULL;
  setMsgType(m, DOM_SLOW_CONTROL);
  setMsgSubtype(m, DSC_SET_TRIG_MODE);
  setMsgStatus(m, 0);
  setMsgID(m, 0);
  setMsgDataLen(m, 1);
  m->data[0] = trigMode & 0xFF;
  return m;
}

void getMasks(unsigned char * mask0, unsigned char * mask1, int * nsamps, int * sampwids) {
  unsigned char atwdmasks[4];
  int i;
  for(i=0; i<4; i++) {
    if(sampwids[i] == 1) {
      switch(nsamps[i]) {
      case 0:  atwdmasks[i] = 0; break;
      case 16: atwdmasks[i] = 9; break;
      case 32: atwdmasks[i] = 1; break;
      case 64: atwdmasks[i] = 5; break;
      case 128:
      default: atwdmasks[i] = 13; break;
      }
    } else {
      switch(nsamps[i]) {
      case 0:  atwdmasks[i] = 0; break;
      case 16: atwdmasks[i] = 11; break;
      case 32: atwdmasks[i] = 3; break;
      case 64: atwdmasks[i] = 7; break;
      case 128:
      default: atwdmasks[i] = 15; break;
      }
    }
  }
  *mask0 = atwdmasks[0] | (atwdmasks[1] << 4);
  *mask1 = atwdmasks[2] | (atwdmasks[3] << 4);
}

DOMMSG * newSetEngFmtMsg(int * nsamps, int * sampwids, int nadc) {
  DOMMSG * m = (DOMMSG *) malloc(sizeof(DOMMSG));
  if(!m) return NULL;
  setMsgType(m, DATA_ACCESS);
  setMsgSubtype(m, DATA_ACC_SET_ENG_FMT);
  setMsgStatus(m, 0);
  setMsgID(m, 0);
  setMsgDataLen(m, 3);
  unsigned char mask0, mask1;
  getMasks(&mask0, &mask1, nsamps, sampwids);
  m->data[0] = nadc & 0xFF;
  m->data[1] = mask0;
  m->data[2] = mask1;
  return m;
}

void setDacMsgDacValue(DOMMSG * m, int dacval) {
  if(!m) return;
  if(msgType(m) != DOM_SLOW_CONTROL) return;
  if(msgSubType(m) != DSC_WRITE_ONE_DAC) return;
  formatShort((unsigned short) (dacval & 0xFFFF), m->data+2);
}

DOMMSG * newGetNumPedestalsMsg(void) {
  DOMMSG * m = (DOMMSG *) malloc(sizeof(DOMMSG));
  if(!m) return NULL;
  setMsgType(m, EXPERIMENT_CONTROL);
  setMsgSubtype(m, EXPCONTROL_GET_NUM_PEDESTALS);
  setMsgStatus(m, 0);
  setMsgID(m, 0);
  setMsgDataLen(m, 0);
  return m;
}

DOMMSG * newGetPedestalAveragesMsg(void) {
  DOMMSG * m = (DOMMSG *) malloc(sizeof(DOMMSG));
  if(!m) return NULL;
  setMsgType(m, EXPERIMENT_CONTROL);
  setMsgSubtype(m, EXPCONTROL_GET_PEDESTAL_AVERAGES);
  setMsgStatus(m, 0);
  setMsgID(m, 0);
  setMsgDataLen(m, 0);
  return m;
}

DOMMSG * newZeroPedestalsMsg(void) {
  DOMMSG * m = (DOMMSG *) malloc(sizeof(DOMMSG));
  if(!m) return NULL;
  setMsgType(m, EXPERIMENT_CONTROL);
  setMsgSubtype(m, EXPCONTROL_ZERO_PEDESTALS);
  setMsgStatus(m, 0);
  setMsgID(m, 0);
  setMsgDataLen(m, 0);
  return m;
}

DOMMSG * newBeginRunMsg(void) {
  DOMMSG * m = (DOMMSG *) malloc(sizeof(DOMMSG));
  if(!m) return NULL;
  setMsgType(m, EXPERIMENT_CONTROL);
  setMsgSubtype(m, EXPCONTROL_BEGIN_RUN);
  setMsgStatus(m, 0);
  setMsgID(m, 0);
  setMsgDataLen(m, 0);
  return m;
}


DOMMSG * newEndRunMsg(void) {
  DOMMSG * m = (DOMMSG *) malloc(sizeof(DOMMSG));
  if(!m) return NULL;
  setMsgType(m, EXPERIMENT_CONTROL);
  setMsgSubtype(m, EXPCONTROL_END_RUN);
  setMsgStatus(m, 0);
  setMsgID(m, 0);
  setMsgDataLen(m, 0);
  return m;
}


DOMMSG * newTestSWCompMsg(void) {
  DOMMSG * m = (DOMMSG *) malloc(sizeof(DOMMSG));
  if(!m) return NULL;
  setMsgType(m, DATA_ACCESS);
  setMsgSubtype(m, DATA_ACC_TEST_SW_COMP);
  setMsgStatus(m, 0);
  setMsgID(m, 0);
  setMsgDataLen(m, 0); /* In case user forgets to fill */
  return m;
}

DOMMSG * newGetBaselineThresholdMsg(void) {
  DOMMSG * m = (DOMMSG *) malloc(sizeof(DOMMSG));
  if(!m) return NULL;
  setMsgType(m, DATA_ACCESS);
  setMsgSubtype(m, DATA_ACC_GET_BASELINE_THRESHOLD);
  setMsgStatus(m, 0);
  setMsgID(m, 0);
  setMsgDataLen(m, 0); /* In case user forgets to fill */
  return m;
}

DOMMSG * newSetBaselineThresholdMsg(unsigned short atwd0thresh[],
				    unsigned short atwd1thresh[],
				    unsigned short fadc_thresh) {
  DOMMSG * m = (DOMMSG *) malloc(sizeof(DOMMSG));
  if(!m) return NULL;
  setMsgType(m, DATA_ACCESS);
  setMsgSubtype(m, DATA_ACC_SET_BASELINE_THRESHOLD);
  setMsgStatus(m, 0);
  setMsgID(m, 0);
  setMsgDataLen(m, 18);
  int ich, of=0;
  formatShort(fadc_thresh, m->data+of);
  of += 2;
  /* Same threshold, both ATWD chips */
  for(ich=0; ich<4; ich++) {
    formatShort(atwd0thresh[ich], m->data+of);
    of+=2;
  }
  for(ich=0; ich<4; ich++) {
    formatShort(atwd1thresh[ich], m->data+of);
    of+=2;
  }
  return m;
}

DOMMSG * newGetDataCompressionMsg(void) {
  DOMMSG * m = (DOMMSG *) malloc(sizeof(DOMMSG));
  if(!m) return NULL;
  setMsgType(m, DATA_ACCESS);
  setMsgSubtype(m, DATA_ACC_GET_SW_DATA_COMPRESSION);
  setMsgStatus(m, 0);
  setMsgID(m, 0);
  setMsgDataLen(m, 1); 
  return m;
}

DOMMSG * newSetDataCompressionMsg(int toggle) {
  DOMMSG * m = (DOMMSG *) malloc(sizeof(DOMMSG));
  if(!m) return NULL;
  setMsgType(m, DATA_ACCESS);
  setMsgSubtype(m, DATA_ACC_SET_SW_DATA_COMPRESSION);
  setMsgStatus(m, 0);
  setMsgID(m, 0);
  setMsgDataLen(m, 1);
  m->data[0] = toggle & 0xFF;
  return m;
}

int sendAndReceive(int filep, int bufsiz, DOMMSG * s, DOMMSG * r, int timeout, int *msgStatus) {
  int isend;
  isend = sendMsg(filep, s, timeout);
  if(isend < msgDataLen(s)+MSG_HDR_LEN) {
    fprintf(stderr,"sendAndReceive: Timeout or error (%d) sending message!!!\n", isend); 
    return 1;
  }
  /* Get reply */
  int len = getMsg(filep, r, bufsiz, timeout);
  if(len == 0) { // EAGAIN
    fprintf(stderr,"sendAndReceive: timeout from getMsg.\n");
    return 1;
  } else if(len < 0) {
    fprintf(stderr,"sendAndReceive: error %d from getMsg.\n", len);
    return 1;
  } else if(len < MSG_HDR_LEN) {
    fprintf(stderr,"sendAndReceive: Short read (%d bytes) on message reply.\n", len);
    return 1;
  }
  *msgStatus = r->head.hd.status;
  return 0;
}

void dumpMsg(FILE *fp, char *name, DOMMSG * m) {
  fprintf(fp, "MSG %s, mt=%d mst=%d dlenHI=%d dlenLO=%d msgID=%d status=%d\n",
	  name,
	  m->head.hd.mt,
	  m->head.hd.mst,
	  m->head.hd.dlenHI,
	  m->head.hd.dlenLO,
	  m->head.hd.msgID,
	  m->head.hd.status);
}

#define DOMAPP_ERR_NOMEM 1
#define DOMAPP_ERR_MSG   2
#define DOMAPP_ERR_ARG   3

int domsg(int filep, int bufsiz, int timeout, UBYTE mt, UBYTE mst, char * fmt,
	  ...) {

  char * sendarg = fmt;
  char * recvarg = fmt;

  DOMMSG * m = (DOMMSG *) malloc(sizeof(DOMMSG));
  if(!m) return DOMAPP_ERR_NOMEM;

  va_list ap;
  va_start(ap, fmt);
  int sending=1;
  int of = 0;
  unsigned char C;
  unsigned long L;
  unsigned short S;
  unsigned char * A;
  unsigned char * Cp;
  unsigned long * Lp;
  unsigned short * Sp;
  while(sending && *sendarg) {
    switch(*sendarg++) {
    case ' ':
    case '\t':
    case '\n':
    case '\r': break; /* Ignore whitespace */
    case '-': sending = 1; break;
    case '+': sending = 0; break;
    case 'C': 
      C = (unsigned char) va_arg(ap, int); /* unsigned char is promoted to int in ... */
      m->data[of++] = C;
      break;
    case 'I': 
    case 'L':
      L = va_arg(ap, unsigned long);
      formatLong(L, m->data+of);
      of += 4;
      break;
    case 'S':
      S = (unsigned short) va_arg(ap, int);
      formatShort(S, m->data+of);
      of += 2;
      break;
    case 'A':
      A = va_arg(ap, char *);
      int ib;
      for(ib=0; A[ib]!='\0'; ib++) {
	m->data[of++] = A[ib];
      }
      break;
    default: 
      free(m); return DOMAPP_ERR_ARG;
    } /* Loop over send arguments */

  }
  va_end(ap);

  zeroMsg(m);
  setMsgType(m, mt);
  setMsgSubtype(m, mst);
  setMsgStatus(m, 0);
  setMsgID(m, 0);
  setMsgDataLen(m, of);
  DOMMSG * reply = (DOMMSG *) malloc(sizeof(DOMMSG));
  if(!reply) {
    free(m); 
    return DOMAPP_ERR_NOMEM;
  }
  zeroMsg(reply);
  int msgStatus;
  int sar;
  if((sar=sendAndReceive(filep, bufsiz, m, reply, timeout, &msgStatus)) != 0) {
    fprintf(stderr,"sendAndReceive failed(%d)!\n", sar);
    free(reply);
    free(m);
    return DOMAPP_ERR_MSG;
  }

  if(msgStatus != 1) {
    fprintf(stderr,"sendAndReceive: message reply had bad status (%d)!\n", msgStatus);
    dumpMsg(stderr,"m", m);
    free(m);
    free(reply);
    return DOMAPP_ERR_MSG;
  }

  va_start(ap, fmt);
  of    = 0;
  int len;
  while(*recvarg) {
    char X = *recvarg++;
    if(sending && X != '+') continue; /* Skip over send arguments */
    switch(X) {
    case ' ': 
    case '\t':
    case '\n':
    case '\r': break; /* Ignore whitespace */
    case '-': sending = 1; break;
    case '+': sending = 0; break;
    case 'C': 
      Cp = va_arg(ap, unsigned char *);
      *Cp = reply->data[of++];
      break;
    case 'I': 
    case 'L':
      Lp = va_arg(ap, unsigned long *);
      *Lp = unformatLong(reply->data+of);
      of += 4;
      break;
    case 'S':
      Sp = va_arg(ap, unsigned short *);
      *Sp = unformatShort(reply->data+of);
      of += 2;
      break;
    case 'A': /* For character string, assume string all the rest of the message */
      if(of != 0) {
        fprintf(stderr,"Huh? (offset %d != 0)\n", of);
	free(reply);
	return DOMAPP_ERR_ARG;
      }
      A = va_arg(ap, unsigned char *);
      len = msgDataLen(reply);
      strncpy(A, reply->data+of, len);
      of += len;
      reply->data[of+1] = '\0';
      of++;
      break;
    case 'X': /* Like 'A', but not null-terminated */
      if(of != 0) {
	fprintf(stderr,"Huh? (offset %d != 0)\n", of);
        free(reply);
        return DOMAPP_ERR_ARG;
      }
      A = va_arg(ap, unsigned char *);
      len = msgDataLen(reply);
      memcpy(A, reply->data+of, len);
      of += len;
      break;
    default: 
      fprintf(stderr,"Huh? (arg specifier %c)\n",X);
      free(reply);
      return DOMAPP_ERR_ARG;
      break;
    }
  }
  va_end(ap);
  if(of != msgDataLen(reply)) {
    fprintf(stderr,"domsg: wanted %d bytes of arguments, got %d in data portion from DOM.\n", 
	   of, msgDataLen(reply));
    dumpMsg(stderr, "query", m);
    dumpMsg(stderr, "reply", reply);
    free(m); 
    free(reply);
    return DOMAPP_ERR_MSG;
  }
  free(m);
  free(reply);
  return 0;
}

#endif

