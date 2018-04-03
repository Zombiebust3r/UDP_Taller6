#include <SFML\Network.hpp>
#include <SFML/Graphics.hpp>
#include <iostream>

#define RIGHT_POS 600
#define LEFT_POS 200

enum Commands { HEY, CON, NEW, MOV };

int playerID = 1; //se irá sumando 1 cada jugador nuevo
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

sf::Vector2i RandoPosGenerator() {
	sf::Vector2i temp;
	temp.x = (rand() % 401) + 200; //entre 200 y 600
	temp.y = (rand() % 401) + 200; //random lel
	return temp;
}
Player MakePlayer(sf::IpAddress ipRem, short portRem) {

	Player tempPlayer;
	tempPlayer.ip = ipRem;
	tempPlayer.port = portRem;
	tempPlayer.playerID = playerID++;
	tempPlayer.pjColor = sf::Color::Red;
	tempPlayer.pos = RandoPosGenerator();

	return tempPlayer;
}

void SendCon(Player player, std::vector<Player> players, int idMessage) {
	//enviar paquete
	sf::Packet pckCon;
	pckCon << CON;
	pckCon << int(players.size()); //así envía el numero de jugadores a añadir
	pckCon << player.playerID;//id
	pckCon << player.pjColor.r;//color
	pckCon << player.pjColor.g;
	pckCon << player.pjColor.b;
	pckCon << player.pos.x; //pos
	pckCon << player.pos.y;
	if (players.size() > 0) {
		for (int i = 0; i < players.size() - 1; i++) {
			std::cout << "sending player: " << i << std::endl;
			//id, color, pos
			pckCon << players[i].playerID;
			pckCon << players[i].pjColor.r;//color
			pckCon << players[i].pjColor.g;
			pckCon << players[i].pjColor.b;
			pckCon << players[i].pos.x; //pos
			pckCon << players[i].pos.y;
		}
	}
	pckCon << idMessage;
	sock.send(pckCon, player.ip, player.port);
}

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
				int command;
				pck >> command;
				switch (command) {
				case HEY:

					Player tempPlayer;
					//comprobar si es el primero
					int idMessage;
					pck >> idMessage;
					if (players.size() > 0) {
						//comprobar si ya existe via ip
						int it = 0;
						bool repeated = false;
						while (!repeated && (it < players.size())) {
							if (ipRem == players[it].ip && portRem == players[it].port) {
								repeated = true;
								tempPlayer = players[it]; //guardamos info jugador para reenviar paquete
								std::cout << "rep";
							}
							it++;
						}
						if (!repeated) {//crear usuario
							tempPlayer = MakePlayer(ipRem, portRem);
							//añadir usuario a lista
							players.push_back(tempPlayer);

							//aquí iria añadir NEW al resto
						}
						
					}
					else {
						//crear usuario
						tempPlayer = MakePlayer(ipRem, portRem);
						//añadir usuario a lista
						players.push_back(tempPlayer);
					}
					//añadido usuario
					std::cout << players.size() << std::endl;
					//enviar paquete
					SendCon(tempPlayer, players, idMessage);
					break;
				}

				/*
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
				*/
			}
			clock.restart();
		}

		SendMessages(clockDeltaTime.getElapsedTime().asMilliseconds());
		clockDeltaTime.restart();

	} while (true);
	sock.unbind();
	return 0;
}