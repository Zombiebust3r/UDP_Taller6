#include <SFML/Graphics.hpp>
#include <SFML/Network.hpp>
#include <lib.h>
#include <iostream>
#include <random>

#define SET 0
#define GET 1

#define IP_SERVER "localhost"
#define PORT_SERVER 50000

#define PERCENT_PACKETLOSS 0.01
#define TIME_PER_MOVEMENT 1.0f

enum Commands { HEY, CON, NEW, ACK, MOV, PIN, DIS, EXE, OKM, PEM, NOK, DED, CLR };

int pos;
sf::UdpSocket sock;
int actualID = 0;
int movementID = 0;
bool firstReset = true;
sf::Vector2i desiredPos = sf::Vector2i(0, 0);
sf::Vector2i previousDesiredPos = sf::Vector2i(0, 0);
int timeAnimation = 100;

sf::Color backgroundColor = sf::Color(125, 125, 125, 255);

struct Cell
{
	sf::Vector2i pos;
	sf::Color color;
};

Cell actualCellToGo;

struct Player
{
	sf::Vector2i cellPos;
	sf::Vector2i pixelPos; //usaremos este para la interpolacion y prediccion
	sf::Vector2i desiredCellPos = sf::Vector2i(0, 0);
	int playerID;
	sf::Color pjColor;
	bool dead;
};

struct PendingMessage {
	sf::Packet packet;
	int id;
	float time;
};

struct PendingMovement {
	sf::Vector2i pos;
	int id;
	float time;
};

std::vector<Player> players = std::vector<Player>();

//HACER UN SISTEMA PARA ENVIO HASTA CONFIRMACIÓN:
	//UNA FUNCIÓN QUE SE LLAME EN EL RECEIVE QUE ELIMINE EL MENSAJE CONFIRMADO DE LA LISTA DE PENDIENTES. SE EJECUTA ANTES DEL ENVIO PARA NO ENVIAR ALGO YA CONFIRMADO.
	//UNA FUNCIÓN QUE PASE POR UN VECTOR DE MESNAJES (ID, MSG, TIEMPO [se añade dt cada frame]) <int, packet, int(milisegundos)> Y LOS ENVIE CADA 500ms.
std::vector<PendingMessage> messagePending = std::vector<PendingMessage>();

std::vector<PendingMovement> movementsPending = std::vector<PendingMovement>();

std::vector<Cell> cells = std::vector<Cell>();

std::vector<Player>::iterator PlayerItIndexByID(int _id) {
	std::vector<Player>::iterator it = players.begin();
	for (auto i : players) {
		if (i.playerID == _id) {
			return it;
		}
		it++;
	}
	return players.end();
}

float GetRandomFloat() {
	std::random_device rd;
	std::mt19937 gen(rd());
	std::uniform_real_distribution<float> dis(0.f, 1.f);
	return dis(gen);
}
void SendMessages(int deltaTime) {
	//std::cout << "DELTATIME: " << deltaTime << std::endl;
	//std::cout << messagePending.size() << std::endl;

	for (int i = 0; i < messagePending.size(); i++) {
		messagePending[i].time += deltaTime;
		//std::cout << messagePending[i].time << std::endl;
		if (messagePending[i].time > 500) {
			std::cout << "-------------------------------------------------------reenvia mensaje" << std::endl;
			//RESEND MESSAGE
			sock.send(messagePending[i].packet, IP_SERVER, PORT_SERVER);
			messagePending[i].time = 0.0f;
		}
	}

	/*for (auto i : messagePending) {
		i.time += deltaTime;
		//std::cout << i.time << std::endl;
		if (i.time > 0.500f) {
			std::cout << "-------------------------------------------------------reenvia mensaje" << std::endl;
			//RESEND MESSAGE
			sock.send(i.packet, IP_SERVER, PORT_SERVER);
			i.time = 0.0f;
		}
	}*/
	
}

void AddMessage(sf::Packet _pack) {	//SOLO PARA EL HEY
	PendingMessage tempPending;
	tempPending.id = actualID;
	_pack << actualID;
	tempPending.packet = _pack;
	tempPending.time = 0.0f;

	sock.send(_pack, IP_SERVER, PORT_SERVER);
	messagePending.push_back(tempPending);

	actualID++;
}

