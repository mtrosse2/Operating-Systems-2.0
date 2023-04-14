make clean
make
echo "Testing Test.pcap"
echo "-----------------------------------"
./redextract data/Test.pcap
echo "Testing testFile.pcap"
echo "-----------------------------------"
./redextract data/testFile.pcap
echo "Testing testFile2.pcap"
echo "-----------------------------------"
./redextract data/testFile2.pcap
echo "Testing testSmall.pcap"
echo "-----------------------------------"
./redextract data/TestSmall.pcap
echo "Testing biggerFiles.txt"
echo "-----------------------------------"
./redextract input/biggerFiles.txt
echo "Testing doubleTest.txt"
echo "-----------------------------------"
./redextract input/doubleTest.txt
echo "Testing singleTest.txt"
echo "-----------------------------------"
./redextract input/singleTest.txt
