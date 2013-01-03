#!/usr/bin/perl

@Info = stat("../kernel/kimg");

$size = $Info[7];
$blocks = $size / 512;

print "$blocks";

