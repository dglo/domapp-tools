#!/bin/bash

 DOM_HAL_DAC_ATWD0_TRIGGER_BIAS=0
     DOM_HAL_DAC_ATWD0_RAMP_TOP=1
    DOM_HAL_DAC_ATWD0_RAMP_RATE=2
    DOM_HAL_DAC_ATWD_ANALOG_REF=3
 DOM_HAL_DAC_ATWD1_TRIGGER_BIAS=4
     DOM_HAL_DAC_ATWD1_RAMP_TOP=5
    DOM_HAL_DAC_ATWD1_RAMP_RATE=6
    DOM_HAL_DAC_PMT_FE_PEDESTAL=7
DOM_HAL_DAC_MULTIPLE_SPE_THRESH=8
  DOM_HAL_DAC_SINGLE_SPE_THRESH=9
      DOM_HAL_DAC_FAST_ADC_REF=10
    DOM_HAL_DAC_INTERNAL_PULSE=11
    DOM_HAL_DAC_LED_BRIGHTNESS=12
DOM_HAL_DAC_FE_AMP_LOWER_CLAMP=13
            DOM_HAL_DAC_FL_REF=14
          DOM_HAL_DAC_MUX_BIAS=15

    DRIVER=/home/jacobsen/icecube/work/dor-driver/driver
    DOMAPP=/home/jacobsen/icecube/work/rev4/testdomapp
DOMAPPTEST=$DRIVER/domapptest
DECOMPRESS=$DOMAPP/resources/swCompression/decompress
 DECODEENG=$DRIVER/decodeeng
DECODEMONI="$DRIVER/decodemoni -v"
  TIMEDIFF=$DOMAPP/dt
      CARD=0
      PAIR=0
        AB=a
       DOM=$CARD$PAIR$AB
  DURATION=10
    let "HWFREQ=$DURATION-1"
    let "CFFREQ=$DURATION-1"
  MONIFREQ=1
   HITFREQ=10
SIMSPETRIG=0
   CPUTRIG=1
  DISCTRIG=2
     NSAMP=128,128,128,128 
      WIDS=2,2,2,2
      NADC=255
     LCWIN=100
 WHICHATWD=0
     THCH0=20
     THCH1=50
     THCH2=50
     THCH3=50
     THADC=50
        HV=1311
  RGTHRESH=$THCH0,$THCH1,$THCH2,$THCH3,$THADC
 SPETHRESH=510 # Kael says 560-575
   SETDACS="-S $DOM_HAL_DAC_ATWD0_TRIGGER_BIAS,850 \
            -S $DOM_HAL_DAC_ATWD1_TRIGGER_BIAS,850 \
            -S $DOM_HAL_DAC_ATWD0_RAMP_RATE,600    \
            -S $DOM_HAL_DAC_ATWD1_RAMP_RATE,600    \
            -S $DOM_HAL_DAC_ATWD0_RAMP_TOP,2097    \
            -S $DOM_HAL_DAC_ATWD1_RAMP_TOP,2097    \
            -S $DOM_HAL_DAC_ATWD_ANALOG_REF,2048   \
            -S $DOM_HAL_DAC_PMT_FE_PEDESTAL,1925   \
            -S $DOM_HAL_DAC_FAST_ADC_REF,700       \
            -S $DOM_HAL_DAC_SINGLE_SPE_THRESH,$SPETHRESH\
            -S $DOM_HAL_DAC_MULTIPLE_SPE_THRESH,600"
# output files
MONSPECOMP=moni_spe_comp.out
MONSSPCOMP=moni_ssp_comp.out
MONCPUCOMP=moni_cpu_comp.out
MONSPENCMP=moni_spe_ncmp.out
MONSSPNCMP=moni_ssp_ncmp.out
MONCPUNCMP=moni_cpu_ncmp.out
HITSPECOMP=hits_spe_comp.out
HITSSPCOMP=hits_ssp_comp.out
HITCPUCOMP=hits_cpu_comp.out
  HITSPECZ=hits_spe.cz
  HITSSPCZ=hits_ssp.cz
  HITCPUCZ=hits_cpu.cz
HITSPENCMP=hits_spe_ncmp.out
HITSSPNCMP=hits_ssp_ncmp.out
HITCPUNCMP=hits_cpu_ncmp.out
 DTSPECOMP=$DOMAPP/dt_spe_comp.out
 DTSSPCOMP=$DOMAPP/dt_ssp_comp.out
 DTCPUCOMP=$DOMAPP/dt_cpu_comp.out
 DTSPENCMP=$DOMAPP/dt_spe_ncmp.out
 DTSSPNCMP=$DOMAPP/dt_ssp_ncmp.out
 DTCPUNCMP=$DOMAPP/dt_cpu_ncmp.out

sb.pl $DOM # Softboot DOM
$driver/upload_domapp.pl $CARD $PAIR $AB $domws/epxa10/bin/domapp.bin.gz

DOSPENCMP="$DOMAPPTEST $DOM -V -d $DURATION -M $MONIFREQ -H $HITFREQ   -m $MONSPENCMP   \
                         -w $HWFREQ      -f $CFFREQ      -I $LCWIN,$LCWIN,$LCWIN,$LCWIN \
                         -i $HITSPENCMP  -T $DISCTRIG    -A $WHICHATWD -N $NSAMP        \
                         -W $WIDS        -F $NADC        $SETDACS      -B               \
                         -L $HV        "
echo "Command is: $DOSPENCMP"
echo -n "About to set HV to $HV [control-C to exit]... "
read YN
$DOSPENCMP || exit -1
echo "Monitor data, real SPE triggers, compressed:"
$DECODEMONI $MONSPENCMP

