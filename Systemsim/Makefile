all: systemsim
systemsim: systemsim.c process.h system.h pcb_queue.h cl_args.h
	gcc -Wall -g -o systemsim systemsim.c process.h system.h pcb_queue.h cl_args.h -lrt -lpthread
clean:
	rm -fr systemsim systemsim.o *~
