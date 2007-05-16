#!/usr/bin/env python

# domapptest.py
# John Jacobsen, NPX Designs, Inc., jacobsen\@npxdesigns.com
# Started: Wed May  9 21:57:21 2007

from __future__ import generators
import time, threading, os, sys
from exc_string import *
import monitoring
import os.path
from dor import Driver
from re import search
from domapp import *

def SNClockOk(clock, prevClock, bins, prevBins):
    DT = 65536
    if clock != prevClock + prevBins*DT: return False
    return True

class ExpectStringNotFoundException(Exception): pass

class MiniDor:
    def __init__(self, card=0, wire=0, dom='A'):
        self.card = card; self.wire=wire; self.dom=dom
        self.devFileName = os.path.join("/", "dev", "dhc%dw%dd%s" % (self.card, self.wire, self.dom))
        self.expectTimeout = 5000
        self.blockSize     = 4092
        self.domapp        = None
        
    def open(self):
        self.fd = os.open(self.devFileName, os.O_RDWR)
    
    def close(self):
        os.close(self.fd)
    
    def dompath(self):
        return os.path.join("/", "proc", "driver", "domhub",
                            "card%d" % self.card,
                            "pair%d" % self.wire,
                            "dom%s"  % self.dom)
    
    def softboot(self):
        f = file(os.path.join(self.dompath(), "softboot"),"w")
        f.write("reset\n")
        f.close()

    def readExpect(self, file, expectStr, timeoutMsec=5000):
        "Read from dev file until expected string arrives - throw exception if it doesn't"
        contents = ""
        start    = time.clock()
        dt       = timeoutMsec/1000.
        while time.clock()-start < timeoutMsec:
            contents += os.read(self.fd, self.blockSize)
            if search(expectStr, contents):
                # break #<-- put this back to simulate failure
                return True
            time.sleep(0.10)
        raise ExpectStringNotFoundException("Expected string '%s' did not arrive in %d msec:\n%s" \
                                            % (expectStr, timeoutMsec, contents))

    def se(self, send, recv, timeout):
        "Send text, wait for recv text in timeout msec"
        try:
            os.write(self.fd, send)
            self.readExpect(self.fd, recv, timeout)
        except Exception, e:
            return (False, exc_string)
        return (True, "")
    
    def isInIceboot(self):         return self.se("\r\n", ">", 1000)
    def isInConfigboot(self):      return self.se("\r\n", "#", 1000)
    def configbootToIceboot(self): return self.se("r",    ">", 5000)
    def icebootToConfigboot(self): return self.se("boot-serial reboot\r\n", "#", 5000)            
    def icebootToDomapp(self):
        ok, txt = self.se("domapp\r\n", "domapp", 1000)
        if ok: time.sleep(3)
        return (ok, txt)
    
STATE_ICEBOOT    = "ib"
STATE_DOMAPP     = "da"
STATE_CONFIGBOOT = "cb"
STATE_ECHO       = "em"
STATE_UNKNOWN    = "??"

class DOMTest:
    def __init__(self, card, wire, dom, dor, start=STATE_ICEBOOT, end=STATE_ICEBOOT):
        self.card       = card
        self.wire       = wire
        self.dom        = dom
        self.dor        = dor
        self.startState = start
        self.endState   = end

        self.runLength  = 10
        self.debugMsgs  = []
        self.result     = None
        self.summary    = ""
        
    def setRunLength(self, l): self.runLength = l

    def getDebugTxt(self):
        if self.debugMsgs: return "\n\t".join(self.debugMsgs)
        else:              return ""

    def name(self):
        str = repr(self)
        m = search(r'\.(\S+) instance', str)
        if(m): return m.group(1)
        return str
    
    def run(self, fd): pass
    
class ConfigbootToIceboot(DOMTest):
    def __init__(self, card, wire, dom, dor):
        DOMTest.__init__(self, card, wire, dom, dor, start=STATE_CONFIGBOOT, end=STATE_ICEBOOT)
    def run(self, fd):
        ok, txt = self.dor.configbootToIceboot()
        if not ok:
            self.result = "FAIL - could not transition into iceboot"
            self.debugMsgs.append(txt)
        else:
            ok, txt = self.dor.isInIceboot()
            if not ok:
                self.result = "FAIL - check of iceboot prompt failed"
                self.debugMsgs.append(txt)
            else:
                self.result = "PASS"
                        
class DomappToIceboot(DOMTest):
    def __init__(self, card, wire, dom, dor):
        DOMTest.__init__(self, card, wire, dom, dor, start=STATE_DOMAPP, end=STATE_ICEBOOT)
    def run(self, fd):
        self.dor.softboot()
        ok, txt = self.dor.isInIceboot()
        if not ok:
            self.result = "FAIL - check of iceboot prompt failed"
            self.debugMsgs.append(txt)
        else:
            self.result = "PASS"

