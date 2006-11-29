#!/usr/bin/perl

@Info = stat("./pmanager2.img");

$size = $Info[7];

printf "0x%x","$size";

