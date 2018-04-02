#include <SFML\Network.hpp>
#include <SFML/Graphics.hpp>
#include <iostream>

#define RIGHT_POS 600
#define LEFT_POS 200

enum Commands { HEY, CON, INF, NEW, MOV };

int actualID = 0;
sf::UdpSocket sock;

struct Player
{
	sf::IpAddress ip;
	unsigned short port;
	sf::Vector2i pos;
	int playerID;
	sf::Color pjColor;
};

struct PendingMessage {
	sf::Packet packet;
	int id;
	int time;
	sf::IpAddress targetAdress;
	unsigned short port;
};

std::vector<PendingMessage> messagePending = std::vector<PendingMessage>();

void AddMessage(sf::Packet _pack, sf::IpAddress _ip, unsigned short _port) {	// SOLO PARA EL NEW
	PendingMessage tempPending;
	tempPending.id = actualID;
	_pack << actualID;
	tempPending.packet = _pack;
	tempPending.targetAdress = _ip;
	tempPending.port = _port;
	tempPending.time = 0;

	sock.send(_pack, _ip, _port);
	messagePending.push_back(tempPending);

	actualID++;
}

void SendMessages(int deltaTime) {
	for (auto i : messagePending) {
		i.time += deltaTime;
		if (i.time > 500) {
			//RESEND MESSAGE
			sock.send(i.packet, i.targetAdress, i.port);
			i.time = 0;
		}
	}

}

void MessageConfirmed(int _ID) {	//SOLO PARA EL ACK
	int indexToDelete = 0;
	bool found = false;
	std::vector<PendingMessage>::iterator iteratorToDelete = messagePending.begin();

	while (!found || (indexToDelete < messagePending.size())) {	//if(iteratorToDelete != messagePending.end()) ALTERNATIVA PARA NO USAR EL INDEXTODELETE
		if (iteratorToDelete->id == _ID) {
			//FOUND MY MESSAGE TO DELETE
			found = true;
		}
		else { indexToDelete++; iteratorToDelete++; }
	}

	if (found) { messagePending.erase(iteratorToDelete); }
}

int main()
{
	std::vector<Player> players = std::vector<Player>();
	
	
	sf::Socket::Status status = sock.bind(50000);
	if (status != sf::Socket::Done)
	{
		std::cout << "Error al vincular\n";
		system("pause");
		exit(0);
	}
	sf::Clock clock;
	sf::Clock clockDeltaTime;
	do
	{
		if (clock.getElapsedTime().asMilliseconds() > 200)
		{
			sf::Packet pck;
			sf::IpAddress ipRem; unsigned short portRem;
			status = sock.receive(pck, ipRem, portRem);
			if (status == sf::Socket::Done)
			{
				int pos;
				pck >> pos;
				if (pos == -1)
				{
					break;
				}

				std::cout << "Se intenta la pos " << pos << std::endl;
				if (pos >= LEFT_POS && pos <= RIGHT_POS)
				{
					std::cout << "Se confirma la pos " << pos << std::endl;
					sf::Packet pckSend;
					pckSend << pos;
					sock.send(pckSend, ipRem, portRem);
					
				}
			}
			clock.restart();
		}

		SendMessages(clockDeltaTime.getElapsedTime().asMilliseconds());
		clockDeltaTime.restart();

	} while (true);
	sock.unbind();
	return 0;
}