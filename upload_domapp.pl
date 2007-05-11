#!/usr/bin/perl

# Simple script which puts newly powered-on DOM into the
# domapp state
#
# John Jacobsen, John Jacobsen IT Services, for LBNL/IceCube
# Dec. 2003
# $Id: upload_domapp.pl,v 1.8 2005/06/23 16:02:00 jacobsen Exp $

use Fcntl;
use strict;
use IO::Socket;
use IO::Select;
use Getopt::Long;
my $verbose = 0;
my $quiet   = 0;
$|++;

sub killdomserv;    sub domserv_command;   sub waitForDomserv;
sub drain_iceboot;  
sub pause { select undef,undef,undef,0.3; };

sub usage { return <<EOF;
Usage: $0 [-f name] <card> <pair> <dom> <file>
	            <dom> is A or B.
          -f option writes flash using name <name>
	  -n option skips gunzip command
          -u option quits immediately after upload
	  -q option runs more quietly
EOF
;
	}

my $flash;
my $help;
my $uploadonly;
my $nogunzip;
my $quiet;
GetOptions("flash=s"    => \$flash,
	   "upload|u"   => \$uploadonly,
	   "nogunzip|n" => \$nogunzip,
	   "quiet|q"    => \$quiet,
	   "help|h"     => \$help) || die usage;

my $O = (split '/', $0)[-1];
print "$O by jacobsen\@npxdesigns.com for LBNL/IceCube...\n" unless $quiet;

die usage if $help;
# Check for domserv & sz...

my $domserv = "/usr/local/bin/domserv";
die "No $domserv!\n" unless -x $domserv;

my $sz = "/usr/bin/sz";
die "No $sz!\n" unless -x $sz;

# Get DOM address...

my $card = shift;
my $pair = shift;
my $dom  = shift;
my $file = shift;
die usage unless defined $card && defined $pair && defined $dom && defined $file;
die "Can't find file $file to upload.\n" unless -e $file;
$dom =~ tr/[a-z]/[A-Z]/;
die usage unless $dom eq "A" || $dom eq "B";
print "$file -> $card $pair $dom...\n" unless $quiet;

my $port = 4001 + $card * 8 + $pair * 2 + ($dom eq 'A' ? 0 : 1);
my $szport = $port + 500;
my $szargs = "-q --ymodem -k --tcp-client localhost:$szport $file";

print "Talking port $port, SZ port $szport.\n" unless $quiet;
# Start domserv...

my $ppid = $$;

my $domservPID = fork;
die "Can't fork: $!\n" unless defined $domservPID;

sub dslog { my $pid = shift; return ".domservpid.$pid"; }

if($domservPID == 0) {
    my $domserv_cmd = "$domserv -dh 2>&1";
    print "EXEC($domserv_cmd)\n" unless $quiet;
    my $pid = open DS, "|$domserv_cmd";
    print "domserv pid $pid\n" unless $quiet;
    my $pidlog = dslog($ppid);
    open(P,">$pidlog") || die "Can't open $pidlog: $!\n";
    print P "$pid\n";
    close P;
    print "$pidlog contains $pid.\n" unless $quiet;

    my $domsrvinput = "open dom $card$pair$dom";
    print DS "$domsrvinput\n";
    print "DOMSERV($$,$domsrvinput)\n" unless $quiet;
    close DS;
    sleep 10000;
    # Domserv waits at this point
    exit; # Never happens
}

# After this point pid is our child process (domserv).
# connect and upload here

$SIG{INT} = $SIG{KILL} = sub { killdomserv $domservPID, $ppid; };

waitForDomserv($ppid);

my $socket = undef;
sub opensock {
    for(1..20) {
	$socket = new IO::Socket::INET(PeerAddr   => "localhost",
				       PeerPort   => $port,
				       Proto      => 'tcp',
				       Blocking   => 1
				       );
        return if defined $socket;
	select undef, undef, undef, 0.5;
    }
    die "FAIL: Can't open socket to port $port: $!\n";
}

