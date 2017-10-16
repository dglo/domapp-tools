/* domapptest.c
   John Jacobsen, jacobsen@npxdesigns.com, for LBNL/IceCube
   Started June, 2004
   $Id: domapptest.c,v 1.36 2006/09/28 17:22:57 jacobsen Exp $

   Tests several functions of DOMapp directly through the 
   DOR card interface/driver, bypassing any Java or network
   peculiarities.

*/

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#include <stdio.h>
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

int usage(void) {
  fprintf(stderr, 
	  "Usage:\n"
	  "  domapptest [options] <DOM>\n"
	  "  <DOM> is of the form 00a, 00A, or /dev/dhc0w0dA\n"
	  "  [options] are any of:\n"
	  "    -h help: show this message\n"
	  "    -v show slightly more verbose output\n"
	  "    -V ask domapp for its release version\n"
	  "    -Q get DOM ID from DOM\n"
	  "    -c change mode from Iceboot to domapp first\n"
	  "    -s stuffing mode for maximum bandwidth\n"
	  "    -d run duration, seconds (default 1)\n"
	  "    -S <dac>,<val> Set DAC <dac> to <val>.  <dac> numeric only, sorry.\n"
	  "       Repeatable.\n"
	  "  Periodic message types:\n"
	  "    -E <freq>: Run echo test w/ relative frequency <freq>\n"
	  "    -M <freq>: Collect monitor data w/ relative frequency <freq>\n"
	  "    -H <freq>: Collect hit data w/ relative frequency <freq>\n"
	  "    -D <DAC#>,<minval>,<maxval>,<freq>:\n"
	  "       Set DAC <DAC#> w/ frequency <freq>,\n"
	  "       random integer values from <minval> to <maxval>, inclusive\n"
	  "  Monitoring:\n"
	  "    -w <sec>: Tell domapp to generate HW moni recs every <sec> seconds\n"
	  "    -f <sec>: Tell domapp to generate config moni recs every <sec> seconds\n"
	  "    -m <file>: Write monitoring data to <file>\n"
	  "    -G: Reset monitoring buffer before sending other messages\n"
	  "    -O: Don't drain monitoring buffer as a seperate stage during initialization\n"
	  "  Hit data:\n"
	  "    -B Initiate triggering/data taking (do not use w/ -u option)\n"
	  "    -i <file>: Write hit data to <file>\n"
	  "    -T <trigmode>:\n"
	  "       Set trigger mode to <trigmode> (0=testpat 1=cpu 2=disc)\n"
          "    -Z <mode>: Set hit data compression mode (0==uncompressed 1==delta)\n"
	  "    -X <mode>: Set hit data format (0==engineering format 1==raw)\n"
	  "    -A <ATWD>: Select ATWD (0 or 1) for hit data\n"
	  "    -N <nch0>,<nch1>,<nch2>,<nch3>: Number of samples (0,16,32,64,128)\n"
	  "                                    for each ATWD channel\n"
	  "    -W <wch0>,<wch1>,<wch2>,<wch3>: Sample width (1 or 2 bytes), each chan.\n"
	  "    -F <num>: Read out <num> (0..255) ATWD samples\n"
	  "    -I <mode>,<pre>,post>: Require local coincidence,\n"
	  "       with the two time windows given in nsec.\n"
	  "       Mode=1 (upper and lower enabled) 2 (upper only) 3 (lower only)\n"
	  "    -p: Run pulser to generate SPE triggers in absence of real PMT\n"
	  "    -P <rate>: Set pulser/heartbeat rate to <rate> Hz\n"
	  "    -L <Volts>: Set DOM high voltage (BE CAREFUL!). Volts == DAC units/2.\n"
	  "    -K <freq>,<mode>,<deadtime>,<file>: collect supernova data in file <file>; \n"
	  "       frequency (0==don't fetch count data) is relative to hit,\n"
	  "       moni etc. messages.  Mode 0=spe, 1=mpe; deadtime in [6400, 512000].\n"
	  "    -k Perform long supernova test.\n"
	  "  Flasher board interface:\n"
	  "    -z Fetch flasher board ID\n"
	  "    -u <bright>,<win>,<delay>,<mask>,<rate>: Flasher board run.  Do not use\n"
	  "       with -B option.\n"
	  "  Roll-your-own Custom Messages:\n"
	  "    -C <type>,<subtype> E.g., -C 4,12 gives EXPCONTROL_BEGIN_RUN message.\n"
	  );
  return 0;
}

#include "domapp.h"

#define MAX_MSGS_IN_FLIGHT    8 /* Don't queue more than 8 msgs at a time */
#define MAX_MSG_BYTES      8092
#define MAXRDFAILED         100
#define READCYCLE            10
#define DO_TEST_INJECT        0 /* Set to true if you want to inject test data */
#define MINLCWIN             25
#define MAXLCWIN           1550
#define MAXDACS              50
#define MAXFILENAME         512
#define ATWDCHSIZ           128
#define FADCSIZ             256

int is_printable(char c) { return c >= 32 && c <= 126; }
char printable(char c) { return is_printable(c) ? c : '.'; }
void showbuf(unsigned char *buf, int n);
void show_fpga(int icard);
int getBufSize(char *procFile);
int getDevFile(char *filename, int len, char *arg);
void drainMsgs(int filep, unsigned char * rdbuf, int bufsiz, int showIt, int timeOutTrials);
void fillEchoMessageData(DOMMSG *m, int max);
int * getCycle(int efreq, int mfreq, int hfreq, int dfreq, int sfreq);
int getRandInt(int min, int max);
int resetLBM(int filep, int bufsiz);
int beginRun(int filep, int bufsiz, int dopulser);
int beginFBRun(int filep, int bufsiz, unsigned short bright,
               unsigned short win, short delay, unsigned short mask, unsigned short rate);
int endRun(int filep, int bufsiz, int dopulser);
int setUpLC(int filep, int bufsiz, int mode, int pre_ns, int post_ns);
int turnOffLC(int filep, int bufsiz);
int setUpSN(int filep, int bufsiz, int snmode, int sndeadt);
int turnOffSN(int filep, int bufsiz);
int testPedestalCollection(int filep, int bufsiz);
int generatePedestals(int filep, int bufsiz);
int setHighVoltage(int filep, int bufsiz, int hv);
int highVoltageOff(int filep, int bufsiz);
int setPulserRate(int filep, int bufsiz, int rate);
int setCompression(int filep, int bufsiz, int mode);
int setDataFormat(int filep, int bufsiz, int mode);
int turnOffPeriodicMonitoring(int filep, int bufsiz);
int setUpTrigMode(int filep, int bufsiz, int trigMode);
int reportableDelta(int dtsec, int lastdtsec);
int doSnCrashTest(int filep, int bufsiz);
int doResetMonitoringMessage(int filep, int bufsiz);
void getTimes(struct timeval * tstart, int * dtsec, int *dtusec, unsigned long long * dt);
int drainMoniMessages(int savemoni, int monifd, int filep, int bufsiz, int maxtries);

#define EMPTY  0
#define ECHO   1
#define MONI   2
#define HITS   3
#define SETDAC 4
#define SN     5
char *msgstr[6] = {
  "EMPTY", "ECHO", "MONITOR", "HIT", "SETDAC", "SN"
};
static unsigned long long msgBytes[6];
static unsigned long msgs[6];

