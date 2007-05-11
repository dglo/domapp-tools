/* 
   decodedelta - delta-compressed PMT hit decoder 

   jacobsen@npxdesigns.com
   Oct., 2006

*/

#include <getopt.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

#define DEFAULTFILE "hits.dz"

#define MAXHITBLOCK 4024 // Can't be any bigger than this because of domapp / comms
struct hitblock {
  unsigned short len;
  unsigned short tmsb;
  unsigned char hits[MAXHITBLOCK];
};

unsigned short unpackShort(unsigned char *buf) {
  return buf[0]<<8|buf[1];
}

struct hitblock * newHitblock(void) {
  return (struct hitblock *) malloc(sizeof(struct hitblock));
}

void freeHitblock(struct hitblock * hb) {
  free(hb);
}

int readHitblock(int fd, struct hitblock * hb) {
  /* Get block length */
  unsigned long word0;
  int tot = 0;
  int nr = read(fd, &word0, sizeof(word0));
  if(nr != sizeof(word0)) {
    return 0; /* Normal EOF, don't complain */
  }
  tot += nr;
  /* Check top byte for compression bit, compression type */
  if(((word0>>24) & 0xFF) != 0x90) {
    printf( "Word 0 (0x%08x) top byte not correct, corrupt data?\n", word0);
    return 0;
  }

  unsigned short beLen = word0 & 0xFFFF;
  hb->len = unpackShort((unsigned char *) &beLen);

  /* Get MSBs of timestamp */
  unsigned long word1;
  nr = read(fd, &word1, sizeof(word1));
  if(nr != sizeof(word1)) {
    printf( "short read of block data, word 1\n");
    return 0;
  }
  tot += nr;

  unsigned short beMsb = word1 & 0xFFFF;
  hb->tmsb = unpackShort((unsigned char *) &beMsb);

  /* Unpack actual buffer of compressed hits */
  int buflen = hb->len - 8;
  if(buflen < 0 || buflen > MAXHITBLOCK) {
    printf( "Bad buffer length (%d), corrupt data?\n", buflen);
    return 0;
  }
  nr = read(fd, hb->hits, buflen);
  if(nr < buflen) {
    printf( "Short read (got %d, wanted %d) getting hit block buffer!\n", nr, buflen);
    return 0;
  }
  tot += nr;
  printf("HDR len=%hu msb=%hu\n", hb->len, hb->tmsb);
  return tot;
}

void dumpbuf(int l, unsigned char * hitbuf) {
  int i;
  for(i=0; i<l; i++) {
    printf("%02x",hitbuf[i]);
    if(!((i+1)%4)) putchar(' ');
    if(!((i+1)%32)) putchar('\n');
  }
  putchar('\n');
}

/* A little shorthand */
inline unsigned long word1(unsigned char * hitbuf) { return *((unsigned long *) hitbuf); }
inline unsigned long word2(unsigned char * hitbuf) { return *((unsigned long *) (hitbuf+4)); }
inline unsigned long word3(unsigned char * hitbuf) { return *((unsigned long *) (hitbuf+8)); }

unsigned compflag(unsigned char * hitbuf)  { return (word1(hitbuf)>>31) & 0x01; }
unsigned trigword(unsigned char * hitbuf)  { return (word1(hitbuf)>>18) & 0x1FFF; }
unsigned LC(unsigned char * hitbuf)        { return (word1(hitbuf)>>16) & 0x3; }
unsigned fadcAvail(unsigned char * hitbuf) { return (word1(hitbuf)>>15) & 0x1; }
unsigned atwdAvail(unsigned char * hitbuf) { return (word1(hitbuf)>>14) & 0x1; }

/* Returns # of ATWD channels in hit */
unsigned atwdnch(unsigned char * hitbuf)   { return ((word1(hitbuf)>>12) & 0x3) + 1; }
unsigned isAtwdB(unsigned char * hitbuf)   { return (word1(hitbuf)>>11) & 0x1; }
unsigned hitsize(unsigned char * hitbuf)   { return word1(hitbuf) & 0x3FF; }
unsigned tlsb(unsigned char * hitbuf)      { return word2(hitbuf); }
unsigned peakrange(unsigned char * hitbuf) { return (word3(hitbuf)>>31) & 0x1; }
unsigned peaksamp(unsigned char * hitbuf)  { return (word3(hitbuf)>>27) & 0x7; }
unsigned prepeak(unsigned char * hitbuf)   { return (word3(hitbuf)>>18) & 0x1FF; }
unsigned peakcnt(unsigned char * hitbuf)   { return (word3(hitbuf)>>9) & 0x1FF; }
unsigned postpeak(unsigned char * hitbuf)  { return word3(hitbuf) & 0x1FF; }

