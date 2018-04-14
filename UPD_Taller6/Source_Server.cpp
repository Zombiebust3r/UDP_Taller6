#include <SFML\Network.hpp>
#include <SFML/Graphics.hpp>
#include <iostream>
#include <random>

#define RIGHT_POS 600
#define LEFT_POS 200

#define MAXPINTTRIES 3
#define PINGMAXTIME_MS 5.0f
#define COMMANDMAXTIME_MS 0.5f

#define PERCENT_PACKETLOSS 0.01

enum Commands { HEY, CON, NEW, ACK, MOV, PIN, DIS, EXE, OKM, PEM };

int playerID = 1; //se irá sumando 1 cada jugador nuevo
int actualID = 0;
sf::UdpSocket sock;
bool firstReset = true;

struct Player
{
	sf::IpAddress ip;
	unsigned short port;
	sf::Vector2i pos;
	int playerID;
	sf::Color pjColor;
	int pingTries;
};

struct PendingMessage {
	sf::Packet packet;
	int id;
	float time;
	float maxTime;
	sf::IpAddress targetAdress;
	unsigned short port;
};

std::vector<PendingMessage> messagePending = std::vector<PendingMessage>();

std::vector<Player> players = std::vector<Player>();

float GetRandomFloat() {
	std::random_device rd;
	std::mt19937 gen(rd());
	std::uniform_real_distribution<float> dis(0.f, 1.f);
	return dis(gen);
}
sf::Vector2i RandoPosGenerator() {
	sf::Vector2i temp;
	temp.x = (rand() % 401) + 200; //entre 200 y 600
	temp.y = (rand() % 401) + 200; //random lel
	return temp;
}

void AddMessage(Commands messageType, sf::Packet _pack, sf::IpAddress _ip, unsigned short _port);	//PREDEFINE


Player MakePlayer(sf::IpAddress ipRem, short portRem) {

	Player tempPlayer;
	tempPlayer.ip = ipRem;
	//std::cout << "IP: " << tempPlayer.ip.toString() << std::endl;
	tempPlayer.port = portRem;
	tempPlayer.playerID = playerID++;
	tempPlayer.pjColor = sf::Color(rand() % 255 + 0, rand() % 255 + 0, rand() % 255 + 0, 255);
	tempPlayer.pos = RandoPosGenerator();
	tempPlayer.pingTries = 0;
	sf::Packet pinPacket;
	pinPacket << Commands::PIN;
	AddMessage(Commands::PIN, pinPacket, tempPlayer.ip, tempPlayer.port);	//ADD PING MESSAGE EACH TIME WE CREATE A NEW PLAYER
	
	return tempPlayer;
}

int PlayerIndexByPORT(unsigned short _portToSearch) {
	int index = 0;
	for (auto i : players) {
		if (i.port == _portToSearch) {
			return index;
		}
		index++;
	}
	return -1;
}

std::vector<Player>::iterator PlayerItIndexByPORT(unsigned short _portToSearch) {
	std::vector<Player>::iterator it = players.begin();
	for (auto i : players) {
		if (i.port == _portToSearch) {
			return it;
		}
		it++;
	}
	return players.end();
}

