CXX = clang++
CXXFLAGS = -std=c++17 -I src
LDFLAGS = -lsqlite3 -lcurl

all: HexBrow

HexBrow: browser.mm src/DatabaseManager.cpp src/NetworkManager.cpp src/HtmlGenerator.cpp
	$(CXX) $(CXXFLAGS) -fobjc-arc browser.mm src/DatabaseManager.cpp src/NetworkManager.cpp src/HtmlGenerator.cpp $(LDFLAGS) -framework Cocoa -framework WebKit -o HexBrow.app/Contents/MacOS/HexBrow

clean:
	rm -f HexBrow.app/Contents/MacOS/HexBrow
	rm -f search.cgi