inline unsigned short mask(int n) { return 0xFFFF & (((unsigned) 1<<n)-1); }

unsigned short getNbits(int bpw, const unsigned char * buf, int buflen, int ib, int nb) {
  /* Get nb (up to 16) bits out of buffer buf, starting at position ib. 
     ib must be less than buflen*8 */

  if(nb==0) return 0;
  unsigned cur = 0;
  int nbytes = (nb-1)/8 + 1; /* Number of bytes we need to collect from: 0, 1 or 2 */
  int idx;
  for(idx = 0; idx < nbytes; idx++) {
    unsigned char c = buf[ib/8+idx];
    int startbit = ib % 8;
    // Shift off unused bits up to startbit
    // Turn on up to nb bits in ncur
    // See if we need to continue
  }
  return (unsigned short) cur;
}

int decodeWaveforms(unsigned char * buf, int buflen) {
  int bpw  = 3;
  int ibit = 0;
  unsigned short chunk = getNbits(bpw, buf, buflen, ibit, bpw);
  ibit += bpw;
  return 1;
}

void decodeHitblock(int len, unsigned char * hitbuf) {
  int nb;
  for(nb = 0; nb < len; ) {
    unsigned cf   = compflag(hitbuf);
    unsigned trig = trigword(hitbuf);
    unsigned lc   = LC(hitbuf);
    unsigned fadc = fadcAvail(hitbuf);
    unsigned atwd = atwdAvail(hitbuf);
    unsigned nch  = atwdnch(hitbuf);
    unsigned isb  = isAtwdB(hitbuf);
    unsigned size = hitsize(hitbuf);
    unsigned tlo  = tlsb(hitbuf);
    unsigned pr   = peakrange(hitbuf);
    unsigned samp = peaksamp(hitbuf);
    unsigned pre  = prepeak(hitbuf);
    unsigned peak = peakcnt(hitbuf);
    unsigned post = postpeak(hitbuf);
    printf("HIT trig=0x%x lc=%u fadc_avail=%u atwd_avail=%u nch=%u atwd=%c "
	   "nb=%u pr=%s samp=%d (%d,%d,%d) t=%u\n", 
	   trig, lc, fadc, atwd, nch, isb?'B':'A', size, pr?"hi":"lo", samp, pre, peak, post, tlo);
    
    if(!cf) {
      fprintf(stderr,"Hit record does not begin with high bit, uncompressed or corrupt data!\n");
      dumpbuf(len, hitbuf);
      return;
    }
    if(!fadc && atwd) {
      fprintf(stderr,"ERROR: ATWD 'available', but not FADC -- corrupt data!\n");
      dumpbuf(len, hitbuf);
      return;
    }
    if(fadc && !decodeWaveforms(hitbuf+12, size-12)) {
      fprintf(stderr,"Waveform decoder failed!\n");
      dumpbuf(len, hitbuf);
      return;
    }
    nb += size;
    hitbuf += size;
  }
}

int usage(void) {
  printf(
          "Usage:\n"
          "  decodedelta <file>\n"
	  "              [-h]   show usage\n");
  return -1;
}

int main(int argc, char *argv[]) {
  static struct option long_options[] = {
    {"help",    0,0,0},
    {0,         0,0,0}
  };
  int option_index = 0;
  while(1) {
    char c = getopt_long (argc, argv, "hcdvs:", long_options, &option_index);
    if (c == -1) break;
    switch(c) {
    case 'h':
    default: exit(usage());  
    }
  }

  int argcount = argc-optind;
  char * fname = DEFAULTFILE;
  if(argcount >= 1) {
    fname = argv[optind];
  }

  int fd = open(fname, O_RDONLY, 0);
  if(fd < 0) {
    printf("Couldn't open file %s for input (%s).\n",
            fname,strerror(errno));
    return -1;
  }

  struct hitblock * hb = newHitblock();
  while(1) {
    int nr;
    if((nr = readHitblock(fd, hb)) < 1) {
      return 0;
    }
    decodeHitblock(hb->len-8, hb->hits);
  }
  freeHitblock(hb);
  close(fd);
  return 0;
}