class IcebootToDomapp(DOMTest):
    def __init__(self, card, wire, dom, dor):
        DOMTest.__init__(self, card, wire, dom, dor, start=STATE_ICEBOOT, end=STATE_DOMAPP)
    def run(self, fd):
        ok, txt = self.dor.icebootToDomapp()
        if not ok:        
            self.result = "FAIL - could not transition into domapp"
            self.debugMsgs.append(txt)
        else:
            # FIXME - test w/ domapp message here
            self.result = "PASS"

class CheckIceboot(DOMTest):
    def __init__(self, card, wire, dom, dor):
        DOMTest.__init__(self, card, wire, dom, dor, start=STATE_ICEBOOT, end=STATE_ICEBOOT)
    def run(self, fd):
        ok, txt = self.dor.isInIceboot()
        if not ok:
            self.result = "FAIL - check of iceboot prompt failed"
            self.debugMsgs.append(txt)
        else:
            self.result = "PASS"
            
class IcebootToConfigboot(DOMTest):
    def __init__(self, card, wire, dom, dor):
        DOMTest.__init__(self, card, wire, dom, dor, start=STATE_ICEBOOT, end=STATE_CONFIGBOOT)
    def run(self, fd):
        ok, txt = self.dor.icebootToConfigboot()
        if not ok:
            self.result = "FAIL - could not transition into configboot"
            self.debugMsgs.append(txt)
        else:
            ok, txt =  self.dor.isInConfigboot()
            if not ok:
                self.result = "FAIL - check of configboot prompt failed"
                self.debugMsgs.append(txt)
            else:
                self.result = "PASS"

class CheckConfigboot(DOMTest):
    def __init__(self, card, wire, dom, dor):
        DOMTest.__init__(self, card, wire, dom, dor, start=STATE_CONFIGBOOT, end=STATE_CONFIGBOOT)
    def run(self, fd):
        ok, txt = self.dor.isInConfigboot()
        if not ok:
            self.result = "FAIL - check of configboot prompt failed"
            self.debugMsgs.append(txt)
        else:
            self.result = "PASS"

class GetDomappRelease(DOMTest):
    def __init__(self, card, wire, dom, dor):
        DOMTest.__init__(self, card, wire, dom, dor, start=STATE_DOMAPP, end=STATE_DOMAPP)
    def run(self, fd):
        domapp = DOMApp(self.card, self.wire, self.dom, fd)
        try:
            self.summary = domapp.getDomappVersion()
            self.result = "PASS"
        except Exception, e:
            self.result = "FAIL"
            self.debugMsgs.append(exc_string())

class SNTest(DOMTest):
    def __init__(self, card, wire, dom, dor):
        DOMTest.__init__(self, card, wire, dom, dor, start=STATE_DOMAPP, end=STATE_DOMAPP)

    def setDAC(self, domapp, dac, val): domapp.writeDAC(dac, val)

    def setDefaultDACs(self, domapp):
        self.setDAC(domapp, DAC_ATWD0_TRIGGER_BIAS, 850)
        self.setDAC(domapp, DAC_ATWD1_TRIGGER_BIAS, 850)
        self.setDAC(domapp, DAC_ATWD0_RAMP_RATE, 350)
        self.setDAC(domapp, DAC_ATWD1_RAMP_RATE, 350)
        self.setDAC(domapp, DAC_ATWD0_RAMP_TOP, 2300)
        self.setDAC(domapp, DAC_ATWD1_RAMP_TOP, 2300)
        self.setDAC(domapp, DAC_ATWD_ANALOG_REF, 2250)
        self.setDAC(domapp, DAC_PMT_FE_PEDESTAL, 2130)
        self.setDAC(domapp, DAC_SINGLE_SPE_THRESH, 560)
        self.setDAC(domapp, DAC_MULTIPLE_SPE_THRESH, 650)
        self.setDAC(domapp, DAC_FADC_REF, 800)
        self.setDAC(domapp, DAC_INTERNAL_PULSER_AMP, 80)

    def run(self, fd):
        domapp = DOMApp(self.card, self.wire, self.dom, fd)
        self.result = "PASS"
        try:
            self.setDefaultDACs(domapp)
            domapp.setTriggerMode(2)
            domapp.setPulser(mode=FE_PULSER, rate=100)
            domapp.selectMUX(255)
            domapp.setEngFormat(0, 4*(2,), (32, 0, 0, 0))
            domapp.resetMonitorBuffer()
            domapp.enableSN(6400, 0)
            domapp.setMonitoringIntervals()
            domapp.startRun()
        except Exception, e:
            self.result = "FAIL"
            self.debugMsgs.append(exc_string())
            return
            
        prevBins, prevClock = None, None

        for i in xrange(0,self.runLength):

            # Fetch monitoring
            try:
                monidata = domapp.getMonitorData()
            except Exception, e:
                self.result = "FAIL"
                self.debugMsgs.append("GET MONI DATA FAILED: %s" % exc_string())
                break
            
            while monidata and len(monidata)>=4:
                moniLen, moniType = unpack('>hh', monidata[0:4])
                if moniType == 0xCB:
                    msg = monidata[10:moniLen]
                    self.debugMsgs.append(msg)
                monidata = monidata[moniLen:]
                
            # Fetch supernova
            try:
                sndata = domapp.getSupernovaData()
            except Exception, e:
                self.result = "FAIL"
                self.debugMsgs.append("GET SN DATA FAILED: %s" % exc_string())
                break
            
            if sndata      == None: continue
            if len(sndata) == 0:    continue
            if len(sndata) < 10:
                self.result = "FAIL"
                self.debugMsgs.append("SN DATA CHECK: %d bytes" % len(sndata))
                break
            bytes, fmtid, t5, t4, t3, t2, t1, t0 = unpack('>hh6B', sndata[0:10])
            clock  = ((((t5 << 8L | t4) << 8L | t3) << 8L | t2) << 8L | t1) << 8L | t0
            scalers = unpack('%dB' % (len(sndata) - 10), sndata[10:])
            bins    = len(scalers)

            if prevBins and not SNClockOk(clock, prevClock, bins, prevBins):
                self.result = "FAIL"
                self.debugMsgs.append("CLOCK CHECK: %d %d %d %d->%d %x->%x" % (i, bytes, fmtid, prevBins,
                                                                               bins, prevClock, clock))
                break
            
            prevClock = clock
            prevBins  = bins

            try:
                time.sleep(1)
            except:
                try: domapp.endRun()
                except: pass
                raise SystemExit
            
        try:
            domapp.endRun()
        except Exception, e:
            self.result = "FAIL"
            self.debugMsgs.append("END RUN FAILED: %s" % exc_string())
                    

