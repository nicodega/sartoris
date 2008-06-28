#!/usr/bin/perl

@Info = stat("kimg");

# Calculo lo que me falta para llegar a un mutiplo de 512

$size = $Info[7];
$zeros = 0;

print "Tamano: $size bytes\n";

$zeros = 512 - ($size % 512); 

if ($zeros < 512)
{
    print "Se agregaran: $zeros bytes\n";
  system("dd if=/dev/zero of=zeros bs=$zeros count=1");
  system("cat zeros >> kimg");
}
