/* decodemoni.c : C program to decode/test monitoring data from DOM
   John Jacobsen, jacobsen@npxdesigns.com, for LBNL/IceCube
   Started May, 2004
   $Id: decodemoni.c,v 1.5 2005/12/19 19:41:31 jacobsen Exp $

   Decode monitor data to make sure it makes sense
*/

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <getopt.h>
#include <errno.h>
#include <string.h>
#include "domapp.h" /* Message subtypes */

#ifndef FPGA_HAL_TICKS_PER_SEC
#define FPGA_HAL_TICKS_PER_SEC 40000000
#endif

int usage(void) {
  fprintf(stderr,
          "Usage:\n"
          "  decodemoni <file>\n"
	  "             [-h]   show usage\n"
	  "             [-c]   use datacollector format\n"
	  "             [-d]   use domhub-prod format\n"
	  "             [-s N] skip first N bytes\n"
	  "             [-v]   verbose (show moni rec contents, else header only)\n\n");
  return -1;
}

#define DEFAULTFILE "moni.out"

#define HLEN 10

struct hdr {
  unsigned short len;
  unsigned short typ;
  unsigned long long time;
};

struct hwr {
  unsigned char recver;
  unsigned char spare;
  unsigned short ADC_VOLTAGE_SUM;
  unsigned short ADC_5V_POWER_SUPPLY;
  unsigned short ADC_PRESSURE;
  unsigned short ADC_5V_CURRENT;
  unsigned short ADC_3_3V_CURRENT;
  unsigned short ADC_2_5V_CURRENT;
  unsigned short ADC_1_8V_CURRENT;
  unsigned short ADC_MINUS_5V_CURRENT;
  unsigned short DAC_ATWD0_TRIGGER_BIAS;
  unsigned short DAC_ATWD0_RAMP_TOP;
  unsigned short DAC_ATWD0_RAMP_RATE;
  unsigned short DAC_ATWD_ANALOG_REF;
  unsigned short DAC_ATWD1_TRIGGER_BIAS;
  unsigned short DAC_ATWD1_RAMP_TOP;
  unsigned short DAC_ATWD1_RAMP_RATE;
  unsigned short DAC_PMT_FE_PEDESTAL;
  unsigned short DAC_MULTIPLE_SPE_THRESH;
  unsigned short DAC_SINGLE_SPE_THRESH;
  unsigned short DAC_LED_BRIGHTNESS;
  unsigned short DAC_FAST_ADC_REF;
  unsigned short DAC_INTERNAL_PULSER;
  unsigned short DAC_FE_AMP_LOWER_CLAMP;
  unsigned short DAC_FL_REF;
  unsigned short DAC_MUX_BIAS;
  unsigned short PMT_base_HV_set_value;
  unsigned short PMT_base_HV_monitor_value;
  unsigned short DOM_MB_Temperature;
  unsigned long  SPE_Scaler;
  unsigned long  MPE_Scaler;
};

struct cfg {
  unsigned char evtvers;
  unsigned char spare;
  unsigned short hwlen;
  unsigned char mbid[6];
  unsigned short align;
  unsigned char baseid[8];
  unsigned short fpga_build;
  unsigned short swlen;
  unsigned short mb_sw_build;
  unsigned char msg_hand_major;
  unsigned char msg_hand_minor;
  unsigned char exp_ctrl_major;
  unsigned char exp_ctrl_minor;
  unsigned char slo_ctrl_major;
  unsigned char slo_ctrl_minor;
  unsigned char data_acc_major;
  unsigned char data_acc_minor;
  unsigned short daqlen;
  unsigned long trig_conf;
  unsigned long atwd_conf;
};


unsigned short unpackShort(unsigned char *buf) {
  return buf[0]<<8|buf[1];
}

unsigned long unpackLong(unsigned char *buf) {
  return (buf[0]<<24)
    |    (buf[1]<<16)
    |    (buf[2]<<8)
    |     buf[3];
}