void MessageConfirmed(int _ID) {	// SOLO PARA EL CON
	int indexToDelete = 0;
	bool found = false;
	std::vector<PendingMessage>::iterator iteratorToDelete = messagePending.begin();

	while( !found && (indexToDelete < messagePending.size()) ) {	//if(iteratorToDelete != messagePending.end()) ALTERNATIVA PARA NO USAR EL INDEXTODELETE
		if (iteratorToDelete->id == _ID) {
			//FOUND MY MESSAGE TO DELETE
			found = true;
		}
		else { indexToDelete++; iteratorToDelete++; }
	}

	if (found) { messagePending.erase(iteratorToDelete); }
}

void MovementConfirmed(int _ID) {	// SOLO PARA EL CON
	int indexToDelete = 0;
	bool found = false;
	std::vector<PendingMovement>::iterator iteratorToDelete = movementsPending.begin();

	while (!found && (indexToDelete < movementsPending.size())) {	//if(iteratorToDelete != messagePending.end()) ALTERNATIVA PARA NO USAR EL INDEXTODELETE
		if (iteratorToDelete->id == _ID) {
			//FOUND MY MESSAGE TO DELETE
			found = true;
		}
		else { indexToDelete++; iteratorToDelete++; }
	}

	if (found) { iteratorToDelete++; movementsPending.erase(movementsPending.begin(), iteratorToDelete); }	
	//HAGO UN iteratorToDelete++ PQ EN UN ERASE NO SE INCLUYE EL SEGUNDO PARÁMETRO. | SEGUNDA OPCIÓN POR SI NO TIRA EL ITERADOR: erase(movementsPending.begin(), movementsPending.begin()+indexToDelete+1);
}

void MovementDenied(int _ID, int _IDPlayer) {	// SOLO PARA EL CON
	int indexToDelete = 0;
	bool found = false;
	std::vector<PendingMovement>::iterator iteratorToDelete = movementsPending.begin();

	while (!found && (indexToDelete < movementsPending.size())) {	//if(iteratorToDelete != messagePending.end()) ALTERNATIVA PARA NO USAR EL INDEXTODELETE
		if (iteratorToDelete->id == _ID) {
			//FOUND MY MESSAGE TO DELETE
			found = true;
		}
		else { indexToDelete++; iteratorToDelete++; }
	}
	if (found) { 
		movementsPending.erase(iteratorToDelete, movementsPending.end());
		if(!movementsPending.empty()) desiredPos = (movementsPending.end()-1)->pos;
		else { desiredPos = players[0].cellPos; players[0].pixelPos = players[0].cellPos * 40; }	//COGE POS PLAYER
		previousDesiredPos = desiredPos;
		//std::vector<Player>::iterator tempItPlayerToMove = PlayerItIndexByID(_IDPlayer); //PARA PREDICTION
		//if (tempItPlayerToMove != players.end()) {
		//	tempItPlayerToMove->pos = desiredPos;
		//}
	}	//BORRA TODO LO SIGUIENTE AL DENEGADO Y REINICIA DESIRED POS CON EL ÚLTIMO NO CONFIRMADO AÚN
}

void PredictionFunc(int playerIndex, sf::Vector2i curCellPos, sf::Vector2i desiredCellPos, int timeAnim) {
	sf::Vector2i distanceToTravel = desiredCellPos - curCellPos;
	distanceToTravel *= 40; //cell to pixel
	bool changeCellPos = false;
	if (distanceToTravel.x != 0) { //cambiar posicion si eso
		distanceToTravel.x /= timeAnim;
		players[playerIndex].pixelPos.x += distanceToTravel.x;

		if (distanceToTravel.x < 0 && players[playerIndex].pixelPos.x < desiredCellPos.x * 40) {
			changeCellPos = true;
		}
		else if (distanceToTravel.x > 0 && players[playerIndex].pixelPos.x > desiredCellPos.x * 40) {
			changeCellPos = true;
		}
	}
	else if (distanceToTravel.y != 0) {
		distanceToTravel.y /= timeAnim;
		players[playerIndex].pixelPos.y += distanceToTravel.y;

		if (distanceToTravel.y < 0 && players[playerIndex].pixelPos.y < desiredCellPos.y * 40) {
			changeCellPos = true;
		}
		else if (distanceToTravel.y > 0 && players[playerIndex].pixelPos.y > desiredCellPos.y * 40) {
			changeCellPos = true;
		}
	}
	if (changeCellPos) {
		players[playerIndex].cellPos = desiredCellPos;
		players[playerIndex].pixelPos = desiredCellPos * 40; //reajustar
	}
		
	
}

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

}