int main(int argc, char *argv[]) {
  unsigned char rdbuf[MAX_MSG_BYTES];
# define BSIZ 512
  char filename[BSIZ];

  srand((int) getpid());

  /************* Process command arguments ******************/

  int bufsiz=getBufSize("/proc/driver/domhub/bufsiz");
  if(sizeof(DOMMSG) < bufsiz) {
    printf("Error: sizeof(DOMMSG)=%d, driver bufsiz=%d, bufsiz too big.\n", 
	   sizeof(DOMMSG), bufsiz);
    exit(-1);
  }

  int doChangeState = 0;
  int secDuration   = 1;
  int stuffit       = 0;
  int hwival        = 0;
  int cfival        = 0;
  int efreq         = 0;
  int mfreq         = 0;
  int hfreq         = 0;
  int dfreq         = 0;
  int dacnum        = 0;
  int dacmin        = 0;
  int dacmax        = 0;
  int savemoni      = 0;
  int verbose       = 0;
  int askversion    = 0;
  int savehits      = 0;
  int trigMode      = 0;
  int pulserRate    = 0;
  int doPulserRate  = 0;
  int dopoll        = 0;
  int nsamps[4] = {0,0,0,0}, sampwids[4] = {2,2,2,2}, nadc = 0;
  int doCustom      = 0;
  int custType, custSubType;
  char monifile[MAXFILENAME];
  char hitsfile[MAXFILENAME];
  char snfile[MAXFILENAME];
  int monifd, hitsfd;
  int defineTrig    = 0;
  int defineEngrFmt = 0;
  int inflight      = 0;
  int dohitbuf       = 0;
  int defineATWD    = 0;
  int whichATWD     = 0;
  int dohv          = 0;
  int hvdac         = 0;
  int getDOMID      = 0;
  int dopulser      = 0;
  unsigned short dacs[MAXDACS],dacvals[MAXDACS];
  int dac,val,ndacs=0;
  int lcmode;
  int dofbid = 0;
  int dofbrun = 0;
  unsigned short bright, win, mask, rate;
  short delay;
  int dolc = 0, pre_ns, post_ns;
  int doFmt = 0, fmtMode = 0;
  int doComp = 0, compMode = 0;
  int doCompTest = 0; 
  int dosn = 0, snmode, sndeadt, sfreq=0;
  int snOn = 0, lcOn = 0, hvOn = 0, moniOn = 0;
  int dosntest = 0;
  int running = 0;
  int skipmonidrain = 0;
  int doResetMoni = 0;
  while(1) {
    char c = getopt(argc, argv, 
		    "QVGovhcBOkspzi:d:E:M:H:D:m:w:f:T:N:"
		    "W:F:C:R:A:S:L:I:P:Z:X:K:u:");
    if (c == -1) break;

    switch(c) {
    case 'Q': getDOMID = 1; break;
    case 'G': doResetMoni = 1; break;
    case 'o': doCompTest = 1; break;
    case 'O': skipmonidrain = 1; break;
    case 'c': doChangeState = 1; break;
    case 'd': secDuration = atoi(optarg); break;
    case 'k': dosntest = 1; break;
    case 's': stuffit = 1; break;
    case 'z': dofbid = 1; break;
    case 'X': doFmt = 1; fmtMode = atoi(optarg); break;
    case 'Z': doComp = 1; compMode = atoi(optarg); break;
    case 'A': whichATWD = atoi(optarg); defineATWD = 0; break;
    case 'E': efreq = atoi(optarg); if(efreq>0) dopoll = 1; break;
    case 'M': mfreq = atoi(optarg); if(mfreq>0) dopoll = 1; break;
    case 'H': hfreq = atoi(optarg); if(hfreq>0) dopoll = 1; break;
    case 'B': dohitbuf = 1; break;
    case 'p': dopulser = 1; break;
    case 'P': doPulserRate = 1; pulserRate = atoi(optarg); break;
    case 'u':
      if(sscanf(optarg, "%hu,%hu,%hd,%hu,%hu", 
		&bright,&win,&delay,&mask,&rate)!=5) exit(usage());
      dofbrun = 1;
      break;
    case 'K': 
      if(sscanf(optarg, "%d,%d,%d,%s", 
		&sfreq, &snmode, &sndeadt, snfile)!=4) exit(usage());
      dosn   = 1;
      if(sfreq>0) dopoll = 1;
      break;
    case 'I': 
      if(sscanf(optarg, "%d,%d,%d", &lcmode,
		&pre_ns, &post_ns)!=3) exit(usage());
      dolc = 1;
      break;
    case 'D': 
      if(sscanf(optarg, "%d,%d,%d,%d", &dacnum,&dacmin,&dacmax,&dfreq)!=4) 
	exit(usage()); 
      break;
    case 'T': 
      if(sscanf(optarg, "%d", &trigMode)!=1) exit(usage()); 
      defineTrig = 1;
      break;
    case 'N': 
      if(sscanf(optarg, "%d,%d,%d,%d", &nsamps[0],&nsamps[1],&nsamps[2],&nsamps[3])!=4) 
	exit(usage()); 
      defineEngrFmt = 1;
      break;
    case 'W': 
      if(sscanf(optarg, "%d,%d,%d,%d", 
		&sampwids[0],&sampwids[1],&sampwids[2],&sampwids[3])!=4) exit(usage()); 
      defineEngrFmt = 1;
      break;
    case 'C': 
      if(sscanf(optarg, "%d,%d", &custType, &custSubType)!=2)
	exit(usage()); 
      doCustom = 1; break;
    case 'F': 
      if(sscanf(optarg, "%d", &nadc) != 1) exit(usage()); 
      defineEngrFmt = 1;
      break;
    case 'm': 
      if(sscanf(optarg,"%s",monifile)!=1) exit(usage());
      savemoni = 1; 
      break;
    case 'i': 
      if(sscanf(optarg,"%s",hitsfile)!=1) exit(usage());
      savehits = 1; 
      break;
    case 'w': hwival = atoi(optarg); break;
    case 'f': cfival = atoi(optarg); break;
    case 'L': dohv = 1; hvdac = atoi(optarg)*2; break;
    case 'S': 
      if(sscanf(optarg, "%d,%d", &dac, &val)!=2) { 
	fprintf(stderr,"Bad arg. format!\n"); 
	exit(usage()); 
      }
      if(dac<0 || val<0) { fprintf(stderr,"DAC values must be positive!\n"); exit(-1); }
      if(ndacs >= MAXDACS) { fprintf(stderr,"Too many DAC values specified!\n"); exit(-1); }
      dacs[ndacs] = dac; 
      dacvals[ndacs++] = val;
      break;
    case 'v': verbose = 1; break;
    case 'V': askversion = 1; break;
    case 'h':
    default: exit(usage());
    }
  }

  /* Validate LC arguments */
  if(dolc && ((pre_ns  < MINLCWIN || post_ns  > MAXLCWIN) ||
	      (lcmode < 1 || lcmode > 3))) {
    fprintf(stderr,"Error: LC windows must be between %d and %d,\n"
	   "mode between 1 and 3.\n", MINLCWIN, MAXLCWIN);
    exit(-1);
  }

  /* Validate engineering format arguments */
  if(nadc<0 || nadc>255) {
    fprintf(stderr,"Error: number of ADC samples must be between 0 and 255.\n");
    exit(-1);
  }

  int ich;
  for(ich=0;ich<4;ich++) {
    if(sampwids[ich] != 1 && sampwids[ich] != 2) {
      fprintf(stderr,"Error: sample widths must be 1 or 2.\n");
      exit(usage());
    }
    if(nsamps[ich] != 0 && nsamps[ich] != 16 && nsamps[ich] != 32
       && nsamps[ich] != 64  && nsamps[ich] != 128) {
      fprintf(stderr,"Error: number of ATWD samples must be 0, 16, 32, 64, or 128.\n");
      exit(usage());
    }
  }

  if(dfreq) 
    fprintf(stderr,"Will set DAC %d to values in the range [%d, %d] with "
	    "a relative freq. %d.\n", dacnum, dacmin, dacmax, dfreq);

  int argcount = argc-optind;

  if(argcount < 1) exit(usage());

  if(getDevFile(filename, BSIZ, argv[optind])) exit(usage());

  int * cyclic = NULL;
  if(dopoll) {
    cyclic = getCycle(efreq, mfreq, hfreq, dfreq, sfreq);
    if(!cyclic) { fprintf(stderr,"Malloc failed or divide by zero\n"); exit(-1); }
    int showfreq = 0, il;
    for(il=0;showfreq && il<efreq+mfreq+hfreq+dfreq;il++) {
      fprintf(stderr,"Entry %d <- %s\n", il, msgstr[cyclic[il]]);
    }
  }

  /* Open file(s) */
  int filep = open(filename, O_RDWR);
  if(filep <= 0) {
    fprintf(stderr,"Can't open file %s (%d:%s)\n", filename, errno, strerror(errno));
    exit(-1);
  }   

  if(doResetMoni) {
    if(doResetMonitoringMessage(filep, bufsiz)) exit(-1);
  }

  if(doCompTest) {
    if(testPedestalCollection(filep, bufsiz)) exit(-1);
    fprintf(stderr,"Done.\n");
    exit(0);
  }
  
  if(dosntest) {
    if(doSnCrashTest(filep, bufsiz)) exit(-1);
    fprintf(stderr,"Done.\n");
    exit(0);
  }

  if(savemoni) {
    fprintf(stderr,"Will save monitoring data to file %s.\n", monifile);
    monifd = open(monifile, O_RDWR|O_TRUNC|O_CREAT, 0644);
    if(monifd <= 0) {
      fprintf(stderr,"Can't open file %s for output (%d:%s)\n", 
	      monifile, errno, strerror(errno));
      exit(-1);
    }
  }

  if(savehits) {
    fprintf(stderr,"Will save hit data to file %s.\n", hitsfile);
    hitsfd = open(hitsfile, O_RDWR|O_TRUNC|O_CREAT, 0644);
    if(hitsfd <= 0) {
      fprintf(stderr,"Can't open file %s for output (%d:%s)\n", 
	      hitsfile, errno, strerror(errno));
      exit(-1);
    }
  }

  /* Change mode to domapp if required */
# define DOMAPPSTR "domapp\r"
  if(doChangeState) {
    drainMsgs(filep, rdbuf, MAX_MSG_BYTES, 1, 10); /* Show iceboot msgs */
    int nw = write(filep, DOMAPPSTR, strlen(DOMAPPSTR));
    fprintf(stderr,"Wrote %d bytes.\n", nw);
    usleep(5000000);
    drainMsgs(filep, rdbuf, MAX_MSG_BYTES, 1, 20);
    usleep(3000000);  
  }

  /* Prepare downgoing messages */
  DOMMSG * echoMsg      = newEchoMsg();
  DOMMSG * moniMsg      = newMoniMsg();
  DOMMSG * getHitsMsg   = newGetHitDataMsg();
  DOMMSG * dacMsg       = newSetDacMsg(dacnum, 0);
  DOMMSG * getSNDataMsg = newGetSNDataMsg();
  DOMMSG * msgReply     = newMsg();
  if(!echoMsg || !moniMsg || !dacMsg || !getHitsMsg || !getSNDataMsg || !msgReply) {
    fprintf(stderr, "Couldn't get message object, probably out of memory?\n");
    exit(-1);
  }
  int maxSendData = bufsiz-MSG_HDR_LEN; /* Size of DATA PORTION */
  fillEchoMessageData(echoMsg, maxSendData);

  /* Start clock */
  int lastdtsec = 0;

  int r;

  if(getDOMID) {
    char ID[MAX_DATA_LEN];
    if((r=domsg(filep, bufsiz, 10000, MESSAGE_HANDLER, MSGHAND_GET_DOM_ID,
                "+X", ID)) != 0) {
      fprintf(stderr,"MSGHAND_GET_DOM_ID failed: %d\n", r);
      exit(-1);
    }
    fprintf(stderr,"DOM ID is '%s'\n", ID);
  }

  if(dofbid) {
    char fbid[MAX_DATA_LEN];
    if((r=domsg(filep, bufsiz, 10000,
                DATA_ACCESS, DATA_ACC_GET_FB_SERIAL,
                "+X", fbid)) != 0) {
      fprintf(stderr,"DATA_ACC_GET_FB_SERIAL failed: %d\n", r);
      exit(-1);
    }
    fprintf(stderr,"Flasher board ID is '%s'\n", fbid);
  }

  if(askversion) {
    char version[MAX_DATA_LEN];
    if((r=domsg(filep, bufsiz, 10000, 
		MESSAGE_HANDLER, MSGHAND_GET_DOMAPP_RELEASE, 
		"+X", version)) != 0) {
      fprintf(stderr,"MSGHAND_GET_DOMAPP_RELEASE failed: %d\n", r); 
      exit(-1);
    }
    fprintf(stderr,"DOMApp version is '%s'\n", version);
  }

  if(doCustom) {
    fprintf(stderr,"Sending message type %d, subtype %d... ",custType, custSubType);
    if((r=domsg(filep, bufsiz, 10000,
                custType, custSubType, "")) != 0) {
      fprintf(stderr,"\ncustom message failed: %d\n", r); 
      exit(-1);
    }
    fprintf(stderr,"OK.\n");
  }

  if(defineATWD) {
    if(whichATWD != 0 && whichATWD != 1) {
      fprintf(stderr,"Error: must select ATWD 0 or 1!\n\n");
      exit(usage());
    }
    fprintf(stderr,"Selecting ATWD %d... ", whichATWD);
    if((r=domsg(filep, bufsiz, 10000, DOM_SLOW_CONTROL, DSC_SELECT_ATWD, "-C",
		(unsigned char) whichATWD)) != 0) {
      fprintf(stderr,"DSC_SELECT_ATWD failed: %d\n", r);
      exit(-1);
    }
    fprintf(stderr,"OK.\n");
  }

  if(defineEngrFmt) {
    fprintf(stderr,"Setting ATWD format to %d(%d) %d(%d) %d(%d) %d(%d) (%d FADC samps)... ",
	   nsamps[0], sampwids[0],
	   nsamps[1], sampwids[1],
	   nsamps[2], sampwids[2],
	   nsamps[3], sampwids[3], nadc);
    unsigned char mask0, mask1;
    getMasks(&mask0, &mask1, nsamps, sampwids);
    if((r=domsg(filep, bufsiz, 10000, DATA_ACCESS, DATA_ACC_SET_ENG_FMT, "-CCC",
		(unsigned char) (nadc&0xFF), mask0, mask1)) != 0) {
      fprintf(stderr,"DATA_ACC_SET_ENG_FMT failed: %d\n", r);
      exit(-1);
    }
    fprintf(stderr,"OK.\n");
  }

  /* Set DACs to desired values */
  int idac;
  for(idac=0; idac<ndacs; idac++) {
    fprintf(stderr,"Setting DAC %d to %d... ", dacs[idac], dacvals[idac]);
    if((r=domsg(filep, bufsiz, 10000, DOM_SLOW_CONTROL, DSC_WRITE_ONE_DAC, "-CCS", 
		dacs[idac], 0, dacvals[idac])) != 0) {
      fprintf(stderr,"DSC_WRITE_ONE_DAC failed: %d\n", r);
      exit(-1);
    }
    fprintf(stderr,"OK.\n");
  }

  if(doPulserRate && setPulserRate(filep, bufsiz, pulserRate)) exit(-1);
 
  if(doFmt && setDataFormat(filep, bufsiz, fmtMode)) exit(-1);

  if(doComp) {
    if(generatePedestals(filep, bufsiz)) exit(-1);
    if(setCompression(filep, bufsiz, compMode)) exit(-1);
  }

  if(defineTrig && setUpTrigMode(filep, bufsiz, trigMode)) exit(-1);

  if(dohv) {
    if(setHighVoltage(filep, bufsiz, hvdac)) exit(-1);
    hvOn = 1;
  } else {
    if(highVoltageOff(filep, bufsiz)) exit(-1);
  }

  /* Set up local coincidence event selection for buffering */
  if(dolc) {
    if(setUpLC(filep, bufsiz, lcmode, pre_ns, post_ns)) {
      fprintf(stderr,"Domapp local coincidence initialization failed.\n");
      exit(-1);
    }
    lcOn = 1;
  } else { /* If !dolc, then set mode to zero */
    turnOffLC(filep, bufsiz);
  }

  if(dohitbuf && dofbrun) {
    fprintf(stderr, "Flasher run is incompatible with normal run modes! (-B)\n");
    exit(-1);
  }

  /* Set up supernova system, if desired */
  int snfd;
  if(dosn) {
    /* fprintf(stderr,"%s %d %d", snfile, snmode, sndeadt); */
    if(setUpSN(filep, bufsiz, snmode, sndeadt)) {
      fprintf(stderr,"Domapp supernova readout initialization failed.\n");
      exit(-1);
    }
    snfd = open(snfile, O_RDWR|O_TRUNC|O_CREAT, 0644);
    if(snfd <= 0) {
      fprintf(stderr,"Can't open file %s for output (%d:%s)\n", snfile, errno, strerror(errno));
      exit(-1);
    }
    snOn = 1;
  }


  /* Start data taking on DOM ... */
  if(dohitbuf) {
    if(beginRun(filep, bufsiz, dopulser)) {
      fprintf(stderr,"Run start failed.\n");
      exit(-1);
    }
    running = 1;
  }

  /* ... or start flasher run on DOM */
  if(dofbrun) {
    if(beginFBRun(filep, bufsiz, bright, win, delay, mask, rate)) {
      fprintf(stderr,"Flasher run start failed.\n");
      exit(-1);
    }
    running = 1;
  }

  if(hwival || cfival) {
    fprintf(stderr,"Setting monitoring intervals (hw=%d sec, cf=%d sec)... ", 
	    hwival, cfival);
    if((r=domsg(filep, bufsiz, 10000,
                DATA_ACCESS, DATA_ACC_SET_MONI_IVAL, 
		"-LL", (unsigned long) hwival, (unsigned long) cfival)) != 0) {
      fprintf(stderr,"DATA_ACC_SET_MONI_IVAL failed: %d\n", r);
      exit(-1);
    }
    fprintf(stderr,"OK.\n");
    moniOn = 1;
  }
  
  /* Drain out any pending moni messages before starting long run to make
     sure they make it out.  FIXME: do this at end of run instead (needs
     redesign of main loop */
  if(!skipmonidrain && drainMoniMessages(savemoni, monifd, filep, bufsiz, 500)) {
    fprintf(stderr,"Couldn't drain pending monitoring messages!\n");
    exit(-1);
  }

  /* keep gettimeofday near beginRun to time run correctly */
  struct timeval tstart; 
  gettimeofday(&tstart, NULL);
  struct timeval lastTWrite, lastTRead;

  if(dopoll) fprintf(stderr,"Entering periodic data collection loop...\n");
  /* Messaging loop */
  int icyc = 0;
  int done = 0;
  int dtsec = 0, dtusec = 0;
  unsigned long long dt = 0;
  if(!dopoll) sleep(secDuration); 
  while(dopoll) {
    int gotwrite = 0;
    int rdfailed = 0;
    /* Send message until driver input buffer is full or inflight gets too big */
    while(inflight < MAX_MSGS_IN_FLIGHT) {
      DOMMSG * msgToSend;
      int sendType;
      switch(cyclic[icyc]) {
      case MONI: sendType = MONI; msgToSend = moniMsg; break;
      case HITS: sendType = HITS; msgToSend = getHitsMsg; break;
      case SN:   sendType = SN;   msgToSend = getSNDataMsg; break;
      case SETDAC: 
	sendType = SETDAC; 
	setDacMsgDacValue(dacMsg, getRandInt(dacmin, dacmax));
	msgToSend = dacMsg; 
	break;
      case ECHO: 
      default: sendType = ECHO; msgToSend = echoMsg; break;
      }
      /* Write the message */
      int len = sendMsg(filep, msgToSend, 100);
      if(len == 0) {
	fprintf(stderr,"Bad return value from sendMsg (%d)!\n", len);
	exit(-1);
      } else if(len == -1 && errno == EAGAIN) {
	break;
      } else if(len == -1) {
	fprintf(stderr,"sendMsg gave -1, errno=%d (%s).\n", errno, strerror(errno));
	exit(-1);
      } else {
	inflight++;
	icyc++; if(icyc >= efreq+mfreq+hfreq+dfreq+sfreq) icyc=0;
	msgBytes[sendType] += len;
	gotwrite = 1;
	/* Remember time of last write */
	gettimeofday(&lastTWrite, NULL);
      }
      if(!stuffit) break; /* Don't do any more unless stuffing mode */
    }

    int gotread = 0;
    int iread   = 0;
    while(1) {
      int len = getMsg(filep, msgReply, bufsiz, 100);
      if(len == 0) { /* EAGAIN */
	if(rdfailed++ >= MAXRDFAILED) break; /* Deal with it below */
	if(iread++ >= READCYCLE) break;
	usleep(1000);
	continue;
      } else if(len < 0) {
	fprintf(stderr,"getMsg gave error %d -- quitting.\n", len);
	exit(-1);
      } 

      gotread  = 1;
      rdfailed = 0;
      gettimeofday(&lastTRead, NULL);
      
      int rmt    = msgType(msgReply);
      int rmst   = msgSubType(msgReply);
      int dlen   = msgDataLen(msgReply);
      int status = msgStatus(msgReply);

      //fprintf(stderr,"R%d ",rmst);

      if(status != 1) {
	fprintf(stderr,"ERROR: bad status from domapp (%d).\n", status);
	fprintf(stderr,"mt=%d mst=%d len=%d\n", rmt, rmst, dlen);
	exit(-1);
      }

      inflight--;
      if(rmt == MESSAGE_HANDLER && rmst == MSGHAND_ECHO_MSG) {
	msgs[ECHO]++;    msgBytes[ECHO] += len;
      } else if(rmt == DATA_ACCESS && rmst == DATA_ACC_GET_NEXT_MONI_REC) {
	if(dlen && savemoni) {
	  int nm = write(monifd, msgReply->data, dlen);
	  if(nm != dlen) { 
	    fprintf(stderr,"Short write (%d of %d bytes) to monitoring stream.\n", 
		    nm, dlen); exit(-1); 
	  }
	}
	msgs[MONI]++;    msgBytes[MONI] += len;
      } else if(rmt == DOM_SLOW_CONTROL && rmst == DSC_WRITE_ONE_DAC) {
	msgs[SETDAC]++;  msgBytes[SETDAC] += len;
      } else if(rmt == DATA_ACCESS && rmst == DATA_ACC_GET_DATA) {
	msgs[HITS]++;    msgBytes[HITS] += len;
	if(dlen && savehits) {
	  int nh = write(hitsfd, msgReply->data, dlen);
	  if(nh != dlen) { 
	    fprintf(stderr,"Short write (%d of %d bytes) to hit data stream.\n",
		    nh, dlen); 
	    exit(-1); 
	  }
	}
      } else if(rmt == DATA_ACCESS && rmst == DATA_ACC_GET_SN_DATA) {
	msgs[SN]++;      msgBytes[SN] += len;
	if(dlen) {
	  int ns = write(snfd, msgReply->data, dlen);
	  if(ns != dlen) { 
	    fprintf(stderr,"Short write (%d of %d bytes) to hit data stream.\n",
		    ns, dlen); 
	    exit(-1);
	  }
	  unsigned short snlen = unformatShort(msgReply->data);
	  if(dlen != snlen) {
	    fprintf(stderr,"msg data portion (%d bytes) != SN data length (%d bytes)!\n",
		    dlen, snlen);
	    exit(-1);
       	  }
	}
      } else {
	fprintf(stderr,"Unknown message mt=%d mst=%d len=%d\n", rmt, rmst, dlen);
      }
      if(!stuffit) break;
    } /* read cycle */

    if(rdfailed >= MAXRDFAILED) {
      fprintf(stderr,"ERROR: Had at least %d failed reads in a row.  DOM died?\n", rdfailed);
      exit(-1);
    }

    /* Check for done, show rates, etc. */
    getTimes(&tstart, &dtsec, &dtusec, &dt);

    if(reportableDelta(dtsec, lastdtsec)) {
      double totBytes = (double) msgBytes[ECHO] + (double) msgBytes[MONI] + 
	(double) msgBytes[HITS] + (double) msgBytes[SETDAC] + (double) msgBytes[SN];
      double echoDataRate = totBytes / (((double) dtsec)*1E3);
      fprintf(stderr,"%d.%06ds: %lu echo, %lu moni, %lu hit, %lu dac, %lu sn, "
	      "%2.2fMB, %2.2f kB/sec", 
	      dtsec, dtusec, msgs[ECHO], msgs[MONI], msgs[HITS], msgs[SETDAC], msgs[SN],
	      totBytes/1E6, echoDataRate);
      fprintf(stderr,"\n");
    }
    
    if(dohitbuf && dtsec >= secDuration && ! done) {
      if(running) {
	if(endRun(filep, bufsiz, dopulser)) {
	  fprintf(stderr,"endRun failed.\n");
	  exit(-1);
	}
	running = 0;
      }
      if(lcOn) {
	if(turnOffLC(filep, bufsiz)) {
	  fprintf(stderr,"ERROR: turnOffLC failed.\n");
	} else {
	  lcOn = 0;
	}
      }
      if(snOn) {
	if(turnOffSN(filep, bufsiz)) {
	  fprintf(stderr,"ERROR: turnOffSN failed.\n");
	} else {
	  snOn = 0;
	}
      }
      if(moniOn) {
	if(turnOffPeriodicMonitoring(filep, bufsiz)) {
	  fprintf(stderr,"ERROR: turnOffPeriodicMonitoring failed.\n");
	} else {
	  moniOn = 0;
	}
      }
      done = 1;
    }
    /* Turn off HV when duration is up so we can see it in 
       monitoring stream */
    if(dtsec >= secDuration && hvOn) {
      if(highVoltageOff(filep, bufsiz)) { /* We'll do again @ end of loop to be sure */
	fprintf(stderr,"ERROR: highVoltageOff failed.\n");
      } else {
	hvOn = 0;
      }
    }
    
    /* Add 1 sec to run length after run stop to get last monitoring, etc. events */
    if(dtsec >= secDuration+1) {
      break;
    }
    lastdtsec = dtsec;
      
  } /* while(dopoll) */

  int hadErr = 0;
  if(running && endRun(filep, bufsiz, dopulser)) {
    fprintf(stderr,"endRun failed.\n");
    hadErr++;
  } else {
    running = 0;
  }
  if(snOn && turnOffSN(filep, bufsiz)) {
    fprintf(stderr,"turnOffSN failed.\n");
    hadErr++;
  } else { 
    snOn = 0; 
  }
  if(hvOn) {
    highVoltageOff(filep, bufsiz);
    hadErr++;
    hvOn = 0;
  }
  if(lcOn && turnOffLC(filep, bufsiz)) {
    fprintf(stderr,"ERROR: turnOffLC failed.\n");
    hadErr++;
  } else { 
    lcOn = 0; 
  }
  if(moniOn && turnOffPeriodicMonitoring(filep, bufsiz)) {
    fprintf(stderr,"ERROR: turnOffPeriodicMonitoring failed.\n");
    hadErr++;
  } else {
    moniOn = 0;
  }

  free(dacMsg);
  free(msgReply);  
  free(echoMsg);
  drainMsgs(filep, rdbuf, MAX_MSG_BYTES, 0, 10);
  close(filep);
  if(savemoni) close(monifd);
  if(savemoni) close(hitsfd);
  if(cyclic != NULL) free(cyclic);

  getTimes(&tstart, &dtsec, &dtusec, &dt);
  if(hadErr) {
    fprintf(stderr,"FAIL (%lld usec).\n", dt);
  } else {
    fprintf(stderr,"Done (%lld usec).\n", dt);
  }
  return 0;
}