echo "Time difference data, real SPE triggers, compressed: "
$DECODEENG $HITSPENCMP| $TIMEDIFF > $DTSPENCMP

exit

DOSPECOMP="$DOMAPPTEST $DOM -V -d $DURATION -M $MONIFREQ -H $HITFREQ   -m $MONSPECOMP   \
                         -w $HWFREQ      -f $CFFREQ      -I $LCWIN,$LCWIN,$LCWIN,$LCWIN \
                         -i $HITSPECZ    -T $DISCTRIG    -A $WHICHATWD -N $NSAMP        \
                         -W $WIDS        -F $NADC        $SETDACS      -B               \
                         -L $HV          -R $RGTHRESH    -O "
echo "Command is: $DOSPECOMP"
$DOSPECOMP || exit -1

echo "Monitor data, real SPE triggers, compressed:"
$DECODEMONI $MONSPECOMP
echo "Decompressing compressed, real SPE triggers... "
$DECOMPRESS $HITSPECZ $HITSPECOMP

echo "Time difference data, real SPE triggers, compressed: "
$DECODEENG $HITSPECOMP | $TIMEDIFF > $DTSPECOMP

exit

sb.pl $DOM # Softboot DOM
$driver/upload_domapp.pl $CARD $PAIR $AB $domws/epxa10/bin/domapp.bin.gz
    
# Run w/ compression, cpu triggers:
DOCPUCOMP="$DOMAPPTEST $DOM -V -d $DURATION -M $MONIFREQ -H $HITFREQ   -m $MONCPUCOMP  \
                         -i $HITCPUCZ    -T $CPUTRIG  -A $WHICHATWD -N $NSAMP       \
                         -W $WIDS        -F $NADC        $SETDACS      -B           \
                         -R $RGTHRESH    -O "
echo "Command is: $DOCPUCOMP"
$DOCPUCOMP 

sb.pl $DOM # Softboot DOM
$driver/upload_domapp.pl $CARD $PAIR $AB $domws/epxa10/bin/domapp.bin.gz

# Run w/o compression, cpu triggers:
DOCPUNCMP="$DOMAPPTEST $DOM -V -d $DURATION -M $MONIFREQ -H $HITFREQ   -m $MONCPUNCMP  \
                         -i $HITCPUNCMP  -T $CPUTRIG  -A $WHICHATWD -N $NSAMP       \
                         -W $WIDS        -F $NADC        $SETDACS      -B           \
                         -R $RGTHRESH "
echo "Command is: $DOCPUNCMP"
$DOCPUNCMP

sb.pl $DOM # Softboot DOM
$driver/upload_domapp.pl $CARD $PAIR $AB $domws/epxa10/bin/domapp.bin.gz

# Run w/ compression, simulated SPEs:
DOSSPCOMP="$DOMAPPTEST $DOM -V -d $DURATION -M $MONIFREQ   -H $HITFREQ   -m $MONSSPCOMP  \
                         -i $HITSSPCZ    -T $SIMSPETRIG -A $WHICHATWD -N $NSAMP       \
                         -W $WIDS        -F $NADC        $SETDACS     -B              \
                         -R $RGTHRESH    -O "

echo "Command is: $DOSSPCOMP"
$DOSSPCOMP

sb.pl $DOM # Softboot DOM
$driver/upload_domapp.pl $CARD $PAIR $AB $domws/epxa10/bin/domapp.bin.gz

# Run w/o compression, simulated SPEs:
DOSSPNCMP="$DOMAPPTEST $DOM -V -d $DURATION -M $MONIFREQ   -H $HITFREQ   -m $MONSSPNCMP  \
                         -i $HITSSPNCMP  -T $SIMSPETRIG -A $WHICHATWD -N $NSAMP       \
                         -W $WIDS        -F $NADC        $SETDACS     -B              \
                         -R $RGTHRESH "
echo "Command is: $DOSSPNCMP"
$DOSSPNCMP

echo "Monitor data, CPU triggers, compressed:"
$DECODEMONI $MONCPUCOMP
echo "Monitor data, CPU triggers, uncompressed:"
$DECODEMONI $MONCPUNCMP
echo "Monitor data, 'SSP' triggers, compressed:"
$DECODEMONI $MONSSPCOMP
echo "Monitor data, 'SSP' triggers, uncompressed:"
$DECODEMONI $MONSSPNCMP

echo "Decompressing compressed CPU triggers... "
$DECOMPRESS $HITCPUCZ $HITCPUCOMP
echo "Decompressing compressed 'SSP' triggers... "
$DECOMPRESS $HITSSPCZ $HITSSPCOMP

echo "Time difference data, CPU triggers, compressed: "
$DECODEENG $HITCPUCOMP | $TIMEDIFF > $DTCPUCOMP
echo "Time difference data, CPU triggers, uncompressed: "
$DECODEENG $HITCPUNCMP | $TIMEDIFF > $DTCPUNCMP
echo "Time difference data, 'SSP' triggers, compressed: "
$DECODEENG $HITSSPCOMP | $TIMEDIFF > $DTSSPCOMP
echo "Time difference data, 'SSP' triggers, uncompressed: "
$DECODEENG $HITSSPNCMP | $TIMEDIFF > $DTSSPNCMP

echo "Resulting files: "
LS="ls -alt "
$LS $HITCPUCZ
$LS $HITCPUCOMP
$LS $HITCPUNCMP
$LS $HITSSPCZ
$LS $HITSSPCOMP
$LS $HITSSPNCMP


