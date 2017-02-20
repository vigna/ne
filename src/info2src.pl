#!/usr/bin/perl -w

use strict;
$| = 1;

# Todd_Lewis@unc.edu
#
# This program reads the ne.info* files and ne's command names
# and descriptions from them, and creates relevant parts of
# ne's source code from this data.  It works with output from
#     makeinfo (GNU texinfo 3.12) 1.68
# (and probably other versions too).

my $copyright =
  qq[	Copyright (C) 1993-1998 Sebastiano Vigna
	Copyright (C) 1999-2017 Todd M. Lewis and Sebastiano Vigna

	This file is part of ne, the nice editor.

	This library is free software; you can redistribute it and/or modify it
	under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 3 of the License, or (at your
	option) any later version.

	This library is distributed in the hope that it will be useful, but
	WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
	or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
	for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, see <http://www.gnu.org/licenses/>.];

my $enums_h_fname = "enums.h";
my $hash_h_fname  = "hash.h";
my $hash_c_fname  = "hash.c";

my $help_h_fname  = "help.h";
my $help_c_fname  = "help.c";

my $names_h_fname = "names.h";
my $names_c_fname = "names.c";

my @infofiles = glob "ne.info{,-*}";

my ( %commands,
     %exts,
     $body_enums,
     $body_names,
     $hash_table_size,
     @long_hash_table,
     @short_hash_table,
   );

foreach ( @infofiles )
  {
    # Make texinfo refs and xrefs more readable.
    # Refs and xrefs can span lines, so we have to
    # deal single lines like this:
    #    See *Note Key Bindings::.
    # as well as multiple lines like this:
    #    See *Note Key
    #    Bindings::.

    open INFO, $_ or die "Can't read command names from '$_'.";
    my @line = <INFO>;
    close INFO;
    my $line = join '', @line;
    $line =~ s<\*note *(\s*)(.+?)::> <$1`$2'>gis;

    my $state = 'searching';
    my $command;
    foreach ( split /\n/, $line )
      {
        chomp;
        if ( $state eq 'searching' )
            {
              next unless ( m/^(Syntax: )['`](([^ ]+).*)'\s*$/ );
              $command = uc $3;
              $commands{$command}->{"cmd"}     = "$3";
              $commands{$command}->{"text"}[0] = "$1$2";
              $state = 'building';
              # print "---1 found $command\n";
            }
          elsif ( $state eq 'building' )
            {
              if ( substr($_,0,1) eq chr(0x1f) )
                  {
                    pop @{$commands{$command}->{"text"}}
                      while ($commands{$command}->{"text"}[$#{$commands{$command}->{"text"}}] eq "");
                      $state = 'searching';
                      next;
                  }

              # print "---2 read \"$_\"\n";

              if ( m/^(Abbreviation: )['`](.+)'/ )
                  {
                    $commands{$command}->{"abbr"} = "$2";
                    push @{$commands{$command}->{"text"}}, "$1$2";
                    # print "---3 pushing $1$2\n";
                  }
                else
                  {
                    push @{$commands{$command}->{"text"}}, $_;
                    # print "---4 pushing $_\n";
                  }
            }
      }
  }

############################################################################
# Now the fun part -- We need to generate a hash table big enough to hold
# all the hashed commands and command abbreviations, without collisions.
#
# The tables are precompiled into the file hash.c. The file hash.h
# contains instead #defines which specify the dimensions of the hash
# tables.
#
# The hash function we choose is very simple: it considers a string of
# characters as a number in base 32, and takes the remainder of the division
# of this number by a suitably large prime number. Suitably large means
# that two commands must have different hash codes.

sub hash
  {
    my ($cmd) = @_;
    my @cvals = reverse( unpack('C*', uc $cmd ) );
    my $h = -1;
    foreach ( @cvals )
      {
        $h = ($h * 31 +  $_ ) % $hash_table_size;
      }
    return $h;
  }

# To this purpose, two different hash tables are built for command names
# and for abbreviations. This helps the distribution of the string to be
# as random as possible.
#
# Using a sieve, the prime numbers up to $max_hash_table_size are generated.
# If $n is prime, $primes[$n] == 1.
#
# This is Eratosthenes's sieve. Instead of marking with a boolean
# each entry of composite, we leave undefined the prime numbers, and
# we use the (index in array) construct in order to check for primality.

my $max_hash_table_size = 6000;
gen_hash_tables();

sub gen_primes
  {
    my ($i,$j);
    my @primes = (1) x $max_hash_table_size;    # mark them all prime.
    for ($i=2; $i<$max_hash_table_size; $i++)
      {
        if ( $primes[$i] )
            {
              for ( $j = 2 * $i;  $j < $max_hash_table_size;  $j += $i )
                {
                  $primes[ $j ]=0;           # This one isn't prime.
                }
            }
      }
    return @primes;
  }

# Then, starting from 1500 we test in order all prime numbers: we build
# the hash tables and we check for conflicts. If there are no suitable
# prime numbers smaller than $max_hash_table_size, we exit with an error.

sub gen_hash_tables
  {
    my($cmd,$long,$short,$cmd_index);
    my @primes = gen_primes();
    HTSIZE:
    for ($hash_table_size = 1500;
         $hash_table_size < $max_hash_table_size;
         $hash_table_size++ )
      {
        next if ( !$primes[$hash_table_size] );
        # start with empty (all-zero) hash tables.

        @long_hash_table  = (0) x $hash_table_size;
        @short_hash_table = (0) x $hash_table_size;

        $cmd_index = 0;

        foreach $cmd ( sort keys %commands )
          {
            $long  = hash($commands{$cmd}->{"cmd" });
            $short = hash($commands{$cmd}->{"abbr"});
            next HTSIZE if ($long_hash_table[$long] || $short_hash_table[$short]);
            $cmd_index++;
            $long_hash_table[ $long ] = $cmd_index;
            $short_hash_table[$short] = $cmd_index;
          }

        # If we get here, we've got hash tables with no conflicts.
        # Declare victory and move on...
        print "$0: HASH_TABLE_SIZE = $hash_table_size\n";
        return 1;
      }
    # If we get here, we failed to generate hash tables with no conflicts.
    # Complain bitterly and die.
    die("Failed to generate non-conflicting hash tables.");
  }

# Now we're done with hash table generation.
############################################################################


############################################################################
# Syntax filename Extension mapping
#
# The Syntax command docs contain a map of filename extensions to
# .jsf file stems for commonly occuring real life files. We'll use that
# to create the ext.c file, which contains that map in C.
{
  my $state = 'searching';
  my $base  = '';

  foreach ( @{$commands{SYNTAX}->{text}} )
    {
      my $line = $_;
      if ( $state eq 'searching' )
          {
            if ( $line =~ m/^\s+([a-z0-9+_]+):\s+/i )
                {
                  $base = $1;
                  $state = 'building';
                }
          }
      if ( $state eq 'building' )
          {
            my @token = split /\s+/, $line;
            foreach my $token ( @token )
              {
                if ( $token =~ m/([a-z0-9+_]+)(,|$)/i ) # conveniently skips "base:"
                    {
                      $exts{$1} = $base;
                    }
              }
            $state = 'searching' unless $line =~ m/,\s*$/;
          }
    }
  if ( open EXTIN,"ext.c.in" )
      {
        my @lines = <EXTIN>;
        close EXTIN;
        my $data = '';
        foreach my $k ( sort keys %exts )
          {
            $data .= qq[\t{ "$k", "$exts{$k}" },\n];
          }
        $data =~ s/,\n$/\n/; # It's C, so take that last ',' off.
        my $lines = join '', @lines;
        $lines =~ s/<%%EXTMAP%%>/$data/;
        if ( open EXT,">ext.c" )
            {
              print EXT $lines;
              close EXT;
            }
      }
}

open_out_files();

foreach my $cmd ( sort keys %commands )
  {
    body_names_h($cmd);
    body_names_c($cmd);

    body_help_h($cmd);
    body_help_c($cmd);

    body_enums_h( $cmd );
    body_hash_h( $cmd );
    body_hash_c( $cmd );
  }

print scalar keys %commands, " commands processed.\n";

close_out_files();
exit 0;

sub open_out_files
  {
    open NAMES_H, ">$names_h_fname" or die("Couldn't write $names_h_fname");
    head_names_h();

    open NAMES_C, ">$names_c_fname" or die("Couldn't write $names_c_fname");
    head_names_c();

    open HELP_C,  ">$help_c_fname" or die("Couldn't write $help_c_fname");
    head_help_c();

    open HELP_H,  ">$help_h_fname" or die("Couldn't write $help_h_fname");
    head_help_h();

    open ENUMS_H, ">$enums_h_fname" or die("Couldn't write $enums_h_fname");
    head_enums_h();

    open HASH_H,  ">$hash_h_fname" or die("Couldn't write $hash_h_fname");
    head_hash_h();

    open HASH_C,  ">$hash_c_fname" or die("Couldn't write $hash_c_fname");
    head_hash_c();
  }

sub close_out_files
  {
    tail_names_h();    close NAMES_H;
    tail_names_c();    close NAMES_C;

    tail_help_c();     close HELP_C;
    tail_help_h();     close HELP_H;

    tail_enums_h();    close ENUMS_H;

    tail_hash_h();     close HASH_H;
    tail_hash_c();     close HASH_C;
  }


##################################################################
##   N A M E S _ H
##################################################################


sub head_names_c
  {
    print NAMES_C <<"_EOT_";
/* Names and abbreviations of all the commands.

$copyright  */

#include "ne.h"

/* This file is generated by info2src.pl from data found in ne.texinfo.
   Changes made directly to this file will we lost when the documentation
   is updated. */

/* Here we have the name of all commands. They are extern'd in names.h. Note
that whenever you add or remove a command, you should immediately change also
the command_names array at the end of this file. */


_EOT_
    $body_names = "";
  }

sub body_names_c
  {
    my ($uccmd) = @_;
    my $cmd     = $commands{$uccmd}->{"cmd"};

    if ( ! defined $commands{$uccmd} )
        {
          print "$0 [", __LINE__, "]: \$commands{$uccmd} undefined.\n";
          exit 2;
        }
    if ( ! defined $commands{$uccmd}->{"abbr"} )
        {
          print "$0 [", __LINE__, "]: \$commands{$uccmd}->{\"abbr\"} undefined.\n";
          exit 2;
        }

    print NAMES_C <<"_EOT_";
const char ${uccmd}_NAME[]   = "$cmd";
const char ${uccmd}_ABBREV[] = "$commands{$uccmd}->{"abbr"}";

_EOT_

    $body_names .= "  ${uccmd}_NAME,";
    $body_names .= "\n" if ( ( my @x = ($body_names =~ m/,/g)) % 4 == 0 );
  }

sub tail_names_c
  {
    print NAMES_C <<"_EOT_";
/* These are extras that are very useful in the default menus and key bindings. */

const char PLAYONCE_ABBREV[] = "PL 1";
const char MIDDLEVIEW_ABBREV[] = "AV M";
const char SHIFTLEFT_ABBREV[] = "SH <";

/* This is the NULL-terminated, ordered list of names, useful for help etc. */

const char * const command_names[ACTION_COUNT+1] = {
$body_names  NULL
};
_EOT_
  }


##################################################################
##   N A M E S _ H
##################################################################


sub head_names_h
  {
    print NAMES_H <<"_EOT_";
/* Extern's for names and abbreviations of all the commands.

$copyright  */


_EOT_
  }

sub body_names_h
  {
    my ($uccmd) = @_;
    my $cmd     = $commands{$uccmd}->{"cmd"};

    print NAMES_H <<"_EOT_";
extern const char ${uccmd}_NAME[];
extern const char ${uccmd}_ABBREV[];

_EOT_
  }

sub tail_names_h
  {
    print NAMES_H <<"_EOT_";

/* These are extras that are very useful in the default menus and key bindings. */
extern const char PLAYONCE_ABBREV[];
extern const char MIDDLEVIEW_ABBREV[];
extern const char SHIFTLEFT_ABBREV[];

/* This is the NULL-terminated, ordered list of names, useful for help etc. */

extern const char * const command_names[ACTION_COUNT+1];

/* This file was automatically generated by $0. */
_EOT_
  }

##################################################################
##   H E L P _ C
##################################################################


sub head_help_c
  {
    print HELP_C <<"_EOT_";
/* Help strings.

$copyright  */

_EOT_
  }

sub body_help_c
  {
    my ($uccmd) = @_;
    my $cmd     = $commands{$uccmd}->{"cmd"};

    my $len   = @{$commands{$uccmd}->{"text"}} + 1;

    print HELP_C "const char * const ${uccmd}_HELP[$len] = {\n";
    for my $txt ( @{$commands{$uccmd}->{"text"}} )
      {
        my $etxt = $txt;
        $etxt =~ s#\\#\\\\#g;
        $etxt =~ s#"#\\"#g;
        print HELP_C "  \"$etxt\",\n";
      }
    print HELP_C "};\n\n";
  }

sub tail_help_c
  {
    print HELP_C <<"_EOT_";
/* This file was automatically generated by $0. */
_EOT_
  }


##################################################################
##   H E L P _ H
##################################################################


sub head_help_h
  {
    print HELP_H <<"_EOT_";
/* Help string externs.

$copyright  */


_EOT_
  }

sub body_help_h
  {
    my ($uccmd) = @_;
    my $cmd     = $commands{$uccmd}->{"cmd"};

    my $len   = @{$commands{$uccmd}->{"text"}} + 1;
    print HELP_H <<"_EOT_";
extern const char * const ${uccmd}_HELP[ $len ];
_EOT_
  }

sub tail_help_h
  {
    print HELP_H <<"_EOT_";

/* This file was automatically generated by $0. */
_EOT_
  }


##################################################################
##   E N U M S _ H
##################################################################


sub head_enums_h
  {
    print ENUMS_H <<"_EOT_";
/* This is the list of all possible actions that do_action() can execute.
Note also that menu handling is governed by such a command (ESCAPE).

$copyright  */

typedef enum {
_EOT_
    $body_enums = "";  # We'll grow this string to contain all the commands.
  }

sub body_enums_h
  {
    my ($uccmd) = @_;
    my $cmd     = $commands{$uccmd}->{"cmd"};

    $body_enums .= "  ${uccmd}_A,";  # Count the commas, add '\n' if we've got multiple of 4.
    $body_enums .= "\n" if ( (my @x = ($body_enums =~ m/,/g)) % 4 == 0 );
  }

sub tail_enums_h
  {
    print ENUMS_H <<"_EOT_";
$body_enums
  ACTION_COUNT
} action;

/* This file was automatically generated by $0. */
_EOT_
  }


##################################################################
##   H A S H _ H
##################################################################


sub head_hash_h
  {
    print HASH_H <<"_EOT_";
/* Header for precompiled hash tables for internal commands.

$copyright  */

_EOT_
  }

sub body_hash_h
  {
    # Nothing to do here.
  }

sub tail_hash_h
  {
    my $max_command_width = 0;
    foreach my $cmd ( keys %commands )
      {
        $max_command_width = length $cmd if length $cmd > $max_command_width;
      }
    print HASH_H <<"_EOT_";
/* These vectors are hash tables with no conflicts. For each command, the
element indexed by the hashed name of the command contains the command
number plus one. Thus, only one strcmp() is necessary when analyzing the
command line. This technique offers a light speed comparison against the
command names, with a very small memory usage. The tables are precompiled,
so they can be moved to the text segment. */

#define HASH_TABLE_SIZE ($hash_table_size)

extern const unsigned char hash_table[HASH_TABLE_SIZE];
extern const unsigned char short_hash_table[HASH_TABLE_SIZE];

/* The maximum width for a command is used when displaying the command
names with the string requester. For example, 18 would allow four columns
on an 80x25 screen.
*/

#define MAX_COMMAND_WIDTH  $max_command_width


/* This file was automatically generated by $0. */
_EOT_
  }


##################################################################
##   H A S H _ C
##################################################################


sub head_hash_c
  {
    print HASH_C <<"_EOT_";
/* Precompiled hash tables for internal commands.

$copyright  */


/* #include "ne.h" */
#include "hash.h"

/* These vectors are hash tables with no conflicts. For each command, the
element indexed by the hashed name of the command contains the command
number plus one. Thus, only one strcmp() is necessary when analyzing the
command line. This technique offers a light speed comparison against the
command names, with a very small memory usage. The tables are precompiled,
so that they can be moved into the text segment. It is *essential* that
any modification whatsoever to the command names, number etc. is reflected
in this table. */

_EOT_

  }

sub body_hash_c
  {
    # Nothing to do here.
  }

sub tail_hash_c
  {
    print HASH_C "const unsigned char hash_table[HASH_TABLE_SIZE] = {\n";
    for (my $i=0; $i<$hash_table_size; $i++)
      {
        print HASH_C "  $long_hash_table[$i],";
        print HASH_C "\n" if ( $i % 20 == 0 );
      }
    print HASH_C "};\n\n";

    print HASH_C "const unsigned char short_hash_table[HASH_TABLE_SIZE] = {\n";
    for (my $i=0; $i<$hash_table_size; $i++)
      {
        print HASH_C "  $short_hash_table[$i],";
        print HASH_C "\n" if ( $i % 20 == 0 );
      }
    print HASH_C "};\n\n";

    print HASH_C <<"_EOT_";
/* This file was automatically generated by $0. */
_EOT_
  }

