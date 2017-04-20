./worker -h lenss-comp4.cse.tamu.edu -p 4632 -m lenss-comp1.cse.tamu.edu -a 4632 -f true  -c 2 &
export p1=$!

./worker -h lenss-comp4.cse.tamu.edu -p 4633 -m lenss-comp1.cse.tamu.edu -a 4632 -c 3 &
export p2=$!

./worker -h lenss-comp4.cse.tamu.edu -p 4634 -m lenss-comp1.cse.tamu.edu -a 4632 -c 4 &
export p3=$!

echo $p1
echo $p2
echo $p3