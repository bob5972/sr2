#!/usr/bin/perl

use strict;
use warnings;

use FindBin qw($Bin);
use lib "$Bin/MBLib/MBPerl";

use MBBasic;

sub Main() {
    my $entries;

    MBBasic::Init();

    my $file = "build/tmp/popMutate.txt";
    $entries = LoadMRegFile($file);

    my $numFleets = $entries->{'numFleets'};
    VERIFY(defined($numFleets) && $numFleets > 0);

    my $totalWins = 0;
    my $totalBattles = 0;
    my $ages = {};
    my $fitnessRange = {};

    for (my $i = 0.1; $i <= 1.0; $i+=0.1) {
        $fitnessRange->{$i} = 0;
    }

    for (my $i = 1; $i <= $numFleets; $i++) {
        my $prefix = "fleet$i";
        if ($entries->{"$prefix.playerType"} eq "Target") {
            if (defined($entries->{"$prefix.numBattles"})) {
                my $numBattles = $entries->{"$prefix.numBattles"};
                my $numWins = $entries->{"$prefix.numWins"};
                $totalWins += $numWins;
                $totalBattles += $numBattles;

                my $f = 0.0;
                if (defined($numBattles) && $numBattles > 0) {
                    $f = $entries->{"$prefix.numWins"} / $numBattles;
                }
                for (my $fi = 0.1; $fi <= 1.0; $fi += 0.1) {
                    if ($f <= $fi) {
                        $fitnessRange->{$fi}++;
                        last;
                    }
                }
            }

            my $a = $entries->{"$prefix.age"};
            if (defined($a)) {
                if (!defined($ages->{$a})) {
                    $ages->{$a} = 0;
                }
                $ages->{$a}++;
            }


        }
    }

    my $fitness = 0.0;

    if ($totalBattles > 0) {
        $fitness = ($totalWins / $totalBattles);
    }

    Console("\n");
    Console(sprintf("%10s %10s\n", "Fitness", "Count"));
    for (my $fi = 0.1; $fi <= 1.0; $fi += 0.1) {
        my $range = sprintf("%3.0f - %3.0f %%", ($fi - 0.1) * 100, ($fi * 100));
        Console(sprintf("%10s %10s\n", $range, $fitnessRange->{$fi}));
    }

    Console("\n");
    Console(sprintf("%8s %8s\n", "Age", "Count"));
    foreach my $x (sort {$a <=> $b} keys %{$ages}) {
        Console(sprintf("%8s %8s\n", $x, $ages->{$x}));
    }

    Console("\n");
    Warning("numFleets = " . $numFleets . "\n");
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
