#!/usr/bin/perl

use strict;
use warnings;

use FindBin qw($Bin);
use lib "$Bin/MBLib/MBPerl";

use Scalar::Util qw(looks_like_number);

use MBBasic;

my $gScriptOptions = {
    "dumpFleet|d=i" => { desc => "Dump the specified fleet",
                         default => undef, },
};

my $gPop;

sub GetCountFromRange($$$)
{
    my $hash = shift;
    my $min = shift;
    my $max = shift;

    my $x = 0;

    for (my $i = $min; $i <= $max; $i++) {
        if (defined($hash->{$i})) {
            $x += $hash->{$i};
        }
    }

    return $x;
}

sub CompareRange($$)
{
    my $a = shift;
    my $b = shift;

    if ($a =~ /^\s*(\d+)\s+-\s+(\d+)$/) {
        $a = $1;
    }

    if ($b =~ /^\s*(\d+)\s+-\s+(\d+)$/) {
        $b = $1;
    }

    if (looks_like_number($a) && looks_like_number($b)) {
        return $a<=>$b;
    }

    NOT_IMPLEMENTED();
}

sub DumpFleet($) {
    my $fn = shift;

    my $prefix = "fleet$fn";

    foreach my $k (keys %{$gPop}) {
        if ($k =~ /^$prefix\./) {
            my $v = $gPop->{$k};
            Console("$k = $v\n");
        }
    }
}

sub Main() {
    MBBasic::LoadOptions($gScriptOptions, __PACKAGE__);
    MBBasic::Init();

    my $file = "build/tmp/popMutate.txt";
    $gPop = LoadMRegFile($file);

    if (defined($OPTIONS->{'dumpFleet'})) {
        DumpFleet($OPTIONS->{'dumpFleet'});
        return 0;
    }

    my $numFleets = $gPop->{'numFleets'};
    VERIFY(defined($numFleets) && $numFleets > 0);

    my $totalWins = 0;
    my $totalBattles = 0;
    my $ages = {};
    my $maxAge = 0;
    my $fitnessRange = {};
    my $maxWinsP;

    for (my $i = 0.1; $i <= 1.0; $i+=0.1) {
        $fitnessRange->{$i} = 0;
    }

    for (my $i = 1; $i <= $numFleets; $i++) {
        my $prefix = "fleet$i";
        if ($gPop->{"$prefix.playerType"} eq "Target") {
            if (defined($gPop->{"$prefix.numBattles"})) {
                my $numBattles = $gPop->{"$prefix.numBattles"};
                my $numWins = $gPop->{"$prefix.numWins"};
                $totalWins += $numWins;
                $totalBattles += $numBattles;

                my $f = 0.0;
                if (defined($numBattles) && $numBattles > 0) {
                    $f = $gPop->{"$prefix.numWins"} / $numBattles;
                }
                for (my $fi = 0.1; $fi <= 1.0; $fi += 0.1) {
                    # This has rounding problems at 100% ?
                    if ($f <= $fi || $fi > 0.9) {
                        $fitnessRange->{$fi}++;
                        last;
                    }
                }
            }

            my $a = $gPop->{"$prefix.age"};
            if (defined($a)) {
                if (!defined($ages->{$a})) {
                    $ages->{$a} = 0;
                }
                $ages->{$a}++;

                if ($a > $maxAge) {
                    $maxAge = $a;
                }
            }

            my $nw = $gPop->{"$prefix.numWins"};
            if (!defined($maxWinsP) ||
                (defined($nw) && $nw > $gPop->{"$maxWinsP.numWins"})) {
                $maxWinsP = $prefix;
            }
        }
    }

    my $fitness = 0.0;

    if ($totalBattles > 0) {
        $fitness = ($totalWins / $totalBattles);
    }

    if (scalar keys %{$ages} > 10) {
        my $collapsedAges = {};
        $collapsedAges->{"0"} = GetCountFromRange($ages, 0, 0);
        $collapsedAges->{"1"} = GetCountFromRange($ages, 1, 1);
        $collapsedAges->{" 2 -  9"} = GetCountFromRange($ages, 2, 9);

        for (my $x = 10; $x < $maxAge; $x+=10) {
            my $low = $x;
            my $high = $x + 9;

            if ($high >= $maxAge) {
                $high = $maxAge - 1;
            }

            my $count = GetCountFromRange($ages, $low, $high);
            if ($count > 0) {
                $collapsedAges->{"$x - $high"} = $count;
            }
        }

        $collapsedAges->{"$maxAge"} = GetCountFromRange($ages, $maxAge, $maxAge);

        $ages = $collapsedAges;
    }

    Console("numFleets = " . $numFleets . "\n");
    Console("\n");
    Console(sprintf("%10s %10s\n", "Fitness", "Count"));
    for (my $fi = 0.1; $fi <= 1.0; $fi += 0.1) {
        my $range = sprintf("%3.0f - %3.0f %%", ($fi - 0.1) * 100, ($fi * 100));
        Console(sprintf("%10s %10s\n", $range, $fitnessRange->{$fi}));
    }

    Console("\n");
    Console(sprintf("%8s %8s\n", "Age", "Count"));
    foreach my $x (sort {CompareRange($a, $b)} keys %{$ages}) {
        Console(sprintf("%8s %8s\n", $x, $ages->{$x}));
    }

    Console("\n");
    my $mw = $gPop->{"$maxWinsP.numWins"};
    my $f = $mw / $gPop->{"$maxWinsP.numBattles"};
    my $a = $gPop->{"$maxWinsP.age"};
    $f = sprintf("%1.2f%%", ($f*100));
    Console("Leader: $maxWinsP, age=$a, numWins=$mw, fitness=$f\n");

    Console("\n");
    $fitness = sprintf("%1.2f", ($fitness*100));
    Console("Average Fitness: $fitness%\n");
    Console("\n");

    MBBasic::Exit();
}

# Call the Main function.
my $exitCode;
eval {
   $exitCode = Main();
}; Panic($@) if $@;
exit $exitCode;
