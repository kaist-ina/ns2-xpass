set ns [new Simulator]

if {$argc < 1} {
  puts "USAGE: ./ns scripts/dumbell-topology.tcl {expriment_id}"
  exit 1
}

# Configurations
set N 10
set ALPHA 0.5
set w_init 0.0625
set linkBW 40Gb
set inputlinkBW1 10Gb
set inputlinkBW2 40Gb
set linkLatency 10us
set creditQueueCapacity [expr 84*10]  ;# bytes
set dataQueueCapacity [expr 1538*100] ;# bytes
set hostQueueCapacity [expr 1538*100] ;# bytes
set maxCrditBurst [expr 84*2] ;# bytes
set basecreditRate 64734895
set creditRate0 [expr 64734895*4]
set creditRate1 [expr 64734895*1] ;# bytes / sec
set creditRate2 [expr 64734895*4] ;# bytes / sec
set interFlowDelay 0.01 ;# secs
set expID [expr int([lindex $argv 0])]

set K 65
set B 250

Agent/TCP set dctcp_ true
Agent/TCP set dctcp_g_ 0.0625

Agent/TCP/FullTcp set segsize_ 1454
Agent/TCP/FullTcp set segsperack_ 1
Agent/TCP/FullTcp set spa_thresh_ 3000
Agent/TCP/FullTcp set interval_ 0
Agent/TCP/FullTcp set nodelay_ true
Agent/TCP/FullTcp set state_ 0

DelayLink set avoidReordering_ true


# Output file
file mkdir "outputs"
set nt [open outputs/trace_$expID.out w]
set fct_out [open outputs/fct_$expID.out w]
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

#Queue/XPassDropTail set credit_limit_ $creditQueueCapacity
#Queue/XPassDropTail set data_limit_ $dataQueueCapacity
#Queue/XPassDropTail set token_refresh_rate_ $creditRate

Queue/XPassRED set credit_limit_ $creditQueueCapacity
Queue/XPassRED set max_tokens_ [expr 2*84] 
Queue/XPassRED set token_refresh_rate_ $creditRate1

Queue/XPassRED set bytes_ true
Queue/XPassRED set queue_in_bytes_ true
Queue/XPassRED set mean_pktsize_ 1538
Queue/XPassRED set setbit_ true
Queue/XPassRED set gentle_ false
Queue/XPassRED set q_weight_ 1.0
Queue/XPassRED set use_mark_p true
Queue/XPassRED set mark_p_ 2.0
Queue/XPassRED set thresh_ $K
Queue/XPassRED set maxthresh_ $K
Queue/XPassRED set limit_ $B

for {set i 0} {$i < $N} {incr i} {
  if {$i<9} {
    $ns simplex-link $left_node($i) $left_gateway $inputlinkBW1 $linkLatency DropTail
    $ns simplex-link $left_gateway $left_node($i) $inputlinkBW1 $linkLatency XPassRED

    $ns simplex-link $right_node($i) $right_gateway $inputlinkBW1 $linkLatency DropTail
    $ns simplex-link $right_gateway $right_node($i) $inputlinkBW1 $linkLatency XPassRED

    [[$ns link $left_gateway $left_node($i)] queue] set token_refresh_rate_ $creditRate1
    [[$ns link $right_gateway $right_node($i)] queue] set token_refresh_rate_ $creditRate1
  } else {
    $ns simplex-link $left_node($i) $left_gateway $inputlinkBW2 $linkLatency DropTail
    $ns simplex-link $left_gateway $left_node($i) $inputlinkBW2 $linkLatency XPassRED

    $ns simplex-link $right_node($i) $right_gateway $inputlinkBW2 $linkLatency DropTail
    $ns simplex-link $right_gateway $right_node($i) $inputlinkBW2 $linkLatency XPassRED

    [[$ns link $left_gateway $left_node($i)] queue] set token_refresh_rate_ $creditRate2
    [[$ns link $right_gateway $right_node($i)] queue] set token_refresh_rate_ $creditRate2
    [[$ns link $left_gateway $left_node($i)] queue] set limit_  [expr $B * 4]
    [[$ns link $right_gateway $right_node($i)] queue] set limit_ [expr $B * 4]
    [[$ns link $left_gateway $left_node($i)] queue] set thresh_  [expr $K * 4]
    [[$ns link $right_gateway $right_node($i)] queue] set thresh_ [expr $K * 4]
    [[$ns link $left_gateway $left_node($i)] queue] set maxthresh_ [expr $K * 4]
    [[$ns link $right_gateway $right_node($i)] queue] set maxthresh_ [expr $K * 4]
  }
}

