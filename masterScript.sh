#echo ""
#echo "Starting initial startup for the reliable server."
#echo ""

# master process
./master -h lenss-comp1.cse.tamu.edu -p 4632 &
export p1=$!

# master replica process
./master -h lenss-comp1.cse.tamu.edu -p 4633 -r replica -m 4632 &
export p2=$!

# worker process
./worker -h lenss-comp1.cse.tamu.edu -p 4634 -m lenss-comp1.cse.tamu.edu -a 4632 -c 0 &
export p3=$!

# worker process
./worker -h lenss-comp1.cse.tamu.edu -p 4635 -m lenss-comp1.cse.tamu.edu -a 4632 -c 1 &
export p4=$!

# print process IDs for processes started
echo $p1
echo $p2
echo $p3
echo $p4