unsigned long long unpackLongLong(unsigned char *buf) {
  long long l = (((unsigned long long) buf[0])<<56)
    |           (((unsigned long long) buf[1])<<48)
    |           (((unsigned long long) buf[2])<<40)
    |           (((unsigned long long) buf[3])<<32)
    |           (((unsigned long long) buf[4])<<24)
    |           (((unsigned long long) buf[5])<<16)
    |           (((unsigned long long) buf[6])<<8)
    |            ((unsigned long long) buf[7]);
  return l;
}

void buf2cfg(unsigned char *buf, struct cfg *h) {
  h->evtvers = *buf++;
  h->spare   = *buf++;
  h->hwlen   = unpackShort(buf); buf+=2;
  int i;
  for(i=0;i<6;i++) {
    h->mbid[i] = *buf++; /* Not sure about endianness */
  }
  h->align   = unpackShort(buf); buf+=2;
  for(i=0;i<8;i++) {
    h->baseid[i] = *buf++; /* Not sure about endianness */
  }
  h->fpga_build  = unpackShort(buf); buf+=2;
  h->swlen       = unpackShort(buf); buf+=2;
  h->mb_sw_build = unpackShort(buf); buf+=2;
  h->msg_hand_major = *buf++;
  h->msg_hand_minor = *buf++;
  h->exp_ctrl_major = *buf++;
  h->exp_ctrl_minor = *buf++;
  h->slo_ctrl_major = *buf++;
  h->slo_ctrl_minor = *buf++;
  h->data_acc_major = *buf++;
  h->data_acc_minor = *buf++;
  h->daqlen         = unpackShort(buf); buf+=2;
  h->trig_conf      = unpackLong(buf); buf+=4;
  h->atwd_conf      = unpackLong(buf); buf+=4;
}


void buf2hwr(unsigned char *buf, struct hwr *h) {
  h->recver = *buf++;
  h->spare  = *buf++;
  h->ADC_VOLTAGE_SUM = unpackShort(buf); buf+=2;
  h->ADC_5V_POWER_SUPPLY = unpackShort(buf); buf+=2;
  h->ADC_PRESSURE = unpackShort(buf); buf+=2;
  h->ADC_5V_CURRENT = unpackShort(buf); buf+=2;
  h->ADC_3_3V_CURRENT = unpackShort(buf); buf+=2;
  h->ADC_2_5V_CURRENT = unpackShort(buf); buf+=2;
  h->ADC_1_8V_CURRENT = unpackShort(buf); buf+=2;
  h->ADC_MINUS_5V_CURRENT = unpackShort(buf); buf+=2;
  h->DAC_ATWD0_TRIGGER_BIAS = unpackShort(buf); buf+=2;
  h->DAC_ATWD0_RAMP_TOP = unpackShort(buf); buf+=2;
  h->DAC_ATWD0_RAMP_RATE = unpackShort(buf); buf+=2;
  h->DAC_ATWD_ANALOG_REF = unpackShort(buf); buf+=2;
  h->DAC_ATWD1_TRIGGER_BIAS = unpackShort(buf); buf+=2;
  h->DAC_ATWD1_RAMP_TOP = unpackShort(buf); buf+=2;
  h->DAC_ATWD1_RAMP_RATE = unpackShort(buf); buf+=2;
  h->DAC_PMT_FE_PEDESTAL = unpackShort(buf); buf+=2;
  h->DAC_MULTIPLE_SPE_THRESH = unpackShort(buf); buf+=2;
  h->DAC_SINGLE_SPE_THRESH = unpackShort(buf); buf+=2;
  h->DAC_LED_BRIGHTNESS = unpackShort(buf); buf+=2;
  h->DAC_FAST_ADC_REF = unpackShort(buf); buf+=2;
  h->DAC_INTERNAL_PULSER = unpackShort(buf); buf+=2;
  h->DAC_FE_AMP_LOWER_CLAMP = unpackShort(buf); buf+=2;
  h->DAC_FL_REF = unpackShort(buf); buf+=2;
  h->DAC_MUX_BIAS = unpackShort(buf); buf+=2;
  h->PMT_base_HV_set_value = unpackShort(buf); buf+=2;
  h->PMT_base_HV_monitor_value = unpackShort(buf); buf+=2;
  h->DOM_MB_Temperature = unpackShort(buf); buf+=2;
  h->SPE_Scaler = unpackLong(buf); buf+=4;
  h->MPE_Scaler = unpackLong(buf); buf+=4;
}

