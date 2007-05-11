/*
   decomp.c - jacobsen@npxdesigns.com

   Simple decompressor for Joshua Sopher's roadgrader format
   Doesn't inspect the hit data really, just checks that 
   domapp packaged it up correctly; could be built up into 
   a full-bore reader for the decompressed format, though...

   May, 2005

   Nov. 23, 2005: update for new data format and change from
   roadgrader to delta compression

*/

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

int usage(void) {
  fprintf(stderr,
"Usage: decomp <filename>\n"
	  );
  return -1;
}

unsigned short swapShort(unsigned short x) { 
  return (((x>>8)&0xFF)|((x&0xFF)<<8)); 
}

int main(int argc, char *argv[]) {
#define BUFS 4096
  unsigned char * hbuf[BUFS];

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
    struct block_hdr { unsigned short len, chead, ts16, spare; } h;
    int nr = read(fd, &h, sizeof(h));
    if(nr == 0) break; // EOF
    if(nr != sizeof(h)) {
      fprintf(stderr,"ERROR: Couldn't read %d bytes for block header!\n", sizeof(h));
      exit(-1);
    }
    if((h.chead & 0xFF00) != 0x9000) {
      fprintf(stderr,"ERROR: Malformed compression header (0x%04x) for delta compression!\n",
	      h.chead);
      exit(-1);
    }
    unsigned short tshi = swapShort(h.ts16);
    unsigned short len  = swapShort(h.len);
    printf("HDR len=%hu tshi=0x%02x\n", len, tshi);
    if(len > BUFS) {
      fprintf(stderr, "ERROR: Corrupt len, %d bytes > MAX(%dB).\n", len, BUFS);
      exit(-1);
    }
    int thisBlock = len-sizeof(h);
    while(thisBlock > 0) {    
      struct hit_hdr { unsigned long word1, word2; } hdr;
      nr = read(fd, &hdr, sizeof(hdr));
      if(nr != sizeof(hdr)) {
	fprintf(stderr,"ERROR: Couldn't read %d bytes for hit header!\n", sizeof(hdr));
	exit(-1);
      }
      thisBlock -= nr;
      unsigned short hitlen = hdr.word1 & 0x7FF;
      int isCompressed      = hdr.word1 >> 31;
      unsigned tslo         = hdr.word2;
      int remain = hitlen-sizeof(hdr);
      printf("\tHIT(%hu,0x%08x)\n", 
	     hitlen, tslo);
      if(!isCompressed) fprintf(stderr, "WARNING: Compressed bit NOT SET!\n");
      if(hitlen > BUFS) {
	fprintf(stderr, "Corrupt hit length, %d bytes > MAX(%dB).\n", hitlen, BUFS);
	exit(-1);
      }
      nr = read(fd, hbuf, remain);
      if(nr != remain) {
	fprintf(stderr, "ERROR: Partial read of hit record (read %d, wanted %d)!\n", 
		nr, remain);
	exit(-1);
      }
      thisBlock -= nr;
    }
    if(thisBlock != 0) 
      fprintf(stderr, "WARNING: Block boundary mismatch, thisBlock=%d\n", thisBlock);
  }
  close(fd);
  return 1;
}

