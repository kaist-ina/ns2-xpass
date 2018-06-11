set ns [new Simulator]

if {$argc < 2} {
  puts "USAGE: ./ns scripts/large-scale-fattree.tcl {experiment_id} {DEPLOY_STEP}"
  puts "DEPLOY_STEP: 0, 25, 50, 75, 100"
  exit 1
}
# Configurations
set linkRate 10 ;# Gb
set linkRateHigh 40 ;#Gb
set hostDelay 0.000002
set linkDelayHostTor 0.000004
set linkDelayTorAggr 0.000004
set linkDelayAggrCore 0.000004
set creditQueueCapacity [expr 84*10] ;# Bytes
set creditRate 64734895 ;# bytes/sec
set creditRateHigh [expr 64734895*4] ;# bytes/sec for 40Gbps link
set baseCreditRate 64734895 ;# bytes/sec
set K 65
set B 250
set B_host 1000
set numFlow 10 ;#100000
set workload "cachefollower" ;# cachefollower, mining, search, webserver
set linkLoad 0.6 ;# ranges from 0.0 to 1.0
set expID [expr int([lindex $argv 0])]
set deployStep [expr int(int([lindex $argv 1])/25)]
puts "expID = $expID"
puts "deployStep = $deployStep"

# Toplogy configurations
set dataBufferHost [expr 1000*1538] ;# bytes / port
set dataBufferFromTorToAggr [expr 250*1538] ;# bytes / port
set dataBufferFromAggrToCore [expr 250*1538] ;# bytes / port
set dataBufferFromCoreToAggr [expr 250*1538] ;# bytes / port
set dataBufferFromAggrToTor [expr 250*1538] ;# bytes / port
set dataBufferFromTorToHost [expr 250*1538] ;# bytes / port

set numCore 8 ;# number of core switches
set numAggr 16 ;# number of aggregator switches
set numTor 32 ;# number of ToR switches
set numNode 192 ;# number of nodes

# XPass configurations
set alpha 0.5
set w_init 0.0625
set creditBuffer [expr 84*8]
set maxCreditBurst [expr 84*2]
set minJitter -0.1
set maxJitter 0.1
set minEthernetSize 84
set maxEthernetSize 1538
set minCreditSize 76
set maxCreditSize 92
set xpassHdrSize 78
set maxPayload [expr $maxEthernetSize-$xpassHdrSize]
set avgCreditSize [expr ($minCreditSize+$maxCreditSize)/2.0]
set creditBW [expr $linkRate*125000000*$avgCreditSize/($avgCreditSize+$maxEthernetSize)]
set creditBW [expr int($creditBW)]

# Simulation setup
set simStartTime 0.1
set simEndTime 60


# Output file
file mkdir "outputs"
set nt [open "outputs/trace_$expID.out" w]
set fct_out [open "outputs/fct_$expID.out" w]
set wst_out [open "outputs/waste_$expID.out" w]
puts $fct_out "Flow ID,Flow Size (bytes),Flow Completion Time (secs)"
puts $wst_out "Flow ID,Flow Size (bytes),Wasted Credit"
close $fct_out
close $wst_out

set flowfile [open flowfile.tr w]

proc finish {} {
  global ns nt flowfile
  $ns flush-trace
  close $nt
  close $flowfile
  puts "Simulation terminated successfully."
  exit 0
}
$ns trace-all $nt

# Basic parameter settings
Agent/XPass set min_credit_size_ $minCreditSize
Agent/XPass set max_credit_size_ $maxCreditSize
Agent/XPass set min_ethernet_size_ $minEthernetSize
Agent/XPass set max_ethernet_size_ $maxEthernetSize
Agent/XPass set max_credit_rate_ $creditBW
Agent/XPass set alpha_ $alpha
Agent/XPass set target_loss_scaling_ 0.125
Agent/XPass set w_init_ $w_init
Agent/XPass set min_w_ 0.01
Agent/XPass set retransmit_timeout_ 0.0001
Agent/XPass set min_jitter_ $minJitter
Agent/XPass set max_jitter_ $maxJitter
Agent/XPass set exp_id_ $expID

Queue/XPassRED set credit_limit_ $creditBuffer
Queue/XPassRED set max_tokens_ $maxCreditBurst
Queue/XPassRED set token_refresh_rate_ $creditBW

DelayLink set avoidReordering_ true
$ns rtproto DV
Agent/rtProto/DV set advertInterval 10
Node set multiPath_ 1
Classifier/MultiPath set symmetric_ true
Classifier/MultiPath set nodetype_ 0

