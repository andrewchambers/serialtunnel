
.PHONY: clean test all

all: testbin tunclient

test: testbin
	./testbin

clean:
	rm -f testbin
	rm -f tunclient

testbin: *.cpp *.h
	g++ -g -Dprivate=public -Wall -Werror test.cpp base64.cpp packets.cpp protocol.cpp -o testbin

tunclient: *.cpp *.h
	g++ -g tunclient.cpp protocol.cpp packets.cpp base64.cpp -Wall -Werror -o tunclient 
