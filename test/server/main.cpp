#include "../../src/jrudp.hpp"

using namespace jrReliableUDP;

int main() {
    Socket listen;
    listen.bind(8888);
    listen.listen();
    Socket server = listen.accept();
    while(true) {
        std::string str = server.recv_pkg();
        if(str.empty()) {
            break;
        }
//        server.send_pkg(str);
        std::cout << "PKG:" << str << std::endl;
    }
    server.disconnect();
}
