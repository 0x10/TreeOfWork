# The Tree of Work
The Tree of Work is a hierarchical threading concept, where several threads/workers are organized hierachically and their execution is controlled by the execution state of the parent states.

There are 2 basic execution policies: 
* Execute a node if all parents are done (and successful)
* Execute a node if at least one parent is done (and successful)

# Compile example
Execute:

`clang++ -Weverything -Wno-c++98-compat -Wno-padded -Wno-covered-switch-default -std=c++11 -pthread main.cpp -o tree_work`


# Limitations
* Current there is no real possibiliy to pass data to the worker thread at runtime (e.g. from parent to child).
* Only success and failure are supported actions during execution. An Application has to implement its own progress reporting for example.