void showbuf(unsigned char *buf, int n) {
  int cpl=16;
  int nl = n/cpl+1;
  fprintf(stderr,"%d bytes: \n", n); 
  int il, ic;
  for(il=0; il<nl; il++) {
    for(ic=0; ic<cpl; ic++) {
      int iof = il*cpl + ic;
      if(iof < n) {
	fprintf(stderr,"%c", printable(buf[iof])); 
      } else {
	fprintf(stderr," ");
      }
    }

    fprintf(stderr,"  ");
    for(ic=0; ic<cpl; ic++) {
      int iof = il*cpl + ic;
      if(iof < n) {
        fprintf(stderr,"%02x ", buf[iof]);
      } else {
	break;
      }
    }
    fprintf(stderr,"\n"); 
  }
  fprintf(stderr,"\n");
  
}


void show_fpga(int icard) {
  /* In case of hardware timeout from the driver, show the DOR FPGA for the appropriate DOR
     card */
  char cmdbuf[1024];
  snprintf(cmdbuf,1024,"cat /proc/driver/domhub/card%d/fpga",icard);
  fprintf(stderr,"Showing FPGA registers: %s.\n",cmdbuf);
  system(cmdbuf);
}


int getBufSize(char * procFile) {
  int bufsiz;
  FILE *bs;
  bs = fopen(procFile, "r");
  if(bs == NULL) {
    fprintf(stderr, "Can't open bufsiz proc file.  Driver not loaded?\n");
    return -1;
  }
  fscanf(bs, "%d\n", &bufsiz);
  fclose(bs);
  return bufsiz;
}

