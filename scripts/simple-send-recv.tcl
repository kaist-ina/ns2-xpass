set ns [new Simulator]

set bandwidth 10Gb
set linkLatency 10us

file mkdir "outputs"

set nt [open outputs/trace.tr w]
set agent0 [new Agent/XPass]
set agent1 [new Agent/XPass]

Node set multiPath_ 1

set node0 [$ns node]
set node1 [$ns node]

proc finish {} {
  global ns nt
  $ns flush-trace
  close $nt
  exit 0
}

$ns trace-all $nt

$ns duplex-link $node0 $node1 $bandwidth $linkLatency XPassDropTail

$ns attach-agent $node0 $agent0
$ns attach-agent $node1 $agent1

$ns connect $agent0 $agent1

$ns at 0.0 "$agent0 advance-bytes 1000000000"
$ns at 1.0 "finish"
$ns run
