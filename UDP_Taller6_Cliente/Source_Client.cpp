#include <SFML/Graphics.hpp>
#include <SFML/Network.hpp>
#include <iostream>

#define IP_SERVER "localhost"
#define PORT_SERVER 50000

enum Commands { HEY, CON, NEW, ACK, MOV };

int pos;
sf::UdpSocket sock;
int actualID = 0;

struct Player
{
	sf::Vector2i pos;
	int playerID;
	sf::Color pjColor;
};

struct PendingMessage {
	sf::Packet packet;
	int id;
	int time;
};
std::vector<Player> players = std::vector<Player>();

//HACER UN SISTEMA PARA ENVIO HASTA CONFIRMACIÓN:
	//UNA FUNCIÓN QUE SE LLAME EN EL RECEIVE QUE ELIMINE EL MENSAJE CONFIRMADO DE LA LISTA DE PENDIENTES. SE EJECUTA ANTES DEL ENVIO PARA NO ENVIAR ALGO YA CONFIRMADO.
	//UNA FUNCIÓN QUE PASE POR UN VECTOR DE MESNAJES (ID, MSG, TIEMPO [se añade dt cada frame]) <int, packet, int(milisegundos)> Y LOS ENVIE CADA 500ms.
std::vector<PendingMessage> messagePending = std::vector<PendingMessage>();

void SendMessages(int deltaTime) {
	//for()
		//if(msg[i].time > 500ms) { reenviar + resetear a 0 el time }
		//else { time + chronoDeltaTime.getTime().asMilliseconds(); }
	for (auto i : messagePending) {
		i.time += deltaTime;
		if (i.time > 500) {
			//RESEND MESSAGE
			sock.send(i.packet, IP_SERVER, PORT_SERVER);
			i.time = 0;
		}
	}
	
}

void AddMessage(sf::Packet _pack) {	//SOLO PARA EL HEY
	PendingMessage tempPending;
	tempPending.id = actualID;
	_pack << actualID;
	tempPending.packet = _pack;
	tempPending.time = 0;

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
					sock.send(pckLeft, IP_SERVER, PORT_SERVER);
					//AÑADIR MENSAJE A LISTA DE PENDIENTES: ID un int que aumenta a cada mensaje añadido | msg es el packete | time iniciado a 0 (milisegundos) MOV

				}
				else if (event.key.code == sf::Keyboard::Right)
				{
					sf::Packet pckRight;
					int posAux = pos + 1;
					pckRight << posAux;
					sock.send(pckRight, IP_SERVER, PORT_SERVER);
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
			int command;
			int idMessage;
			pck >> command;
			Player sPlayer;
			switch (command) {
			case CON:
				int numPlayers;
				pck >> numPlayers;
				std::cout << "recibo un CON con " << numPlayers << " jugs" << std::endl;
				pck >> sPlayer.playerID;
				pck >> sPlayer.pjColor.r;
				pck >> sPlayer.pjColor.g;
				pck >> sPlayer.pjColor.b;
				pck >> sPlayer.pos.x;
				pck >> sPlayer.pos.y;
				players.push_back(sPlayer); //own player siempre estará en posicion 0 del vector
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
			case NEW:
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
		}

		//REENVIAR PENDIENTES CADA 500ms.
		SendMessages(chronoDeltaTime.getElapsedTime().asMilliseconds());
		chronoDeltaTime.restart();

		window.clear();

		sf::RectangleShape rectBlanco(sf::Vector2f(1, 600));
		rectBlanco.setFillColor(sf::Color::White);
		rectBlanco.setPosition(sf::Vector2f(200, 0));
		window.draw(rectBlanco);
		rectBlanco.setPosition(sf::Vector2f(600, 0));
		window.draw(rectBlanco);

		//For para cada jugador -> rectAvatar(players[i].pos) | rectAvatar(sf::Vector2f(players[i].pos.x, players[i].pos.y))
		sf::RectangleShape rectAvatar(sf::Vector2f(60, 60));
		rectAvatar.setFillColor(sf::Color::Green);
		rectAvatar.setPosition(sf::Vector2f(pos, 280));
		window.draw(rectAvatar);

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