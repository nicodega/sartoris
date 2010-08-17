#!/usr/bin/perl

@Info = stat("kimg");

# Calculate how much we need to reach a 512 multiple

$size = $Info[7];
$zeros = 0;

print "Size: $size bytes\n";

$zeros = 512 - ($size % 512); 

if ($zeros < 512)
{
    print "Se agregaran: $zeros bytes\n";
    system("dd if=/dev/zero of=zeros bs=$zeros count=1");
    system("cat zeros >> kimg");
}
