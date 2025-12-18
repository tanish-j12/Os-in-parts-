#Compile the launch.c by linking it with the lib_simpleloader.so

#Provide the command for cleanup

all:
	gcc -m32 launch.c -L../bin -l_simpleloader -Wl,-rpath,'$$ORIGIN' -o ../bin/launch

clean:
	rm -f ../bin/launch