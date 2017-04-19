./worker -h lenss-comp4.cse.tamu.edu -p 4232 -m lenss-comp1.cse.tamu.edu -a 4232 -f true &
export p1=$!

./worker -h lenss-comp4.cse.tamu.edu -p 4233 -m lenss-comp1.cse.tamu.edu -a 4232 &
export p2=$!

./worker -h lenss-comp4.cse.tamu.edu -p 4234 -m lenss-comp1.cse.tamu.edu -a 4232 &
export p3=$!

echo $p1
echo $p2
echo $p3