class TestSet2:
    def __init__(self, domDict, testNameList):
        self.domDict     = domDict
        self.testList    = testNameList
        self.threads     = {}
        self.numpassed   = 0
        self.numfailed   = 0
        self.numtests    = 0
        self.counterLock = threading.Lock()
        
    def cycle(self, testList, startState, c, w, d):
        state = startState
        doneDict = {}
        while True:
            allDone = True
            allDoneThisState = True
            nextTest = None
            nextStateChangeTest = None
            for test in testList:
                if (test not in doneDict or not doneDict[test]) and test.startState == state:
                    allDone = False
                    nextTest = test
                    if test.endState == state:
                        allDoneThisState = False
                        break
                    else:
                        nextStateChangeTest = test
            if allDone: return
            elif allDoneThisState:
                nextTest = nextStateChangeTest
            state = nextTest.endState
            doneDict[nextTest] = True
            yield nextTest

    def doAllTests(self, domid, c, w, d):
        startState = STATE_ICEBOOT
        testObjList = []
        dor = MiniDor(c, w, d)
        dor.open()
        for testName in self.testList:
            testObjList.append(testName(c, w, d, dor))
        for test in self.cycle(testObjList, startState, c, w, d):
            test.run(dor.fd)
            if(test.startState != test.endState): # If state change, flush buffers etc. to get clean IO
                dor.close()
                dor.open()
                    
            #### LOCK
            self.counterLock.acquire()
            print "%s%s%s %s->%s %s: %s %s" % (c,w,d, test.startState,
                                                     test.endState, test.name(), test.result, test.summary)
            if test.result == "PASS":
                self.numpassed += 1
            else:
                self.numfailed += 1
                dbg = test.getDebugTxt()
                if len(dbg) > 0: print test.getDebugTxt()
            self.numtests += 1
            self.counterLock.release()
            #### UNLOCK
            
    def runThread(self, domid):
        c, w, d = self.domDict[domid]
        # print "Running thread for %s: %s %s %s" % (domid, c, w, d)
        self.doAllTests(domid, c,w,d)
        
    def go(self): 
        for dom in self.domDict:
            # print dom, self.domDict[dom]
            self.threads[dom] = threading.Thread(target=self.runThread, args=(dom, ))
            self.threads[dom].start()
        for dom in self.domDict:
            self.threads[dom].join()
        
    def summary(self):
        "show summary of results"
        return "Passed tests: %d   Failed tests: %d   Total: %d" % (self.numpassed,
                                                                    self.numfailed,
                                                                    self.numtests)

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
            break
        for dom in self.domDict:
            self.threads[dom].join()
            break
        
    def summary(self):
        "show summary of results"
        # FIXME - make counters thread-safe (currently they seem racy)
        return "Passed tests: %d   Failed tests: %d   Total: %d" % (self.numpassed,
                                                                    self.numfailed,
                                                                    self.numtests)

def main():
    dor = Driver()
    domDict = dor.get_active_doms()
    
    startState = STATE_ICEBOOT
    
    testNameList = (IcebootToConfigboot, CheckIceboot, ConfigbootToIceboot, IcebootToDomapp, 
                    DomappToIceboot, CheckConfigboot, GetDomappRelease, SNTest)
    
    testSet = TestSet2(domDict, testNameList)
    testSet.go()
    print testSet.summary()
    
    raise SystemExit

if __name__ == "__main__": main()
