#echo ""
#echo "Starting initial startup for the reliable server."
#echo ""

./fbsd -p 4632 &     #-m true 
#export p1=$!
#./worker 4633 
./master 4633 
#export p2=$!


#./fbsd -p 4633

#export p3=$!

#echo "Press enter to kill the following processes individually:"
#echo $p1
#echo $p2
#echo $p3
#read input
#kill $p1
#kill $p2
#kill $p3