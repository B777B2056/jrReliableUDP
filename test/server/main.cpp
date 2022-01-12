#include "../../src/jrudp.hpp"

using namespace jrReliableUDP;

int main() {
    Socket listen;
    listen.bind(8888);
    listen.listen();
    Socket server = listen.accept();
    while(!server.recv_pkg().empty());
    server.disconnect();
}
