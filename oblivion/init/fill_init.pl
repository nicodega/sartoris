#!/usr/bin/perl

@Info = stat("init.img");

# Calculo lo que me falta para llegar a 0x1000

$size = $Info[7];
$zeros = 0;

print "Size: $size bytes\n";

$zeros = 0x1000 - $size;

if($zeros < 0)
{
	die "Init is too big!";
}

print "Se agregaran: $zeros bytes\n";
system("dd if=/dev/zero of=zeros bs=$zeros count=1");
system("cat zeros >> init.img");