int getDevFile(char * filename, int len, char *arg) {
  /* copy at most len characters into filename based on arg.
     If arg is of the form "00a" or "00A", file filename
     as "/dev/dhc0w0dA"; otherwise return a copy of arg. */

  int icard, ipair;
  char cdom;
  if(len < 13) return 1;
  if(arg[0] >= '0' && arg[0] <= '7') { /* 00a style */
    icard = arg[0]-'0';
    ipair = arg[1]-'0';
    cdom = arg[2];
    if(cdom == 'a') cdom = 'A';
    if(cdom == 'b') cdom = 'B';
    if(icard < 0 || icard > 7) return 1;
    if(ipair < 0 || ipair > 3) return 1;
    if(cdom != 'A' && cdom != 'B') return 1;
    snprintf(filename, len, "/dev/dhc%dw%dd%c", icard, ipair, cdom);
  } else {
    memcpy(filename, arg, strlen(arg)>len?len:strlen(arg));
  }
  return 0;
}


void drainMsgs(int filep, unsigned char * rdbuf, int bufsiz, int showIt, int timeOutTrials) {
  /* if timeOutTrials is set, use that as max # of retries to drain messages 
     otherwise, just wait to drain 1 message */
  struct pollfd  pfd;

  int nr;
  int i = 0;
  pfd.events = POLLIN;
  pfd.fd     = filep;
  while(1) {
    if(!poll(&pfd, 1, 100) || !(pfd.revents & POLLIN)) return;
    nr = read(filep, rdbuf, bufsiz);
    if(nr > 0) {
      if(showIt) showbuf(rdbuf, nr);
      if(!timeOutTrials) return;
      i = 0;
    } else {
      i++;
      if(timeOutTrials && i>=timeOutTrials) {
	return;
      }
      usleep(1000);
    }
  }
}

