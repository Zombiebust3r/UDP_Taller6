#include <SFML\Network.hpp>
#include <SFML/Graphics.hpp>
#include <lib.h>
#include <iostream>
#include <random>
#include "Chronometer.h"

#define SET 0
#define GET 1

#define RIGHT_POS 600
#define LEFT_POS 200
#define RIGHT_CELL 10
#define LEFT_CELL 0

#define MAXPINTTRIES 3
#define PINGMAXTIME_MS 5.0f
#define COMMANDMAXTIME_MS 0.5f

#define PERCENT_PACKETLOSS 0.01
#define TIME_PER_MOVEMENT 0.2f


#define TIME_PER_TURN 10

enum Commands { HEY, CON, NEW, ACK, MOV, PIN, DIS, EXE, OKM, PEM, NOK, DED, CLR, STP };
//CLR_R_G_B						INFORMA DEL COLOR A BUSCAR
//DED_#PLAYERSDEAD_ID_ID_ID...	INFORMA DE LOS JUGADORES MUERTOS
//STP							MARCA EL FINAL DEL TURNO Y PARA EL MOVIMIENTO.

enum State { WAITINGFORPLAYERS, CHOSINGCOLOR, WAITINGFORTURNEND, COMPUTINGMOVEMENT, WAITINGCLRCONFIRMATION };

State actualState;
int playerID = 1; //se irá sumando 1 cada jugador nuevo
int actualID = 0;
sf::UdpSocket sock;
bool firstReset = true;
sftools::Chronometer chrono;
int playersColorConfirmed = 0; //Compare this to players.size();

struct Cell
{
	sf::Vector2i pos;
	sf::Color color;
};

Cell actualCellPicked;

struct Player
{
	sf::IpAddress ip;
	unsigned short port;
	sf::Vector2i pos;
	int playerID;
	sf::Color pjColor;
	int pingTries;
	bool dead;
	bool colorConfirmed;
};

struct PendingMessage {
	sf::Packet packet;
	int id;
	float time;
	float maxTime;
	sf::IpAddress targetAdress;
	unsigned short port;
	Commands commandType;
};

struct PendingMovement {
	sf::Vector2i pos;
	Player player;
	int idMov;
	int idMessage;
	float time;
};

std::vector<PendingMessage> messagePending = std::vector<PendingMessage>();

std::vector<PendingMovement> movementsPending = std::vector<PendingMovement>();

std::vector<Player> players = std::vector<Player>();

std::vector<Cell> cells = std::vector<Cell>();

float GetRandomFloat() {
	std::random_device rd;
	std::mt19937 gen(rd());
	std::uniform_real_distribution<float> dis(0.f, 1.f);
	return dis(gen);
}


sf::Vector2i RandoPosGenerator() {
	sf::Vector2i temp;
	temp.x = (rand() % 10) + 0; //entre 0 y 10
	temp.y = (rand() % 10) + 0; //random lel
	return temp;
}

void AddMessage(Commands messageType, sf::Packet _pack, sf::IpAddress _ip, unsigned short _port);	//PREDEFINE

State SetGetMode(int setOrGet, State mode) {
	//static Mode sMode = Mode::NOTHING;
	if (setOrGet == SET) {
		switch (mode)
		{
		case WAITINGFORPLAYERS:
			if (actualState != WAITINGFORPLAYERS) {  } // FIRST TIME CHANGING MODE
			break;
		case WAITINGFORTURNEND:
			if (actualState != WAITINGFORTURNEND) { chrono.reset(true); } // FIRST TIME CHANGING MODE
			break;
		case CHOSINGCOLOR:
			if (actualState != CHOSINGCOLOR) { 
				playersColorConfirmed = 0;
				for (int indexP = 0; indexP < players.size(); indexP++) { 
					players[indexP].colorConfirmed = false; 
				} 
			} // FIRST TIME CHANGING MODE
			break;
		case COMPUTINGMOVEMENT:
			if (actualState != COMPUTINGMOVEMENT) { chrono.reset(false); } // FIRST TIME CHANGING MODE
			break;
		default:
			break;
		}
		actualState = mode;

	}
	return actualState;
}

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
	tempPending.commandType = messageType;

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
	case STP:
		tempPending.maxTime = COMMANDMAXTIME_MS;
	case DED:
		tempPending.maxTime = COMMANDMAXTIME_MS;
	case CLR:
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
			int playerIndex = 0;
			if (iteratorToDelete->maxTime == PINGMAXTIME_MS) {
				std::cout << "ACK PING" << std::endl;
				pingMessage = true;
				playerIndex = PlayerIndexByPORT(iteratorToDelete->port);
				if (playerIndex != -1) {
					players[playerIndex].pingTries = 0;
				}
			}
			if (iteratorToDelete->commandType == Commands::CLR && !players[playerIndex].colorConfirmed) {	//VERIFICACION PARA SABER SI EL ACK ES PARA UN CLR Y PODER SABER DESPUÉS SI YA SE HAN CONFIRMADO TODOS.
				playersColorConfirmed++;
				players[playerIndex].colorConfirmed = true;
			}
			found = true;
		}
		else { indexToDelete++; iteratorToDelete++; }
	}

	if (found && !pingMessage) { messagePending.erase(iteratorToDelete); }
}

