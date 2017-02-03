#!/usr/bin/perl -w

use strict;
$| = 1;             

# This program looks for Copyright notices in .c, .in, .h, .pl, and .jsf files
# in the current directory and updates them for the new year.
# Give it two parameters: from_year and to_year, each of which
# should be four digits.
#   It changes files with lines containing text like these:
#     Copyright (C) ####-$from_year
#     Copyright (C) $from_year
# to these:
#     Copyright (C) ####-$to_year
#     Copyright (C) $from_year-$to_year
#
# It should work on itself too! Behold:
#   Copyright (C) 2011-2017 Todd M. Lewis and Sebastiano Vigna
# Ciao!

use Getopt::Long;

my ($from_year, $to_year) = (shift,shift);
if ( @ARGV || !defined $from_year || !defined $to_year ||
     $from_year !~ m/^\d\d\d\d$/ || $to_year !~ m/^\d\d\d\d$/ )
    {
      print qq[Usage: $0 <from_year> <to_year>

where from_year and to_year are 4-digit years. This program updates any
.c, .h, .pl, .in, and .jsf files in the current directory that have strings
matching these patterns:
   Copyright (C) ####-<from_year>
   Copyright (C) <from_year>
to these:
   Copyright (C) ####-<to_year>
   Copyright (C) <from_year>-<to_year>\n];
   
      exit 1;
    }

my ($pass,$fail,$changes) = (0,0,0);
my @files = grep { -f $_ } glob('*.c *.h *.pl *.in *.jsf');
for my $file ( @files )
  {
    if (! open FILE, "<", $file )
        {
          print "Note: couldn't read '$file'; $!\n";
          next;
        }
    my $text = join('',<FILE>);
    close FILE;
    my $count = $text =~ s/(Copyright\s+\(C\)\s+$from_year)/$1-${to_year}/ig;
    $count   += $text =~ s/(Copyright\s+\(C\)\s+\d\d\d\d-)$from_year/$1${to_year}/ig;
    # printf "%8d %s\n", $count, $file;
    next unless $count > 0;
    if ( ! open FILE, ">", $file )
        {
          print "Error: couldn't update '$file'; $!\n";
          $fail++;
          next;
        }
    print FILE $text;
    if (close FILE)
        {
          printf "Lines updated: %4d %s\n", $count, $file;
          $pass++;
          $changes += $count;
        }
      else
        {
          print "Error updating '$file'; $!\n";
          $fail++;
        }
  }
print "Files updated: $pass\nLines updated: $changes\nErrors: $fail\n";
exit ($fail ? 2 : 0);
