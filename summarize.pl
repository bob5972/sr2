#!/usr/bin/perl

use strict;
use warnings;

use FindBin qw($Bin);
use lib "$Bin/MBLib/MBPerl";

use Scalar::Util qw(looks_like_number);

use MBBasic;

my $gScriptOptions = {
    "file=s" => { desc => "MBRegistry file to use",
                  default => "build/tmp/stable.zoo" },
    "date!" => {desc => "Add a date line to the bottom",
                default => FALSE },
    "opSummary!" => { desc => "Include an opcode summary",
                      default => FALSE },
    "evolveLog!" => { desc => "Include the evolve log",
                      default => FALSE },
    "dumpFleet|d=i" => { desc => "Dump the specified fleet",
                         default => undef, },
    "cFormat|c" => { desc => "Print fleet with C formatting",
                         default => undef, },
    "graph!" => { desc => "Print the fleet as graphviz source",
                  default => FALSE },
    "resetHistory|r!" => { desc => "Reset fleet history",
                           default => FALSE },
};

my $gPop;
my $gFile;

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

sub GetFleet($;$) {
    my $fn = shift;
    my $dropPrefix = shift;

    if (!defined($dropPrefix)) {
        $dropPrefix = FALSE;
    }

    my $oup = {};

    my $prefix = "fleet$fn";

    foreach my $k (keys %{$gPop}) {
        if ($k =~ /^$prefix\./) {
            my $v = $gPop->{$k};

            if ($dropPrefix) {
                $k =~ s/^$prefix\.//;
            }
            $oup->{$k} = $v;
        }
    }

    return $oup;
}

sub DumpFleet($) {
    my $fn = shift;

    if ($OPTIONS->{'graph'}) {
        DumpGraph($fn);
    } else {
        my $h = GetFleet($fn, FALSE);
        my $prefix = "fleet$fn";
        foreach my $k (sort keys %{$h}) {
            my $v = $gPop->{$k};
            if (!$OPTIONS->{'cFormat'}) {
                Console("$k = $v\n");
            } else {
                $k =~ s/^$prefix\.//;
                Console("{ \"$k\", \"$v\" },\n");
            }
        }
    }
}

sub DumpGraph($) {
    my $fn = shift;

    my $fleet;

    if (-x "build/sr2") {
        my @fleetLines = `build/sr2 sanitizeFleet -f $fn -U $gFile` or
            Panic("Unable to sanitize fleet: build/sr2 error", $!);
        $fleet = MBBasic::LoadMRegLines(\@fleetLines);
    } else {
        $fleet = GetFleet($fn, TRUE);
    }

    Console("digraph G {\n");

    my $nodes = {};
    my $oNodes = {};
    my $iNodes = {};

    foreach my $k (sort keys %{$fleet}) {
        if ($k =~ /^floatNet.node\[(\d+)\]\.op$/) {
            my $n = $1;
            my $op = $fleet->{$k};
            if ($op ne "ML_FOP_VOID") {
                $nodes->{$n} = "$n\\n$op";
            }
        }
    }
    foreach my $k (sort keys %{$fleet}) {
        if ($k =~ /^input\[(\d+)\]\.valueType/) {
            my $n = $1;
            my $type = $fleet->{$k};

            if ($type eq 'NEURAL_VALUE_FORCE') {
                $type = $fleet->{"input[$n].forceType"};
            } elsif ($type eq 'NEURAL_VALUE_CROWD') {
                $type = $fleet->{"input[$n].crowdType"};
            }

            if ($type ne "NEURAL_VALUE_VOID") {
                $nodes->{$n} = "$n\\n$type";
                $iNodes->{$n} = $type;
            }
        }
    }

    foreach my $k (sort keys %{$fleet}) {
        if ($k =~ /^output\[(\d+)\]\.forceType/) {
            my $n = $1;
            my $type = $fleet->{$k};
            if ($type ne "NEURAL_FORCE_VOID") {
                $oNodes->{$n} = $type;
            }
        }
    }

    # Dump Nodes
    foreach my $k (sort keys %{$nodes}) {
        my $v = $nodes->{$k};
        my $color = "";
        if ($iNodes->{$k}) {
            $color .= "color=blue";
        }
        if (!$oNodes->{$k}) {
            Console("$k [label=\"$v\" $color];\n");
        }
    }
    # Dump Outputs
    foreach my $k (sort keys %{$oNodes}) {
        my $v = $nodes->{$k};
        my $color = "";
        if (defined($nodes->{$k})) {
            $color .= "color=red";
            Console("$k [label=\"$v\"];\n");

            Console("Output_$k [label=\"" . $oNodes->{$k} . "\" color=red ];\n");
            Console("{ rank=sink Output_$k }\n");
            Console("$k -> Output_$k\n");
        }
    }

    # Rank Input
    Console("{ rank=source ");
    foreach my $k (sort keys %{$iNodes}) {
        Console("$k ");
    }
    Console("}\n");

    # Dump Edges
    foreach my $k (sort keys %{$fleet}) {
        if ($k =~ /^floatNet.node\[(\d+)\]\.inputs$/) {
            my $n = $1;
            my $v = $fleet->{$k};
            $v =~ s/^\{//;
            $v =~ s/\}$//;
            my $inputs = $v;

            while ($inputs =~ /^(\d+)\,/) {
                my $i = $1;

                if (defined($nodes->{$i})) {
                    # Avoid voided inputs to live nodes
                    Console("$i -> $n\n");
                }
                $inputs =~ s/^\s*\d+\,\s*//;
            }
        }
    }

    Console("}\n");
}

