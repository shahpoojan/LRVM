all:
	g++ -c -o rvm.o rvm.cpp
	ar rc rvm.a rvm.o
	ranlib rvm.a
	g++ -o basic basic.c rvm.a
