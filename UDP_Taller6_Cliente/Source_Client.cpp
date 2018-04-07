#include <SFML/Graphics.hpp>
#include <SFML/Network.hpp>
#include <iostream>
#include <random>

#include "InputMemoryBitStream.h"
#include "InputMemoryStream.h"
#include "OutputMemoryBitStream.h"
#include "OutputMemoryStream.h"

#define IP_SERVER "localhost"
#define PORT_SERVER 50000

#define PERCENT_PACKETLOSS 0.3

enum Commands { HEY, CON, NEW, ACK, MOV, PIN, DIS, EXE };

int pos;
sf::UdpSocket sock;
unsigned short actualID = 0;

struct Player
{
	sf::Vector2<short> pos;
	uint8_t playerID;
	sf::Color pjColor;
};

struct PendingMessage {
	sf::Packet packet;
	unsigned short id;
	float time;
};
std::vector<Player> players = std::vector<Player>();

//HACER UN SISTEMA PARA ENVIO HASTA CONFIRMACIÓN:
	//UNA FUNCIÓN QUE SE LLAME EN EL RECEIVE QUE ELIMINE EL MENSAJE CONFIRMADO DE LA LISTA DE PENDIENTES. SE EJECUTA ANTES DEL ENVIO PARA NO ENVIAR ALGO YA CONFIRMADO.
	//UNA FUNCIÓN QUE PASE POR UN VECTOR DE MESNAJES (ID, MSG, TIEMPO [se añade dt cada frame]) <int, packet, int(milisegundos)> Y LOS ENVIE CADA 500ms.
std::vector<PendingMessage> messagePending = std::vector<PendingMessage>();

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

