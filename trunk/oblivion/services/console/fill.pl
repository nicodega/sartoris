#!/usr/bin/perl

@Info = stat("console.img");

# Calculo lo que me falta para llegar a 0x2000

$size = $Info[7];
$zeros = 0;

print "Size: $size bytes\n";

$zeros = 0x4000 - $size; 

print "Se agregaran: $zeros bytes\n";
system("dd if=/dev/zero of=zeros bs=$zeros count=1");
system("cat zeros >> console.img");

