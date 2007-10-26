bootpdutil: main.c ../build/Debug/libbootplib.a
	cc	-Wall								\
		-g								\
		-I../bootplib							\
		-I/usr/local/include						\
		-o bootpdutil							\
		main.c								\
		-L../build/Debug -lbootplib					\
		-framework CoreFoundation					\

../build/Debug/libbootplib.a:
	@(cd ..; xcodebuild -target bootplib -configuration Debug)

clean:
	rm -f bootpdutil