void fillEchoMessageData(DOMMSG *m, int max) {
  setMsgDataLen(m, max);
  int i;
  for(i=0; i<max; i++) m->data[i] = i & 0xFF;
}

int * getCycle(int efreq, int mfreq, int hfreq, int dfreq, int sfreq) {
  int n = efreq+mfreq+hfreq+dfreq+sfreq;
  int denom = n;
  if(denom == 0) return NULL;
  int * a = malloc(n*sizeof(int));
  if(!a) return NULL;
  int i;
  for(i=0;i<n; i++) {
    double x = rand()/(RAND_MAX+1.0);
    double eint = (double) efreq / (double) denom;
    double mint = (double) (efreq+mfreq) / (double) denom;
    double hint = (double) (efreq+mfreq+hfreq) / (double) denom;
    double sint = (double) (efreq+mfreq+hfreq+dfreq) / (double) denom;
    if(x < eint) {
      a[i] = ECHO;
      efreq--;
    } else if(x < mint) {
      a[i] = MONI;
      mfreq--;
    } else if(x < hint) {
      a[i] = HITS;
      hfreq--;
    } else if(x < sint) {
      a[i] = SETDAC;
      dfreq--;
    } else {
      a[i] = SN;
      sfreq--;
    }
    denom--;    
  }
  return a;
}