sub GetFleetSummary($) {
        my $fPrefix = shift;

        my $nb = $gPop->{"$fPrefix.numBattles"};
        my $mw = $gPop->{"$fPrefix.numWins"};
        my $ms = $gPop->{"$fPrefix.numSpawn"};
        my $f = $mw / $gPop->{"$fPrefix.numBattles"};
        my $a = $gPop->{"$fPrefix.age"};

        if (!defined($ms)) {
            $ms = 0;
        }

        $f = sprintf("%1.2f%%", ($f*100));

        my $fleet = $fPrefix;
        $fleet =~ s/\.abattle//;
        return "$fleet, age=$a, numWins=$mw, numSpawn=$ms, fitness=$f";
}

sub ComputeDiversity() {
    my $numFleets = $gPop->{'numFleets'};
    VERIFY(defined($numFleets) && $numFleets > 0);

    my $totalEntries = 0;
    my $uniqueEntries = 0;
    my $uniqueHash = {};

    my $targetHash = {};

    for (my $i = 1; $i <= $numFleets; $i++) {
        my $prefix = "fleet$i";
        if ($gPop->{"$prefix.abattle.playerType"} eq "Target") {
            $targetHash->{$prefix} = 1;
        }
    }

    foreach my $k (keys %{$gPop}) {
        if ($k eq 'numFleets') {
            # Not a fleet entry.
        } elsif ($k =~ /^(fleet\d+)\./) {
            my $prefix = $1;
            if ($targetHash->{$prefix} &&
                $k !~ /^fleet\d+\.abattle\.age$/ &&
                $k !~ /^fleet\d+\.abattle\.num(Battles|Wins|Losses|Draws|Spawn)$/ &&
                $k !~ /^fleet\d+\.abattle\.(fleetName|playerName|playerType)$/) {
                # Target fleet entry.
                $totalEntries++;

                my $v = $gPop->{$k};
                $k =~ s/^fleet\d+.//;
                my $ue = $k . " == " . $v;
                if (!defined($uniqueHash->{$ue})) {
                    $uniqueEntries++;
                    $uniqueHash->{$ue} = 1;
                }
            } else {
                # Control fleet or meta entry
            }
        } else {
            Panic("Unknown entry: $k\n");
        }
    }

    my $linesPerFleet;
    my $uniquePerFleet;
    my $diversity;

    if ($numFleets != 0) {
        $linesPerFleet = $totalEntries / $numFleets;
        $uniquePerFleet = $uniqueEntries / $numFleets;
    } else {
        $linesPerFleet = 0;
        $uniquePerFleet = 0;
    }

    if ($linesPerFleet != 0) {
        $diversity = $uniquePerFleet / $linesPerFleet;
    } else {
        $diversity = 0;
    }

    return $diversity;
}


