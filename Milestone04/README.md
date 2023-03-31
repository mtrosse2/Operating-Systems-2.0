<ul>
  <li><b>Jackson Ballow - jballow@nd.edu</b></li>
<li><b>Connnor Ding - cding2@nd.edu</b></li>
<li><b>Jack Lambert - jlamber4@nd.edu</b></li>
<li><b>Mitchell Trossen - mtrosse2@nd.edu</b></li>
</ul>
  
Runs with 10 producers and 25000 iterations WITHOUT condition variables:
<ul>
  <li>1 Consumer   : (41s + 43s + 35s) / 3 = 39.67s</li>
  <li>2 Consumers  : (6s + 7s + 6s) / 3 = 6.34s</li>
  <li>5 Consumers  : (4s + 4s + 4s) / 3 = 4s</li>
  <li>8 Consumers  : (5s + 5s + 5s) / 3 = 5s</li>
  <li>10 Consumers : (5s + 6s + 6s) / 3 = 5.67s</li>
</ul>

Runs with 10 producers and 25000 iterations WITH condition variables:
<ul>
  <li>1 Consumer   : (9s + 9s + 9s) / 3 = 9s</li>
  <li>2 Consumers  : (5s + 5s + 5s) / 3 = 5s</li>
  <li>5 Consumers  : (4s + 4s + 3s) / 3 = 3.67s</li>
  <li>8 Consumers  : (4s + 4s + 3s) / 3 = 3.67s</li>
  <li>10 Consumers : (4s + 4s + 4s) / 3 = 4s</li>
</ul>

Using conditional variables led to a speed up on average compared to its non condition variable counterpart. By not simply spinning when the lock was not obtained, the threads were able to run more efficiently when using condition variables.