void DibujaSFML()
{
	sf::RenderWindow window(sf::VideoMode(800, 600), "Sin acumulación en cliente");
	
	//INICIAR CHRONO PARA DELTATIME ----------------------------------------------
	sf::Clock chronoMovementDelta;
	sf::Clock chronoDeltaTime;
	sf::Clock chronoAnimationTime;
	while (window.isOpen())
	{
		sf::Event event;

		while (window.pollEvent(event))
		{
			switch (event.type)
			{
			case sf::Event::Closed:
				window.close();
				break;
			case sf::Event::KeyPressed:
				if (event.key.code == sf::Keyboard::Left && !players[0].dead)
				{
					desiredPos.x -= 1;
					std::cout << "MOVE LEFT" << std::endl;
					//AÑADIR MENSAJE A LISTA DE PENDIENTES: ID un int que aumenta a cada mensaje añadido | msg es el packete | time iniciado a 0 (milisegundos) MOV

				}
				else if (event.key.code == sf::Keyboard::Right && !players[0].dead)
				{
					desiredPos.x += 1;
					std::cout << "MOVE RIGHT" << std::endl;
					//AÑADIR MENSAJE A LISTA DE PENDIENTES: ID un int que aumenta a cada mensaje añadido | msg es el packete | time iniciado a 0 (milisegundos) MOV
				}
				else if (event.key.code == sf::Keyboard::Up && !players[0].dead)
				{
					desiredPos.y -= 1;
					std::cout << "MOVE UP" << std::endl;
					//AÑADIR MENSAJE A LISTA DE PENDIENTES: ID un int que aumenta a cada mensaje añadido | msg es el packete | time iniciado a 0 (milisegundos) MOV

				}
				else if (event.key.code == sf::Keyboard::Down && !players[0].dead)
				{
					desiredPos.y += 1;
					std::cout << "MOVE DOWN" << std::endl;
					//AÑADIR MENSAJE A LISTA DE PENDIENTES: ID un int que aumenta a cada mensaje añadido | msg es el packete | time iniciado a 0 (milisegundos) MOV
				}
				break;
				
			default:
				break;

			}
		}
		if (firstReset) { chronoMovementDelta.restart(); firstReset = false; }
		float movementDeltaTime = chronoMovementDelta.getElapsedTime().asSeconds();

		if (movementDeltaTime > TIME_PER_MOVEMENT) {
			if (desiredPos.x != previousDesiredPos.x || desiredPos.y != previousDesiredPos.y) {
				PendingMovement tempMov;
				tempMov.id = movementID;
				movementID++;
				tempMov.pos = desiredPos;
				previousDesiredPos = desiredPos;
				tempMov.time = movementDeltaTime;
				movementsPending.push_back(tempMov);
				sf::Packet pckMovement;
				pckMovement << Commands::MOV;
				pckMovement << desiredPos.x;
				pckMovement << desiredPos.y;
				pckMovement << tempMov.id;
				AddMessage(pckMovement);
			}
			chronoMovementDelta.restart();
		}

		sf::Packet pck;
		sf::IpAddress ipRem;
		unsigned short portRem;
		sf::Socket::Status status = sock.receive(pck, ipRem, portRem);
		if (status == sf::Socket::Done)
		{
			//Gestionar si se conecta un nuevo jugador con prefijo NEW en el packet.
			//Gestionar si un jugador se mueve: Recibimos su ID y su nueva pos como dos ints (x, y) por separado con un prefijo MOV en el packet.
			int command;
			int idMessage;
			pck >> command;
			Player sPlayer;
			float randomNumber = GetRandomFloat();
			if (randomNumber > PERCENT_PACKETLOSS){
				switch (command) {
					case CON:
					{
						int numPlayers;
						pck >> numPlayers;
						std::cout << "recibo un CON con " << numPlayers << " jugs" << std::endl;
						pck >> sPlayer.playerID;
						pck >> sPlayer.pjColor.r;
						pck >> sPlayer.pjColor.g;
						pck >> sPlayer.pjColor.b;
						pck >> sPlayer.cellPos.x;
						pck >> sPlayer.cellPos.y;
						desiredPos = sPlayer.cellPos;
						previousDesiredPos = desiredPos;
						sPlayer.pixelPos = sPlayer.cellPos * 40;
						sPlayer.dead = false;
						players.push_back(sPlayer); //own player siempre estará en posicion 0 del vector -- IMPORTANTE!!! SI SE CAMBIA ESTO HAY QUE CAMBIAR EL SET DEL DESIRED POS EN EL MOVEMENT DENIDED
						std::cout << "OWN ID: " << sPlayer.playerID << std::endl;
						for (int i = 0; i < numPlayers - 1; i++) { //-1 ya que no se añade a si mismo lel
							std::cout << "other jugador: " << i << std::endl;
							Player tempPlayer;
							pck >> tempPlayer.playerID;
							pck >> tempPlayer.pjColor.r;
							pck >> tempPlayer.pjColor.g;
							pck >> tempPlayer.pjColor.b;
							pck >> tempPlayer.cellPos.x;
							pck >> tempPlayer.cellPos.y;
							tempPlayer.pixelPos = tempPlayer.cellPos * 40;
							tempPlayer.desiredCellPos = tempPlayer.cellPos;
							tempPlayer.dead = false;
							players.push_back(tempPlayer);
						}
						pck >> idMessage;
						MessageConfirmed(idMessage);

						backgroundColor = players[0].pjColor;

						break;
					}
					case NEW:
					{
						//pillar jugador y devolver ack con idmessage
						Player tempPlayer;
						pck >> tempPlayer.playerID;
						pck >> tempPlayer.pjColor.r;
						pck >> tempPlayer.pjColor.g;
						pck >> tempPlayer.pjColor.b;
						pck >> tempPlayer.cellPos.x;
						pck >> tempPlayer.cellPos.y;
						tempPlayer.pixelPos = tempPlayer.cellPos * 40;
						tempPlayer.desiredCellPos = tempPlayer.cellPos;
						tempPlayer.dead = false;

						std::cout << "recibo un nuevo jugador id: " << tempPlayer.playerID << std::endl;
						//revisar si ya tengo a este jugador i guess
						int it = 0;
						bool found = false;
						while (!found && (it < players.size())) {
							if (tempPlayer.playerID == players[it].playerID) found = true;
							it++;
						}

						if (!found) players.push_back(tempPlayer);
						else std::cout << "Se ha enviado un jugador repetido" << std::endl;
						//enviar ack
						pck >> idMessage;
						sf::Packet pckAck;
						pckAck << Commands::ACK;
						pckAck << idMessage;
						sock.send(pckAck, IP_SERVER, PORT_SERVER);
						std::cout << "enviado ACK" << std::endl;
						std::cout << "Confirmacion numero de jugadores en lista: " << players.size() << std::endl;
						break;
					}
					case PIN:
					{
						std::cout << "PING RECIBIDO" << std::endl;
						pck >> idMessage;
						sf::Packet pckAck;
						pckAck << Commands::ACK;
						pckAck << idMessage;
						sock.send(pckAck, IP_SERVER, PORT_SERVER);
						break;
					}
					case DIS: {
						int playerToDeleteID;
						pck >> playerToDeleteID;
						std::cout << "PLAYER DISCONNECTED WITH ID: " << playerToDeleteID << std::endl;
						std::vector<Player>::iterator itErased = PlayerItIndexByID(playerToDeleteID);
						if (itErased != players.end()) {
							players.erase(itErased);
						}
						pck >> idMessage;
						sf::Packet pckAck;
						pckAck << Commands::ACK;
						pckAck << idMessage;
						sock.send(pckAck, IP_SERVER, PORT_SERVER);
						break;
					}
					case EXE: {
						std::cout << "Te han desconectado" << std::endl;
						system("pause");
						window.close();
						break;
					}
					case OKM: {
						std::cout << "ok mov" << std::endl;
						int tempIDPlayer = -1;
						int idMov = -1;
						int actionToDo = -1;
						pck >> tempIDPlayer;
						sf::Vector2i confirmedPos;
						pck >> confirmedPos.x;
						pck >> confirmedPos.y;
						pck >> idMov;
						std::vector<Player>::iterator tempItPlayerToMove = PlayerItIndexByID(tempIDPlayer);
						if (tempItPlayerToMove != players.end()) {
							//tempItPlayerToMove->cellPos = confirmedPos;
							//tempItPlayerToMove->pixelPos = tempItPlayerToMove->cellPos * 40;
						}

						pck >> idMessage;
						MessageConfirmed(idMessage);	//CONFIRMO MSG
						MovementConfirmed(idMov);		//CONFIRMO EL MOVIMIENTO Y BORRO TODO LO ANTERIOR. SI RECIBO UN OKM PARA UN MOVIMIENTO YA BORRADO POR UNA CONFIRMACIÓN ANTERIOR NO HARÁ CASO PQ NO ENCONTRARÁ LO QUE BUSCA EN EL VECTOR PERO SI EL MSG.
						break;
					}
					case NOK: {
						std::cout << "movement denied" << std::endl;
						int idMov = -1;
						int idPlayer = -1;
						int actionToDo = -1;
						pck >> idPlayer;
						sf::Vector2i confirmedPos;
						pck >> confirmedPos.x;
						pck >> confirmedPos.y;
						pck >> idMov;
						pck >> idMessage;
						MessageConfirmed(idMessage);				//CONFIRMO MSG
						MovementDenied(idMov, idPlayer);			//BORRO EL HISTORIAL A PARTIR DEL DENEGADO
						players[0].cellPos = confirmedPos;
						desiredPos = confirmedPos;
						players[0].pixelPos = players[0].cellPos * 40;
						break;
					}
					case PEM: {
						int tempIDPlayer = -1;
						pck >> tempIDPlayer;
						sf::Vector2i confirmedPos;
						pck >> confirmedPos.x;
						pck >> confirmedPos.y;
						std::vector<Player>::iterator tempItPlayerToMove = PlayerItIndexByID(tempIDPlayer);
						if (tempItPlayerToMove != players.end()) {
							tempItPlayerToMove->desiredCellPos = confirmedPos;
							//tempItPlayerToMove->pixelPos = tempItPlayerToMove->cellPos * 40;
						}
						pck >> idMessage;
						sf::Packet pckAck;
						pckAck << Commands::ACK;
						pckAck << idMessage;
						sock.send(pckAck, IP_SERVER, PORT_SERVER);
						break;
					}
					case CLR: {
						sf::Color recievedColor;
						pck >> recievedColor.r;
						pck >> recievedColor.g;
						pck >> recievedColor.b;
						recievedColor.a = 255;
						actualCellToGo.color = recievedColor;
						std::cout << "COLOR RECIEVED: (" << unsigned(recievedColor.r) << ", " << unsigned(recievedColor.g) << ", " << unsigned(recievedColor.b) << ")" << std::endl;
						pck >> idMessage;
						sf::Packet pckAck;
						pckAck << Commands::ACK;
						pckAck << idMessage;
						sock.send(pckAck, IP_SERVER, PORT_SERVER);
					}
					case DED: {
						int numPlayersDead = 0;
						pck >> numPlayersDead;
						if (numPlayersDead > 0) {
							for (int asd = 0; asd < numPlayersDead; asd++) {
								int playerIDDeaded = -1;
								pck >> playerIDDeaded;
								std::vector<Player>::iterator tempItPlayerToKill = PlayerItIndexByID(playerIDDeaded);
								if (tempItPlayerToKill != players.end() && !tempItPlayerToKill->dead) {
									tempItPlayerToKill->dead = true;
									std::cout << "Player " << tempItPlayerToKill->playerID << " didn't make it in time.." << std::endl;
								}
							}
						}

						pck >> idMessage;
						sf::Packet pckAck;
						pckAck << Commands::ACK;
						pckAck << idMessage;
						sock.send(pckAck, IP_SERVER, PORT_SERVER);
					}
				} //esto esta bien tabulado
			}
			else {
				std::cout << "Se ha ignorado el mensaje con comando " << command << " (Roll: " << randomNumber << ")" << std::endl;
			}
		}

		//REENVIAR PENDIENTES CADA 500ms.
		int time = chronoDeltaTime.restart().asMilliseconds();
		//std::cout << time << std::endl;
		SendMessages(time);
		//std::cout << chronoDeltaTime.getElapsedTime().asMilliseconds() << std::endl;
		//chronoDeltaTime.restart().asMilliseconds();
		if (players.size() > 0) {

			players[0].desiredCellPos = desiredPos;
			//funcion que a partir de decide el smooth del prediction a partir del tiempo que queremos que dure la animacion
			int curTime = chronoAnimationTime.getElapsedTime().asMilliseconds();
			if (curTime > timeAnimation / 10) { //10 es el numero de movimientos antes de llegar al destino
				for (int i = 0; i < players.size(); i++) {
					PredictionFunc(i, players[i].cellPos, players[i].desiredCellPos, timeAnimation / 10);
				}
				chronoAnimationTime.restart();
			}
		}
		window.clear();


		sf::RectangleShape bg(sf::Vector2f(800, 600));
		bg.setFillColor(backgroundColor);
		bg.setPosition(sf::Vector2f(0, 0));
		window.draw(bg);

		sf::RectangleShape rectToSearch(sf::Vector2f(80, 80));
		rectToSearch.setFillColor(actualCellToGo.color);
		rectToSearch.setOutlineThickness(2.0f);
		rectToSearch.setOutlineColor(sf::Color::Black);
		rectToSearch.setPosition(sf::Vector2f(200 + actualCellToGo.pos.x * 80, actualCellToGo.pos.y * 80));
		window.draw(rectToSearch);

		sf::RectangleShape rectBlanco(sf::Vector2f(1, 600));
		rectBlanco.setFillColor(sf::Color::Black);
		rectBlanco.setPosition(sf::Vector2f(199, 0));
		window.draw(rectBlanco);
		rectBlanco.setPosition(sf::Vector2f(600, 0));
		window.draw(rectBlanco);
		
		//DRAW CELLS
		for each (Cell c in cells)
		{
			sf::RectangleShape rectAvatar(sf::Vector2f(80, 80));
			rectAvatar.setFillColor(c.color);
			rectAvatar.setPosition(sf::Vector2f(200 + c.pos.x * 80, c.pos.y * 80));
			window.draw(rectAvatar);

		}

		//For para cada jugador -> rectAvatar(players[i].pos) | rectAvatar(sf::Vector2f(players[i].pos.x, players[i].pos.y))
		for (auto p : players) {
			if (!p.dead) {
				sf::RectangleShape rectAvatar(sf::Vector2f(40, 40));
				rectAvatar.setFillColor(p.pjColor);
				rectAvatar.setOutlineThickness(2.0f);
				rectAvatar.setOutlineColor(sf::Color::Black);
				rectAvatar.setPosition(sf::Vector2f(200 + p.pixelPos.x, p.pixelPos.y));
				window.draw(rectAvatar);
			}
		}

		

		window.display();
	}

}

int main(){
	pos = 200;
	InitCells();
	actualCellToGo.pos = sf::Vector2i(2, 6);
	actualCellToGo.color = backgroundColor;
	sock.setBlocking(false);
	//sistema HEY
	sf::Packet pckHey;
	pckHey << Commands::HEY;
	AddMessage(pckHey);
	DibujaSFML();
	return 0;
}