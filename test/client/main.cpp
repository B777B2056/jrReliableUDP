#include "../../src/jrudp.hpp"

using namespace jrReliableUDP;

int main() {
    Socket client;
    client.bind(8000);
    client.connect("127.0.0.1", 8888);
    for(int i = 0; i < 10; ++i) {
        client.send_pkg("Package" + std::to_string(i));
//        client.recv_pkg();
    }
    client.disconnect();
}
