#!/usr/bin/perl

@Info = stat("./pmanager.img");

$size = $Info[7];

printf "0x%x","$size";