int endRun(int filep, int bufsiz, int dopulser) {
  int r;
  fprintf(stderr,"Ending run... ");
  if((r=domsg(filep, bufsiz, 10000,
              EXPERIMENT_CONTROL, EXPCONTROL_END_RUN, "")) != 0) {
    fprintf(stderr,"EXPCONTROL_END_RUN failed: %d\n", r);
    return 1;
  }
  fprintf(stderr,"OK.\n");
  if(dopulser) {
    fprintf(stderr,"Turning off front-end pulser... ");
    if((r=domsg(filep, bufsiz, 10000, DOM_SLOW_CONTROL, DSC_SET_PULSER_OFF, ""))) {
      fprintf(stderr,"DSC_SET_PULSER_OFF failed: %d\n", r);
      exit(-1);
    }
    fprintf(stderr,"OK.\n");
  }

  return 0;
}

int setUpSN(int filep, int bufsiz, int snmode, int sndeadt) {
  int r;
  if((r=domsg(filep, bufsiz, 10000,
              DOM_SLOW_CONTROL, DSC_ENABLE_SN, "-LC", sndeadt, snmode)) != 0) {
    fprintf(stderr,"DSC_ENABLE_SN failed: %d\n", r);
    return 1;
  }
  return 0;
}

int turnOffSN(int filep, int bufsiz) {
  int r;
  if((r=domsg(filep, bufsiz, 10000,
              DOM_SLOW_CONTROL, DSC_DISABLE_SN, "")) != 0) {
    fprintf(stderr,"DSC_ENABLE_SN failed: %d\n", r);
    return 1;
  }
  return 0;
}

int setUpLC(int filep, int bufsiz, int mode,
	    int pre_ns, int post_ns) {
  int r;
  if((r=domsg(filep, bufsiz, 10000,
              DOM_SLOW_CONTROL, DSC_SET_LOCAL_COIN_WINDOW, "-LL", 
	      pre_ns, post_ns)) != 0) {
    fprintf(stderr,"DSC_SET_LOCAL_COIN_WINDOW failed: %d\n", r);
    return 1;
  }
  if((r=domsg(filep, bufsiz, 10000,
	      DOM_SLOW_CONTROL, DSC_SET_LOCAL_COIN_MODE, "-C", mode)) != 0) {
    fprintf(stderr,"DSC_SET_LOCAL_COIN_MODE failed: %d\n", r);
    return 1;
  }
  return 0;
}

int turnOffLC(int filep, int bufsiz) {
   int r;
   if((r=domsg(filep, bufsiz, 10000,
	       DOM_SLOW_CONTROL, DSC_SET_LOCAL_COIN_MODE, "-C", 0)) != 0) {
     fprintf(stderr,"DSC_SET_LOCAL_COIN_MODE failed: %d\n", r);
     return 1;
   }
   return 0;
}

int beginFBRun(int filep, int bufsiz, unsigned short bright,
	       unsigned short win, short delay, unsigned short mask, unsigned short rate) {
  int r;
  fprintf(stderr,"Starting flasher run... ");
  if((r=domsg(filep, bufsiz, 10000,
              EXPERIMENT_CONTROL, EXPCONTROL_BEGIN_FB_RUN, "-SSSSS",
	      bright, win, delay, mask, rate)) != 0) {
    fprintf(stderr,"EXPCONTROL_BEGIN_FB_RUN failed: %d\n", r);
    return 1;
  }
  fprintf(stderr,"OK.\n");
  return 0;
}

int beginRun(int filep, int bufsiz, int dopulser) {
  int r;

  if(dopulser) {
    fprintf(stderr,"Turning on front-end pulser... ");
    if((r=domsg(filep, bufsiz, 10000, DOM_SLOW_CONTROL, DSC_SET_PULSER_ON, ""))) {
      fprintf(stderr,"DSC_SET_PULSER_ON failed: %d\n", r);
      exit(-1);
    }
    fprintf(stderr,"OK.\n");
  }

  fprintf(stderr,"Starting run... ");
  if((r=domsg(filep, bufsiz, 10000,
	      EXPERIMENT_CONTROL, EXPCONTROL_BEGIN_RUN, "")) != 0) {
    fprintf(stderr,"EXPCONTROL_BEGIN_RUN failed: %d\n", r);
    return 1;
  }
  fprintf(stderr,"OK.\n");

  return 0;
}

int generatePedestals(int filep, int bufsiz) {
  fprintf(stderr,"Generating pedestals...\n");
  int targetATWD0 = 1000;
  int targetATWD1 = 1000;
  int targetFADC  = 2000;
  int r;
  if((r=domsg(filep, bufsiz, 10000,
	      EXPERIMENT_CONTROL, EXPCONTROL_DO_PEDESTAL_COLLECTION, "-LLL",
	      targetATWD0, targetATWD1, targetFADC)) != 0) {
    fprintf(stderr,"EXPCONTROL_DO_PEDESTAL_COLLECTION failed: %d\n", r);
    return 1;
  }
  return 0;
}