sub DisplaySummary() {
    my $numFleets = $gPop->{'numFleets'};
    VERIFY(defined($numFleets) && $numFleets > 0);

    my $totalWins = 0;
    my $totalBattles = 0;
    my $ages = {};
    my $maxAge = 0;
    my $minAge;
    my $fitnessRange = {};
    my $maxWinsP;
    my $maxFP;
    my $maxFV;

    for (my $i = 0.1; $i <= 1.0; $i+=0.1) {
        $fitnessRange->{$i} = 0;
    }

    for (my $i = 1; $i <= $numFleets; $i++) {
        my $prefix = "fleet$i";
        my $fPrefix = "$prefix.abattle";
        if ($gPop->{"$fPrefix.playerType"} eq "Target") {
            my $numWins = $gPop->{"$fPrefix.numWins"};
            my $numBattles = $gPop->{"$fPrefix.numBattles"};
            my $f = 0.0;

            if (!defined($numWins)) {
                $numWins = 0;
            }

            if (defined($numBattles) && $numBattles > 0) {
                $f = $numWins / $numBattles;
            }

            if (defined($numBattles)) {
                $totalWins += $numWins;
                $totalBattles += $numBattles;

                for (my $fi = 0.1; $fi <= 1.0; $fi += 0.1) {
                    # This has rounding problems at 100% ?
                    if ($f <= $fi || $fi > 0.9) {
                        $fitnessRange->{$fi}++;
                        last;
                    }
                }
            }

            my $a = $gPop->{"$fPrefix.age"};
            if (defined($a)) {
                if (!defined($ages->{$a})) {
                    $ages->{$a} = 0;
                }
                $ages->{$a}++;

                if ($a > $maxAge) {
                    $maxAge = $a;
                }
                if (!defined($minAge) || $a < $minAge) {
                    $minAge = $a;
                }
            }

            if (!defined($maxWinsP) ||
                !defined($gPop->{"$maxWinsP.numWins"}) ||
                $numWins > $gPop->{"$maxWinsP.numWins"}) {
                $maxWinsP = $fPrefix;
            }
            if (!defined($maxFP) || $f > $maxFV) {
                $maxFP = $fPrefix;
                $maxFV = $f;
            }
        }
    }

    my $fitness = 0.0;

    if ($totalBattles > 0) {
        $fitness = ($totalWins / $totalBattles);
    }

    if (scalar keys %{$ages} > 10) {
        my $collapsedAges = {};

        if ($minAge < 10) {
            my $nextMin = $minAge + 1;
            $collapsedAges->{"$minAge"} = GetCountFromRange($ages, $minAge, $minAge);
            if ($nextMin < 9) {
                $collapsedAges->{" $nextMin -  9"} = GetCountFromRange($ages, $nextMin, 9);
            } else {
                $collapsedAges->{"9"} = GetCountFromRange($ages, 9, 9);
            }
        }

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
    Console(sprintf("%10s %10s\n", "Age", "Count"));
    foreach my $x (sort {CompareRange($a, $b)} keys %{$ages}) {
        Console(sprintf("%10s %10s\n", $x, $ages->{$x}));
    }

    if (defined($maxWinsP)) {
        my $fPrefix = $maxWinsP;
        Console("\n");
        Console(" Leader: " . GetFleetSummary($fPrefix) . "\n");
    }
    if (defined($maxFP)) {
        my $fPrefix = $maxFP;
        Console("Upstart: " . GetFleetSummary($fPrefix) . "\n");
    }

    Console("\n");
    $fitness = sprintf("%1.2f", ($fitness*100));
    Console("Average Fitness: $fitness%\n");

    my $diversity = ComputeDiversity();
    $diversity = sprintf("%1.2f", ($diversity*100));
    Console("      Diversity: $diversity%\n");
    Console("\n");
}

sub ResetHistory() {
    my $numFleets = $gPop->{'numFleets'};
    VERIFY(defined($numFleets) && $numFleets > 0);

    for (my $i = 1; $i <= $numFleets; $i++) {
        my $prefix = "fleet$i";
        my $fPrefix = "$prefix.abattle";
        delete $gPop->{"$fPrefix.numBattles"};
        delete $gPop->{"$fPrefix.numWins"};
        delete $gPop->{"$fPrefix.numLosses"};
        delete $gPop->{"$fPrefix.numDraws"};

        # Keep age and numSpawn because they're just
        # used for statistics.
    }

    MBBasic::SaveMRegFile($gPop, $gFile);
    Console("Fleet history reset.\n");
}

sub Main() {
    MBBasic::LoadOptions($gScriptOptions, __PACKAGE__);
    MBBasic::Init();

    my $evolveLog = "build/tmp/evolve.log";
    if ($OPTIONS->{'evolveLog'} && -f $evolveLog) {
        open(my $fh, '<', $evolveLog) or Panic($!);
        my @s = <$fh>;
        close($fh);
        Console(join('', @s));
    }

    $gFile = $OPTIONS->{'file'};
    $gPop = MBBasic::LoadMRegFile($gFile);

    if (defined($OPTIONS->{'dumpFleet'})) {
        DumpFleet($OPTIONS->{'dumpFleet'});
    } elsif ($OPTIONS->{'resetHistory'}) {
        ResetHistory();
    } else {
        DisplaySummary();
    }

    if ($OPTIONS->{'date'}) {
        my $date = `date`;
        Console("$date\n");
    }

    if ($OPTIONS->{'opSummary'}) {
        my $s = `grep op $gFile | awk -F= '{print \$2}'| sort |uniq -c | sort -nr`;
        Console($s);
        Console("\n");
    }

    MBBasic::Exit();
}

# Call the Main function.
my $exitCode;
eval {
   $exitCode = Main();
}; Panic($@) if $@;
exit $exitCode;
