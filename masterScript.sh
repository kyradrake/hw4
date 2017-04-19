#echo ""
#echo "Starting initial startup for the reliable server."
#echo ""

# master process
./master -h lenss-comp1.cse.tamu.edu -p 4232 &
export p1=$!

# master replica process
./master -h lenss-comp1.cse.tamu.edu -p 4233 -r replica -m 4232 &
export p2=$!

# worker process
./worker -h lenss-comp1.cse.tamu.edu -p 4234 -m lenss-comp1.cse.tamu.edu -a 4232 &
export p3=$!

# print process IDs for processes started
echo $p1
echo $p2
echo $p3