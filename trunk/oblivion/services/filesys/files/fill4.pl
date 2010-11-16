#!/usr/bin/perl

@Info = stat("file4.img");

# Calculo lo que me falta para llegar a 0x2000

$size = $Info[7];
$zeros = 0;

print "Size: $size bytes\n";

$zeros = 0x400 - $size; 

print "Se agregaran: $zeros bytes\n";
system("dd if=/dev/zero of=zeros bs=$zeros count=1");
system("cat zeros >> file4.img");

