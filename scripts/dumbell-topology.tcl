set ns [new Simulator]

# Configurations
set N 100
set ALPHA 0.5
set w_init 0.5
set linkBW 10Gb
set inputlinkBW 10Gb
set linkLatency 10us
set creditQueueCapacity [expr 84*2]  ;# bytes
set dataQueueCapacity [expr 1538*100] ;# bytes
set hostQueueCapacity [expr 1538*100] ;# bytes
set maxCrditBurst [expr 84*2] ;# bytes
set creditRate 64734895 ;# bytes / sec
set interflowDelay 0 ;# secs

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
  puts "Simluation terminated successfully."
  exit 0
}

puts "Creating Nodes..."
set left_gateway [$ns node]
set right_gateway [$ns node]

for {set i 0} {$i < $N} {incr i} {
  set left_node($i) [$ns node]
}

for {set i 0} {$i < $N} {incr i} {
  set right_node($i) [$ns node]
}

puts "Creating Links..."
Queue/DropTail set mean_pktsize_ 1538
Queue/DropTail set qlim_ [expr $hostQueueCapacity/1538]

Queue/XPassDropTail set credit_limit_ $creditQueueCapacity
Queue/XPassDropTail set data_limit_ $dataQueueCapacity
Queue/XPassDropTail set token_refresh_rate_ $creditRate

for {set i 0} {$i < $N} {incr i} {
  $ns simplex-link $left_node($i) $left_gateway $inputlinkBW $linkLatency DropTail
  $ns simplex-link $left_gateway $left_node($i) $inputlinkBW $linkLatency XPassDropTail

  $ns simplex-link $right_node($i) $right_gateway $inputlinkBW $linkLatency DropTail
  $ns simplex-link $right_gateway $right_node($i) $inputlinkBW $linkLatency XPassDropTail
}

$ns duplex-link $left_gateway $right_gateway $linkBW $linkLatency XPassDropTail
#$ns trace-queue $left_gateway $right_gateway $nt
#$ns trace-all $nt

puts "Creating Agents..."
Agent/XPass set max_credit_rate_ $creditRate
Agent/XPass set cur_credit_rate_ [expr $ALPHA*$creditRate]
Agent/XPass set w_ $w_init

for {set i 0} {$i < $N} {incr i} {
  set sender($i) [new Agent/XPass]
  set receiver($i) [new Agent/XPass]

  $ns attach-agent $left_node($i) $sender($i)
  $ns attach-agent $right_node($i) $receiver($i)

  $sender($i) set fid_ [expr $i]
  $receiver($i) set fid_ [expr $i]

  $ns connect $sender($i) $receiver($i)
}

puts "Simulation started."
for {set i 0} {$i < $N} {incr i} {
  $ns at 0.0 "$sender($i) advance-bytes 100000000"
}

$ns at 10.0 "finish"
$ns run
