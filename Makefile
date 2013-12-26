
.PHONY: clean test all

all: testbin tunclient fakelink

test: testbin
	./testbin

clean:
	rm -f fakelink
	rm -f testbin
	rm -f tunclient

testbin: *.cpp *.h
	g++ -g -Dprivate=public -Wall -Werror -Wfatal-errors test.cpp base64.cpp protocol.cpp -o testbin

fakelink: fakelink.cpp
	g++ -Wall -Werror -Wfatal-errors fakelink.cpp -o fakelink

tunclient: *.cpp *.h
	g++ -g tunclient.cpp protocol.cpp base64.cpp -Wall -Werror -Wfatal-errors -o tunclient 
