# wing\wing_process

This library provides a better API to work with daemon processes on windows systems using PHP.

## Installation

The dll is available on [dll download](https://github.com/jilieryuyi/wing_process/blob/master/Release/wing_process.dll). You can install it on your windows system using php7.0.*.

##Build

php version 7.0.9 ZTS
visual stdio 2015(vc14)

##Demo dir

Release/tests

##Interface

	interface wing_process{
		/**
		 * @__construct all file path onle support full path
		 *
		 * @param string|int $mixed command or process_id
		 * @param string $output_file process output will write into this file
		 */
		public function __construct( $mixed, $output_file = '');
		/**
		 * @run as deamon process
		 */
		public function run();

		public function wait();
		public function getProcessId();
		public function getThreadId();
		public function getCommandLine();
		public function kill();
		public function getMemory();
	}
	
##Demo

####run.php
	$count = 0;
	while( 1 )
	{
		echo $count,"\r\n";
		sleep(1);
		$count++;
	}

####process.php
	//run .php file as a deamon process
	$process = new \wing\wing_process(__DIR__."/run.php",__DIR__."/process.log");
	echo "process_id=",$process->run(),"\r\n";
	echo "process_id=",$process->getProcessId(),"\r\n";
	echo "thread_id=",$process->getThreadId(),"\r\n";
	echo "command line=",$process->getCommandLine(),"\r\n";
	file_put_contents("process.pid",$process->getProcessId());

	//var_dump($process->kill());

	//$exit = $process->wait();
	//echo "process exit code=",$exit,"\r\n";

	/*$process = new wing_process("D:/Wing/Release/Wing.exe",__DIR__."/Wing.log");
	echo "process_id=",$process->run(),"\r\n";*/

	
####create_process_by_pid.php
	$process_id = file_get_contents("process.pid");
	//create process by pid
	$process = new \wing\wing_process($process_id);
	echo "process_id=",$process->getProcessId(),"\r\n";
	echo "thread_id=",$process->getThreadId(),"\r\n";
	echo "command line=",$process->getCommandLine(),"\r\n";
	//$process->kill();
	