# Workloads setting
if {[string compare $workload "mining"] == 0} {
  set workloadPath "workloads/workload_mining.tcl"
  set avgFlowSize 7410212
} elseif {[string compare $workload "search"] == 0} {
  set workloadPath "workloads/workload_search.tcl"
  set avgFlowSize 1654275
} elseif {[string compare $workload "cachefollower"] == 0} {
  set workloadPath "workloads/workload_cachefollower.tcl"
  set avgFlowSize 701490
} elseif {[string compare $workload "webserver"] == 0} {
  set workloadPath "workloads/workload_webserver.tcl"
  set avgFlowSize 63735
} else {
  puts "Invalid workload: $workload"
  exit 0
}

set overSubscRatio [expr double($numNode/$numTor)/double($numTor/$numAggr)]
set lambda [expr ($numNode*$linkRate*1000000000*$linkLoad)/($avgFlowSize*8.0/$maxPayload*$maxEthernetSize)]
set avgFlowInterval [expr $overSubscRatio/$lambda]

# Random number generators
set RNGFlowSize [new RNG]
$RNGFlowSize seed 61569011

set RNGFlowInterval [new RNG]
$RNGFlowInterval seed 94762103

set RNGSrcNodeId [new RNG]
$RNGSrcNodeId seed 17391005

set RNGDstNodeId [new RNG]
$RNGDstNodeId seed 35010256

set randomFlowSize [new RandomVariable/Empirical]
$randomFlowSize use-rng $RNGFlowSize
$randomFlowSize set interpolation_ 2
$randomFlowSize loadCDF $workloadPath

set randomFlowInterval [new RandomVariable/Exponential]
$randomFlowInterval use-rng $RNGFlowInterval
$randomFlowInterval set avg_ $avgFlowInterval

set randomSrcNodeId [new RandomVariable/Uniform]
$randomSrcNodeId use-rng $RNGSrcNodeId
$randomSrcNodeId set min_ 0
$randomSrcNodeId set max_ $numNode

set randomDstNodeId [new RandomVariable/Uniform]
$randomDstNodeId use-rng $RNGDstNodeId
$randomDstNodeId set min_ 0
$randomDstNodeId set max_ $numNode

# Node
puts "Creating nodes..."
for {set i 0} {$i < $numNode} {incr i} {
  set dcNode($i) [$ns node]
  $dcNode($i) set nodetype_ 1
}
for {set i 0} {$i < $numTor} {incr i} {
  set dcTor($i) [$ns node]
  $dcTor($i) set nodetype_ 2
}
for {set i 0} {$i < $numAggr} {incr i} {
  set dcAggr($i) [$ns node]
  $dcAggr($i) set nodetype_ 3
}
for {set i 0} {$i < $numCore} {incr i} {
  set dcCore($i) [$ns node]
  $dcCore($i) set nodetype_ 4
}

# Link
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

for {set i 0} {$i < $numAggr} {incr i} {
  set coreIndex [expr $i%2]
  for {set j $coreIndex} {$j < $numCore} {incr j 2} {
    $ns simplex-link $dcAggr($i) $dcCore($j) [set linkRateHigh]Gb $linkDelayAggrCore XPassRED
    set link_aggr_core [$ns link $dcAggr($i) $dcCore($j)]
    set queue_aggr_core [$link_aggr_core queue]
    $queue_aggr_core set data_limit_ $dataBufferFromAggrToCore

    $ns simplex-link $dcCore($j) $dcAggr($i) [set linkRateHigh]Gb $linkDelayAggrCore XPassRED
    set link_core_aggr [$ns link $dcCore($j) $dcAggr($i)]
    set queue_core_aggr [$link_core_aggr queue]
    $queue_core_aggr set data_limit_ $dataBufferFromCoreToAggr
  }
}

set traceQueueCnt 0

for {set i 0} {$i < $numTor} {incr i} {
  set aggrIndex [expr $i/4*2]
  for {set j $aggrIndex} {$j <= $aggrIndex+1} {incr j} {
    set cLinkRate $linkRate
    if {([expr $i/4] == 0) || ([expr $i/4] == 2)} {
      set cLinkRate $linkRateHigh
    }
    $ns simplex-link $dcTor($i) $dcAggr($j) [set cLinkRate]Gb $linkDelayTorAggr XPassRED
    set link_tor_aggr [$ns link $dcTor($i) $dcAggr($j)]
    set queue_tor_aggr [$link_tor_aggr queue]
    $queue_tor_aggr set data_limit_ $dataBufferFromTorToAggr
    $queue_tor_aggr set trace_ 1
    $queue_tor_aggr set qidx_ $traceQueueCnt
    $queue_tor_aggr set exp_idx_ $expID
    set traceQueueCnt [expr $traceQueueCnt + 1]

    $ns simplex-link $dcAggr($j) $dcTor($i) [set cLinkRate]Gb $linkDelayTorAggr XPassRED
    set link_aggr_tor [$ns link $dcAggr($j) $dcTor($i)]
    set queue_aggr_tor [$link_aggr_tor queue]
    $queue_aggr_tor set data_limit_ $dataBufferFromAggrToTor
  }
}

