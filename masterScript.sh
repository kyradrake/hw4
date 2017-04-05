echo ""
echo "Starting initial startup for the reliable server."
echo ""

./fbsd -p 3055 -m true &
export p1=$!
./master &
export p2=$!
./worker -p 3056 &
export p3=$!

echo "Press enter to kill the following processes individually:"
echo $p1
echo $p2
echo $p3
read input
kill $p1
kill $p2
kill $p3