$ns duplex-link $left_gateway $right_gateway $linkBW $linkLatency XPassRED
[[$ns link $left_gateway $right_gateway] queue] set token_refresh_rate_ $creditRate0
[[$ns link $right_gateway $left_gateway] queue] set token_refresh_rate_ $creditRate0
[[$ns link $left_gateway $right_gateway] queue] set limit_  [expr $B * 4]
[[$ns link $right_gateway $left_gateway] queue] set limit_ [expr $B * 4]
[[$ns link $left_gateway $right_gateway] queue] set thresh_  [expr $K * 4]
[[$ns link $right_gateway $left_gateway] queue] set thresh_ [expr $K * 4]
[[$ns link $left_gateway $right_gateway] queue] set maxthresh_ [expr $K * 4]
[[$ns link $right_gateway $left_gateway] queue] set maxthresh_ [expr $K * 4]



$ns trace-queue $left_gateway $right_gateway $nt

puts "Creating Agents..."
#Agent/XPass set max_credit_rate_ $creditRate
Agent/XPass set cur_credit_rate_ [expr $ALPHA*$basecreditRate]
Agent/XPass set w_ $w_init
Agent/XPass set exp_id_ $expID
Agent/XPass set target_loss_scaling_ 0.125
Agent/XPass set base_credit_rate_ $creditRate1
#Agent/TCP/FullTcp/XPass set cur_credit_rate_ [expr $ALPHA*$basecreditRate]

Agent/TCP/FullTcp/XPass set w_init_ $w_init
Agent/TCP/FullTcp/XPass set exp_id_ $expID
Agent/TCP/FullTcp/XPass set target_loss_scaling_ 0.125
Agent/TCP/FullTcp/XPass set base_credit_rate_ $creditRate1

Agent/TCP/FullTcp/XPass set alpha_ $ALPHA
Agent/TCP/FullTcp/XPass set min_credit_size_ 78
Agent/TCP/FullTcp/XPass set max_credit_size_ 90
Agent/TCP/FullTcp/XPass set min_ethernet_size_ 78
Agent/TCP/FullTcp/XPass set max_ethernet_size_ 1538
Agent/TCP/FullTcp/XPass set w_init_ 0.0625
Agent/TCP/FullTcp/XPass set min_w_ 0.01
Agent/TCP/FullTcp/XPass set retransmit_timeout_ 0.001
Agent/TCP/FullTcp/XPass set default_credit_stop_timeout_ 0.001
Agent/TCP/FullTcp/XPass set min_jitter_ -0.1
Agent/TCP/FullTcp/XPass set max_jitter_ 0.1
Agent/TCP/FullTcp/XPass set xpass_hdr_size_ 78
Agent/TCP/FullTcp/XPass set max_credit_rate_ $creditRate1

for {set i 0} {$i < $N} {incr i} {
  set sender($i) [new Agent/TCP/FullTcp/XPass]
  set receiver($i) [new Agent/TCP/FullTcp/XPass]

  $ns attach-agent $left_node($i) $sender($i)
  $ns attach-agent $right_node($i) $receiver($i)

  if {$i<9} {
    $receiver($i) set max_credit_rate_ $creditRate1
    $sender($i) set max_credit_rate_ $creditRate1
  } else {
    $receiver($i) set max_credit_rate_ $creditRate2
    $receiver($i) set dctcp_g_ 0.03125
    $sender($i) set max_credit_rate_ $creditRate2
    $sender($i) set dctcp_g_ 0.03125
  }

  $sender($i) set fid_ [expr $i]
  $receiver($i) set fid_ [expr $i]
  $receiver($i) listen
 $ns connect $sender($i) $receiver($i)
 
}

puts "Simulation started."
set nextTime 0.0
for {set i 0} {$i < $N} {incr i} {
  $ns at $nextTime "$sender($i) advance-bytes 10000000000"
  set nextTime [expr $nextTime + $interFlowDelay]
}

$ns at 1.5 "finish"
$ns run