void buf2hdr(unsigned char *buf, struct hdr *h) {
  h->len  = buf[0]<<8|buf[1];
  h->typ  = buf[2]<<8|buf[3];
  int i;
  h->time = 0;
  for(i=0;i<6;i++) {
    //printf("buf2hdr i=%d tb=0x%02x\n", i, buf[4+i]);
    h->time <<= 8;
    h->time |= buf[4+i];
  }
}

struct tdwrap {
  unsigned int wraplen;
  unsigned int formatid;
  unsigned long long domid;
  unsigned long long resv;
  unsigned long long caltime;
};


int main(int argc, char *argv[]) {
  int verbose = 0;
  int option_index = 0;
  static struct option long_options[] = {
    {"help",    0,0,0},
    {"skip",    0,0,0},
    {"datacollector", 0,0,0},
    {"verbose", 0,0,0},
    {0,         0,0,0}
  };

  int skip=0, wrapped=0, dhprod=0;
  while(1) {
    char c = getopt_long (argc, argv, "hcdvs:", long_options, &option_index);
    if (c == -1) break;
    switch(c) {
    case 'v': verbose = 1; break;
    case 'd': dhprod  = 1; break;
    case 'c': wrapped = 1; break;
    case 's': skip = atoi(optarg); break; 
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
    fprintf(stderr,"Couldn't open file %s for input (%s).\n", 
	    fname,strerror(errno));
    return -1;
  }

  int monicount = 0;
  unsigned long long tprev = 0;

  unsigned char hbuf[HLEN];
  int nr;
  if(skip) {
    fprintf(stderr,"Skipping first %d bytes...\n", skip);
    nr = lseek(fd, skip, SEEK_SET);
    if(nr != skip) {
      fprintf(stderr,"Couldn't seek %d bytes... aborting.\n", skip);
      exit(-1);
    }
  }

  if(dhprod) { 
    /* Read first record and skip it -- just a bunch of DOM IDs */
    nr = read(fd, hbuf, 4);
    unsigned nsw = unpackLong(hbuf);
    nr = lseek(fd, nsw, SEEK_SET);
    if(nr != nsw) {
      fprintf(stderr,"Couldn't seek %d bytes... aborting.\n", nsw);
      exit(-1);
    }
  }

  unsigned long long thisdom = 0;
  unsigned long long lastdom = 0;

  int thisFrame = 0;

  while(1) {
#define MAXPAYLOAD 1024
    unsigned char mbuf[MAXPAYLOAD];
    struct hdr h;
    struct tdwrap wrapper;

    if(wrapped) {
      nr = read(fd, &wrapper, sizeof(wrapper));
      if(nr != sizeof(wrapper)) {
	fprintf(stderr, "Short read from monitoring file: %d bytes.\n", nr);
	exit(-1);
      }
      thisdom = wrapper.domid>>16;
      printf("DOM %llx ", thisdom);
    }

    if(dhprod && thisFrame==0) { /* Read DOM Hub wrapper */
      nr = read(fd, hbuf, 4);
      thisFrame = unpackLong(hbuf);
      printf("Bytes in frame = %u\n", thisFrame);
      thisFrame -= 4;
      
      nr = read(fd, hbuf, 4); if(nr!=4) { exit(-fprintf(stderr,"short read (%dB)\n", nr)); }
      int itype = unpackLong(hbuf);
      printf("FrameType=%u\n", itype);
      thisFrame -= 4;

      unsigned long long domid;
      nr = read(fd, hbuf, 8); if(nr!=8) { exit(-fprintf(stderr,"short read (%dB)\n", nr)); }
      domid = unpackLongLong(hbuf);
      printf("DOM %llx\n", domid);
      thisFrame -= 8;
    }

    nr = read(fd, hbuf, HLEN);
    //printf("nr=%d\n",nr);

    if(nr == 0) break;
    if(dhprod) thisFrame -= nr;

    if(nr != HLEN) {
      fprintf(stderr, "Got short read or EOF (%d bytes)\n", nr);
      break;
    }

    buf2hdr(hbuf,&h);
    char * typeStr;
    switch(h.typ) {
    case 0xCB: typeStr = "  (ASCII LOG)"; break;
    case 0xC8: typeStr = "(HRDWR STATE)"; break;
    case 0xC9: typeStr = "(CONFG STATE)"; break;
    case 0xCA: typeStr = "(STATE CHNGE)"; break;
    case 0xCC: typeStr = "    (GENERIC)"; break;
    default:   typeStr = "    (UNKNOWN)"; break;
    }


    printf("HDR len=%hu typ=0x%0hx %s time=%06llx (%2.2f sec) ",
	   h.len, h.typ, typeStr, h.time, (float) h.time / (float) FPGA_HAL_TICKS_PER_SEC);
    if((wrapped && thisdom == lastdom)
      || 
       (!wrapped && monicount > 0)) {
      unsigned long long dt;
      dt = h.time - tprev;
      int dtusec = (int) ((((float) dt / (float) FPGA_HAL_TICKS_PER_SEC)*1E6)+0.5);
      printf("dt=%10lld (%2.2f sec, %d usec)", dt, 
	     (float) dt / (float) FPGA_HAL_TICKS_PER_SEC, dtusec);
    }
    printf("\n");
    
    if(wrapped) lastdom = thisdom;

    int remain = h.len-HLEN;
    if(remain > MAXPAYLOAD) {
      fprintf(stderr, "Can't fit %d bytes into buffer (fix MAXPAYLOAD).\n", remain);
      return -1;
    }

    nr = read(fd, mbuf, remain);
    //printf("Read remaining %d bytes.\n", nr);
    if(nr != remain) {
      fprintf(stderr, "Short read of remaining record bytes (wanted %d, got %d).\n",
	      remain, nr);
      return -1;
    }

    if(dhprod) thisFrame -= nr;

    if(verbose) {
      if(h.typ == 0xCB) { // 203 : ASCII moni record 
	printf("\t\"");
	int i;
	for(i=0; i<remain; i++) printf("%c", mbuf[i]);
	printf("\"\n");
      } else if(h.typ == 0xC8) { // Hardware state event
	struct hwr hwrec;
	buf2hwr(mbuf, &hwrec);
	printf("\tHW EVT %d %d "
	       "%hu %hu %hu %hu %hu "
               "%hu %hu %hu %hu %hu "
               "%hu %hu %hu %hu %hu "
               "%hu %hu %hu %hu %hu "
               "%hu %hu %hu %hu %hu "
               "%hu %hu "
	       "%lu %lu\n",
	       hwrec.recver, hwrec.spare,
	       hwrec.ADC_VOLTAGE_SUM,
	       hwrec.ADC_5V_POWER_SUPPLY,
	       hwrec.ADC_PRESSURE,
	       hwrec.ADC_5V_CURRENT,
	       hwrec.ADC_3_3V_CURRENT,
	       hwrec.ADC_2_5V_CURRENT,
	       hwrec.ADC_1_8V_CURRENT,
	       hwrec.ADC_MINUS_5V_CURRENT,
	       hwrec.DAC_ATWD0_TRIGGER_BIAS,
	       hwrec.DAC_ATWD0_RAMP_TOP,
	       hwrec.DAC_ATWD0_RAMP_RATE,
	       hwrec.DAC_ATWD_ANALOG_REF,
	       hwrec.DAC_ATWD1_TRIGGER_BIAS,
	       hwrec.DAC_ATWD1_RAMP_TOP,
	       hwrec.DAC_ATWD1_RAMP_RATE,
	       hwrec.DAC_PMT_FE_PEDESTAL,
	       hwrec.DAC_MULTIPLE_SPE_THRESH,
	       hwrec.DAC_SINGLE_SPE_THRESH,
	       hwrec.DAC_LED_BRIGHTNESS,
	       hwrec.DAC_FAST_ADC_REF,
	       hwrec.DAC_INTERNAL_PULSER,
	       hwrec.DAC_FE_AMP_LOWER_CLAMP,
	       hwrec.DAC_FL_REF,
	       hwrec.DAC_MUX_BIAS,
	       hwrec.PMT_base_HV_set_value,
	       hwrec.PMT_base_HV_monitor_value,
	       hwrec.DOM_MB_Temperature,
	       hwrec.SPE_Scaler, 
	       hwrec.MPE_Scaler);
      } else if(h.typ == 0xC9) { // Config state event
	struct cfg cfrec;
	buf2cfg(mbuf, &cfrec);
	printf("\tCF EVT %d %d %d 0x",
	       cfrec.evtvers, cfrec.spare, cfrec.hwlen);
	int i;
	for(i=0;i<6;i++) printf("%02x", cfrec.mbid[i]);
	printf(" %d 0x", cfrec.align);
        for(i=0;i<8;i++) printf("%02x", cfrec.baseid[i]);
	printf(" %hd %hd %hd ", cfrec.fpga_build, cfrec.swlen, cfrec.mb_sw_build);
	printf("%d.%d %d.%d %d.%d %d.%d %hd 0x%lx 0x%lx", 
	       cfrec.msg_hand_major,
	       cfrec.msg_hand_minor,
	       cfrec.exp_ctrl_major,
	       cfrec.exp_ctrl_minor,
	       cfrec.slo_ctrl_major,
	       cfrec.slo_ctrl_minor,
	       cfrec.data_acc_major,
	       cfrec.data_acc_minor,
	       cfrec.daqlen,
	       cfrec.trig_conf,
	       cfrec.atwd_conf);
	
	printf("\n");
      } else if(h.typ == 0xCA) { // State change event 
	int type = mbuf[1];
	switch(type) {
	case DSC_WRITE_ONE_DAC: 
	  {
	    unsigned chan = (unsigned) mbuf[2]; /* mbuf[3] is spare */
	    unsigned value = mbuf[5] | (mbuf[4]<<8);
	    printf("\tSTATE CHANGE: WRITE DAC(%u) <- %u\n", chan, value);
	  }
	  break;
	case DSC_SET_LOCAL_COIN_MODE:
	  { 
	    unsigned char mode = mbuf[2];
	    printf("\tSTATE CHANGE: LC MODE <- %u\n", (unsigned) mode);
	  }
	  break;
	case DSC_SET_LOCAL_COIN_WINDOW:
	  { 
	    unsigned long pre  = unpackLong(mbuf+2);
	    unsigned long post = unpackLong(mbuf+6);
	    printf("\tSTATE CHANGE: LC WIN <- (%lu, %lu)\n",
		   pre, post);
	  }
	  break;
	case DSC_ENABLE_PMT_HV:
	  printf("\tSTATE CHANGE: ENABLE PMT HV\n");
	  break;
	case DSC_SET_PMT_HV:
	  {
	    unsigned short hvdac = unpackShort(mbuf+2);
	    printf("\tSTATE CHANGE: SET PMT HV (%hu DAC, %hu V)\n", hvdac, hvdac/2);
	  }
	  break;
	case DSC_DISABLE_PMT_HV:
          printf("\tSTATE CHANGE: DISABLE PMT HV\n");
          break;
       	default: 
	  printf("\tSTATE CHANGE TYPE 0x%02x\n", mbuf[1]);
	  break;
	}
      }
    }

    monicount++;
    tprev = h.time;
  }
		  

  close(fd);

  return 0;
}

