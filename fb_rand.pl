#!/usr/bin/perl
# fb_rand.pl
# Run fb with random parameters
#

# Equal chance of blur mask being size 1, 3, or 5
# (blur=1 means no blur)
$r = rand();
if ( $r > 0.667 ) {
	$blur = 1;
}
elsif ( $r > 0.333 ) {
	$blur = 3;
}
else {
	$blur = 5;
}

# sharpen, [0,2]
$sharpen = 2.0 * rand();

# camera roll angle, degrees [0,360]
$roll = 360.0 * rand();

# zoom in factor [1,1.05]
$zoom = 1.0 + 0.05*rand();

# blend coefficient [0,1]
# (values outside this range cause clipping)
$blend = rand();

# noise factor [0,0.25]
$noise = 0.2 * rand();

# mutate factor [0,1]
$mutate = rand();

# color crawl parameters [-2.0,2.0]
$crawlds = 4.0 * (rand()-0.5);
$crawldv = 4.0 * (rand()-0.5);
$crawldsv = 4.0 * (rand()-0.5);
$crawld = 4.0 * (rand()-0.5);

# specify random seed (for replicability)
$seed = int(4294967295.0 * rand());

$cmd = "--blur=$blur --sharpen=$sharpen --roll=$roll --zoom=$zoom --blend=$blend --crawl=$crawlds,$crawldv,$crawldsv,$crawld --noise=$noise,$mutate --seed=$seed";

# randomly permute order of operations
@a = split(' ',$cmd);
fisher_yates_shuffle(\@a);

$cmd = "fb " . join(' ',@a) . " --histeq";

print "$cmd\n";
`$cmd`;


# generate a random permutation of array in place
sub fisher_yates_shuffle {
    my $array = shift;
    my $i;
    for ($i = @$array; --$i; ) {
        my $j = int rand ($i+1);
        next if $i == $j;
        @$array[$i,$j] = @$array[$j,$i];
    }
}