void AddMessage(OutputMemoryStream _oms) {	//SOLO PARA EL HEY
	PendingMessage tempPending;
	tempPending.id = actualID;
	_oms.Write(actualID);
	sf::Packet pack;
	pack << _oms.GetBufferPtr();
	pack << _oms.GetLength();
	tempPending.packet = pack;
	tempPending.time = 0.0f;

	sock.send(pack, IP_SERVER, PORT_SERVER);
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

void DibujaSFML()
{
	sf::RenderWindow window(sf::VideoMode(800, 600), "Sin acumulación en cliente");
	
	//INICIAR CHRONO PARA DELTATIME ----------------------------------------------
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
					sf::Packet pckLeft;
					int posAux = pos - 1;
					pckLeft << posAux;
					//sock.send(pckLeft, IP_SERVER, PORT_SERVER);
					//AÑADIR MENSAJE A LISTA DE PENDIENTES: ID un int que aumenta a cada mensaje añadido | msg es el packete | time iniciado a 0 (milisegundos) MOV

				}
				else if (event.key.code == sf::Keyboard::Right)
				{
					sf::Packet pckRight;
					int posAux = pos + 1;
					pckRight << posAux;
					//sock.send(pckRight, IP_SERVER, PORT_SERVER);
					//AÑADIR MENSAJE A LISTA DE PENDIENTES: ID un int que aumenta a cada mensaje añadido | msg es el packete | time iniciado a 0 (milisegundos) MOV
				}
				break;
				
			default:
				break;

			}
		}
		
		sf::Packet pck;
		sf::IpAddress ipRem;
		unsigned short portRem;
		sf::Socket::Status status = sock.receive(pck, ipRem, portRem);
		if (status == sf::Socket::Done)
		{
			//Gestionar si se conecta un nuevo jugador con prefijo NEW en el packet.
			//Gestionar si un jugador se mueve: Recibimos su ID y su nueva pos como dos ints (x, y) por separado con un prefijo MOV en el packet.
			char* message = NULL;
			uint32_t size;
			pck >> message;
			pck >> size;
			InputMemoryStream ims(message, size);
			uint8_t command;
			ims.Read(&command);
			Player sPlayer;
			float randomNumber = GetRandomFloat();
			if (randomNumber > PERCENT_PACKETLOSS){
				uint8_t idMessage;
				switch (command) {
					case CON:
					{
						uint8_t numPlayers;
						ims.Read(&numPlayers);
						std::cout << "recibo un CON con " << unsigned (numPlayers) << " jugs" << std::endl;
						ims.Read(&sPlayer.playerID);
						ims.Read(&sPlayer.pjColor.r);
						ims.Read(&sPlayer.pjColor.g);
						ims.Read(&sPlayer.pjColor.b);
						ims.Read(&sPlayer.pos.x);
						ims.Read(&sPlayer.pos.y);
						players.push_back(sPlayer); //own player siempre estará en posicion 0 del vector
						std::cout << "OWN ID: " << unsigned(sPlayer.playerID) << std::endl;
						for (int i = 0; i < numPlayers - 1; i++) { //-1 ya que no se añade a si mismo lel
							std::cout << "other jugador: " << i << std::endl;
							Player tempPlayer;
							ims.Read(&tempPlayer.playerID);
							ims.Read(&tempPlayer.pjColor.r);
							ims.Read(&tempPlayer.pjColor.g);
							ims.Read(&tempPlayer.pjColor.b);
							ims.Read(&tempPlayer.pos.x);
							ims.Read(&tempPlayer.pos.y);
							players.push_back(tempPlayer);
						}
						ims.Read(&idMessage);
						MessageConfirmed(idMessage);
						break;
					}
					case NEW:
					{
						//pillar jugador y devolver ack con idmessage
						Player tempPlayer;
						ims.Read(&tempPlayer.playerID);
						ims.Read(&tempPlayer.pjColor.r);
						ims.Read(&tempPlayer.pjColor.g);
						ims.Read(&tempPlayer.pjColor.b);
						ims.Read(&tempPlayer.pos.x);
						ims.Read(&tempPlayer.pos.y);

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
						ims.Read(&idMessage);
						sf::Packet pckAck;
						OutputMemoryStream omsAck;
						omsAck.Write(uint8_t(Commands::ACK));
						omsAck.Write(idMessage);
						pckAck << omsAck.GetBufferPtr();
						pckAck << omsAck.GetLength();
						sock.send(pckAck, IP_SERVER, PORT_SERVER);
						std::cout << "enviado ACK" << std::endl;
						std::cout << "Confirmacion numero de jugadores en lista: " << players.size() << std::endl;
						break;
					}
					case PIN:
					{
						std::cout << "PING RECIBIDO" << std::endl;
						ims.Read(&idMessage);
						sf::Packet pckAck;
						OutputMemoryStream omsAck;
						pckAck << Commands::ACK;
						omsAck.Write(idMessage);
						pckAck << omsAck.GetBufferPtr();
						pckAck << omsAck.GetLength();
						sock.send(pckAck, IP_SERVER, PORT_SERVER);
						break;
					}
					case DIS: {
						uint8_t playerToDeleteID;
						ims.Read(&playerToDeleteID);
						std::cout << "PLAYER DISCONNECTED WITH ID: " << playerToDeleteID << std::endl;
						std::vector<Player>::iterator itErased = PlayerItIndexByID(playerToDeleteID);
						if (itErased != players.end()) {
							players.erase(itErased);
						}
						ims.Read(&idMessage);

						sf::Packet pckAck;
						OutputMemoryStream omsAck;
						pckAck << Commands::ACK;
						omsAck.Write(idMessage);
						pckAck << omsAck.GetBufferPtr();
						pckAck << omsAck.GetLength();
						sock.send(pckAck, IP_SERVER, PORT_SERVER);
						break;
					}
					case EXE: {
						std::cout << "Te han desconectado" << std::endl;
						system("pause");
						window.close();
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
	OutputMemoryStream oms;
	//sf::Packet pckHey;
	oms.Write(uint8_t(Commands::HEY));
	//pckHey << Commands::HEY;
	//std::cout << unsigned(oms.GetBufferPtr()) << std::endl;
	AddMessage(oms);
	DibujaSFML();
	return 0;
}