std::vector<PendingMessage>::iterator MessageItIndexByIP(int _id) {
	std::vector<PendingMessage>::iterator it = messagePending.begin();
	for (auto i : messagePending) {
		if (i.id == _id) {
			return it;
		}
		it++;
	}
	return messagePending.end();
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
			std::cout << "sending old player: " << i << std::endl;
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

void AddMessage(Commands messageType, sf::Packet _pack, sf::IpAddress _ip, unsigned short _port) {	// SOLO PARA EL NEW
	PendingMessage tempPending;
	tempPending.id = actualID;
	_pack << actualID;
	tempPending.packet = _pack;
	tempPending.targetAdress = _ip;
	tempPending.port = _port;
	tempPending.time = 0.0f;

	switch (messageType)
	{
	case NEW:
		tempPending.maxTime = COMMANDMAXTIME_MS;
		break;
	case MOV:
		tempPending.maxTime = COMMANDMAXTIME_MS;
		break;
	case PIN:
		tempPending.maxTime = PINGMAXTIME_MS;
		break;
	case PEM:
		tempPending.maxTime = COMMANDMAXTIME_MS;
	default:
		tempPending.maxTime = COMMANDMAXTIME_MS;
		break;
	}

	if(tempPending.maxTime != PINGMAXTIME_MS) sock.send(_pack, _ip, _port);
	messagePending.push_back(tempPending);

	actualID++;
}

void SendMessages(float deltaTime) {	//NO SE LLAMA PARA LOS PINGs PQ SE TIENEN QUE MANDAR SIEMPRE Y CUANDO EL JUGADOR AL QUE SE ENVÍA NO ESTÁ DESCONECTADO
	std::vector<PendingMessage>::iterator messageToDelete;
	//std::cout << "MESSAGE SIZE: " << messagePending.size() << std::endl;
	for (int i = 0; i < messagePending.size(); i++) {
		bool send = true;
		messagePending[i].time += deltaTime;
		//std::cout << "TIME " << messagePending[i].maxTime << " ACTUALTIME " << messagePending[i].time << std::endl;
		if (messagePending[i].time > messagePending[i].maxTime) {
			//RESEND MESSAGE
			std::cout << "EXCEEDED TIME " << messagePending[i].maxTime << std::endl;
			if (messagePending[i].maxTime == PINGMAXTIME_MS) { //RESSENDING PING
				std::cout << "RESENDING PING" << std::endl;
				int playerIndex = PlayerIndexByPORT(messagePending[i].port);
				if (playerIndex != -1) {
					players[playerIndex].pingTries += 1;
					std::cout << "PLAYER " << players[playerIndex].playerID << " PING TRIES " << players[playerIndex].pingTries << std::endl;
					if (players[playerIndex].pingTries >= MAXPINTTRIES) {
						std::cout << "DISCONNECTED PLAYER " << players[playerIndex].playerID << std::endl;
						//CREATE DIS PACKET: DIS_IDPLAYER_IDMESSAGE
						sf::Packet disconnectionPacket;	//PAQUETE PARA EL RESTO DE JUGADORES QUE NO DESCONECTAS
						disconnectionPacket << Commands::DIS;
						sf::Packet exePacket; //PAQUETE PARA EL QUE DESCONECTAS
						exePacket << Commands::EXE;
						//DISCONNECT THAT PLAYER -----------------------------------------------------------------------------------------------------------------------------------
						std::vector<Player>::iterator playerToDelete = PlayerItIndexByPORT(messagePending[i].port);
						sock.send(exePacket, playerToDelete->ip, playerToDelete->port);
						disconnectionPacket << playerToDelete->playerID;
						if (playerToDelete != players.end()) players.erase(playerToDelete);
						//DELETE PING MESSAGE FROM PENDING MESSAGES ----------------------------------------------------------------------------------------------------------------
						messageToDelete = MessageItIndexByIP(messagePending[i].id);
						send = false;
						
						//MANDAR A TODOS LOS JUGADORES:
						for (int toAll = 0; toAll < players.size(); toAll++) {
							AddMessage(Commands::DIS, disconnectionPacket, players[toAll].ip, players[toAll].port);
						}
					}
				}
			}
			if (send) { sock.send(messagePending[i].packet, messagePending[i].targetAdress, messagePending[i].port); messagePending[i].time = 0.0f;	}
			else { 
				if (messageToDelete != messagePending.end()) 
					messagePending.erase(messageToDelete); 
			}
			
		}
	}
	
	/*for (auto i : messagePending) {
		bool send = true;
		i.time += deltaTime;
		if (i.time > i.maxTime) {
			//RESEND MESSAGE
			if (i.maxTime == PINGMAXTIME_MS) { //RESSENDING PING
				int playerIndex = PlayerIndexByIP(i.targetAdress);
				if (playerIndex != -1) {
					players[playerIndex].pingTries += 1;
					if (players[playerIndex].pingTries >= MAXPINTTRIES) {
						//DISCONNECT THAT PLAYER -----------------------------------------------------------------------------------------------------------------------------------
						std::vector<Player>::iterator playerToDelete = PlayerItIndexByIP(i.targetAdress);
						if(playerToDelete != players.end()) players.erase(playerToDelete);
						//DELETE PING MESSAGE FROM PENDING MESSAGES ----------------------------------------------------------------------------------------------------------------
						messageToDelete = MessageItIndexByIP(i.id);
						send = false;
					}
				}
			}
			if (send) { sock.send(i.packet, i.targetAdress, i.port); }
			else { if (messageToDelete != messagePending.end()) messagePending.erase(messageToDelete); }
			i.time = 0;
		}
	}*/
}

void MessageConfirmed(int _ID) {	//SOLO PARA EL ACK
	int indexToDelete = 0;
	bool found = false;
	bool pingMessage = false;
	std::vector<PendingMessage>::iterator iteratorToDelete = messagePending.begin();

	while (!found && (indexToDelete < messagePending.size())) {	//if(iteratorToDelete != messagePending.end()) ALTERNATIVA PARA NO USAR EL INDEXTODELETE
		if (iteratorToDelete->id == _ID) {
			//FOUND MY MESSAGE TO DELETE
			if (iteratorToDelete->maxTime == PINGMAXTIME_MS) {
				std::cout << "ACK PING" << std::endl;
				pingMessage = true;
				int playerIndex = PlayerIndexByPORT(iteratorToDelete->port);
				if (playerIndex != -1) {
					players[playerIndex].pingTries = 0;
				}
			}
			found = true;
		}
		else { indexToDelete++; iteratorToDelete++; }
	}

	if (found && !pingMessage) { messagePending.erase(iteratorToDelete); }
}

int main()
{
	srand(time(NULL));
	sock.setBlocking(false);
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
			//std::cout << "preReceive------------------------------------------------------------------------" << std::endl;
			status = sock.receive(pck, ipRem, portRem);
			//std::cout << "postReceive------------------------------------------------------------------------" << std::endl;
			if (status == sf::Socket::Done)
			{
				int command;
				Player tempPlayer;
				int idMessage;
				
				sf::Vector2i movPos;
				sf::Packet okMovePack;
				sf::Packet penMovePack;
				int indexPlayerOkMove = -1;

				pck >> command;
				float randomNumber = GetRandomFloat();
				if (randomNumber > PERCENT_PACKETLOSS) {
					switch (command) {
					case HEY:
						std::cout << "RECEIVED HEY FROM PORT " << portRem << std::endl;
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
									std::cout << "repeated player" << std::endl;
								}
								it++;
							}
							if (!repeated) {//crear usuario
								tempPlayer = MakePlayer(ipRem, portRem);

								for (int i = 0; i < players.size(); i++) {//aquí iria añadir NEW al resto
									sf::Packet pckNew;
									pckNew << Commands::NEW;
									pckNew << tempPlayer.playerID;
									pckNew << tempPlayer.pjColor.r;
									pckNew << tempPlayer.pjColor.g;
									pckNew << tempPlayer.pjColor.b;
									pckNew << tempPlayer.pos.x;
									pckNew << tempPlayer.pos.y;
									std::cout << "envio new al puerto " << players[i].port << std::endl;
									AddMessage(Commands::NEW, pckNew, players[i].ip, players[i].port);
								}
								//añadir usuario a lista
								players.push_back(tempPlayer);
							}

						}
						else {
							//crear usuario
							tempPlayer = MakePlayer(ipRem, portRem);
							//añadir usuario a lista
							players.push_back(tempPlayer);
						}
						//añadido usuario
						std::cout << "Current player size: " << players.size() << std::endl;
						//enviar paquete
						SendCon(tempPlayer, players, idMessage);
						break;

					case ACK:
						pck >> idMessage;
						std::cout << "ID MESSAGE" << idMessage << std::endl;
						MessageConfirmed(idMessage);
						break;
					case MOV:
						std::cout << "MOVE" << std::endl;
						pck >> movPos.x;
						pck >> movPos.y;
						std::cout << movPos.x << std::endl;
						pck >> idMessage;
						if (movPos.x > LEFT_POS && movPos.x < RIGHT_POS) {
							std::cout << "MOVE ACCEPTED" << std::endl;
							//ACK_MOVE
							okMovePack << Commands::OKM;
							penMovePack << Commands::PEM;
							indexPlayerOkMove = PlayerIndexByPORT(portRem);
							if (indexPlayerOkMove != -1) {
								std::cout << "MOVE PLAYER FOUND" << std::endl;
								players[indexPlayerOkMove].pos = movPos;
								okMovePack << players[indexPlayerOkMove].playerID;
								penMovePack << players[indexPlayerOkMove].playerID;
								okMovePack << movPos.x;
								okMovePack << movPos.y;
								okMovePack << idMessage;
								penMovePack << movPos.x;
								penMovePack << movPos.y;
								sock.send(okMovePack, ipRem, portRem);	//SE MANDA AL JUGADOR QUE SE MUEVE
								if (players.size() > 1) {
									for (int h = 0; h < players.size(); h++) {	//SE MANDA AL RESTO
										if (players[h].port != portRem) {
											AddMessage(Commands::PEM, penMovePack, players[h].ip, players[h].port);
										}
									}
								}
								
							}
						}
						
					}
				}
				else {
					std::cout << "Se ha ignorado el mensaje con comando " << command << " (Roll: " << randomNumber << ")" << std::endl;
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
		if (firstReset) { clockDeltaTime.restart(); firstReset = false; }
		//std::cout << "SEND MESSAGES" << std::endl;
		SendMessages(clockDeltaTime.getElapsedTime().asSeconds());
		//std::cout << "FINISH SEND MESSAGES" << std::endl;
		clockDeltaTime.restart();

	} while (true);
	sock.unbind();
	return 0;
}