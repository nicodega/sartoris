#!/usr/bin/perl

@Info = stat("../bin/asgard.img");

$size = $Info[7] + (($Info[7] % 512 == 0)? 0 : (512 - ($Info[7] % 512)));

printf "%i","$size";

