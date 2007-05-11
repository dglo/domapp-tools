/*
   decodesn.c - jacobsen@npxdesigns.com
   Simple decompressor for supernova data
   May, 2005
*/

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <getopt.h>

typedef struct phdr_struct {
  unsigned recl;
  unsigned payload_type;
  unsigned long long utc;
  unsigned long long domid;
} payloadHdr_t;

int usage(void) {
  fprintf(stderr,
"Usage: decodesn <filename>\n"
"       [-p] Use payload format\n"
          );
  return -1;
}

unsigned short swapShort(unsigned short x) {
  return (((x>>8)&0xFF)|((x&0xFF)<<8));
}

unsigned long long getTimeStamp(unsigned char * buf) {
  unsigned long long t = 0;
  int i; for(i=0;i<6;i++) {
    t <<= 8;
    t |= buf[i];
  }
  return t;
}

#define BINTIME 65536

int haveTimeGap(unsigned long long t, unsigned long long tprev, unsigned nbins) {
  if(tprev == 0) return 0;
  return t > (tprev + nbins*BINTIME);
}

int timesOutOfOrder(unsigned long long t, unsigned long long tprev, unsigned nbins) {
  if(tprev == 0) return 0;
  //if(t != tprev+nbins*BINTIME) {
  //  printf("%lld - %lld = %lld<-- %lld %d\n", t, tprev+nbins*BINTIME, t-(tprev+nbins*BINTIME), tprev, nbins);
  //}
  return t < (tprev + nbins*BINTIME);
}

int main(int argc, char *argv[]) {
  int option_index = 0;
  static struct option long_options[] = {
    {"help",    0,0,0},
    {"payload",    0,0,0},
    {0,         0,0,0}
  };
  int dopayload=0;
  while(1) {
    char c = getopt_long (argc, argv, "hcdvs:", long_options, &option_index);
    if (c == -1) break;
    switch(c) {
    case 'p': dopayload=1;
    case 'h':
    default: exit(usage());
    }
  }

  
#define BUFS 4096
  unsigned long long tprev = 0;
  if(argc != 2) return usage();
  char *filen = argv[1];
  printf("%s:\n", filen);
  int fd = open(filen, O_RDONLY, 0);
  if(fd < 0) {
    fprintf(stderr,"Couldn't open file %s for input (%s).\n",
            filen,strerror(errno));
    return -1;
  }

  while(1) {
    unsigned short hlen, hfmt;
    unsigned char tsbuf[6];
    payloadHdr_t phdr;
    int nr;
    if(dopayload) {
      nr = read(fd, &phdr, sizeof(payloadHdr_t));
      if(nr == 0) break; // EOF
      if(nr != sizeof(payloadHdr_t)) {
	fprintf(stderr,"Couldn't read %d bytes for payload header!\n", sizeof(hlen));
	exit(-1);
      }
    }

    int p = 0;
    nr = read(fd, &hlen, sizeof(hlen));
    if(nr == 0) break; // EOF
    if(nr != sizeof(hlen)) {
      fprintf(stderr,"Couldn't read %d bytes for block header!\n", sizeof(hlen));
      exit(-1);
    }
    unsigned short len = swapShort(hlen);
    p += nr;
    /* Get format ID */
    nr = read(fd, &hfmt, sizeof(hfmt));
    if(nr != sizeof(hfmt)) {
      fprintf(stderr,"Couldn't read %d bytes for event type!\n", sizeof(hfmt));
      exit(-1);
    }
    unsigned short fmtid = swapShort(hfmt);
#   define FMT_ID 300
    if(fmtid != FMT_ID) {
      fprintf(stderr,"Bad format ID, got %hu, expected %hu.\n", fmtid, FMT_ID);
      exit(-1);
    }
    p += nr;

    nr = read(fd, tsbuf, 6);
    if(nr != 6) {
      fprintf(stderr,"Couldn't read 6 bytes for timestamp!\n");
      exit(-1);
    }
    p += nr;
    unsigned long long t = getTimeStamp(tsbuf);
    int nbins = len-p;
    int nbinsprev;
    if(nbins > BUFS) {
      fprintf(stderr, "Corrupt nbins, len=%d bytes, nbins=%d > MAX(%dB).\n", len, nbins, BUFS);
      exit(-1);
    }
    unsigned long long dt = t-tprev;
    unsigned long long tratio = (nbinsprev == 0) ? 0 : dt/nbinsprev;
    printf("HDR(len=%huB t=%lld,0x%06llx dt=%lld tratio=%lld nbins=%d)\n", 
	   len, t, t, dt, tratio, nbins);
    if(haveTimeGap(t, tprev, nbinsprev)) {
      fprintf(stderr, "WARNING: gap in time stamps!\n");
    }
    if(timesOutOfOrder(t, tprev, nbinsprev)) {
      fprintf(stderr, "WARNING: time stamps out of order!\n");
    }
    nbinsprev = nbins;
    int ibin; for(ibin=0; ibin<nbins; ibin++) {
      unsigned char ccounts = 0;
      nr = read(fd, &ccounts, 1);
      if(nr != 1) {
	fprintf(stderr, "Short read of count data at bin %d!\n", ibin);
	exit(-1);
      }
      unsigned count = (int) ccounts;
      printf("\t%lld -> %u counts\n", t+ibin*BINTIME, count);
    }
    tprev = t;
  }
  close(fd);
  return 1;
}
