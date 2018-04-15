#include <SFML/Graphics.hpp>
#include <SFML/Network.hpp>
#include <iostream>
#include <random>

#define IP_SERVER "localhost"
#define PORT_SERVER 50000

#define PERCENT_PACKETLOSS 0.01
#define TIME_PER_MOVEMENT 0.01f

enum Commands { HEY, CON, NEW, ACK, MOV, PIN, DIS, EXE, OKM, PEM, NOK };

int pos;
sf::UdpSocket sock;
int actualID = 0;
int movementID = 0;
bool firstReset = true;
sf::Vector2i desiredPos = sf::Vector2i(0, 0);
sf::Vector2i previousDesiredPos = sf::Vector2i(0, 0);

struct Player
{
	sf::Vector2i pos;
	int playerID;
	sf::Color pjColor;
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

	while (!found && (indexToDelete < messagePending.size())) {	//if(iteratorToDelete != messagePending.end()) ALTERNATIVA PARA NO USAR EL INDEXTODELETE
		if (iteratorToDelete->id == _ID) {
			//FOUND MY MESSAGE TO DELETE
			found = true;
		}
		else { indexToDelete++; iteratorToDelete++; }
	}

	if (found) { iteratorToDelete++; movementsPending.erase(movementsPending.begin(), iteratorToDelete); }	//HAGO UN iteratorToDelete++ PQ EN UN ERASE NO SE INCLUYE EL SEGUNDO PARÁMETRO. | SEGUNDA OPCIÓN POR SI NO TIRA EL ITERADOR: erase(movementsPending.begin(), movementsPending.begin()+indexToDelete+1);
}

void MovementDenied(int _ID, int _IDPlayer) {	// SOLO PARA EL CON
	int indexToDelete = 0;
	bool found = false;
	std::vector<PendingMovement>::iterator iteratorToDelete = movementsPending.begin();

	while (!found && (indexToDelete < messagePending.size())) {	//if(iteratorToDelete != messagePending.end()) ALTERNATIVA PARA NO USAR EL INDEXTODELETE
		if (iteratorToDelete->id == _ID) {
			//FOUND MY MESSAGE TO DELETE
			found = true;
		}
		else { indexToDelete++; iteratorToDelete++; }
	}

	if (found) { 
		movementsPending.erase(iteratorToDelete, movementsPending.end());
		//iteratorToDelete--;
		if(!movementsPending.empty()) desiredPos = (movementsPending.end()-1)->pos;
		else { desiredPos = players[0].pos; }	//COGE POS PLAYER
		//std::vector<Player>::iterator tempItPlayerToMove = PlayerItIndexByID(_IDPlayer); //PARA PREDICTION
		//if (tempItPlayerToMove != players.end()) {
		//	tempItPlayerToMove->pos = desiredPos;
		//}
	}	//BORRA TODO LO SIGUIENTE AL DENEGADO Y REINICIA DESIRED POS CON EL ÚLTIMO NO CONFIRMADO AÚN
}

void DibujaSFML()
{
	sf::RenderWindow window(sf::VideoMode(800, 600), "Sin acumulación en cliente");
	
	//INICIAR CHRONO PARA DELTATIME ----------------------------------------------
	sf::Clock chronoMovementDelta;
	sf::Clock chronoDeltaTime;

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
				if (event.key.code == sf::Keyboard::Left)
				{
					//sf::Packet pckLeft;
					//pckLeft << Commands::MOV;
					desiredPos.x -= 10;
					//pckLeft << desiredPos.x;
					//pckLeft << desiredPos.y;
					//AddMessage(pckLeft);
					std::cout << "MOVE LEFT" << std::endl;
					//AÑADIR MENSAJE A LISTA DE PENDIENTES: ID un int que aumenta a cada mensaje añadido | msg es el packete | time iniciado a 0 (milisegundos) MOV

				}
				else if (event.key.code == sf::Keyboard::Right)
				{
					//sf::Packet pckRight;
					//pckRight << Commands::MOV;
					desiredPos.x += 10;
					//pckRight << desiredPos.x;
					//pckRight << desiredPos.y;
					//AddMessage(pckRight);
					std::cout << "MOVE RIGHT" << std::endl;
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
				std::cout << "TIME" << std::endl;
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
						pck >> sPlayer.pos.x;
						pck >> sPlayer.pos.y;
						desiredPos = sPlayer.pos;
						previousDesiredPos = desiredPos;
						players.push_back(sPlayer); //own player siempre estará en posicion 0 del vector -- IMPORTANTE!!! SI SE CAMBIA ESTO HAY QUE CAMBIAR EL SET DEL DESIRED POS EN EL MOVEMENT DENIDED
						std::cout << "OWN ID: " << sPlayer.playerID << std::endl;
						for (int i = 0; i < numPlayers - 1; i++) { //-1 ya que no se añade a si mismo lel
							std::cout << "other jugador: " << i << std::endl;
							Player tempPlayer;
							pck >> tempPlayer.playerID;
							pck >> tempPlayer.pjColor.r;
							pck >> tempPlayer.pjColor.g;
							pck >> tempPlayer.pjColor.b;
							pck >> tempPlayer.pos.x;
							pck >> tempPlayer.pos.y;
							players.push_back(tempPlayer);
						}
						pck >> idMessage;
						MessageConfirmed(idMessage);
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
						pck >> tempPlayer.pos.x;
						pck >> tempPlayer.pos.y;

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
							tempItPlayerToMove->pos = confirmedPos;
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
							tempItPlayerToMove->pos = confirmedPos;
						}
						pck >> idMessage;
						std::cout << "ID MESSAGE" << idMessage << std::endl;
						sf::Packet pckAck;
						pckAck << Commands::ACK;
						pckAck << idMessage;
						sock.send(pckAck, IP_SERVER, PORT_SERVER);
						break;
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

		window.clear();

		sf::RectangleShape rectBlanco(sf::Vector2f(1, 600));
		rectBlanco.setFillColor(sf::Color::White);
		rectBlanco.setPosition(sf::Vector2f(200, 0));
		window.draw(rectBlanco);
		rectBlanco.setPosition(sf::Vector2f(600, 0));
		window.draw(rectBlanco);

		//For para cada jugador -> rectAvatar(players[i].pos) | rectAvatar(sf::Vector2f(players[i].pos.x, players[i].pos.y))
		for (auto p : players) {
			sf::RectangleShape rectAvatar(sf::Vector2f(60, 60));
			rectAvatar.setFillColor(p.pjColor);
			rectAvatar.setPosition(sf::Vector2f(p.pos.x, p.pos.y));
			window.draw(rectAvatar);
		}
		

		window.display();
	}

}

int main(){
	pos = 200;
	sock.setBlocking(false);
	//sistema HEY
	sf::Packet pckHey;
	pckHey << Commands::HEY;
	AddMessage(pckHey);
	DibujaSFML();
	return 0;
}