/*

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

*/

void InitCells() {
	//INIT CELLS:
	//5x5 (0, 0) : (200, 0)
	Cell tempCell;
	//0, 0
	tempCell.pos = sf::Vector2i(0, 0);
	tempCell.color = sf::Color(255, 255, 0, 255);
	cells.push_back(tempCell);

	//0, 1
	tempCell.pos = sf::Vector2i(0, 1);
	tempCell.color = sf::Color(51, 255, 51, 255);
	cells.push_back(tempCell);

	//0, 2
	tempCell.pos = sf::Vector2i(0, 2);
	tempCell.color = sf::Color(51, 51, 255, 255);
	cells.push_back(tempCell);

	//0, 3
	tempCell.pos = sf::Vector2i(0, 3);
	tempCell.color = sf::Color(204, 0, 153, 255);
	cells.push_back(tempCell);
	
	//0, 4
	tempCell.pos = sf::Vector2i(0, 4);
	tempCell.color = sf::Color(255, 255, 255, 255);
	cells.push_back(tempCell);

	//1, 0
	tempCell.pos = sf::Vector2i(1, 0);
	tempCell.color = sf::Color(255, 102, 0, 255);
	cells.push_back(tempCell);

	//1, 1
	tempCell.pos = sf::Vector2i(1, 1);
	tempCell.color = sf::Color(0, 102, 102, 255);
	cells.push_back(tempCell);

	//1, 2
	tempCell.pos = sf::Vector2i(1, 2);
	tempCell.color = sf::Color(0, 102, 153, 255);
	cells.push_back(tempCell);

	//1, 3
	tempCell.pos = sf::Vector2i(1, 3);
	tempCell.color = sf::Color(102, 0, 51, 255);
	cells.push_back(tempCell);

	//1, 4
	tempCell.pos = sf::Vector2i(1, 4);
	tempCell.color = sf::Color(255, 51, 0, 255);
	cells.push_back(tempCell);

	//2, 0
	tempCell.pos = sf::Vector2i(2, 0);
	tempCell.color = sf::Color(255, 255, 153, 255);
	cells.push_back(tempCell);

	//2, 1
	tempCell.pos = sf::Vector2i(2, 1);
	tempCell.color = sf::Color(255, 0, 255, 255);
	cells.push_back(tempCell);

	//2, 2
	tempCell.pos = sf::Vector2i(2, 2);
	tempCell.color = sf::Color(102, 102, 153, 255);
	cells.push_back(tempCell);

	//2, 3
	tempCell.pos = sf::Vector2i(2, 3);
	tempCell.color = sf::Color(51, 51, 0, 255);
	cells.push_back(tempCell);

	//2, 4
	tempCell.pos = sf::Vector2i(2, 4);
	tempCell.color = sf::Color(0, 153, 255, 255);
	cells.push_back(tempCell);

	//3, 0
	tempCell.pos = sf::Vector2i(3, 0);
	tempCell.color = sf::Color(24, 24, 111, 255);
	cells.push_back(tempCell);

	//3, 1
	tempCell.pos = sf::Vector2i(3, 1);
	tempCell.color = sf::Color(121, 122, 43, 255);
	cells.push_back(tempCell);

	//3, 2
	tempCell.pos = sf::Vector2i(3, 2);
	tempCell.color = sf::Color(204, 153, 255, 255);
	cells.push_back(tempCell);

	//3, 3
	tempCell.pos = sf::Vector2i(3, 3);
	tempCell.color = sf::Color(102, 255, 102, 255);
	cells.push_back(tempCell);

	//3, 4
	tempCell.pos = sf::Vector2i(3, 4);
	tempCell.color = sf::Color(160, 160, 74, 255);
	cells.push_back(tempCell);

	//4, 0
	tempCell.pos = sf::Vector2i(4, 0);
	tempCell.color = sf::Color(241, 88, 88, 255);
	cells.push_back(tempCell);

	//4, 1
	tempCell.pos = sf::Vector2i(4, 1);
	tempCell.color = sf::Color(135, 38, 131, 255);
	cells.push_back(tempCell);

	//4, 2
	tempCell.pos = sf::Vector2i(4, 2);
	tempCell.color = sf::Color(204, 0, 0, 255);
	cells.push_back(tempCell);

	//4, 3
	tempCell.pos = sf::Vector2i(4, 3);
	tempCell.color = sf::Color(201, 165, 35, 255);
	cells.push_back(tempCell);

	//4, 4
	tempCell.pos = sf::Vector2i(4, 4);
	tempCell.color = sf::Color(51, 153, 0, 255);
	cells.push_back(tempCell);


	actualCellPicked = cells[0];
}

