if { $argc != 2 } {
	puts "Please check the input arguments\n"
	puts "Format: ns <file> <TCP Flavour> <Case Number>\n"
	exit 1
   }

set flavor [lindex $argv 0]
set case_number [lindex $argv 1]

#check valid tcp flavours
if {$flavor == "VEGAS"} {
	set TCP_Flavor "Vegas"
} elseif {$flavor == "SACK"} {
	set TCP_Flavor "Sack1"
} else {
	puts "Please enter valid TCP flavour\n"
	exit 1
}

#setup delays as specified in handouts
global delay1
global delay2
	
global TCP_Flavor

set delay1 5ms
set delay2 0

#check case number validity
if {$case_number == "1"} {
	set delay2 "12.5ms"
} elseif {$case_number == "2"} {
	set delay2 "20ms"
} elseif {$case_number == "3"} {
	set delay2 "27.5ms"
} else {
	puts "Please enter valid case number\n"
	exit 1
}

	
set ns [new Simulator]
set file "output_$flavor$case_number"



set trace_file1 [open output.tr w]
$ns trace-all $trace_file1

set nam_file1 [open output.nam w]
$ns namtrace-all $nam_file1


#define source nodes
set src1 [$ns node]
set src2 [$ns node]
#define routers
set R1 [$ns node]
set R2 [$ns node]
#define receiver nodes
set rcv1 [$ns node]
set rcv2 [$ns node]

#initialize variables
set total_sent_bytes1 0
set total_sent_bytes2 0
set iterations 0

#set labels for nodes
$ns at 0 "$src1 label src1"
$ns at 0 "$src2 label src2"
$ns at 0 "$R1 label R1"
$ns at 0 "$R2 label R2"
$ns at 0 "$rcv1 label rcv1"
$ns at 0 "$rcv2 label rcv2"

#set colors for packet flows
$ns color 0 Green
$ns color 1 Red

#setup tcp connections
set tcp_pipe1 [new Agent/TCP/$TCP_Flavor]
set tcp_pipe2 [new Agent/TCP/$TCP_Flavor]

#attaching tcp agents to source nodes
$ns attach-agent $src1 $tcp_pipe1
$ns attach-agent $src2 $tcp_pipe2

#setting the attribute for the tcp connection
$tcp_pipe1 set class_ 0
$tcp_pipe2 set class_ 1

#initializing links - duplex
$ns duplex-link $src1 $R1 10.0Mb $delay1 DropTail  
$ns duplex-link $rcv1 $R2 10.0Mb $delay1 DropTail
$ns duplex-link $src2 $R1 10.0Mb $delay2 DropTail  
$ns duplex-link $rcv2 $R2 10.0Mb $delay2 DropTail

$ns duplex-link $R1 $R2 1.0Mb $delay1 DropTail

#set orientation of the nodes
$ns duplex-link-op $R1 $R2 orient right

$ns duplex-link-op $src2 $R1 orient right-up
$ns duplex-link-op $src1 $R1 orient right-down

$ns duplex-link-op $R2 $rcv1 orient right-up
$ns duplex-link-op $R2 $rcv2 orient right-down

#setup FTP application on top of FTP
set ftp_src1 [new Application/FTP]
set ftp_src2 [new Application/FTP]

#attaching FTP agent to tcp pipeline
$ftp_src1 attach-agent $tcp_pipe1
$ftp_src2 attach-agent $tcp_pipe2

#setup sink
set sink_src1 [new Agent/TCPSink]
set sink_src2 [new Agent/TCPSink]

$ns attach-agent $rcv1 $sink_src1
$ns attach-agent $rcv2 $sink_src2

$ns connect $tcp_pipe1 $sink_src1
$ns connect $tcp_pipe2 $sink_src2

proc measure {} {
	set ns [Simulator instance]
	
	global sink_src1 sink_src2 
	global total_sent_bytes1 total_sent_bytes2 
	global iterations
	
	#set the step
	set time 0.1
	
	#store the number of bytes sent to the variable
	set sent_bytes1 [$sink_src1 set bytes_]	
    	set sent_bytes2 [$sink_src2 set bytes_]
	
	set current [$ns now]
	
	#calculate bytes to bits
	set temp1 [expr $sent_bytes1 * 8]
	set temp2 [expr $sent_bytes2 * 8]
	
	#calculate rate
	set total_sent_bytes1 [expr $total_sent_bytes1 + {$temp1/$time}] 
	set total_sent_bytes2 [expr $total_sent_bytes2 + {$temp2/$time}]
	
	set iterations [expr $iterations + 1]
	
	$sink_src1 set bytes_ 0
	$sink_src2 set bytes_ 0
	
	$ns at [expr $current + $time] "measure"
}
	

proc finish {} {
	global ns
	global trace_file1 nam_file1 
	global total_sent_bytes1 total_sent_bytes2 
	global iterations
	
	#stop trace and calculate the throughput values along with ratio
	puts "SRC1 Average Throughput = [expr $total_sent_bytes1/$iterations] bps\n"
	puts "SRC2 Average Throughput = [expr $total_sent_bytes2/$iterations] bps\n"
	puts "Throughput Ratio = [expr $total_sent_bytes1/$total_sent_bytes2] \n"
	
	$ns flush-trace
	#close respective files
	close $trace_file1
	close $nam_file1
	#open nam instance
	exec nam output.nam &
	exit 0
}
	
	
#start sending at 0
$ns at 0 "$ftp_src1 start"
$ns at 0 "$ftp_src2 start"

#start measurement at 100
$ns at 100 "measure"

#stop sending at 400
$ns at 400 "$ftp_src1 stop"
$ns at 400 "$ftp_src2 stop"

$ns at 400 "finish"


set end_file1 [open "$file--src1.tr" w]
$ns trace-queue  $src1  $R1  $end_file1

set end_file2 [open "$file--src2.tr" w]
$ns trace-queue  $src2  $R1  $end_file2

$ns run


