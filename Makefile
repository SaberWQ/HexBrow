CXX = clang++
CXXFLAGS = -std=c++17 -I src -Wall -Wextra
LDFLAGS = -lsqlite3 -lcurl

# Додаємо cgi до загальної збірки
all: HexBrow cgi

HexBrow: browser.mm src/DatabaseManager.cpp src/NetworkManager.cpp src/HtmlGenerator.cpp
	mkdir -p HexBrow.app/Contents/MacOS
	$(CXX) $(CXXFLAGS) -fobjc-arc browser.mm src/DatabaseManager.cpp src/NetworkManager.cpp src/HtmlGenerator.cpp $(LDFLAGS) -framework Cocoa -framework WebKit -o HexBrow.app/Contents/MacOS/HexBrow

# Окрема ціль для CGI
cgi: src/cgi_main.cpp src/DatabaseManager.cpp src/HtmlGenerator.cpp
	mkdir -p cgi-bin
	$(CXX) $(CXXFLAGS) src/cgi_main.cpp src/DatabaseManager.cpp src/HtmlGenerator.cpp -lsqlite3 -o cgi-bin/search.cgi

clean:
	rm -f HexBrow.app/Contents/MacOS/HexBrow
	rm -rf cgi-bin
	rm -f search.cgi