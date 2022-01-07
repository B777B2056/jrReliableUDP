#include "../../src/jrudp.hpp"

using namespace jrReliableUDP;

int main() {
    Socket server;
    server.bind(8888);
    server.listen();
    Socket client = server.accept();

    client.disconnect();
}
