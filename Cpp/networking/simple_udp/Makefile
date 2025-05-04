CXX = g++
CXXFLAGS = -std=c++17 -Wall -I/usr/include # Change this line
LDFLAGS = -L/usr/lib -lboost_system -lboost_serialization # /usr/lib is the standard library path

sender: sender.cpp
	$(CXX) $(CXXFLAGS) -o sender sender.cpp $(LDFLAGS)

receiver: receiver.cpp
	$(CXX) $(CXXFLAGS) -o receiver receiver.cpp $(LDFLAGS)

clean:
	rm -f sender receiver