int testPedestalCollection(int filep, int bufsiz) {
  int targetATWD0 = 1000;
  int targetATWD1 = 1000;
  int targetFADC  = 2000;
  unsigned long nped0, nped1, nadc;

  int r;
  int itrial; for(itrial=0; itrial<100; itrial++) {
    fprintf(stderr,"Ped. run %d\n", itrial);
    if((r=domsg(filep, bufsiz, 10000,
                EXPERIMENT_CONTROL, EXPCONTROL_DO_PEDESTAL_COLLECTION, "-LLL",
                targetATWD0, targetATWD1, targetFADC)) != 0) {
      fprintf(stderr,"EXPCONTROL_DO_PEDESTAL_COLLECTION failed: %d\n", r);
      return 1;
    }

    if((r=domsg(filep, bufsiz, 10000,
                EXPERIMENT_CONTROL, EXPCONTROL_GET_NUM_PEDESTALS, "+LLL",
                &nped0, &nped1, &nadc)) != 0) {
      fprintf(stderr,"EXPCONTROL_GET_NUM_PEDESTALS failed: %d.\n", r);
      return 1;
    }

    if(nped0 < targetATWD0 || nped1 < targetATWD1 || nadc < targetFADC) {
      fprintf(stderr,"Pedestal sums (%ld,%ld,%ld) are below targets (%d, %d, %d)!\n", 
	      nped0, nped1, nadc, targetATWD0, targetATWD1, targetFADC);
      return 1;
    }
  }
  return 0;
}

int getRandInt(int min, int max) {
  if(min == max) return min;
  double x = rand()/(RAND_MAX+1.0);
  int n = max-min+1;
  return min + (int) (x*n);
}


int setPulserRate(int filep, int bufsiz, int rate) {
  int r;
  fprintf(stderr,"Setting pulser rate to %d Hz... ", rate);
  if((r=domsg(filep, bufsiz, 10000,
              DOM_SLOW_CONTROL, DSC_SET_PULSER_RATE, "-S", (unsigned short) rate)) != 0) {
    fprintf(stderr,"DSC_SET_PULSER_RATE failed: %d\n", r);
    return 1;
  } else {
    fprintf(stderr,"OK.\n");
  }
  return 0;
}

int setCompression(int filep, int bufsiz, int mode) {
  int r;
  fprintf(stderr,"Setting DOM Data compression type to %d... ", mode);
  if((r=domsg(filep, bufsiz, 10000,
	      DATA_ACCESS, DATA_ACC_SET_COMP_MODE, "-C", (unsigned char) mode)) != 0) {
    fprintf(stderr, "DATA_ACC_SET_COMP_MODE failed: %d\n", r);
    return 1;
  }
  unsigned char mget;
  if((r=domsg(filep, bufsiz, 10000,
              DATA_ACCESS, DATA_ACC_GET_COMP_MODE, "+C", &mget)) != 0) {
    fprintf(stderr, "DATA_ACC_GET_COMP_MODE failed: %d\n", r);
    return 1;
  }
  if(mget != (unsigned char) mode) {
    fprintf(stderr, "Compression mode: set (%d) != get (%d).\n",
	    (int) mode, (int) mget);
    return 1;
  }
  fprintf(stderr,"OK.\n");
  return 0;
}

int setDataFormat(int filep, int bufsiz, int fmt) {
  int r;
  fprintf(stderr,"Setting DOM Data format to %d... ", fmt);
  if((r=domsg(filep, bufsiz, 10000,
              DATA_ACCESS, DATA_ACC_SET_DATA_FORMAT, "-C", (unsigned char) fmt)) != 0) {
    fprintf(stderr, "DATA_ACC_SET_DATA_FORMAT failed: %d\n", r);
    return 1;
  }
  unsigned char mfmt;
  if((r=domsg(filep, bufsiz, 10000,
              DATA_ACCESS, DATA_ACC_GET_DATA_FORMAT, "+C", &mfmt)) != 0) {
    fprintf(stderr, "DATA_ACC_GET_DATA_FORMAT failed: %d\n", r);
    return 1;
  }
  if(mfmt != (unsigned char) fmt) {
    fprintf(stderr, "Set data format: set (%d) != get (%d).\n",
            (int) fmt, (int) mfmt);
    return 1;
  }
  fprintf(stderr,"OK.\n");
  return 0;
}

int setHighVoltage(int filep, int bufsiz, int hvdac) {
  int r;
#define MAXHV      2048 /* VOLTS */
#define MAXHVDELTA 50   /* Volts */
#define MAXTRIALS  15
#define SLEEPMS    300 
  if(hvdac > 2*MAXHV) {
    fprintf(stderr,"setHighVoltage: voltage too high (%d V, %d is maximum)!\n",
	   hvdac/2, MAXHV);
    return 1;
  }
  fprintf(stderr,"Enabling high voltage...\n");
  if((r=domsg(filep, bufsiz, 10000,
	      DOM_SLOW_CONTROL, DSC_ENABLE_PMT_HV, "")) != 0) {
    fprintf(stderr,"DSC_ENABLE_PMT_HV failed: %d\n", r);
    return 1;
  }

  fprintf(stderr,"Setting high voltage...\n");
  if((r=domsg(filep, bufsiz, 10000,
              DOM_SLOW_CONTROL, DSC_SET_PMT_HV, "-S", hvdac)) != 0) {
    fprintf(stderr,"DSC_SET_PMT_HV(%d DAC units) failed: %d\n", hvdac, r);
    return 1;
  }

  fprintf(stderr,"Waiting for HV to ramp up.... "); fflush(stdout);
  int ok=0, isec;
  unsigned short qadc,qdac;
  for(isec=0;isec<MAXTRIALS;isec++) {
    if((r=domsg(filep, bufsiz, 10000,
		DOM_SLOW_CONTROL, DSC_QUERY_PMT_HV, "+SS", &qadc, &qdac)) != 0) {
      fprintf(stderr,"DSC_QUERY_PMT_HV failed: %d\n", r);
      highVoltageOff(filep, bufsiz);
      return 1;
    }
    /* Check return DAC setting */
    if(qdac != hvdac) {
      fprintf(stderr,"ERROR: High voltage setting (%d DAC units) in DOM is wrong!  (Should be %d).\n",
	     qdac, hvdac);
      highVoltageOff(filep, bufsiz);
      return 1;
    }
    fprintf(stderr,"%d V... ", qadc/2); fflush(stdout);
    if(abs(((int) hvdac) - ((int) qadc)) < MAXHVDELTA*2) {
      ok = 1;
      break;
    }
    usleep(1000*SLEEPMS);
  }
  if(!ok) {
    fprintf(stderr,"HV (%d V) never reached target value (%d V)!\n", qadc/2, hvdac/2);
    highVoltageOff(filep, bufsiz);
    return 1;
  } else {
    fprintf(stderr,"OK.\n");
  }

  return 0;
}


int highVoltageOff(int filep, int bufsiz) {
  fprintf(stderr,"Turning off high voltage.\n");
  int r;
  if((r=domsg(filep, bufsiz, 10000,
              DOM_SLOW_CONTROL, DSC_SET_PMT_HV, "-S", (unsigned short) 0)) != 0) {
    fprintf(stderr,"DSC_SET_PMT_HV(0) failed: %d\n", r);
    return 1;
  }
  if((r=domsg(filep, bufsiz, 10000,
              DOM_SLOW_CONTROL, DSC_DISABLE_PMT_HV, "")) != 0) {
    fprintf(stderr,"DSC_DISABLE_PMT_HV failed: %d\n", r);
    return 1;
  }
  return 0;
}

