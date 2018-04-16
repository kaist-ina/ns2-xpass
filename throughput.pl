$infile='outputs/trace_0.out';
$numflow=2;
$srcnode=0;
$dstnode=1;
$gran=0.001; # 1ms


@sum = (0) x $numflow;
$clock=0;

open (DATA, "<$infile") || die "Can't open $infile $!";

while (<DATA>) {
@x = split(' ');

# x[1]: time
if ($x[1]-$clock <= $gran) {
# x[0]: type
if ($x[0] eq '-') {
# x[2]: srcnode
if ($x[2] eq $srcnode) {
# x[3]: dstnode
if ($x[3] eq $dstnode) {
# x[4]: pkt type
if ($x[4] eq 'XPASS_DATA') {
# x[7]: flow id
if ($x[7]<$numflow) {
  $sum[$x[7]]=$sum[$x[7]]+$x[5]
}
}
}
}
}
} else {
  $bandwidth=0;
  print STDOUT "$x[1]";
  for($i=0;$i<$numflow;$i++) {
    $bandwidth=$bandwidth + $sum[$i];
    $throughput=$sum[$i]/$gran*8/1000/1000/1000;
    print STDOUT " $throughput";
    $sum[$i]=0;
  }
  $throughput=$bandwidth/$gran*8/1000/1000/1000;
  print STDOUT " $throughput\n";
  $clock=$clock+$gran
}
}
$bandwidth=0;
print STDOUT "$x[1]";
for($i=0;$i<$numflow;$i++) {
  $bandwidth=$bandwidth+$sum[$i];
  $throughput=$sum[$i]/$gran*8/1000/1000/1000;
  print STDOUT " $throughput";
  $sum[$i]=0;
}
$throughput=$bandwidth/$gran*8/1000/1000/1000;
print STDOUT " $throughput\n";
close DATA;
exit(0);
