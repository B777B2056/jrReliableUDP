#include "../../src/jrudp.hpp"

using namespace jrReliableUDP;

int main() {
    Socket listen;
    listen.bind(8888);
    listen.listen();
    Socket server = listen.accept();
    for(int i = 0; i < 10; ++i) {
        server.recv_pkg();
    }
    server.disconnect();
}
