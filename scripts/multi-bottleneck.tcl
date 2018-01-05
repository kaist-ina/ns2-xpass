set ns [new Simulator]

# Configurations
set N 64
set ALPHA [expr 1./32.]
set w_init [expr 1./32.]
set creditQueueCapacity [expr 84*2]  ;# bytes
set hostQueueCapacity [expr 1538*100]  ;# bytes
set dataQueueCapacity [expr 1538*100] ;# bytes

set maxCrditBurst [expr 84*2] ;# bytes
set creditRate 64734895 ;# bytes / sec
set interFlowDelay 0 ;# secs

#
# Toplogy configurations
#
set linkRate 10 ;# Gb
set hostDelay 0.000001 ;# secs
set linkDelayRouterHost   0.000004 ;#secs
set linkDelayRouterRouter 0.000004 ;#secs

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

#$ns trace-all $nt

puts "Creating Nodes..."

set router_left [$ns node]
set router_right [$ns node]
set sender_host [$ns node]
for {set i 0} {$i <= $N} {incr i} {
	set host($i) [$ns node]
}

puts "Creating Links..."
Queue/DropTail set mean_pktsize_ 1538
Queue/DropTail set qlim_ [expr $hostQueueCapacity/1538]

Queue/XPassDropTail set credit_limit_ $creditQueueCapacity
Queue/XPassDropTail set data_limit_ $dataQueueCapacity
Queue/XPassDropTail set token_refresh_rate_ $creditRate

$ns duplex-link $router_left $router_right [set linkRate]Gb $linkDelayRouterRouter XPassDropTail
$ns duplex-link $router_right $sender_host [set linkRate]Gb $linkDelayRouterHost XPassDropTail
$ns duplex-link $router_right $host(0) [set linkRate]Gb $linkDelayRouterHost XPassDropTail
$ns trace-queue $sender_host $router_right $nt
		
for {set i 1} {$i <= $N} {incr i} {
	$ns duplex-link $router_left $host($i) [set linkRate]Gb $linkDelayRouterHost XPassDropTail
}

puts "Creating Agents..."
Agent/XPass set max_credit_rate_ $creditRate
#Agent/XPass set cur_credit_rate_ [expr $ALPHA*$creditRate]
Agent/XPass set alpha_ $ALPHA
Agent/XPass set w_ $w_init

for {set i 0} {$i <= $N} {incr i} {
  set sender($i) [new Agent/XPass]
  set receiver($i) [new Agent/XPass]
  $sender($i) set fid_ $i
  $receiver($i) set fid_ $i
  $ns attach-agent $sender_host $sender($i)
  $ns attach-agent $host($i) $receiver($i)
  $ns connect $sender($i) $receiver($i)
}

puts "Simulation started."
set nextTime 0.0
for {set i 0} {$i <= $N} {incr i} {
  $ns at $nextTime "$sender($i) advance-bytes 100000000"
  set nextTime [expr $nextTime + $interFlowDelay]
}

$ns at 10.0 "finish"
$ns run
