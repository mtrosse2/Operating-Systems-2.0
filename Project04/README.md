<ul>
    <li>Jackson Ballow - jballow@nd.edu</li>
    <li>Connnor Ding - cding2@nd.edu</li>
    <li>Jack Lambert - jlamber4@nd.edu</li>
    <li>Mitchell Trossen - mtrosse2@nd.edu</li>
<ul>

# Report:#

Our optimal number of threads run based on the number of pcap files that need to be processed. The number of producers will be equal to the number of pcap files, and the remaining
threads will be run as consumers. Therefore, the optimal of threads will be one more than the number of pcap files, but this will never exceed the maximum of 8 threads.

Based on our estimates of the packet size entries being ~500 bytes (obtained from example files provided), the size of our hash table array being 9973, and our stack size being 10,
we feel confident that the our total memory consumption will never exceed 256 MB.


<b>Testing:</b>


When running Test.pcap 6 times in a text file:
    2 Threads: 8.795 sec
    4 Threads: 3.062 sec
    6 Threads: 2.127 sec
    8 Threads: 2.314 sec

    These results show a significant speedup when increasing the number of threads up to the point where it exceeds the optimal number of threads. In this case as there were 6 files,
    the optimal number of threads would have been 7 which is why we observe the performance decrease when running 8 threads.

When running each of the provided input files:

    TestSmall.pcap: 13.12% Duplicate Percent
    Test.pcap: 0.34% Duplicate Percent
    testFile.pcap: 0.00% Duplicate Percent
    testFile2.pcap: 0.14% Duplicate Percent

    biggerFile.txt (ie. testFile2.pcap x3): 43.73% Duplicate Percent
    doubleTest.txt (ie. testFile.pcap x2): 45.63% Duplicate Percent
    singleTest.txt (ie. testFile.pcap x1): 0.00% Duplicate Percent 