# Linux programming - exercises
Repository consists of exercises done for "Programming in Linux OS" university course.

## Exercises description

### Sparse files

**In the exercise I used:** file I/O, lseek().<br/><br/>

Exercise consists of 4 programs:
* empty_file - program creates sparsely allocated file.
* randomize - program writes specified values to the file at random offsets.
* file_content - program prints the file content in blocks of the same value.
* extend - program extends allocated blocks in the file.

### Getting resources

**In the exercise I used:** pipes, FIFOs, registering function by on_exit(), fork(), execvp().<br/><br/>

'Department' creates child processes('searcher'). Each one of them creates as many files with specified name as possible and registers functions by on_exit(), which change files' names. Child processes start creating files at the same time. 'Department' informs child processes when to start via pipe. It also uses FIFO to inform them when they should stop creating files. 'Department' deletes files that was created after informing them about it.

### I/O multiplexing

**In the exercise I used:** I/O multiplexing by poll().<br/><br/>

Exercise consists of 3 programs:
* empty - program checks if descriptors(pipes or FIFOs) are full. If so, program starts emptying it.
* filler - program uses poll() on descriptors(pipes or FIFOs). It writes data to them. If it successfully read value from control descriptor, then the value specify descriptor that have to be ignored.
* manager - creates one 'filler' and few 'empty' processes. Number of 'empty' processes and way of connecting them with 'filler' is specified by environment variables: PIPES and FIFOS.

### SIGCHLD signal

**In the exercise I used:** Linux signals sending and handling, POSIX timers, fork().<br/><br/>

Program creates a child process and reacts(e.g. it sends SIGCONT to child process) to its state changes. Child process uses timer generating SIGSTOP signals.

