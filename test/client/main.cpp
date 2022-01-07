#include "../../src/jrudp.hpp"

using namespace jrReliableUDP;

int main() {
    Socket client;
    client.bind(8000);
    client.connect("127.0.0.1", 8888);

    client.disconnect();
}
