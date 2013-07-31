#!/usr/bin/env perl

use File::Path qw/make_path/;
use FindBin '$Bin';
use Term::ANSIColor qw/colored/;

my @tests = @ARGV;
my $out = "$Bin/../tmp/tests";

make_path $out;

my $CC = "gcc -Werror -Wall -I$Bin -I$Bin/../src $Bin/../src/*.c";
my $res = 0;
my $i = 0;

system( "rm -Rf $out/*" );

for( @tests ) {
  $i++;
  
  my $name = $i;
  my $cmd = "$CC $_ -o $out/$name && $out/$name";
  
  print qq/===> Running test "$_"\n/;
  
  $res = system( $cmd ) || $res;
}

if( $res ) {
  print colored ['red'], qq/FAILED\n/;
}
else {
  print colored ['green'], qq/PASS\n/;
}

exit $res;
