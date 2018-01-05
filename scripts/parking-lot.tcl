set ns [new Simulator]

# Configurations
set N 6
set ALPHA [expr 1./32.]
set w_init [expr 1./32.]
set creditQueueCapacity [expr 84*2]  ;# bytes
set dataQueueCapacity [expr 1538*100] ;# bytes
set hostQueueCapacity [expr 1538*100] ;# bytes
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
set dataBufferHost [expr 1000*1538] ;# bytes / port
set dataBufferRouterHost [expr 250*1538] ;#bytes / port
set dataBufferRouterRouter [expr 250*1538] ;# bytes / port

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

set leftTerm [$ns node]
set rightTerm [$ns node]
for {set i 0} {$i < $N} {incr i} {
	set host($i) [$ns node]
	set router($i) [$ns node]
}

puts "Creating Links..."
Queue/DropTail set mean_pktsize_ 1538
Queue/DropTail set qlim_ [expr $hostQueueCapacity/1538]

Queue/XPassDropTail set credit_limit_ $creditQueueCapacity
Queue/XPassDropTail set data_limit_ $dataQueueCapacity
Queue/XPassDropTail set token_refresh_rate_ $creditRate

for {set i 0} {$i < $N} {incr i} {
	if {$i > 0} {
		$ns duplex-link $router([expr $i - 1]) $router($i) [set linkRate]Gb $linkDelayRouterRouter XPassDropTail
		set link_rl [$ns link $router([expr $i - 1]) $router($i)]
		set queue_rl [$link_rl queue]
		$queue_rl set data_limit_ $dataBufferRouterRouter
		$ns trace-queue $router([expr $i - 1]) $router($i) $nt
	}	
		
	$ns duplex-link $host($i) $router($i) [set linkRate]Gb $linkDelayRouterHost XPassDropTail
	set link_ll [$ns link $host($i) $router($i)]
	set queue_ll [$link_ll queue]
	$queue_ll set data_limit_ $dataBufferRouterHost
}

$ns duplex-link $leftTerm $router(0) [set linkRate]Gb $linkDelayRouterHost XPassDropTail
set link_lt [$ns link $leftTerm $router(0)]
set queue_lt [$link_lt queue]
$queue_lt set data_limit_ $dataBufferRouterHost

$ns duplex-link $rightTerm $router([expr $N - 1]) [set linkRate]Gb $linkDelayRouterHost XPassDropTail
set link_rt [$ns link $rightTerm $router([expr $N - 1])]
set queue_rt [$link_rt queue]
$queue_rt set data_limit_ $dataBufferRouterHost

puts "Creating Agents..."
Agent/XPass set max_credit_rate_ $creditRate
#Agent/XPass set cur_credit_rate_ [expr $ALPHA*$creditRate]
Agent/XPass set alpha_ $ALPHA
Agent/XPass set w_ $w_init

for {set i 0} {$i < [expr $N - 1]} {incr i} {
  set sender($i) [new Agent/XPass]
  set receiver($i) [new Agent/XPass]
  $sender($i) set fid_ $i
  $receiver($i) set fid_ $i
  $ns attach-agent $host($i) $sender($i)
  $ns attach-agent $host([expr $i+ 1]) $receiver($i)
  $ns connect $sender($i) $receiver($i)
}
set longest [expr $N - 1]
set sender($longest) [new Agent/XPass]
set receiver($longest) [new Agent/XPass]
$sender($longest) set fid_ $longest
$receiver($longest) set fid_ $longest
$ns attach-agent $leftTerm $sender($longest)
$ns attach-agent $rightTerm $receiver($longest)
$ns connect $sender($longest) $receiver($longest)


puts "Simulation started."
set nextTime 0.0
for {set i 0} {$i < $N} {incr i} {
  $ns at $nextTime "$sender($i) advance-bytes 100000000"
  set nextTime [expr $nextTime + $interFlowDelay]
}

$ns at 10.0 "finish"
$ns run
