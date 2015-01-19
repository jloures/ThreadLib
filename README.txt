This is a thread library created entirely in C.	
It is capable managing running/blocked and suspended threads on its own. Some of its functions are:
- Creating new threads and allocating memory for them down to the stack level.
- Blocking threads upon certain conditions.
- Set threads to the running state
- Exiting threads that are not needed anymore.



This entire thread library assumes that the user implementing will take care of the thread scheduler. In other words, how much time each thread will run for among other things. 

Joao Loures
