#!/usr/bin/perl

# Simple script which puts newly powered-on DOM into the
# domapp state
#
# jacobsen@npxdesigns.com
# $Id: uda.pl,v 1.2 2005/06/23 16:03:08 jacobsen Exp $

use strict;
use Getopt::Long;

sub sendstr { my $s = shift; return "send \"$s\"\n"; }
sub expect  { my $e = shift; return "expect \"$e\"\n"; }
sub getprompt;
sub sendfile;
sub execit;
sub gunzip;
$|++;

my $O = (split '/', $0)[-1];

sub usage { return <<EOF;
Usage: $O [options] CWD <file>
                    CWD is e.g. 01A or 11b

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

print "$O by jacobsen\@npxdesigns.com for LBNL/IceCube...\n" unless $quiet;

die usage if $help;

my $sz = "/usr/bin/sz";
die "No $sz!\n" unless -x $sz;

my $dom = shift;
die usage unless defined $dom;
die usage unless $dom =~ /(\d)(\d)(\S)/;
die usage unless ($3 eq 'a' || $3 eq 'A' || $3 eq 'b' || $3 eq 'B');

my $file = shift;
die usage unless defined $file;
die "Can't find $file!\n" unless -e $file;

my $dt = "/usr/local/bin/domterm";
my $se = "/usr/local/bin/se";
die "Need $dt -- get domhub-tools?\n" unless -x $dt;
die "Need $se -- get domhub-tools?\n" unless -x $se;

my $tmpin = ".uda.in.$$";
my $tmpout = ".uda.out.$$";

sendfile($file) && die "File transfer failed!\n";
getprompt() && die "Didn't get prompt after file transfer!\n";
exit if defined $uploadonly;
if(! $nogunzip) {
    gunzip() && die "Gunzip failed!\n";
}

if(defined $flash) {
    flashit($flash) && die "Flash failed!\n";
} else {
    execit() && die "Exec failed!\n";
}

print "SUCCESS\n";
exit;

sub docmd {
    my $cmd = shift; die unless defined $cmd;
    open SE, ">$tmpin";
    print SE sendstr "$cmd\r";
    close SE;
    system "$se $dom < $tmpin 2>$tmpout 1>$tmpout";
    my $result = `cat $tmpout`; unlink $tmpout || die "Can't unlink $tmpout: $!\n";
    return $result;
}

sub execit {
    print "Exec... ";
    my $result = docmd "exec";
# FIXME: more intelligent error checking
    if($result !~ /error/i) {
	print "OK.\n";
	return 0;
    } else {
	print "Exec failed!\n$result\n";
	return 1;
    }
}

sub flashit {
    my $file = shift; die unless defined $file;
    print "Create $file... ";
    my $result = docmd "s\\\" $file\\\" create";
# FIXME -- need better error handling here?
    print "OK.\n";
    return 0;
}

sub gunzip {
    print "Gunzip... ";
    my $result = docmd "gunzip";
# FIXME: more intelligent error checking
    if($result !~ /invalid/i) {
	print "OK.\n";
	return 0;
    } else {
	print "Bad gunzip!!!\n$result";
	return 1;
    }
}

sub sendfile {
    my $file = shift;
    print "Upload $file... ";
    open SE, ">$tmpin";
    print SE sendstr "ymodem1k\r";
    print SE expect  "CCC";
    print SE sendstr "^A";
    print SE expect  "^domterm> ";
    print SE sendstr "$sz -k --ymodem $file\r";
    print SE expect  "^domterm> ";
    close SE;
    system "$se $dom < $tmpin 2>$tmpout 1>$tmpout";
    my $result = `cat $tmpout`; unlink $tmpout || die "Can't unlink $tmpout: $!\n";
    if($result =~ /Transfer complete/) {
	print "OK.\n";
	return 0;
    } else {
	print "Bad file transfer!!!\n$result";
	return 1;
    }
}

sub getprompt {
    print "Check for prompt... ";
    my $result = docmd "";
    if($result =~ /> /) {
	print "OK.\n";
	return 0;
    } else {
	print "Didn't get prompt!\n$result";
	return 1;
    }
}
