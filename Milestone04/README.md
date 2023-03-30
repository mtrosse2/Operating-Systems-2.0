Jackson Ballow - jballow@nd.edu
Connnor Ding - cding2@nd.edu
Jack Lambert - jlamber4@nd.edu
Mitchell Trossen - mtrosse2@nd.edu

Runs with 10 producers and 25000 iterations WITHOUT condition variables:

1 Consumer   : (41s + 43s + 35s) / 3 = 39.67s
2 Consumers  : (6s + 7s + 6s) / 3 = 6.34s
5 Consumers  : (4s + 4s + 4s) / 3 = 4s
8 Consumers  : (5s + 5s + 5s) / 3 = 5s
10 Consumers : (5s + 6s + 6s) / 3 = 5.67s

Runs with 10 producers and 25000 iterations WITH condition variables:

1 Consumer   : (9s + 9s + 9s) / 3 = 9s
2 Consumers  : (5s + 5s + 5s) / 3 = 5s
5 Consumers  : (4s + 4s + 3s) / 3 = 3.67s
8 Consumers  : (4s + 4s + 3s) / 3 = 3.67s
10 Consumers : (4s + 4s + 4s) / 3 = 4s

Using conditional variables led to a speed up on average compared to its non condition variable counterpart