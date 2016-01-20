#!/bin/bash
date

#make clean
#make
 
if [ $# != 3 ]; then
	echo "Shell Argument error!"
	echo "Usage: $0 IP PORT NUMBER. (eg: $0 192.168.30.19 5002 3)"
	exit
fi

echo "Server $1:$2"

declare -i num=$3
declare -i i=1
while  ((i <= num))
do
	declare filename=log$i.txt
	echo $filename
	./client $1 $2 > $filename &
	let ++i
done

while true;do
	running=$(ps -e | grep bin\/client $ | awk '{print $1}')
	echo -n "$(date +%T)  pids named sp: "
	#不能写成echo "$running"，不然多个id号会多行输出
	echo $running
	if [ -z "$running" ];then
		echo "All process named sp is finished."
		break
	fi
	sleep 1
done

date
