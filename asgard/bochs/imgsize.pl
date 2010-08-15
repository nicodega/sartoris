#!/usr/bin/perl

$size = (stat($ARGV[0]))[7];

@Info = stat("../bin/asgard.img");

$size = $Info[7] + (($Info[7] % 512 == 0)? 0 : (512 - ($Info[7] % 512)));

if($ARGV[0] eq "-f")
{
	#generate a file with the size
	open FILE, ">", "./length.hdb" or die $!;
	print FILE pack "L", $size;
	close FILE;

	$size = $size / 512;
	#generate a file with sectors count
	open FILE, ">", "./length-s.hdb" or die $!;
	print FILE pack "S", $size;
	close FILE;
}
else
{
	printf "%i","$size";	
}




