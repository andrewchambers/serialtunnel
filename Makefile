
.PHONY: clean test

test: testbin
	./testbin

clean:
	rm testbin

testbin: test.cpp base64.cpp protocol.cpp protocol.h base64.h
	g++ -Dprivate=public -Wall -Werror test.cpp base64.cpp protocol.cpp -o testbin
