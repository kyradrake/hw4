#echo ""
#echo "Starting initial startup for the reliable server."
#echo ""

#./fbsd -p 4632 &     #-m true 
#export p1=$!

./master -h lenss-comp1.cse.tamu.edu -p 4732 &
export p1=$!

./worker -h lenss-comp1.cse.tamu.edu -p 4733 -m lenss-comp1.cse.tamu.edu -a 4732 &
export p2=$!


#./worker 4634

#./fbsd -p 4633

#export p3=$!

#echo "Press enter to kill the following processes individually:"
echo $p1
echo $p2

#echo $p3
#read input
#kill $p1
#kill $p2
#kill $p3