set ns [new Simulator]

# Configurations
set linkBW 10Gb
set linkLatency 10us
set creditQueueCapacity [expr 84*10] ;# Bytes
set dataQueueCapacity [expr 1538*100] ;# Bytes
set creditRate 64734895 ;# bytes/sec

# Output file
file mkdir "outputs"
set nt [open outputs/trace.out w]
set fct_out [open outputs/fct.out w]
puts $fct_out "Flow ID,Flow Size (bytes),Flow Completion Time (secs)"
close $fct_out

proc finish {} {
  global ns nt
  $ns flush-trace
  close $nt
  puts "Simulation terminated successfully."
  exit 0
}

puts "Creating Nodes..."
set node0 [$ns node]
set node1 [$ns node]

$ns trace-all $nt

puts "Creating Links..."
Queue/XPassDropTail set credit_limit_ $creditQueueCapacity
Queue/XPassDropTail set data_limit_ $dataQueueCapacity
Queue/XPassDropTail set token_refresh_rate_ $creditRate

$ns duplex-link $node0 $node1 $linkBW $linkLatency XPassDropTail

puts "Creating Agents..."
set agent0 [new Agent/XPass]
set agent1 [new Agent/XPass]

$ns attach-agent $node0 $agent0
$ns attach-agent $node1 $agent1

$ns connect $agent0 $agent1

puts "Simulation started."
$ns at 0.0 "$agent0 advance-bytes 1000000000"
$ns at 2.0 "finish"
$ns run
