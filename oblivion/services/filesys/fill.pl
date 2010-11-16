#!/usr/bin/perl

@Info = stat("fd0.img");

# Calculo lo que me falta para llegar a 1474048

$size = $Info[7];
$zeros = 0;

print "Tamano: $size bytes\n";

$zeros = 1474048 - $size; 

print "Se agregaran: $zeros bytes\n";
system("dd if=/dev/zero of=zeros bs=$zeros count=1");
system("cat zeros >> fd0.img");