int turnOffPeriodicMonitoring(int filep, int bufsiz) {
  fprintf(stderr,"Turning off periodic monitoring... ");
  int r;
  if((r=domsg(filep, bufsiz, 10000, DATA_ACCESS, DATA_ACC_SET_MONI_IVAL, "-LL", 0, 0)) != 0) {
    fprintf(stderr,"DATA_ACC_SET_MONI_IVAL failed: %d\n", r);
    return 1;
  }
  fprintf(stderr,"OK.\n");
  return 0;
}

int doResetMonitoringMessage(int filep, int bufsiz) {
  int r;
  fprintf(stderr,"Resetting monitoring message buffer... ");
  if((r=domsg(filep, bufsiz, 10000, DATA_ACCESS, DATA_ACC_RESET_MONI_BUF, "")) != 0) {
    fprintf(stderr,"DATA_ACC_RESET_MONI_BUF failed: %d\n", r);
    return 1;
  }
  fprintf(stderr,"OK.\n");
  return 0;
}

int setUpTrigMode(int filep, int bufsiz, int trigMode) {
  int r;
  fprintf(stderr,"Setting trigger mode to %d... ", trigMode);
  if((r=domsg(filep, bufsiz, 10000, DOM_SLOW_CONTROL, DSC_SET_TRIG_MODE, "-C", 
	      (unsigned char) trigMode)) != 0) {
    fprintf(stderr,"DSC_SET_TRIG_MODE failed: %d\n", r);
    return 1;
  }
  
  unsigned char trigModeCheck = 0xFF; 
  if((r=domsg(filep, bufsiz, 10000, DOM_SLOW_CONTROL, DSC_GET_TRIG_MODE, "+C",
	      &trigModeCheck)) != 0) {
    fprintf(stderr,"DSC_GET_TRIG_MODE failed: %d\n", r);
    return 1;
  }
  if(trigModeCheck != trigMode) { 
    fprintf(stderr,"DSC_GET_TRIG_MODE failed: trigModeCheck=%d trigMode=%d\n", 
	    trigModeCheck, trigMode);
    return 1;
  }
  fprintf(stderr,"OK.\n");
  return 0;
}

int reportableDelta(int dtsec, int lastdtsec) {
  return (dtsec != lastdtsec && dtsec != 0
    && ((dtsec <= 10 /* Periodic criteria */
	|| (!((dtsec)%10) && dtsec<100)
	|| (!((dtsec)%100)&& dtsec<1000)
	|| (!((dtsec)%1000)))));
}

void getTimes(struct timeval * tstart, int * dtsec, int *dtusec, unsigned long long * dt) {
    struct timeval tnow;
    gettimeofday(&tnow, NULL);
    *dt = (tnow.tv_sec - tstart->tv_sec)*1000000 
      + (tnow.tv_usec - tstart->tv_usec);
    *dtsec = tnow.tv_sec - tstart->tv_sec;
    *dtusec = tnow.tv_usec - tstart->tv_usec;
    if(*dtusec < 0) {
      *dtusec += 1000000;
      *dtsec --;
    }
}

int doSnCrashTest(int filep, int bufsiz) {
  int r;
  int sndeadt = 6400, snmode = 0;
#define OUT(a) fprintf(stderr,a"\n")
  OUT("DSC_ENABLE_SN");
  if((r=domsg(filep, bufsiz, 10000,
              DOM_SLOW_CONTROL, DSC_ENABLE_SN, "-LC", sndeadt, snmode)) != 0) {
    fprintf(stderr,"Warning: DSC_ENABLE_SN failed: %d\n", r);
  }

  fprintf(stderr,"DATA_ACC_GET_SN_DATA...");
  DOMMSG * getSNDataMsg = newGetSNDataMsg();
  DOMMSG * getMoniMsg   = newMoniMsg();
  DOMMSG * msgReply     = newMsg();
  long long i; for(i=0; i<1000000; i++) {
    int len = sendMsg(filep, getSNDataMsg, 100);
    if(len != 8) {
      fprintf(stderr,"DATA_ACC_GET_SN_DATA: short write (%d bytes)\n", len);
      return 1;
    }
    len = getMsg(filep, msgReply, bufsiz, 1000);
    if(len < 8) {
      fprintf(stderr,"DATA_ACC_GET_SN_DATA: Questionable length on read: %d bytes!\n", len);
    }
    len = sendMsg(filep, getMoniMsg, 100);
    if(len != 8) {
      fprintf(stderr,"Get Moni Msg: short write (%d bytes)\n", len);
      return 1;
    }
    len = getMsg(filep, msgReply, bufsiz, 1000);
    if(len < 8) {
      fprintf(stderr,"Get Moni Msg: Questionable length on read: %d bytes!\n", len);
    }

    if(!(i%600)) fprintf(stderr,".");
  }
  OUT("\nDSC_DISABLE_SN");
  if((r=domsg(filep, bufsiz, 10000,
              DOM_SLOW_CONTROL, DSC_DISABLE_SN, "")) != 0) {
    fprintf(stderr,"DSC_ENABLE_SN failed: %d\n", r);
    return 1;
  }

  return 0;
}

int drainMoniMessages(int savemoni, int monifd, int filep, int bufsiz, int maxtries) {
  int r;
  int itry;
  int terminated = 0;
  for(itry=0; itry<maxtries; itry++) {
    unsigned char avail;
    if((r=domsg(filep, bufsiz, 10000,
		DATA_ACCESS, DATA_ACC_MONI_AVAIL, "+C", &avail)) != 0) {
      fprintf(stderr,"DATA_ACC_MONI_AVAIL failed: %d\n", r);
      return 1;
    }
    if(!avail) {
      terminated = 1; 
      break;
    }
    DOMMSG * moniMsg      = newMoniMsg();
    DOMMSG * msgReply     = newMsg();
    int len = sendMsg(filep, moniMsg, 1000);
    if(len < 8) {
      fprintf(stderr,"DATA_ACC_GET_NEXT_MONI_REC: short write (%d bytes)\n", len);
      return 1;
    }
    len = getMsg(filep, msgReply, bufsiz, 10000);
    if(len < 8) {
      fprintf(stderr,"DATA_ACC_GET_NEXT_MONI_REC: short read (%d bytes)\n", len);
      return 1;
    }
    if(savemoni) {
      int rmt    = msgType(msgReply);
      int rmst   = msgSubType(msgReply);
      int dlen   = msgDataLen(msgReply);
      int status = msgStatus(msgReply);
      if(rmt != DATA_ACCESS) { 
	fprintf(stderr,"drainMoniMessages: Bad message type (%d)\n", rmt);
	return 1;
      }
      if(rmst != DATA_ACC_GET_NEXT_MONI_REC) {
        fprintf(stderr,"drainMoniMessages: Bad message subtype (%d)\n", rmst);
        return 1;
      }
      if(status != 1) {
	fprintf(stderr,"drainMoniMessages: Bad message status (%d)\n", status);
	return 1;
      }
      if(dlen < 1) {
	fprintf(stderr,"drainMoniMessages: Bad message length (%d), "
		"should have monitoring data!\n", dlen);
	return 1;
      }
      int nm = write(monifd, msgReply->data, dlen);
      if(nm != dlen) {
	fprintf(stderr,"Short write (%d of %d bytes) to monitoring stream.\n",
		nm, dlen); 
	return 1;
      }
    }
  }
  if(!terminated) {
    fprintf(stderr,"drainMoniMessages: never got empty moni buffer!!!\n");
    return 1;
  }
  return 0;
}