Cell RandomCell() {
	Cell tempCell;
	int randomCellNum = (rand() % 5) + 0; //random lel

	tempCell = cells[randomCellNum];
	return tempCell;
}

int main()
{
	srand(time(NULL));

	InitCells();

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
	sf::Clock clockMovTime;
	actualState = SetGetMode(SET, State::WAITINGFORPLAYERS);

	do
	{
		sf::Time time = chrono;	//Time per turn
		if (clock.getElapsedTime().asMilliseconds() > 200)
		{
			float movementDeltaTime = clockMovTime.getElapsedTime().asSeconds();
			if (movementDeltaTime > TIME_PER_MOVEMENT && movementsPending.size() > 0) {
				for (int i = 0; i < movementsPending.size(); i++) {
					PendingMovement tempMov = movementsPending[i]; //pillar el primero de la lista
					if (tempMov.pos.x >= LEFT_CELL && tempMov.pos.x < RIGHT_CELL) {
						sf::Packet okMovePack;
						sf::Packet penMovePack;
						std::cout << "MOVE ACCEPTED" << std::endl;
						//ACK_MOVE
						okMovePack << Commands::OKM;
						penMovePack << Commands::PEM;
						int indexPlayerOkMove;
						indexPlayerOkMove = PlayerIndexByPORT(tempMov.player.port);
						if (indexPlayerOkMove != -1) {
							std::cout << "MOVE PLAYER FOUND" << std::endl;
							players[indexPlayerOkMove].pos = tempMov.pos;
							//packet al que se mueve
							okMovePack << tempMov.player.playerID;
							okMovePack << tempMov.pos.x;
							okMovePack << tempMov.pos.y;
							okMovePack << tempMov.idMov;
							okMovePack << tempMov.idMessage;
							//packet al resto
							penMovePack << players[indexPlayerOkMove].playerID;
							penMovePack << tempMov.pos.x;
							penMovePack << tempMov.pos.y;
							sock.send(okMovePack, tempMov.player.ip, tempMov.player.port);	//SE MANDA AL JUGADOR QUE SE MUEVE
							if (players.size() > 1) {
								for (int h = 0; h < players.size(); h++) {	//SE MANDA AL RESTO
									if (players[h].port != tempMov.player.port) {
										AddMessage(Commands::PEM, penMovePack, players[h].ip, players[h].port);
									}
								}
							}
						}
					}
					else {	//MOVIMIENTO DENEGADO => MANDAR UN NOK
						std::cout << "envio denied" << std::endl;
						sf::Packet nokPack;
						nokPack << Commands::NOK;
						nokPack << tempMov.player.playerID;
						//enviar ultima pos aceptada
						int indexPlayerOkMove;
						indexPlayerOkMove = PlayerIndexByPORT(tempMov.player.port);
						nokPack << players[indexPlayerOkMove].pos.x;
						nokPack << players[indexPlayerOkMove].pos.y;
						nokPack << tempMov.idMov;
						nokPack << tempMov.idMessage;
						sock.send(nokPack, tempMov.player.ip, tempMov.player.port);	//SE MANDA AL JUGADOR QUE SE MUEVE
					}
				}
				std::vector<PendingMovement>().swap(movementsPending);
				clockMovTime.restart();
			}

			//RECIEVE

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

				PendingMovement tempMov;
				sf::Vector2i movPos;
				int idMov;
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
								tempPlayer.dead = false;
								tempPlayer.colorConfirmed = false;
								players.push_back(tempPlayer);
							}

						}
						else {
							//crear usuario
							tempPlayer = MakePlayer(ipRem, portRem);
							//añadir usuario a lista
							tempPlayer.dead = false;
							tempPlayer.colorConfirmed = false;
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
						std::cout << "RECEIVED MOVE" << std::endl;
						pck >> movPos.x;
						pck >> movPos.y;
						std::cout << movPos.x << std::endl;
						pck >> idMov;
						std::cout << "IDMOV" << idMov << std::endl;
						pck >> idMessage;
						int tempPlayerid = players[PlayerIndexByPORT(portRem)].playerID;
						//comprobar que no sea uno ya enviado
						bool found = false;
						if (movementsPending.size() > 0) {
							int it = 0;
							while (it < movementsPending.size() && !found) {
								if (movementsPending[it].idMessage == idMessage && movementsPending[it].player.playerID == tempPlayerid) {
									found = true;
									std::cout << "copied" << std::endl;
								}
								it++;
							}
						}
						if (!found) {
							tempMov.pos = movPos;
							tempMov.player = players[PlayerIndexByPORT(portRem)];
							tempMov.idMov = idMov;
							tempMov.idMessage = idMessage;
							movementsPending.push_back(tempMov);
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

				//GAME LOGIC:
				if (actualState != WAITINGFORPLAYERS) {	//ALL PLAYERS CONNECTED AND READY
					//CHOOSE COLOR
					if (actualState == State::CHOSINGCOLOR) {
						actualCellPicked = RandomCell();
						sf::Packet colorPacket;
						colorPacket << Commands::CLR;
						colorPacket << actualCellPicked.color.r;
						colorPacket << actualCellPicked.color.g;
						colorPacket << actualCellPicked.color.b;
						//SEND COLOR
						for each (Player p in players) {
							AddMessage(Commands::CLR, colorPacket, p.ip, p.port);
						}

						SetGetMode(SET, State::WAITINGCLRCONFIRMATION);
					}
					else if (actualState == State::WAITINGCLRCONFIRMATION) {
						if (playersColorConfirmed >= players.size()) {	//>= para evitar que si un jugador se desconecta en mitad y el número es superior de repente que aún así se triguee. MODO SEGURO MIRAR QUE TODOS TENGAN EL FLAG DE COLOR CONFIRMED PUESTO A TRUE.
							SetGetMode(SET, State::WAITINGFORTURNEND);
						}
					}
					else if (actualState == State::WAITINGFORTURNEND) {	//WAIT FOR MOVEMENT
						if (int(time.asSeconds()) >= TIME_PER_TURN) {
							sf::Packet stopPacket;
							stopPacket << Commands::STP;
							for each (Player p in players) {
								AddMessage(Commands::STP, stopPacket, p.ip, p.port);
							}

							SetGetMode(SET, State::COMPUTINGMOVEMENT);
						}
					}
					else if (actualState == State::COMPUTINGMOVEMENT) {	//COMPUTE MOVEMENT WHEN TIME'S UP
						int playersDead = 0;
						sf::Packet deadPlayersPacket;
						deadPlayersPacket << Commands::DED;
						for (int asd = 0; asd < players.size(); asd++) {	//MIRO SI HAN MUERTO Y SUMO 1 AL NÚMERO DE PLAYERS MUERTOS
							if (players[asd].pos != actualCellPicked.pos) {	//PLAYER IS DEAD
								players[asd].dead = true;
								playersDead++;
							}
						}

						if (playersDead > 0) {	//SI EL NÚMERO DE PLAYERS MUERTOS ES > 0
							deadPlayersPacket << playersDead;
							for (int asd = 0; asd < players.size(); asd++) {	//BUCLE PARA PILLAR EL ID DE LOS MUERTOS Y METERLOS EN EL PACKET
								if (players[asd].dead) {	//PLAYER IS DEAD
									deadPlayersPacket << players[asd].playerID;
								}
							}
							for (int asd = 0; asd < players.size(); asd++) {	//BUCLE PARA MANDAR EL PACKET
								AddMessage(Commands::DED, deadPlayersPacket, players[asd].ip, players[asd].port);
							}
							
						}
					}

					//REPEAT
				}
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