opensock; 
print "Got socket connection to $port.\n" unless $quiet;

syswrite $socket, "\r\rymodem1k\r";

if(domserv_command(undef, "CCC")) {
    warn "FAIL: ymodem1k didn't give expected 'CCC...' pattern (wrong DOM state?)\n";
    killdomserv $domservPID, $ppid;
    exit;
}

close($socket);

my $cmd      = "$sz $szargs 2>&1\n";  
print $cmd unless $quiet;
my $szhappy = 0;
for(1..10) { # Keep trying until sz is happy
    my $szresult = `$cmd`;    
    if($szresult =~ /refused/i) {
	select undef, undef, undef, 0.3;
    } else {
	$szhappy = 1;
	last;
    }
}

if(! $szhappy) {
    killdomserv $domservPID, $ppid;
    die "SZ never finished successfully!\n";
}

opensock;

if(domserv_command(".s")) {
    killdomserv $domservPID, $ppid;
    die "FAIL: domserv_command failed (.s).\n";
}

if(!defined $uploadonly) {
    if(! $nogunzip && domserv_command("gunzip", "gunzip.+?>")) {
	killdomserv $domservPID, $ppid;
	die "FAIL: domserv_command failed (gunzip).\n";
    }
    
    if(defined $flash) {
	if(domserv_command("s\" $flash\" create","create.+?>")) {
	    killdomserv $domservPID, $ppid;
	    die "FAIL: domserv_command failed (write flash).\n";
	}
    } else {
	if(domserv_command("exec","exec")) {
	    killdomserv $domservPID, $ppid;
	    die "FAIL: domserv_command failed (exec).\n";
	}
    }
}

print "\n" unless $quiet;

close $socket;
sleep 1; # Make sure last message gets down to DOM
killdomserv $domservPID, $ppid;

my $version = `domapptest -V -O $card$pair$dom 2>&1`;
print "DOMApp version: $version";

print "SUCCESS\n";
exit;

sub printable {
    my $b = shift;
    return 1 if ord($b) > 31 && ord($b) < 127;
    return 0;
}

sub domserv_command {
    my $cmd      = shift; 
    my $expect   = shift;

    if(!defined $socket) {
	warn "FAIL: No socket to port $port :-- $!\n";
	return 1;
    }
    if(defined $cmd) {
	syswrite $socket, "\r\r$cmd\r";
    }
    if(!defined $expect) {
	return 0;
    }
    my $sel = new IO::Select($socket);
    my $buf;
    my $totbuf    = "";
    my $gotexpect = 0;
    my $maxtries  = 20;
    my $itries    = 0;
    while(1) {
	if(!$sel->can_read(0.5)) {
	    $itries++;
	    if($itries >= $maxtries) {
		print "TIMEOUT!\n";
		last;
	    }
	    next;
	}
	$itries = 0;
	my $read = sysread($socket, $buf, 1);
	return 1 unless $read > 0;
	
	$totbuf .= $buf;
	print " " if !$quiet && $buf eq "\r";
	print $buf if !$quiet && printable($buf);
	if($totbuf =~ /$expect/s) {
	    # print "Got EXPECT($expect)!\n";
	    $gotexpect = 1;
	    last;
	}
    }
    return 1 unless $gotexpect;
    return 0;
}

sub killdomserv {
    my $pid = shift;
    my $ppid = shift;
    die unless defined $pid;
    die unless defined $ppid;
    print "killing $pid.\n" unless $quiet;
    kill 9, $pid;
    my $dspidfile = dslog($ppid);
    my $dspid = `cat $dspidfile`; chomp $dspid;
    if($dspid =~ /(\d+)/) {
	print "killing $dspid.\n" unless $quiet;
	kill 9, $dspid;
    }
}

sub waitForDomserv {
    sleep 3;
    my $pid = shift;
    for(1..30) {
	my $dsfile = dslog($pid);
	return if -f $dsfile && `cat $dsfile` =~ /\d+/;
	select undef, undef, undef, 0.33;
    }
    sleep 1;
    die "Never got domserv process file!\n";
}