for {set i 0} {$i < $numNode} {incr i} {
  set torIndex [expr $i/($numNode/$numTor)]
  set cLinkRate $linkRate
  if {([expr $torIndex/4] == 0) || ([expr $torIndex/4] == 2)} {
    set cLinkRate $linkRateHigh
  }
  $ns simplex-link $dcNode($i) $dcTor($torIndex) [set cLinkRate]Gb [expr $linkDelayHostTor+$hostDelay] XPassRED
  set link_host_tor [$ns link $dcNode($i) $dcTor($torIndex)]
  set queue_host_tor [$link_host_tor queue]
  $queue_host_tor set data_limit_ $dataBufferHost
  $queue_host_tor set trace_ 1
  $queue_host_tor set qidx_ $traceQueueCnt
  $queue_tor_aggr set exp_idx_ $expID
  set traceQueueCnt [expr $traceQueueCnt + 1]

  $ns simplex-link $dcTor($torIndex) $dcNode($i) [set cLinkRate]Gb $linkDelayHostTor XPassRED
  set link_tor_host [$ns link $dcTor($torIndex) $dcNode($i)]
  set queue_tor_host [$link_tor_host queue]
  $queue_tor_host set data_limit_ $dataBufferFromTorToHost
}

puts "Creating agents and flows..."
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

DelayLink set avoidReordering_ true

for {set i 0} {$i < $numFlow} {incr i} {
  set src_nodeid [expr int([$randomSrcNodeId value])]
  set dst_nodeid [expr int([$randomDstNodeId value])]

  while {$src_nodeid == $dst_nodeid} {
#    set src_nodeid [expr int([$randomSrcNodeId value])]
    set dst_nodeid [expr int([$randomDstNodeId value])]
  }

  set src_cluster [expr $src_nodeid/($numNode/$numTor)/4]
  set dst_cluster [expr $dst_nodeid/($numNode/$numTor)/4]
  puts "src cluster=$src_cluster, dst cluster=$dst_cluster"

  set sender($i) [new Agent/TCP/FullTcp]
  set receiver($i) [new Agent/TCP/FullTcp]
  if {$src_cluster < [expr 2*$deployStep]} {
    puts "src cluster : xpass" 
    set sender($i) [new Agent/TCP/FullTcp/XPass]
  } else {
    puts "src cluster : dctcp"
    set sender($i) [new Agent/TCP/FullTcp]
  }
  if {$dst_cluster < [expr 2*$deployStep]} {
    puts "dst cluster : xpass" 
    set receiver($i) [new Agent/TCP/FullTcp/XPass]
  } else {
    puts "dst cluster : dctcp" 
    set receiver($i) [new Agent/TCP/FullTcp]
  }

  if {$src_cluster == 0 || $src_cluster == 2} {
    puts "send - rateHig : $creditRateHigh"
    $sender($i) set max_credit_rate_ $creditRateHigh
  }

  if {$dst_cluster == 0 || $dst_cluster == 2} {
    puts "recv - rateHig : $creditRateHigh"
    $receiver($i) set max_credit_rate_ $creditRateHigh
  }

  $sender($i) set fid_ $i
  $sender($i) set host_id_ $src_nodeid
  $receiver($i) set fid_ $i
  $receiver($i) set host_id_ $dst_nodeid
 
  $receiver($i) listen

  $ns attach-agent $dcNode($src_nodeid) $sender($i)
  $ns attach-agent $dcNode($dst_nodeid) $receiver($i)
 
  $ns connect $sender($i) $receiver($i)

  $ns at $simEndTime "$sender($i) close"
  $ns at $simEndTime "$receiver($i) close"

  set srcIndex($i) $src_nodeid
  set dstIndex($i) $dst_nodeid
}

set nextTime $simStartTime
set fidx 0

proc sendBytes {} {
  global ns random_flow_size nextTime sender fidx randomFlowSize randomFlowInterval numFlow srcIndex dstIndex flowfile
  while {1} {
    set fsize [expr ceil([expr [$randomFlowSize value]])]
    if {$fsize > 0} {
      break;
    }
  }

  puts $flowfile "$nextTime $srcIndex($fidx) $dstIndex($fidx) $fsize"
  puts "$nextTime $srcIndex($fidx) $dstIndex($fidx) $fsize"
  
  $ns at $nextTime "$sender($fidx) advance-bytes $fsize"

  set nextTime [expr $nextTime+[$randomFlowInterval value]]
  set fidx [expr $fidx+1]

  if {$fidx < $numFlow} {
    $ns at $nextTime "sendBytes"
  }
}

$ns at 0.0 "puts \"Simulation starts!\""
$ns at $nextTime "sendBytes"
$ns at [expr $simEndTime+1] "finish"
$ns run
