./worker -h lenss-comp4.cse.tamu.edu -p 4732 -m lenss-comp1.cse.tamu.edu -a 4732 &
export p1=$!

./worker -h lenss-comp4.cse.tamu.edu -p 4733 -m lenss-comp1.cse.tamu.edu -a 4732 &
export p1=$!

./worker -h lenss-comp4.cse.tamu.edu -p 4734 -m lenss-comp1.cse.tamu.edu -a 4732 &
export p1=$!

echo $p1
echo $p2
echo $p3