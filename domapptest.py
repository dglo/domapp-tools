#!/usr/bin/env python

# domapptest.py
# John Jacobsen, NPX Designs, Inc., jacobsen\@npxdesigns.com
# Started: Wed May  9 21:57:21 2007

import time, threading, os, sys
import monitoring
import os.path
from dor import Driver
from re import search
from domapp import *


def SNClockOk(clock, prevClock, bins, prevBins):
    DT = 65536
    if clock != prevClock + prevBins*DT: return False
    return True

class MiniDor:
    def __init__(self, card=0, wire=0, dom='A'):
        self.card = card; self.wire=wire; self.dom=dom
    def dompath(self):
        return os.path.join("/", "proc", "driver", "domhub",
                            "card%d" % self.card,
                            "pair%d" % self.wire,
                            "dom%s"  % self.dom)
    
    def softboot(self):
        f = file(os.path.join(self.dompath(), "softboot"),"w")
        f.write("reset\n")
        f.close()
        
    def isInIceboot(self): return False
    
class DOMAppTest:
    def __init__(self, card=0, wire=0, dom='A'):
        self.domapp    = DOMApp(card,wire,dom)
        self.dor       = MiniDor(card,wire,dom)
        self.runLength = 10
        self.debugMsgs = []

    def getDebugTxt(self):
        if self.debugMsgs: return "\n\t".join(self.debugMsgs)
        else:              return ""
    
    def setDAC(self, dac, val): self.domapp.writeDAC(dac, val)
    def setRunLength(self, l): self.runLength = l
    
    def setDefaultDACs(self):
        self.setDAC(DAC_ATWD0_TRIGGER_BIAS, 850)
        self.setDAC(DAC_ATWD1_TRIGGER_BIAS, 850)
        self.setDAC(DAC_ATWD0_RAMP_RATE, 350)
        self.setDAC(DAC_ATWD1_RAMP_RATE, 350)
        self.setDAC(DAC_ATWD0_RAMP_TOP, 2300)
        self.setDAC(DAC_ATWD1_RAMP_TOP, 2300)
        self.setDAC(DAC_ATWD_ANALOG_REF, 2250)
        self.setDAC(DAC_PMT_FE_PEDESTAL, 2130)
        self.setDAC(DAC_SINGLE_SPE_THRESH, 560)
        self.setDAC(DAC_MULTIPLE_SPE_THRESH, 650)
        self.setDAC(DAC_FADC_REF, 800)
        self.setDAC(DAC_INTERNAL_PULSER_AMP, 80)

    def getTests(self):
        methods = dir(self)
        ret = []
        for m in methods:
            if search(r'^Test', m): ret.append(m)
        return ret

    def TOAtSoftbootToIceboot(self):
        self.dor.softboot()
        if not self.dor.isInIceboot():
            return "FAIL - not in iceboot"
        return "PASS"
    
    def TOAstIcebootToDomapp(self):
        return "FAIL -- didn't get domapp message"
    
    def TestSN(self):
        self.setDefaultDACs()
        self.domapp.setTriggerMode(2)
        self.domapp.setPulser(mode=FE_PULSER, rate=100)
        self.domapp.selectMUX(255)
        self.domapp.setEngFormat(0, 4*(2,), (32, 0, 0, 0))
        self.domapp.resetMonitorBuffer()
        self.domapp.enableSN(6400, 0)
        self.domapp.setMonitoringIntervals()
        self.domapp.startRun()
        prevBins, prevClock = None, None

        ret = "PASS"

        for i in xrange(0,self.runLength):

            # Fetch monitoring
            monidata = self.domapp.getMonitorData()
            while monidata and len(monidata)>=4:
                moniLen, moniType = unpack('>hh', monidata[0:4])
                # print "%d %d" % (moniLen, moniType)
                if moniType == 0xCB:
                    msg = monidata[10:moniLen]
                    self.debugMsgs.append(msg)
                monidata = monidata[moniLen:]
                
            # Fetch supernova
            sndata = self.domapp.getSupernovaData()
            if sndata      == None: continue
            if len(sndata) == 0:    continue
            if len(sndata) < 10:
                ret = "FAIL -- SN DATA CHECK: %d bytes" % len(sndata)
                break
            bytes, fmtid, t5, t4, t3, t2, t1, t0 = unpack('>hh6B', sndata[0:10])
            clock  = ((((t5 << 8L | t4) << 8L | t3) << 8L | t2) << 8L | t1) << 8L | t0
            scalers = unpack('%dB' % (len(sndata) - 10), sndata[10:])
            bins    = len(scalers)

            if prevBins and not SNClockOk(clock, prevClock, bins, prevBins):
                ret = "FAIL -- CLOCK CHECK: %d %d %d %d->%d %x->%x" % (i, bytes, fmtid, prevBins,
                                                                       bins, prevClock, clock)
                break
            
            prevClock = clock
            prevBins  = bins

            try:
                time.sleep(1)
            except:
                self.domapp.endRun()
                raise SystemExit
            
        self.domapp.endRun()
        return ret

class TestSet:
    def __init__(self, domDict):
        self.domDict   = domDict
        self.threads   = {}
        self.numpassed = 0
        self.numfailed = 0
        self.numtests  = 0
        
    def doAllTests(self, domid, c, w, d):
        r = DOMAppTest(c, w, d)
        r.setRunLength(10)
        tests = r.getTests()
        tests.reverse()
        for test in tests:
            status = eval("r.%s()" % test)
            print "%s%s%s %s: %s" % (c,w,d, test, status)
            if status == "PASS":
                self.numpassed += 1
            else:
                self.numfailed += 1
                dbg = r.getDebugTxt()
                if len(dbg) > 0: print r.getDebugTxt()
                
            self.numtests += 1
            # print "%d %d %d" % (self.numpassed, self.numfailed, self.numtests)

    def runThread(self, domid):
        c, w, d = self.domDict[domid]
        print "Running thread for %s: %s %s %s" % (domid, c, w, d)
        self.doAllTests(domid, c,w,d)
        
    def go(self): 
        for dom in self.domDict:
            # print dom, self.domDict[dom]
            self.threads[dom] = threading.Thread(target=self.runThread, args=(dom, ))
            self.threads[dom].start()
        for dom in self.domDict: self.threads[dom].join()
        
    def summary(self):
        "show summary of results"
        return "Passed tests: %d   Failed tests: %d   Total: %d" % (self.numpassed,
                                                                    self.numfailed,
                                                                    self.numtests)
                                                                    
def main():
    dor = Driver()
    domDict = dor.get_active_doms()
    tests = TestSet(domDict)
    tests.go()
    print tests.summary()
    
    raise SystemExit
        
if __name__ == "__main__": main()
