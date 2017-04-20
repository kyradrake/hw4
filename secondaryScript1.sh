./worker -h lenss-comp3.cse.tamu.edu -p 4132 -m lenss-comp1.cse.tamu.edu -a 4132 -f true  -c 5 &
export p1=$!

./worker -h lenss-comp3.cse.tamu.edu -p 4133 -m lenss-comp1.cse.tamu.edu -a 4132 -c 6 &
export p2=$!

./worker -h lenss-comp3.cse.tamu.edu -p 4134 -m lenss-comp1.cse.tamu.edu -a 4132 -c 7 &
export p3=$!

echo $p1
echo $p2
echo $p3