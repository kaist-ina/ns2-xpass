set ns [new Simulator]

# Configurations
set linkBW 10Gb
set linkLatency 10us
set creditQueueCapacity [expr 84*10] ;# Bytes
set dataQueueCapacity [expr 1538*100] ;# Bytes
set creditRate 64734895 ;# bytes/sec
set baseCreditRate 64734895 ;# bytes/sec
set K 65
set B 250
set B_host 1000
set expID 8536

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

puts "Creating Nodes..."
set node0 [$ns node]
set node1 [$ns node]

$ns trace-all $nt

puts "Creating Links..."
Queue/DropTail set mean_pktsize_ 1538
Queue/DropTail set limit_ $B_host

#Credit Setting
Queue/XPassRED set credit_limit_ $creditQueueCapacity
Queue/XPassRED set max_tokens_ [expr 2*84] 
Queue/XPassRED set token_refresh_rate_ $creditRate
#RED Setting
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

$ns duplex-link $node0 $node1 $linkBW $linkLatency XPassRED

puts "Creating Agents..."
Agent/TCP/FullTcp/XPass set min_credit_size_ 84
Agent/TCP/FullTcp/XPass set max_credit_size_ 84
Agent/TCP/FullTcp/XPass set min_ethernet_size_ 84
Agent/TCP/FullTcp/XPass set max_ethernet_size_ 1538
Agent/TCP/FullTcp/XPass set max_credit_rate_ $creditRate
Agent/TCP/FullTcp/XPass set base_credit_rate_ $baseCreditRate
Agent/TCP/FullTcp/XPass set target_loss_scaling_ 0.125
Agent/TCP/FullTcp/XPass set alpha_ 1.0
Agent/TCP/FullTcp/XPass set w_init_ 0.0625
Agent/TCP/FullTcp/XPass set min_w_ 0.01
Agent/TCP/FullTcp/XPass set retransmit_timeout_ 0.0001
Agent/TCP/FullTcp/XPass set min_jitter_ -0.1
Agent/TCP/FullTcp/XPass set max_jitter_ 0.1
Agent/TCP/FullTcp/XPass set exp_id_ $expID
Agent/TCP/FullTcp/XPass set default_credit_stop_timeout_ 0.002 ;# 2ms

Agent/TCP set ecn_ 1
Agent/TCP set old_ecn_ 1
Agent/TCP set packetSize_ 1454
Agent/TCP set window_ 180
Agent/TCP set slow_start_restart_ false
Agent/TCP set tcpTick_ 0.000001
Agent/TCP set mintro_ 0.004
Agent/TCP set rtxcur_init_ 0.001
Agent/TCP set windowOption_ 0
Agent/TCP set tcpip_base_hdr_size_ 84

Agent/TCP set dctcp_ true
Agent/TCP set dctcp_g_ 0.0625

Agent/TCP/FullTcp set segsize_ 1454
Agent/TCP/FullTcp set segsperack_ 1
Agent/TCP/FullTcp set spa_thresh_ 3000
Agent/TCP/FullTcp set interval_ 0
Agent/TCP/FullTcp set nodelay_ true
Agent/TCP/FullTcp set state_ 0
Agent/TCP/FullTcp set exp_id_ $expID

DelayLink set avoidReordering_ true

set agent0 [new Agent/TCP/FullTcp]
set agent1 [new Agent/TCP/FullTcp]

$agent1 listen

$ns attach-agent $node0 $agent0
$ns attach-agent $node1 $agent1

$ns connect $agent0 $agent1

puts "Simulation started."
$ns at 0.0 "$agent0 advance-bytes 30000"
$ns at 1.0 "finish"
$ns run
